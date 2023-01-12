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

#include <ctime>
#include <string>

#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

static AppCheckProviderFactory* g_provider_factory = nullptr;

AppCheckInternal::AppCheckInternal(App* app) : app_(app), cached_token_(), cached_provider_() {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
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
    bool is_token_auto_refresh_enabled) {}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  if (!force_refresh && HasValidCacheToken()) {
    // If the cached token is valid, and not told to refresh, return the cache
    future()->CompleteWithResult(handle, 0, cached_token_);
  } else {
    // Get a new token, and pass the result into the future.
    AppCheckProvider* provider = nullptr;
    if (g_provider_factory && app_) {
      provider = g_provider_factory->CreateProvider(app_);
    }
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

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (listener) {
    token_listeners_.push_back(listener);

    // Following the Android pattern, if there is a cached token, call the listener.
    // Note that the iOS implementation does not do this.
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

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
