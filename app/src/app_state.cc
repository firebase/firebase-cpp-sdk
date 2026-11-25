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

#include "app/src/app_state.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "app/src/callback.h"
#include "app/src/log.h"

namespace firebase {
namespace internal {

const char kAuthServiceTag = 0;
const char kAppCheckServiceTag = 0;

namespace {

// Per-subscription synchronization gate ensuring
// TokenSubscription::~TokenSubscription blocks until any in-flight notification
// callback on another thread completes.
struct CallbackGate {
  std::mutex mutex;
  std::condition_variable cv;
  bool active = true;
  int in_flight = 0;
  std::vector<std::thread::id> active_threads;

  bool TryEnter() {
    std::lock_guard<std::mutex> lock(mutex);
    if (!active) return false;
    ++in_flight;
    active_threads.push_back(std::this_thread::get_id());
    return true;
  }

  void Exit() {
    std::lock_guard<std::mutex> lock(mutex);
    const std::thread::id self = std::this_thread::get_id();
    auto it = std::find(active_threads.begin(), active_threads.end(), self);
    if (it != active_threads.end()) {
      active_threads.erase(it);
    }
    --in_flight;
    if (!active) {
      cv.notify_all();
    }
  }

  void CancelAndWait() {
    std::unique_lock<std::mutex> lock(mutex);
    active = false;
    const std::thread::id self = std::this_thread::get_id();
    // Wait until every thread other than `self` has exited the callback.
    cv.wait(lock, [this, self]() {
      for (const std::thread::id& tid : active_threads) {
        if (tid != self) return false;
      }
      return true;
    });
  }

  bool IsActive() {
    std::lock_guard<std::mutex> lock(mutex);
    return active;
  }
};

struct AuthSubEntry {
  CallbackGate gate;
  std::function<void()> callback;
  bool keep_refresh = false;
  bool refresh_started = false;
};

struct AppCheckSubEntry {
  CallbackGate gate;
  std::function<void(const std::string&)> callback;
};

}  // namespace

struct TokenSubscriptionHub {
  std::mutex mutex;
  std::mutex refresh_op_mutex;
  AuthTokenProvider* auth_provider = nullptr;
  int active_refresh_leases = 0;
  bool provider_refresh_started = false;
  std::vector<std::shared_ptr<AuthSubEntry>> auth_subs;
  std::vector<std::shared_ptr<AppCheckSubEntry>> app_check_subs;

