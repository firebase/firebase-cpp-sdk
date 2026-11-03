// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "app_check/src/desktop/app_check_desktop.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "app/src/function_registry.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app_check/src/common/common.h"

namespace firebase::app_check::internal {

namespace {

// Leaked recursive mutex protecting g_provider_factory so static exit teardown
// never hits a destroyed mutex and CreateProvider() can safely re-enter.
Mutex& GetProviderFactoryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}

AppCheckProviderFactory* g_provider_factory = nullptr;

// Maps FunctionRegistryAppCheckListener* to its owning AppCheckInternal's
// LifetimeState so OnAppCheckTokenChanged can detect re-entrant destruction of
// AppCheckInternal without touching `this` after a callback returns.
Mutex& GetListenerLifetimeMapMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}

std::map<const FunctionRegistryAppCheckListener*,
         std::shared_ptr<AppCheckInternal::LifetimeState>>&
GetListenerLifetimeMap() {
  static auto* map =
      new std::map<const FunctionRegistryAppCheckListener*,
                   std::shared_ptr<AppCheckInternal::LifetimeState>>();
  return *map;
}

void RegisterListenerLifetimeState(
    const FunctionRegistryAppCheckListener* listener,
    std::shared_ptr<AppCheckInternal::LifetimeState> state) {
  MutexLock lock(GetListenerLifetimeMapMutex());
  GetListenerLifetimeMap()[listener] = std::move(state);
}

void UnregisterListenerLifetimeState(
    const FunctionRegistryAppCheckListener* listener) {
  MutexLock lock(GetListenerLifetimeMapMutex());
  GetListenerLifetimeMap().erase(listener);
}

std::shared_ptr<AppCheckInternal::LifetimeState> GetListenerLifetimeState(
    const FunctionRegistryAppCheckListener* listener) {
  MutexLock lock(GetListenerLifetimeMapMutex());
  auto& map = GetListenerLifetimeMap();
  auto it = map.find(listener);
  return (it != map.end()) ? it->second : nullptr;
}

bool IsLifetimeStateAlive(
    const std::shared_ptr<AppCheckInternal::LifetimeState>& state) {
  if (!state) return false;
  std::lock_guard lock(state->mutex);
  return state->owner != nullptr;
}

// 5-minute buffer before expiration (in milliseconds).
constexpr int64_t kTokenRefreshBufferMillis = 5 * 60 * 1000;

int64_t CurrentTimeMillis() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

// Scoped operation lease that keeps AppCheckInternal from completing
// destruction while an asynchronous callback is executing on another thread,
// without holding the state mutex during user callback / future completion.
class OpLease {
 public:
  explicit OpLease(std::shared_ptr<AppCheckInternal::LifetimeState> state)
      : state_(std::move(state)) {
    if (!state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->owner != nullptr) {
      self_ = state_->owner;
      state_->active_threads.push_back(std::this_thread::get_id());
    }
  }

  ~OpLease() {
    if (!self_ || !state_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it = std::find(state_->active_threads.begin(),
                        state_->active_threads.end(),
                        std::this_thread::get_id());
    if (it != state_->active_threads.end()) {
      state_->active_threads.erase(it);
    }
    state_->cv.notify_all();
  }

  OpLease(const OpLease&) = delete;
  OpLease& operator=(const OpLease&) = delete;

  AppCheckInternal* self() const { return self_; }

  bool is_alive() const {
    if (!self_ || !state_) return false;
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->owner != nullptr;
  }

 private:
  std::shared_ptr<AppCheckInternal::LifetimeState> state_;
  AppCheckInternal* self_{nullptr};
};

}  // namespace

