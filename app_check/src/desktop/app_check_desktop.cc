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
#include <ctime>
#include <string>

#include "app/src/function_registry.h"
#include "app/src/log.h"
#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

static AppCheckProviderFactory* g_provider_factory = nullptr;

AppCheckInternal::AppCheckInternal(App* app)
    : app_(app),
      cached_token_(),
      cached_provider_(),
      is_token_auto_refresh_enabled_(true) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  AddAppCheckListener(&internal_listener_);
  InitRegistryCalls();
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  CleanupRegistryCalls();
  app_ = nullptr;
  // Clear the cached token by setting the expiration
  cached_token_.expire_time_millis = 0;
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

bool AppCheckInternal::HasValidCacheToken() const {
  // Get the current time, in milliseconds
  int64_t current_time = std::time(nullptr) * 1000;
  // TODO(amaurice): Add some additional time to the check
  return cached_token_.expire_time_millis > current_time;
}

void AppCheckInternal::UpdateCachedToken(AppCheckToken token) {
  cached_token_ = token;
  // Call the token listeners
  for (AppCheckListener* listener : token_listeners_) {
    listener->OnAppCheckTokenChanged(token);
  }
}

AppCheckProvider* AppCheckInternal::GetProvider() {
  if (!cached_provider_ && g_provider_factory && app_) {
    cached_provider_ = g_provider_factory->CreateProvider(app_);
  }
  return cached_provider_;
}

// static
void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  is_token_auto_refresh_enabled_ = is_token_auto_refresh_enabled;
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  if (!force_refresh && HasValidCacheToken()) {
    // If the cached token is valid, and not told to refresh, return the cache
    future()->CompleteWithResult(handle, 0, cached_token_);
  } else {
    // Get a new token, and pass the result into the future.
    AppCheckProvider* provider = GetProvider();
    if (provider != nullptr) {
      auto token_callback{
          [this, handle](firebase::app_check::AppCheckToken token,
                         int error_code, const std::string& error_message) {
            if (error_code == firebase::app_check::kAppCheckErrorNone) {
              UpdateCachedToken(token);
              future()->CompleteWithResult(handle, 0, token);
            } else {
              future()->Complete(handle, error_code, error_message.c_str());
            }
          }};
      provider->GetToken(token_callback);
    } else {
      future()->Complete(
          handle, firebase::app_check::kAppCheckErrorInvalidConfiguration,
          "No AppCheckProvider installed.");
    }
  }
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<std::string> AppCheckInternal::GetAppCheckTokenStringInternal() {
  auto handle =
      future()->SafeAlloc<std::string>(kAppCheckFnGetAppCheckStringInternal);
  if (HasValidCacheToken()) {
    future()->CompleteWithResult(handle, 0, cached_token_.token);
  } else if (is_token_auto_refresh_enabled_) {
    // Only refresh the token if it is enabled
    AppCheckProvider* provider = GetProvider();
    if (provider != nullptr) {
      // Get a new token, and pass the result into the future.
      // Note that this is slightly different from the one above, as the
      // Future result is just the string token, and not the full struct.
      auto token_callback{
          [this, handle](firebase::app_check::AppCheckToken token,
                         int error_code, const std::string& error_message) {
            if (error_code == firebase::app_check::kAppCheckErrorNone) {
              UpdateCachedToken(token);
              future()->CompleteWithResult(handle, 0, token.token);
            } else {
              future()->Complete(handle, error_code, error_message.c_str());
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
  return MakeFuture(future(), handle);
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (listener) {
    token_listeners_.push_back(listener);

    // Following the Android pattern, if there is a cached token, call the
    // listener. Note that the iOS implementation does not do this.
    if (HasValidCacheToken()) {
      listener->OnAppCheckTokenChanged(cached_token_);
    }
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  if (listener) {
    token_listeners_.remove(listener);
  }
}

static int g_app_check_registry_count = 0;

void AppCheckInternal::InitRegistryCalls() {
  if (g_app_check_registry_count == 0) {
    app_->function_registry()->RegisterFunction(
        ::firebase::internal::FnAppCheckGetTokenAsync,
        AppCheckInternal::GetAppCheckTokenAsyncForRegistry);
    app_->function_registry()->RegisterFunction(
        ::firebase::internal::FnAppCheckAddListener,
        AppCheckInternal::AddAppCheckListenerForRegistry);
    app_->function_registry()->RegisterFunction(
        ::firebase::internal::FnAppCheckRemoveListener,
        AppCheckInternal::RemoveAppCheckListenerForRegistry);
  }
  g_app_check_registry_count++;
}

void AppCheckInternal::CleanupRegistryCalls() {
  g_app_check_registry_count--;
  if (g_app_check_registry_count == 0) {
    app_->function_registry()->UnregisterFunction(
        ::firebase::internal::FnAppCheckGetTokenAsync);
    app_->function_registry()->UnregisterFunction(
        ::firebase::internal::FnAppCheckAddListener);
    app_->function_registry()->UnregisterFunction(
        ::firebase::internal::FnAppCheckRemoveListener);
  }
}

// Static functions used by the internal registry tool
bool AppCheckInternal::GetAppCheckTokenAsyncForRegistry(App* app,
                                                        void* /*unused*/,
                                                        void* out) {
  Future<std::string>* out_future = static_cast<Future<std::string>*>(out);
  if (!app || !out_future) {
    return false;
  }

  AppCheck* app_check = AppCheck::GetInstance(app);
  if (app_check && app_check->internal_) {
    *out_future = app_check->internal_->GetAppCheckTokenStringInternal();
    return true;
  }
  return false;
}

void FunctionRegistryAppCheckListener::AddListener(
    FunctionRegistryCallback callback, void* context) {
  callbacks_.emplace_back(callback, context);
}

void FunctionRegistryAppCheckListener::RemoveListener(
    FunctionRegistryCallback callback, void* context) {
  Entry entry = {callback, context};

  auto iter = std::find(callbacks_.begin(), callbacks_.end(), entry);
  if (iter != callbacks_.end()) {
    callbacks_.erase(iter);
  }
}

void FunctionRegistryAppCheckListener::OnAppCheckTokenChanged(
    const AppCheckToken& token) {
  for (const Entry& entry : callbacks_) {
    entry.first(token.token, entry.second);
  }
}

// static
bool AppCheckInternal::AddAppCheckListenerForRegistry(App* app, void* callback,
                                                      void* context) {
  auto typed_callback = reinterpret_cast<FunctionRegistryCallback>(callback);
  if (!app || !typed_callback) {
    return false;
  }

  AppCheck* app_check = AppCheck::GetInstance(app);
  if (app_check && app_check->internal_) {
    app_check->internal_->internal_listener_.AddListener(typed_callback,
                                                         context);
    // If there is a cached token, pass it along to the callback
    if (app_check->internal_->HasValidCacheToken()) {
      typed_callback(app_check->internal_->cached_token_.token, context);
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

  AppCheck* app_check = AppCheck::GetInstance(app);
  if (app_check && app_check->internal_) {
    app_check->internal_->internal_listener_.RemoveListener(typed_callback,
                                                            context);
    return true;
  }
  return false;
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