  // Synchronizes AuthTokenProvider Start/Stop transitions strictly 0->1 / 1->0
  // outside `mutex`, preventing out-of-order Stop-before-Start underflow.
  void SyncRefreshState() {
    std::lock_guard<std::mutex> op_lock(refresh_op_mutex);
    AuthTokenProvider* provider = nullptr;
    bool should_start = false;
    bool should_stop = false;
    {
      std::lock_guard<std::mutex> lock(mutex);
      provider = auth_provider;
      if (provider == nullptr) {
        provider_refresh_started = false;
        return;
      }
      if (active_refresh_leases > 0 && !provider_refresh_started) {
        provider_refresh_started = true;
        should_start = true;
      } else if (active_refresh_leases == 0 && provider_refresh_started) {
        provider_refresh_started = false;
        should_stop = true;
      }
    }
    if (should_start) {
      provider->StartTokenListener();
    } else if (should_stop) {
      provider->StopTokenListener();
    }
  }
};

AppState::AppState(std::string name, AppOptions options)
    : name_(std::move(name)),
      options_(std::move(options)),
      system_logger_(),
      logger_(&system_logger_, kLogLevelVerbose),
      token_hub_(std::make_shared<TokenSubscriptionHub>()) {
  LogDebug(
      "Created app name=%s: options, api_key=%s, app_id=%s, database_url=%s, "
      "messaging_sender_id=%s, storage_bucket=%s, project_id=%s (0x%08x)",
      name_.c_str(), options_.api_key(), options_.app_id(),
      options_.database_url(), options_.messaging_sender_id(),
      options_.storage_bucket(), options_.project_id(),
      static_cast<int>(reinterpret_cast<intptr_t>(this)));
  callback::Initialize();
}

AppState::~AppState() {
  LogDebug("Deleting app %s (0x%08x)", name_.c_str(),
           static_cast<int>(reinterpret_cast<intptr_t>(this)));
  // Destroy all attached product services first while AppState (options,
  // logger, platform data, JNI/FIRApp) is still alive.
  DestroyServices();
  // Tear down platform-specific state (HeartbeatController, JNI refs, FIRApp).
  DestroyPlatformData();
  callback::Terminate(/*flush_all=*/false);
}

void AppState::OnAuthTokenProviderAttached(AuthTokenProvider* provider) {
  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    token_hub_->auth_provider = provider;
  }
  token_hub_->SyncRefreshState();
  // Notify any consumers registered before Auth existed so they can sync
  // against the newly attached Auth provider.
  NotifyAuthStateChanged();
}

void AppState::OnAuthTokenProviderDetaching() {
  std::lock_guard<std::mutex> op_lock(token_hub_->refresh_op_mutex);
  AuthTokenProvider* provider = nullptr;
  bool should_stop = false;
  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    provider = token_hub_->auth_provider;
    token_hub_->auth_provider = nullptr;
    if (provider != nullptr && token_hub_->provider_refresh_started) {
      token_hub_->provider_refresh_started = false;
      should_stop = true;
    }
  }
  if (should_stop) {
    provider->StopTokenListener();
  }
}

TokenSubscription AppState::SubscribeAuthState(std::function<void()> on_changed,
                                               bool keep_token_refresh_active) {
  auto entry = std::make_shared<AuthSubEntry>();
  entry->callback = std::move(on_changed);
  entry->keep_refresh = keep_token_refresh_active;

  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    token_hub_->auth_subs.push_back(entry);
    if (keep_token_refresh_active) {
      entry->refresh_started = true;
      ++token_hub_->active_refresh_leases;
    }
  }
  if (keep_token_refresh_active) {
    token_hub_->SyncRefreshState();
  }

  std::shared_ptr<TokenSubscriptionHub> hub = token_hub_;
  return TokenSubscription([hub = std::move(hub), entry = std::move(entry)]() {
    // Wait for any in-flight notification on another thread before releasing.
    entry->gate.CancelAndWait();
    bool lease_released = false;
    {
      std::lock_guard<std::mutex> lock(hub->mutex);
      auto& subs = hub->auth_subs;
      subs.erase(std::remove(subs.begin(), subs.end(), entry), subs.end());
      if (entry->refresh_started) {
        entry->refresh_started = false;
        if (hub->active_refresh_leases > 0) {
          --hub->active_refresh_leases;
        }
        lease_released = true;
      }
    }
    if (lease_released) {
      hub->SyncRefreshState();
    }
  });
}

TokenSubscription AppState::SubscribeAppCheckToken(
    std::function<void(const std::string& token)> on_changed) {
  auto entry = std::make_shared<AppCheckSubEntry>();
  entry->callback = std::move(on_changed);

  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    token_hub_->app_check_subs.push_back(entry);
  }

  std::shared_ptr<TokenSubscriptionHub> hub = token_hub_;
  return TokenSubscription([hub = std::move(hub), entry = std::move(entry)]() {
    entry->gate.CancelAndWait();
    std::lock_guard<std::mutex> lock(hub->mutex);
    auto& subs = hub->app_check_subs;
    subs.erase(std::remove(subs.begin(), subs.end(), entry), subs.end());
  });
}

void AppState::NotifyAuthStateChanged() {
  std::vector<std::shared_ptr<AuthSubEntry>> snapshot;
  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    snapshot = token_hub_->auth_subs;
  }
  for (const auto& entry : snapshot) {
    if (!entry->gate.TryEnter()) continue;
    if (entry->callback) {
      entry->callback();
    }
    entry->gate.Exit();
  }
}