AppCheckInternal::AppCheckInternal(App* app)
    : app_(app),
      lifetime_state_(std::make_shared<LifetimeState>()),
      cached_provider_(nullptr),
      cached_provider_factory_(nullptr),
      cached_token_(),
      is_token_auto_refresh_enabled_(true) {
  lifetime_state_->owner = this;
  RegisterListenerLifetimeState(&internal_listener_, lifetime_state_);
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  AddAppCheckListener(&internal_listener_);
  InitRegistryCalls();
}

AppCheckInternal::~AppCheckInternal() {
  // Unregister function-registry entry points first so no new registry calls
  // can enter while teardown waits for in-flight operations.
  CleanupRegistryCalls();
  if (lifetime_state_) {
    std::unique_lock<std::mutex> lock(lifetime_state_->mutex);
    lifetime_state_->owner = nullptr;
    const auto self_tid = std::this_thread::get_id();
    lifetime_state_->cv.wait(lock, [this, self_tid]() {
      return std::all_of(
          lifetime_state_->active_threads.begin(),
          lifetime_state_->active_threads.end(),
          [self_tid](const std::thread::id& tid) { return tid == self_tid; });
    });
  }
  UnregisterListenerLifetimeState(&internal_listener_);
  {
    std::lock_guard<std::mutex> lock(mutex_);
    token_listeners_.clear();
    cached_token_ = AppCheckToken{};
    cached_provider_ = nullptr;
    cached_provider_factory_ = nullptr;
  }
  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

bool AppCheckInternal::HasValidCacheTokenLocked() const {
  return cached_token_.is_valid(CurrentTimeMillis(), kTokenRefreshBufferMillis);
}

bool AppCheckInternal::HasValidCacheToken() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return HasValidCacheTokenLocked();
}

void AppCheckInternal::UpdateCachedToken(AppCheckToken token) {
  OpLease lease(lifetime_state_);
  if (!lease.is_alive()) {
    return;
  }
  std::vector<AppCheckListener*> listeners_snapshot;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    cached_token_ = token;
    listeners_snapshot.assign(token_listeners_.begin(), token_listeners_.end());
  }
  // Invoke listeners outside the lock so re-entrant RemoveAppCheckListener or
  // GetAppCheckToken calls inside OnAppCheckTokenChanged never deadlock.
  // Check lease.is_alive() before and after each listener in case a callback
  // destroys AppCheck re-entrantly on the current thread.
  for (AppCheckListener* listener : listeners_snapshot) {
    if (!lease.is_alive()) {
      return;
    }
    bool still_registered = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      still_registered =
          std::find(token_listeners_.begin(), token_listeners_.end(),
                    listener) != token_listeners_.end();
    }
    if (still_registered && listener != nullptr) {
      listener->OnAppCheckTokenChanged(token);
      if (!lease.is_alive()) {
        return;
      }
    }
  }
}

AppCheckProvider* AppCheckInternal::GetProvider() {
  App* current_app = nullptr;
  AppCheckProviderFactory* factory = nullptr;
  {
    MutexLock factory_lock(GetProviderFactoryMutex());
    factory = g_provider_factory;
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (factory == cached_provider_factory_ && cached_provider_ != nullptr) {
      return cached_provider_;
    }
    if (factory != cached_provider_factory_) {
      cached_provider_ = nullptr;
      cached_provider_factory_ = factory;
    }
    current_app = app_;
  }
  if (factory == nullptr || current_app == nullptr) {
    return nullptr;
  }
  // Invoke factory->CreateProvider(current_app) outside this->mutex_ so a
  // custom factory that queries AppCheck never deadlocks, while holding the
  // recursive GetProviderFactoryMutex() to serialize against concurrent
  // SetAppCheckProviderFactory() calls.
  AppCheckProvider* created_provider = nullptr;
  {
    MutexLock factory_lock(GetProviderFactoryMutex());
    if (g_provider_factory == factory) {
      created_provider = factory->CreateProvider(current_app);
    }
  }
  std::lock_guard<std::mutex> lock(mutex_);
  if (cached_provider_factory_ == factory && cached_provider_ == nullptr) {
    cached_provider_ = created_provider;
  }
  return cached_provider_;
}

