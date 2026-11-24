/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_SRC_APP_STATE_H_
#define FIREBASE_APP_SRC_APP_STATE_H_

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/logger.h"
#include "app/src/service_interfaces.h"

namespace firebase {
namespace internal {

class AppInternal;

// Reference-counted core owned by firebase::App and kept alive by all product
// handles derived from it via aliasing std::shared_ptr handles.
//
// Ownership graph (strictly acyclic):
//   App              --strong--> AppState
//   Product handle   --strong--> AppState (aliasing shared_ptr<ProductImpl>)
//   AppState         --strong--> Service (owned_services_)
//   Service          --weak----> AppState (plus non-owning AppState&)
//
// When the last App and product handle drop, ~AppState() tears down all owned
// services in priority + reverse creation order while AppState itself (name,
// options, logger, platform app/JNI) is still fully alive, then tears down
// platform state without touching any global App registry.
class AppState : public std::enable_shared_from_this<AppState> {
 public:
  AppState(std::string name, AppOptions options);
  ~AppState();

  AppState(const AppState&) = delete;
  AppState& operator=(const AppState&) = delete;

  const std::string& name() const { return name_; }
  const AppOptions& options() const { return options_; }
  AppOptions& mutable_options() { return options_; }

  const std::map<std::string, InitResult>& init_results() const {
    return init_results_;
  }
  std::map<std::string, InitResult>& mutable_init_results() {
    return init_results_;
  }

  Logger& logger() { return logger_; }
  const Logger& logger() const { return logger_; }

  // Creates an App value handle referencing this AppState.
  // Must only be called while this AppState has a live strong reference (not
  // inside ~AppState()).
  App MakeAppHandle() { return App(shared_from_this()); }

  // Safely attempts to create an App value handle; returns an invalid App if
  // this AppState is already being destroyed.
  App TryMakeAppHandle() { return App(weak_from_this().lock()); }

  // Platform-specific state managed by app_{desktop,android,ios,stub}.*
  AppInternal* platform_data() const { return platform_data_; }
  void set_platform_data(AppInternal* data) { platform_data_ = data; }

#if FIREBASE_PLATFORM_ANDROID
  jobject activity() const { return activity_; }
  void set_activity(jobject activity) { activity_ = activity; }
  void set_owns_cached_classes(bool owns) { owns_cached_classes_ = owns; }
  bool owns_cached_classes() const { return owns_cached_classes_; }
#endif  // FIREBASE_PLATFORM_ANDROID

  // Looks up an existing service by (type_tag, instance_key) and returns a
  // non-owning raw pointer, or nullptr if not present (or already removed
  // during teardown). Safe to call from service destructors.
  template <typename T>
  T* FindService(const void* type_tag, std::string_view key = {}) const {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    MutexLock lock(services_mutex_);
    auto it = index_.find(ServiceKey{type_tag, std::string(key)});
    if (it == index_.end()) return nullptr;
    return static_cast<T*>(it->second);
  }

  // Looks up an existing service and returns an aliasing shared_ptr<T> sharing
  // this AppState's control block (or empty if not found or AppState is being
  // destroyed).
  template <typename T>
  std::shared_ptr<T> FindServiceHandle(const void* type_tag,
                                       std::string_view key = {}) {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    auto self = weak_from_this().lock();
    if (!self) return {};
    MutexLock lock(services_mutex_);
    auto it = index_.find(ServiceKey{type_tag, std::string(key)});
    if (it == index_.end()) return {};
    return std::shared_ptr<T>(std::move(self), static_cast<T*>(it->second));
  }

  // Finds or creates a service attached to this AppState and returns an
  // aliasing std::shared_ptr<T> sharing this AppState's control block.
  //
  // `factory` is invoked as `std::shared_ptr<T>(AppState&)` (or returning
  // `std::unique_ptr<T>`) only if no service currently exists for
  // (type_tag, key). Returning nullptr from `factory` signals initialization
  // failure and does not cache an entry.
  template <typename T, typename Factory>
  std::shared_ptr<T> GetOrCreateService(const void* type_tag,
                                        std::string_view key,
                                        Factory&& factory) {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    auto self = weak_from_this().lock();
    if (!self) return {};

    MutexLock lock(services_mutex_);
    ServiceKey lookup_key{type_tag, std::string(key)};
    auto it = index_.find(lookup_key);
    if (it != index_.end()) {
      return std::shared_ptr<T>(std::move(self), static_cast<T*>(it->second));
    }

    std::shared_ptr<T> created = factory(*this);
    if (!created) return {};

    T* raw = created.get();
    index_.emplace(lookup_key, static_cast<Service*>(raw));
    owned_services_.push_back(
        ServiceEntry{type_tag, std::move(lookup_key.instance_key),
                     std::static_pointer_cast<Service>(std::move(created))});
    if constexpr (std::is_base_of_v<AuthTokenProvider, T>) {
      if (type_tag == &kAuthServiceTag && key.empty()) {
        auth_token_provider_.store(raw, std::memory_order_release);
      }
    }
    if constexpr (std::is_base_of_v<AppCheckTokenProvider, T>) {
      if (type_tag == &kAppCheckServiceTag && key.empty()) {
        app_check_token_provider_.store(raw, std::memory_order_release);
      }
    }
    return std::shared_ptr<T>(std::move(self), raw);
  }