void AppState::NotifyAppCheckTokenChanged(const std::string& token) {
  std::vector<std::shared_ptr<AppCheckSubEntry>> snapshot;
  {
    std::lock_guard<std::mutex> lock(token_hub_->mutex);
    snapshot = token_hub_->app_check_subs;
  }
  for (const auto& entry : snapshot) {
    if (!entry->gate.TryEnter()) continue;
    if (entry->callback) {
      entry->callback(token);
    }
    entry->gate.Exit();
  }
}

void AppState::DestroyServices() {
  {
    std::lock_guard<std::mutex> lock(services_mutex_);
    destroying_services_ = true;
    services_cv_.notify_all();
  }
  while (true) {
    std::shared_ptr<Service> to_destroy;
    bool detaching_auth = false;
    {
      std::lock_guard<std::mutex> lock(services_mutex_);
      if (owned_services_.empty()) {
        index_.clear();
        break;
      }

      // Pick the service with the highest teardown_priority(); break ties by
      // reverse creation order (later entries in owned_services_ first).
      size_t best_idx = owned_services_.size() - 1;
      int best_priority =
          owned_services_[best_idx].service->teardown_priority();
      for (size_t i = owned_services_.size() - 1; i > 0; --i) {
        size_t candidate = i - 1;
        int priority = owned_services_[candidate].service->teardown_priority();
        if (priority > best_priority) {
          best_priority = priority;
          best_idx = candidate;
        }
      }

      ServiceEntry entry = std::move(owned_services_[best_idx]);
      owned_services_.erase(owned_services_.begin() +
                            static_cast<std::ptrdiff_t>(best_idx));

      ServiceKey key{entry.type_tag, entry.instance_key};
      auto it = index_.find(key);
      if (it != index_.end() && it->second == entry.service.get()) {
        index_.erase(it);
      }
      if (entry.type_tag == &kAuthServiceTag && entry.instance_key.empty()) {
        auth_token_provider_.store(nullptr, std::memory_order_release);
        detaching_auth = true;
      } else if (entry.type_tag == &kAppCheckServiceTag &&
                 entry.instance_key.empty()) {
        app_check_token_provider_.store(nullptr, std::memory_order_release);
      }
      to_destroy = std::move(entry.service);
    }
    if (detaching_auth) {
      OnAuthTokenProviderDetaching();
    }
    // Destroy outside services_mutex_ so the service's destructor can safely
    // call FindService() or join worker threads without deadlocking.
    to_destroy.reset();
  }
  OnAuthTokenProviderDetaching();
}

bool AppState::DetachService(const void* type_tag, std::string_view key,
                             const Service* expected) {
  std::shared_ptr<Service> removed;
  bool detaching_auth = false;
  {
    std::lock_guard<std::mutex> lock(services_mutex_);
    ServiceKey lookup_key{type_tag, std::string(key)};
    auto it = index_.find(lookup_key);
    if (it == index_.end()) return false;
    if (expected != nullptr && it->second != expected) return false;

    Service* target = it->second;
    index_.erase(it);
    if (type_tag == &kAuthServiceTag && key.empty()) {
      auth_token_provider_.store(nullptr, std::memory_order_release);
      detaching_auth = true;
    } else if (type_tag == &kAppCheckServiceTag && key.empty()) {
      app_check_token_provider_.store(nullptr, std::memory_order_release);
    }

    for (auto vec_it = owned_services_.begin(); vec_it != owned_services_.end();
         ++vec_it) {
      if (vec_it->service.get() == target) {
        removed = std::move(vec_it->service);
        owned_services_.erase(vec_it);
        break;
      }
    }
  }
  if (detaching_auth) {
    OnAuthTokenProviderDetaching();
  }
  // Drop `removed` outside services_mutex_.
  return true;
}

}  // namespace internal
}  // namespace firebase