// static
void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  MutexLock lock(GetProviderFactoryMutex());
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  is_token_auto_refresh_enabled_ = is_token_auto_refresh_enabled;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  Future<AppCheckToken> result = MakeFuture(future(), handle);
  if (!force_refresh) {
    AppCheckToken cached;
    bool valid = false;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (HasValidCacheTokenLocked()) {
        cached = cached_token_;
        valid = true;
      }
    }
    if (valid) {
      future()->CompleteWithResult(handle, 0, cached);
      return result;
    }
  }

  // Get a new token, and pass the result into the future.
  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::shared_ptr<LifetimeState> state = lifetime_state_;
    auto token_callback{
        [state, handle](firebase::app_check::AppCheckToken token,
                        int error_code, const std::string& error_message) {
          OpLease lease(state);
          if (!lease.is_alive()) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            lease.self()->UpdateCachedToken(token);
            if (!lease.is_alive()) return;
            lease.self()->future()->CompleteWithResult(handle, 0, token);
          } else {
            lease.self()->future()->Complete(handle, error_code,
                                             error_message.c_str());
          }
        }};
    provider->GetToken(token_callback);
  } else {
    future()->Complete(
        handle, firebase::app_check::kAppCheckErrorInvalidConfiguration,
        "No AppCheckProvider installed.");
  }
  return result;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  auto handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetLimitedUseAppCheckToken);
  Future<AppCheckToken> result = MakeFuture(future(), handle);
  // Get a new token, and pass the result into the future.
  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::shared_ptr<LifetimeState> state = lifetime_state_;
    auto token_callback{
        [state, handle](firebase::app_check::AppCheckToken token,
                        int error_code, const std::string& error_message) {
          OpLease lease(state);
          if (!lease.is_alive()) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            // Note that we do NOT update the cached token for limited-use
            // tokens.
            lease.self()->future()->CompleteWithResult(handle, 0, token);
          } else {
            lease.self()->future()->Complete(handle, error_code,
                                             error_message.c_str());
          }
        }};
    provider->GetToken(token_callback);
  } else {
    future()->Complete(handle,
                       firebase::app_check::kAppCheckErrorInvalidConfiguration,
                       "No AppCheckProvider installed.");
  }
  return result;
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
}

Future<std::string> AppCheckInternal::GetAppCheckTokenStringInternal() {
  auto handle =
      future()->SafeAlloc<std::string>(kAppCheckFnGetAppCheckStringInternal);
  Future<std::string> result = MakeFuture(future(), handle);
  std::string cached_token_str;
  bool has_valid_cached = false;
  bool auto_refresh_enabled = true;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (HasValidCacheTokenLocked()) {
      cached_token_str = cached_token_.token;
      has_valid_cached = true;
    } else {
      auto_refresh_enabled = is_token_auto_refresh_enabled_;
    }
  }
  if (has_valid_cached) {
    future()->CompleteWithResult(handle, 0, cached_token_str);
    return result;
  }

  if (auto_refresh_enabled) {
    // Only refresh the token if it is enabled
    AppCheckProvider* provider = GetProvider();
    if (provider != nullptr) {
      std::shared_ptr<LifetimeState> state = lifetime_state_;
      auto token_callback{
          [state, handle](firebase::app_check::AppCheckToken token,
                          int error_code, const std::string& error_message) {
            OpLease lease(state);
            if (!lease.is_alive()) return;
            if (error_code == firebase::app_check::kAppCheckErrorNone) {
              lease.self()->UpdateCachedToken(token);
              if (!lease.is_alive()) return;
              lease.self()->future()->CompleteWithResult(handle, 0,
                                                         token.token);
            } else {
              lease.self()->future()->Complete(handle, error_code,
                                               error_message.c_str());
            }
          }};
      provider->GetToken(token_callback);
    } else {
      future()->Complete(
          handle, firebase::app_check::kAppCheckErrorInvalidConfiguration,
          "No AppCheckProvider installed.");
    }
  } else {
    future()->Complete(
        handle, kAppCheckErrorUnknown,
        "No AppCheck token available, and auto refresh is disabled");
  }
  return result;
}