  // Finds or creates a service that may later be detached from AppState (e.g.,
  // Firestore::Terminate()) while existing handles remain valid. Returns both
  // the owning AppState shared_ptr and the service's own shared_ptr.
  template <typename T, typename Factory>
  std::pair<std::shared_ptr<AppState>, std::shared_ptr<T>>
  GetOrCreateDetachableService(const void* type_tag, std::string_view key,
                               Factory&& factory) {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    auto self = weak_from_this().lock();
    if (!self) return {};

    MutexLock lock(services_mutex_);
    ServiceKey lookup_key{type_tag, std::string(key)};
    auto it = index_.find(lookup_key);
    if (it != index_.end()) {
      for (const auto& entry : owned_services_) {
        if (entry.service.get() == it->second) {
          return {std::move(self), std::static_pointer_cast<T>(entry.service)};
        }
      }
    }

    std::shared_ptr<T> created = factory(*this);
    if (!created) return {};

    Service* raw_service = static_cast<Service*>(created.get());
    index_.emplace(lookup_key, raw_service);
    owned_services_.push_back(
        ServiceEntry{type_tag, std::move(lookup_key.instance_key),
                     std::static_pointer_cast<Service>(created)});
    return {std::move(self), std::move(created)};
  }

  // Removes a service from this AppState's index and ownership list without
  // holding services_mutex_ when the released reference drops. If `expected`
  // is non-null, detaches only if the indexed entry matches `expected`.
  bool DetachService(const void* type_tag, std::string_view key = {},
                     const Service* expected = nullptr);

  // Typed lock-free convenience accessors for cross-product token providers on
  // this App (avoids lock-order inversion when called from product internal
  // mutexes such as Firestore::EnsureClientConfigured).
  AuthTokenProvider* auth_token_provider() const {
    return auth_token_provider_.load(std::memory_order_acquire);
  }

  AppCheckTokenProvider* app_check_token_provider() const {
    return app_check_token_provider_.load(std::memory_order_acquire);
  }

  // Static helper so internal code can extract AppState from a public App.
  static const std::shared_ptr<AppState>& FromApp(const App& app) {
    return app.state_;
  }

 private:
  struct ServiceKey {
    const void* type_tag;
    std::string instance_key;

    bool operator<(const ServiceKey& other) const {
      if (type_tag != other.type_tag) return type_tag < other.type_tag;
      return instance_key < other.instance_key;
    }
  };

  struct ServiceEntry {
    const void* type_tag;
    std::string instance_key;
    std::shared_ptr<Service> service;
  };

  void DestroyServices();
  // Implemented per-platform in app_{desktop,android,ios,stub}.*
  void DestroyPlatformData();

  std::string name_;
  AppOptions options_;
  std::map<std::string, InitResult> init_results_;
  SystemLogger system_logger_;
  Logger logger_;

  AppInternal* platform_data_ = nullptr;
#if FIREBASE_PLATFORM_ANDROID
  jobject activity_ = nullptr;
  bool owns_cached_classes_ = false;
#endif  // FIREBASE_PLATFORM_ANDROID

  mutable Mutex services_mutex_;
  std::vector<ServiceEntry> owned_services_;
  std::map<ServiceKey, Service*> index_;
  std::atomic<AuthTokenProvider*> auth_token_provider_{nullptr};
  std::atomic<AppCheckTokenProvider*> app_check_token_provider_{nullptr};
};

// Convenience service adapter for attaching an arbitrary teardown callback
// (e.g., analytics::Terminate / messaging::Terminate) directly onto an
// AppState.
class TeardownGuardService : public Service {
 public:
  explicit TeardownGuardService(std::function<void()> on_destroy)
      : on_destroy_(std::move(on_destroy)) {}

  ~TeardownGuardService() override {
    if (on_destroy_) {
      auto fn = std::move(on_destroy_);
      fn();
    }
  }

  void Cancel() { on_destroy_ = nullptr; }

 private:
  std::function<void()> on_destroy_;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_STATE_H_
