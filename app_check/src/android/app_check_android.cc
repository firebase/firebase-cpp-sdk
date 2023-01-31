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

#include "app_check/src/android/app_check_android.h"

#include "app/src/util_android.h"
#include "app_check/src/android/common_android.h"
#include "app_check/src/android/debug_provider_android.h"
#include "app_check/src/android/play_integrity_provider_android.h"
#include "app_check/src/android/safety_net_provider_android.h"
#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

static AppCheckProviderFactory* g_provider_factory = nullptr;
static int g_initialized_count = 0;

// Release cached Java classes.
static void ReleaseClasses(JNIEnv* env) {
  ReleaseCommonAndroidClasses(env);
  ReleaseDebugProviderClasses(env);
}

AppCheckInternal::AppCheckInternal(App* app) : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);

  // Cache the JNI method ids so we only have to look them up by name once.
  if (!g_initialized_count) {
    // TODO: should I have a non-constructor getter that can handle failure to
    // initialize?
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (util::Initialize(env, activity)) {
      if (!(CacheCommonAndroidMethodIds(env, activity) &&
            CacheDebugProviderMethodIds(env, activity) &&
            CachePlayIntegrityProviderMethodIds(env, activity) &&
            CacheSafetyNetProviderMethodIds(env, activity))) {
        ReleaseClasses(env);
        util::Terminate(env);
      } else {
        g_initialized_count++;
      }
    }
  } else {
    g_initialized_count++;
  }
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  JNIEnv* env = app_->GetJNIEnv();
  app_ = nullptr;

  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_initialized_count == 0) {
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  AppCheckToken token;
  future()->CompleteWithResult(handle, 0, token);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
