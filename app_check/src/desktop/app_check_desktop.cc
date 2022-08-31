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

#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

static AppCheckProviderFactory* g_provider_factory = nullptr;

AppCheckInternal::AppCheckInternal(App* app)
    : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  g_provider_factory = factory;
  // TODO: Refresh tokens?
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(bool isTokenAutoRefreshEnabled) {
  
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool forceRefresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  // TODO: Get the AppCheckToken correctly
  AppCheckToken token;
  future()->CompleteWithResult(handle, 0, token);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
}

}
}
}
