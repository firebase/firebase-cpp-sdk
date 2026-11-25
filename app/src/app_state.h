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
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/logger.h"
#include "app/src/service_interfaces.h"

namespace firebase {
namespace internal {

class AppInternal;
struct TokenSubscriptionHub;

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
  AppInternal* platform_data() const {
    return platform_data_.load(std::memory_order_acquire);
  }
  void set_platform_data(AppInternal* data) {
    platform_data_.store(data, std::memory_order_release);
  }

#if FIREBASE_PLATFORM_ANDROID
  jobject activity() const { return activity_; }
  void set_activity(jobject activity) { activity_ = activity; }
  void set_owns_cached_classes(bool owns) { owns_cached_classes_ = owns; }
  bool owns_cached_classes() const { return owns_cached_classes_; }
#endif  // FIREBASE_PLATFORM_ANDROID

#if FIREBASE_PLATFORM_ANDROID || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  void set_owns_native_lease(bool owns) { owns_native_lease_ = owns; }
  bool owns_native_lease() const { return owns_native_lease_; }
#endif

  // Looks up an existing service by (type_tag, instance_key) and returns a
  // non-owning raw pointer, or nullptr if not present (or already removed
  // during teardown). Safe to call from service destructors.
  template <typename T>
  T* FindService(const void* type_tag, std::string_view key = {}) const {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    std::lock_guard<std::mutex> lock(services_mutex_);
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
    std::lock_guard<std::mutex> lock(services_mutex_);
    auto it = index_.find(ServiceKey{type_tag, std::string(key)});
    if (it == index_.end()) return {};
    return std::shared_ptr<T>(std::move(self), static_cast<T*>(it->second));
  }

  // Finds or creates a service attached to this AppState using per-key
  // single-flight initialization so `factory(*this)` never runs while holding
  // `services_mutex_`. Returns an aliasing std::shared_ptr<T> sharing this
  // AppState's control block, or empty on failure or teardown.
  template <typename T, typename Factory>
  std::shared_ptr<T> GetOrCreateService(const void* type_tag,
                                        std::string_view key,
                                        Factory&& factory) {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    auto self = weak_from_this().lock();
    if (!self) return {};

    ServiceKey lookup_key{type_tag, std::string(key)};
    const std::thread::id tid = std::this_thread::get_id();

    for (;;) {
      {
        std::unique_lock<std::mutex> lock(services_mutex_);
        if (destroying_services_) return {};
        auto it = index_.find(lookup_key);
        if (it != index_.end()) {
          return std::shared_ptr<T>(std::move(self),
                                    static_cast<T*>(it->second));
        }
        auto inflight = in_flight_inits_.find(lookup_key);
        if (inflight != in_flight_inits_.end()) {
          if (inflight->second == tid) {
            return {};
          }
          services_cv_.wait(lock, [this, &lookup_key]() {
            return destroying_services_ ||
                   in_flight_inits_.find(lookup_key) == in_flight_inits_.end();
          });
          continue;
        }
        in_flight_inits_.emplace(lookup_key, tid);
      }

      InFlightInitGuard guard(this, lookup_key);
      std::shared_ptr<T> created = factory(*this);

      T* raw = created.get();
      AuthTokenProvider* attached_auth = nullptr;
      std::shared_ptr<T> discard_outside_lock;
      {
        std::lock_guard<std::mutex> lock(services_mutex_);
        guard.ClearLocked();
        if (!created || destroying_services_) {
          discard_outside_lock = std::move(created);
          raw = nullptr;
        } else {
          index_.emplace(lookup_key, static_cast<Service*>(raw));
          owned_services_.push_back(ServiceEntry{
              type_tag, lookup_key.instance_key,
              std::static_pointer_cast<Service>(std::move(created))});
          if constexpr (std::is_base_of_v<AuthTokenProvider, T>) {
            if (type_tag == &kAuthServiceTag && key.empty()) {
              auth_token_provider_.store(raw, std::memory_order_release);
              attached_auth = raw;
            }
          }
          if constexpr (std::is_base_of_v<AppCheckTokenProvider, T>) {
            if (type_tag == &kAppCheckServiceTag && key.empty()) {
              app_check_token_provider_.store(raw, std::memory_order_release);
            }
          }
        }
        services_cv_.notify_all();
      }
      discard_outside_lock.reset();
      if (raw == nullptr) return {};
      if (attached_auth != nullptr) {
        OnAuthTokenProviderAttached(attached_auth);
      }
      return std::shared_ptr<T>(std::move(self), raw);
    }
  }

  // Type-safe overload deriving the service tag automatically from T.
  template <typename T, typename Factory>
  std::shared_ptr<T> GetOrCreateService(std::string_view key,
                                        Factory&& factory) {
    return GetOrCreateService<T>(ServiceTypeTag<T>(), key,
                                 std::forward<Factory>(factory));
  }