Future<std::string>
AppCheckInternal::GetLimitedUseAppCheckTokenStringInternal() {
  auto handle = future()->SafeAlloc<std::string>(
      kAppCheckFnGetLimitedUseAppCheckStringInternal);
  Future<std::string> result = MakeFuture(future(), handle);

  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::shared_ptr<LifetimeState> state = lifetime_state_;
    auto token_callback{
        [state, handle](firebase::app_check::AppCheckToken token,
                        int error_code, const std::string& error_message) {
          OpLease lease(state);
          if (!lease.is_alive()) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            lease.self()->future()->CompleteWithResult(handle, 0, token.token);
          } else {
            lease.self()->future()->Complete(handle, error_code,
                                             error_message.c_str());
          }
        }};
    provider->GetToken(token_callback);
  } else {
    future()->Complete(handle,
                       firebase::app_check::kAppCheckErrorInvalidConfiguration,
                       "No AppCheckProvider installed.");
  }
  return result;
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  AppCheckToken cached;
  bool notify_immediately = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    bool inserted = false;
    if (std::find(token_listeners_.begin(), token_listeners_.end(), listener) ==
        token_listeners_.end()) {
      token_listeners_.push_back(listener);
      inserted = true;
    }
    if (inserted && HasValidCacheTokenLocked()) {
      cached = cached_token_;
      notify_immediately = true;
    }
  }
  // Following the Android pattern, if there is a cached token, call a newly
  // added listener outside the mutex lock.
  if (notify_immediately) {
    listener->OnAppCheckTokenChanged(cached);
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  std::lock_guard<std::mutex> lock(mutex_);
  token_listeners_.remove(listener);
}

void AppCheckInternal::InitRegistryCalls() {
  if (!app_ || !app_->function_registry()) return;
  app_->function_registry()->RegisterFunction(
      ::firebase::internal::FnAppCheckGetTokenAsync,
      AppCheckInternal::GetAppCheckTokenAsyncForRegistry);
  app_->function_registry()->RegisterFunction(
      ::firebase::internal::FnAppCheckGetLimitedUseTokenAsync,
      AppCheckInternal::GetLimitedUseAppCheckTokenAsyncForRegistry);
  app_->function_registry()->RegisterFunction(
      ::firebase::internal::FnAppCheckAddListener,
      AppCheckInternal::AddAppCheckListenerForRegistry);
  app_->function_registry()->RegisterFunction(
      ::firebase::internal::FnAppCheckRemoveListener,
      AppCheckInternal::RemoveAppCheckListenerForRegistry);
}

void AppCheckInternal::CleanupRegistryCalls() {
  if (!app_ || !app_->function_registry()) return;
  app_->function_registry()->UnregisterFunction(
      ::firebase::internal::FnAppCheckGetTokenAsync);
  app_->function_registry()->UnregisterFunction(
      ::firebase::internal::FnAppCheckGetLimitedUseTokenAsync);
  app_->function_registry()->UnregisterFunction(
      ::firebase::internal::FnAppCheckAddListener);
  app_->function_registry()->UnregisterFunction(
      ::firebase::internal::FnAppCheckRemoveListener);
}

// Static functions used by the internal registry tool
bool AppCheckInternal::GetAppCheckTokenAsyncForRegistry(App* app,
                                                        void* /*unused*/,
                                                        void* out) {
  Future<std::string>* out_future = static_cast<Future<std::string>*>(out);
  if (!app || !out_future) {
    return false;
  }

  AppCheck* app_check = GetExistingAppCheckInstance(app);
  if (app_check && app_check->internal_) {
    *out_future = app_check->internal_->GetAppCheckTokenStringInternal();
    return true;
  }
  return false;
}

