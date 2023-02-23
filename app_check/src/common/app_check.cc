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

#include "app_check/src/include/firebase/app_check.h"

#include <assert.h>

#include <map>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app_check/src/common/common.h"

// Include the header that matches the platform being used.
#if FIREBASE_PLATFORM_ANDROID
#include "app_check/src/android/app_check_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "app_check/src/ios/app_check_ios.h"
#else
#include "app_check/src/desktop/app_check_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    app_check,
    {
      // Create the AppCheck object for the given app.
      ::firebase::app_check::AppCheck::GetInstance(app);
      return ::firebase::kInitResultSuccess;
    },
    {
      ::firebase::app_check::AppCheck* app_check =
          ::firebase::app_check::internal::GetExistingAppCheckInstance(app);
      if (app_check) {
        delete app_check;
      }
    },
    // App Check wants to be turned on by default
    true);

namespace firebase {
namespace app_check {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAppCheck);

Mutex g_app_check_lock;  // NOLINT
static std::map<::firebase::App*, AppCheck*>* g_app_check_map = nullptr;

// Define the destructors for the virtual listener/provider/factory classes.
AppCheckListener::~AppCheckListener() {}
AppCheckProvider::~AppCheckProvider() {}
AppCheckProviderFactory::~AppCheckProviderFactory() {}

namespace internal {

AppCheck* GetExistingAppCheckInstance(::firebase::App* app) {
  if (!app) return nullptr;

  MutexLock lock(g_app_check_lock);
  if (g_app_check_map) {
    auto it = g_app_check_map->find(app);
    if (it != g_app_check_map->end()) {
      return it->second;
    }
  }
  return nullptr;
}

}  // namespace internal

AppCheck* AppCheck::GetInstance(::firebase::App* app) {
  if (!app) return nullptr;

  MutexLock lock(g_app_check_lock);
  if (!g_app_check_map) {
    g_app_check_map = new std::map<::firebase::App*, AppCheck*>();
  }

  auto it = g_app_check_map->find(app);
  if (it != g_app_check_map->end()) {
    return it->second;
  }

  AppCheck* app_check = new AppCheck(app);
  g_app_check_map->insert(std::make_pair(app, app_check));
  return app_check;
}

void AppCheck::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  internal::AppCheckInternal::SetAppCheckProviderFactory(factory);
}

AppCheck::AppCheck(::firebase::App* app) {
  internal_ = new internal::AppCheckInternal(app);
}

AppCheck::~AppCheck() { DeleteInternal(); }

void AppCheck::DeleteInternal() {
  MutexLock lock(g_app_check_lock);

  if (!internal_) return;

  // Remove it from the map
  g_app_check_map->erase(app());
  delete internal_;
  internal_ = nullptr;
  // If it is the last one, delete the map.
  if (g_app_check_map->empty()) {
    delete g_app_check_map;
    g_app_check_map = nullptr;
  }
}

::firebase::App* AppCheck::app() {
  return internal_ ? internal_->app() : nullptr;
}

void AppCheck::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  if (!internal_) return;
  internal_->SetTokenAutoRefreshEnabled(is_token_auto_refresh_enabled);
}

Future<AppCheckToken> AppCheck::GetAppCheckToken(bool force_refresh) {
  return internal_ ? internal_->GetAppCheckToken(force_refresh)
                   : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetAppCheckTokenLastResult() {
  return internal_ ? internal_->GetAppCheckTokenLastResult()
                   : Future<AppCheckToken>();
}

void AppCheck::AddAppCheckListener(AppCheckListener* listener) {
  if (!internal_) return;
  internal_->AddAppCheckListener(listener);
}

void AppCheck::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!internal_) return;
  internal_->RemoveAppCheckListener(listener);
}

}  // namespace app_check
}  // namespace firebase