  // Finds or creates a service that may later be detached from AppState (e.g.,
  // Firestore::Terminate()) while existing handles remain valid. Uses per-key
  // single-flight initialization so `factory(*this)` runs outside
  // `services_mutex_`.
  template <typename T, typename Factory>
  std::pair<std::shared_ptr<AppState>, std::shared_ptr<T>>
  GetOrCreateDetachableService(const void* type_tag, std::string_view key,
                               Factory&& factory) {
    static_assert(std::is_base_of_v<Service, T>, "T must derive from Service");
    auto self = weak_from_this().lock();
    if (!self) return {};

    ServiceKey lookup_key{type_tag, std::string(key)};
    const std::thread::id tid = std::this_thread::get_id();

    for (;;) {
      {
        std::unique_lock<std::mutex> lock(services_mutex_);
        if (destroying_services_) return {};
        auto it = index_.find(lookup_key);
        if (it != index_.end()) {
          for (const auto& entry : owned_services_) {
            if (entry.service.get() == it->second) {
              return {std::move(self),
                      std::static_pointer_cast<T>(entry.service)};
            }
          }
        }
        auto inflight = in_flight_inits_.find(lookup_key);
        if (inflight != in_flight_inits_.end()) {
          if (inflight->second == tid) {
            return {};
          }
          services_cv_.wait(lock, [this, &lookup_key]() {
            return destroying_services_ ||
                   in_flight_inits_.find(lookup_key) == in_flight_inits_.end();
          });
          continue;
        }
        in_flight_inits_.emplace(lookup_key, tid);
      }

      InFlightInitGuard guard(this, lookup_key);
      std::shared_ptr<T> created = factory(*this);

      std::shared_ptr<T> discard_outside_lock;
      std::shared_ptr<T> result;
      {
        std::lock_guard<std::mutex> lock(services_mutex_);
        guard.ClearLocked();
        if (!created || destroying_services_) {
          discard_outside_lock = std::move(created);
        } else {
          Service* raw_service = static_cast<Service*>(created.get());
          index_.emplace(lookup_key, raw_service);
          owned_services_.push_back(
              ServiceEntry{type_tag, lookup_key.instance_key,
                           std::static_pointer_cast<Service>(created)});
          result = std::move(created);
        }
        services_cv_.notify_all();
      }
      discard_outside_lock.reset();
      if (!result) return {};
      return {std::move(self), std::move(result)};
    }
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

  // Reactive RAII subscriptions for cross-product Auth and App Check token
  // changes. Safe regardless of whether the consumer or provider is created
  // first, and guaranteed to synchronize destruction against in-flight
  // notifications.
  TokenSubscription SubscribeAuthState(std::function<void()> on_changed,
                                       bool keep_token_refresh_active = false);
  TokenSubscription SubscribeAppCheckToken(
      std::function<void(const std::string& token)> on_changed);

  // Called by AuthTokenProvider / AppCheckTokenProvider implementations when
  // state or tokens change. Invokes active subscriptions outside internal
  // locks.
  void NotifyAuthStateChanged();
  void NotifyAppCheckTokenChanged(const std::string& token);

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

  class InFlightInitGuard {
   public:
    InFlightInitGuard(AppState* owner, const ServiceKey& key)
        : owner_(owner), key_(key) {}
    ~InFlightInitGuard() {
      if (active_) {
        std::lock_guard<std::mutex> lock(owner_->services_mutex_);
        owner_->in_flight_inits_.erase(key_);
        owner_->services_cv_.notify_all();
      }
    }
    void ClearLocked() {
      owner_->in_flight_inits_.erase(key_);
      active_ = false;
    }

   private:
    AppState* owner_;
    const ServiceKey& key_;
    bool active_ = true;
  };

  void OnAuthTokenProviderAttached(AuthTokenProvider* provider);
  void OnAuthTokenProviderDetaching();
  void DestroyServices();
  // Implemented per-platform in app_{desktop,android,ios,stub}.*
  void DestroyPlatformData();

  std::string name_;
  AppOptions options_;
  std::map<std::string, InitResult> init_results_;
  SystemLogger system_logger_;
  Logger logger_;

  std::atomic<AppInternal*> platform_data_{nullptr};
#if FIREBASE_PLATFORM_ANDROID
  jobject activity_ = nullptr;
  bool owns_cached_classes_ = false;
#endif  // FIREBASE_PLATFORM_ANDROID
#if FIREBASE_PLATFORM_ANDROID || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  bool owns_native_lease_ = false;
#endif

  mutable std::mutex services_mutex_;
  std::condition_variable services_cv_;
  bool destroying_services_ = false;
  std::map<ServiceKey, std::thread::id> in_flight_inits_;
  std::vector<ServiceEntry> owned_services_;
  std::map<ServiceKey, Service*> index_;
  std::atomic<AuthTokenProvider*> auth_token_provider_{nullptr};
  std::atomic<AppCheckTokenProvider*> app_check_token_provider_{nullptr};
  std::shared_ptr<TokenSubscriptionHub> token_hub_;
};

// Convenience service adapter for attaching an arbitrary teardown callback
// (e.g., analytics::Terminate / messaging::Terminate) directly onto an
// AppState. Thread-safe against concurrent Cancel() and destruction.
class TeardownGuardService : public Service {
 public:
  explicit TeardownGuardService(std::function<void()> on_destroy)
      : on_destroy_(std::move(on_destroy)) {}

  ~TeardownGuardService() override {
    std::function<void()> fn;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      fn = std::move(on_destroy_);
      on_destroy_ = nullptr;
    }
    if (fn) {
      fn();
    }
  }

  void Cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    on_destroy_ = nullptr;
  }

 private:
  std::mutex mutex_;
  std::function<void()> on_destroy_;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_STATE_H_