// static
bool AppCheckInternal::GetLimitedUseAppCheckTokenAsyncForRegistry(
    App* app, void* /*unused*/, void* out) {
  Future<std::string>* out_future = static_cast<Future<std::string>*>(out);
  if (!app || !out_future) {
    return false;
  }

  AppCheck* app_check = GetExistingAppCheckInstance(app);
  if (app_check && app_check->internal_) {
    *out_future =
        app_check->internal_->GetLimitedUseAppCheckTokenStringInternal();
    return true;
  }
  return false;
}

bool FunctionRegistryAppCheckListener::AddListener(
    FunctionRegistryCallback callback, void* context) {
  if (!callback) {
    return false;
  }
  std::lock_guard lock(mutex_);
  Entry entry = {callback, context};
  if (std::find(callbacks_.begin(), callbacks_.end(), entry) ==
      callbacks_.end()) {
    callbacks_.push_back(entry);
    return true;
  }
  return false;
}

void FunctionRegistryAppCheckListener::RemoveListener(
    FunctionRegistryCallback callback, void* context) {
  std::lock_guard<std::mutex> lock(mutex_);
  Entry entry = {callback, context};
  auto iter = std::find(callbacks_.begin(), callbacks_.end(), entry);
  if (iter != callbacks_.end()) {
    callbacks_.erase(iter);
  }
}

void FunctionRegistryAppCheckListener::OnAppCheckTokenChanged(
    const AppCheckToken& token) {
  std::shared_ptr<AppCheckInternal::LifetimeState> state =
      GetListenerLifetimeState(this);
  std::vector<Entry> snapshot;
  {
    std::lock_guard lock(mutex_);
    snapshot = callbacks_;
  }
  for (const Entry& entry : snapshot) {
    if (state && !IsLifetimeStateAlive(state)) {
      return;
    }
    bool still_registered = false;
    {
      std::lock_guard lock(mutex_);
      still_registered =
          std::find(callbacks_.begin(), callbacks_.end(), entry) !=
          callbacks_.end();
      if (still_registered && entry.first) {
        entry.first(token.token, entry.second);
      }
    }
    if (state && !IsLifetimeStateAlive(state)) {
      return;
    }
  }
}

// static
bool AppCheckInternal::AddAppCheckListenerForRegistry(App* app, void* callback,
                                                      void* context) {
  auto typed_callback = reinterpret_cast<FunctionRegistryCallback>(callback);
  if (!app || !typed_callback) {
    return false;
  }

  AppCheck* app_check = GetExistingAppCheckInstance(app);
  if (app_check && app_check->internal_) {
    std::string cached_token_str;
    bool notify_immediately = false;
    {
      std::lock_guard lock(app_check->internal_->mutex_);
      bool inserted = app_check->internal_->internal_listener_.AddListener(typed_callback,
                                                           context);
      if (inserted &&
          app_check->internal_->HasValidCacheTokenLocked()) {
        cached_token_str = app_check->internal_->cached_token_.token;
        notify_immediately = true;
      }
    }
    if (notify_immediately) {
      typed_callback(cached_token_str, context);
    }
    return true;
  }
  return false;
}

// static
bool AppCheckInternal::RemoveAppCheckListenerForRegistry(App* app,
                                                         void* callback,
                                                         void* context) {
  auto typed_callback = reinterpret_cast<FunctionRegistryCallback>(callback);
  if (!app || !typed_callback) {
    return false;
  }

  AppCheck* app_check = GetExistingAppCheckInstance(app);
  if (app_check && app_check->internal_) {
    app_check->internal_->internal_listener_.RemoveListener(typed_callback,
                                                            context);
    return true;
  }
  return false;
}

}  // namespace firebase::app_check::internal
