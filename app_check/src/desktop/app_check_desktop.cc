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
#include <atomic>
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/log.h"
#include "app/src/service_interfaces.h"
#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

namespace {
std::atomic<AppCheckProviderFactory*> g_provider_factory{nullptr};
}  // namespace

AppCheckInternal::AppCheckInternal(::firebase::internal::AppState& app_state)
    : app_state_(app_state) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  MutexLock lock(mutex_);
  cached_token_.expire_time_millis = 0;
  token_listeners_.clear();
}

::firebase::App AppCheckInternal::app() const {
  return app_state_.TryMakeAppHandle();
}

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

bool AppCheckInternal::HasValidCacheTokenLocked() const {
  // Get the current time, in milliseconds (done in two lines because of x86).
  int64_t current_time = std::time(nullptr);
  current_time *= 1000;
  return cached_token_.expire_time_millis > current_time;
}

void AppCheckInternal::UpdateCachedToken(AppCheckToken token) {
  std::vector<AppCheckListener*> public_listeners;
  {
    MutexLock lock(mutex_);
    cached_token_ = token;
    public_listeners.assign(token_listeners_.begin(), token_listeners_.end());
  }
  for (AppCheckListener* listener : public_listeners) {
    listener->OnAppCheckTokenChanged(token);
  }
  app_state_.NotifyAppCheckTokenChanged(token.token);
}

AppCheckProvider* AppCheckInternal::GetProvider() {
  {
    MutexLock lock(mutex_);
    if (cached_provider_) {
      return cached_provider_;
    }
  }
  AppCheckProviderFactory* factory = g_provider_factory.load();
  if (!factory) {
    return nullptr;
  }
  App app_handle = app_state_.TryMakeAppHandle();
  if (!app_handle.is_valid()) {
    return nullptr;
  }
  AppCheckProvider* provider = factory->CreateProvider(app_handle);
  MutexLock lock(mutex_);
  if (!cached_provider_) {
    cached_provider_ = provider;
  }
  return cached_provider_;
}

// static
void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  g_provider_factory.store(factory);
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  MutexLock lock(mutex_);
  is_token_auto_refresh_enabled_ = is_token_auto_refresh_enabled;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  if (!force_refresh) {
    MutexLock lock(mutex_);
    if (HasValidCacheTokenLocked()) {
      future()->CompleteWithResult(handle, 0, cached_token_);
      return MakeFuture(future(), handle);
    }
  }

  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::weak_ptr<AppCheckInternal> weak_self =
        app_state_.FindServiceHandle<AppCheckInternal>(
            &::firebase::internal::kAppCheckServiceTag);
    auto token_callback =
        [weak_self, handle](firebase::app_check::AppCheckToken token,
                            int error_code, const std::string& error_message) {
          auto self = weak_self.lock();
          if (!self) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            self->UpdateCachedToken(token);
            self->future()->CompleteWithResult(handle, 0, token);
          } else {
            self->future()->Complete(handle, error_code, error_message.c_str());
          }
        };
    provider->GetToken(token_callback);
  } else {
    future()->Complete(handle,
                       firebase::app_check::kAppCheckErrorInvalidConfiguration,
                       "No AppCheckProvider installed.");
  }
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  auto handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetLimitedUseAppCheckToken);
  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::weak_ptr<AppCheckInternal> weak_self =
        app_state_.FindServiceHandle<AppCheckInternal>(
            &::firebase::internal::kAppCheckServiceTag);
    auto token_callback =
        [weak_self, handle](firebase::app_check::AppCheckToken token,
                            int error_code, const std::string& error_message) {
          auto self = weak_self.lock();
          if (!self) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            // Note that we do NOT update the cached token for limited-use
            // tokens.
            self->future()->CompleteWithResult(handle, 0, token);
          } else {
            self->future()->Complete(handle, error_code, error_message.c_str());
          }
        };
    provider->GetLimitedUseToken(token_callback);
  } else {
    future()->Complete(handle,
                       firebase::app_check::kAppCheckErrorInvalidConfiguration,
                       "No AppCheckProvider installed.");
  }
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
}

Future<std::string> AppCheckInternal::GetTokenAsync() {
  auto handle =
      future()->SafeAlloc<std::string>(kAppCheckFnGetAppCheckStringInternal);
  bool auto_refresh = false;
  {
    MutexLock lock(mutex_);
    if (HasValidCacheTokenLocked()) {
      future()->CompleteWithResult(handle, 0, cached_token_.token);
      return MakeFuture(future(), handle);
    }
    auto_refresh = is_token_auto_refresh_enabled_;
  }

  if (auto_refresh) {
    AppCheckProvider* provider = GetProvider();
    if (provider != nullptr) {
      std::weak_ptr<AppCheckInternal> weak_self =
          app_state_.FindServiceHandle<AppCheckInternal>(
              &::firebase::internal::kAppCheckServiceTag);
      auto token_callback = [weak_self, handle](
                                firebase::app_check::AppCheckToken token,
                                int error_code,
                                const std::string& error_message) {
        auto self = weak_self.lock();
        if (!self) return;
        if (error_code == firebase::app_check::kAppCheckErrorNone) {
          self->UpdateCachedToken(token);
          self->future()->CompleteWithResult(handle, 0, token.token);
        } else {
          self->future()->Complete(handle, error_code, error_message.c_str());
        }
      };
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
  return MakeFuture(future(), handle);
}

Future<std::string> AppCheckInternal::GetLimitedUseTokenAsync() {
  auto handle = future()->SafeAlloc<std::string>(
      kAppCheckFnGetLimitedUseAppCheckStringInternal);

  AppCheckProvider* provider = GetProvider();
  if (provider != nullptr) {
    std::weak_ptr<AppCheckInternal> weak_self =
        app_state_.FindServiceHandle<AppCheckInternal>(
            &::firebase::internal::kAppCheckServiceTag);
    auto token_callback =
        [weak_self, handle](firebase::app_check::AppCheckToken token,
                            int error_code, const std::string& error_message) {
          auto self = weak_self.lock();
          if (!self) return;
          if (error_code == firebase::app_check::kAppCheckErrorNone) {
            self->future()->CompleteWithResult(handle, 0, token.token);
          } else {
            self->future()->Complete(handle, error_code, error_message.c_str());
          }
        };
    provider->GetLimitedUseToken(token_callback);
  } else {
    future()->Complete(handle,
                       firebase::app_check::kAppCheckErrorInvalidConfiguration,
                       "No AppCheckProvider installed.");
  }
  return MakeFuture(future(), handle);
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  bool notify = false;
  AppCheckToken token_copy;
  {
    MutexLock lock(mutex_);
    token_listeners_.push_back(listener);
    if (HasValidCacheTokenLocked()) {
      notify = true;
      token_copy = cached_token_;
    }
  }
  if (notify) {
    listener->OnAppCheckTokenChanged(token_copy);
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  MutexLock lock(mutex_);
  token_listeners_.remove(listener);
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
