#include "app/src/cleanup_notifier.h"
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

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
      // Cleanup for any AppCheck instance associated with this App is
      // normally performed by CleanupNotifier; delete if still present
      // (for example, if the App had no CleanupNotifier).
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

namespace {

Mutex& GetAppCheckLock() {
  static Mutex lock;
  return lock;
}

std::map<::firebase::App*, AppCheck*>* g_app_check_map = nullptr;
std::map<const AppCheck*, std::shared_ptr<internal::AppCheckInternal>>*
    g_internal_map = nullptr;

// Acquires a shared_ptr to the AppCheckInternal for `app_check` under
// GetAppCheckLock() and releases the lock before returning, preventing both
// TOCTOU data races on `internal_` and lock-order inversions between
// GetAppCheckLock() and AppCheckInternal's internal mutexes.
std::shared_ptr<internal::AppCheckInternal> LockInternal(
    const AppCheck* app_check) {
  MutexLock lock(GetAppCheckLock());
  if (!app_check || !g_internal_map) return nullptr;
  if (auto it = g_internal_map->find(app_check); it != g_internal_map->end()) {
    return it->second;
  }
  return nullptr;
}

}  // namespace

// Define the destructors for the virtual listener/provider/factory classes.
AppCheckListener::~AppCheckListener() {
  std::vector<std::shared_ptr<internal::AppCheckInternal>> internals;
  {
    MutexLock lock(GetAppCheckLock());
    if (g_internal_map) {
      internals.reserve(g_internal_map->size());
      for (const auto& [owner, internal_ptr] : *g_internal_map) {
        [[maybe_unused]] const auto& used_owner = owner;
        if (internal_ptr) {
          internals.push_back(internal_ptr);
        }
      }
    }
  }
  // Invoke RemoveAppCheckListener outside GetAppCheckLock() so a listener
  // callback that calls AppCheck::GetInstance() or destroys a listener never
  // causes an ABBA lock-order inversion.
  for (const auto& internal_ptr : internals) {
    internal_ptr->RemoveAppCheckListener(this);
  }
}

AppCheckProvider::~AppCheckProvider() = default;

void AppCheckProvider::GetLimitedUseToken(
    std::function<void(AppCheckToken, int, const std::string&)>
        completion_callback) {
  LogWarning(
      "A limited-use token was requested, but the custom provider did not "
      "implement the GetLimitedUseToken method. The default implementation is "
      "triggered as a result, and GetToken has been invoked instead.");
  GetToken(completion_callback);
}

AppCheckProviderFactory::~AppCheckProviderFactory() = default;

namespace internal {

AppCheck* GetExistingAppCheckInstance(::firebase::App* app) {
  if (!app) return nullptr;

  MutexLock lock(GetAppCheckLock());
  if (g_app_check_map) {
    if (auto it = g_app_check_map->find(app); it != g_app_check_map->end()) {
      return it->second;
    }
  }
  return nullptr;
}

}  // namespace internal

AppCheck* AppCheck::GetInstance(::firebase::App* app) {
  return GetInstance(app, nullptr);
}

AppCheck* AppCheck::GetInstance(::firebase::App* app,
                                InitResult* init_result_out) {
  if (!app) {
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }

  MutexLock lock(GetAppCheckLock());
  if (!g_app_check_map) {
    g_app_check_map = new std::map<::firebase::App*, AppCheck*>();
  }

  if (auto it = g_app_check_map->find(app); it != g_app_check_map->end()) {
    if (init_result_out) {
      *init_result_out = kInitResultSuccess;
    }
    return it->second;
  }

  AppCheck* app_check = new AppCheck(app);
  g_app_check_map->emplace(app, app_check);
  if (init_result_out) {
    *init_result_out = kInitResultSuccess;
  }
  return app_check;
}

void AppCheck::SetAppCheckProviderFactory(AppCheckProviderFactory* factory) {
  internal::AppCheckInternal::SetAppCheckProviderFactory(factory);
}

AppCheck::AppCheck(::firebase::App* app)
    : internal_(new internal::AppCheckInternal(app)) {
  {
    MutexLock lock(GetAppCheckLock());
    if (!g_internal_map) {
      g_internal_map = new std::map<const AppCheck*,
                                    std::shared_ptr<internal::AppCheckInternal>>();
    }
    (*g_internal_map)[this] =
        std::shared_ptr<internal::AppCheckInternal>(internal_);
  }
  CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app);
  if (app_notifier) {
    app_notifier->RegisterObject(
        this, static_cast<CleanupNotifier::CleanupCallback>([](void* object) {
          AppCheck* app_check = reinterpret_cast<AppCheck*>(object);
          app_check->DeleteInternal();
        }));
  }
}

AppCheck::~AppCheck() { DeleteInternal(); }

void AppCheck::DeleteInternal() {
  std::shared_ptr<internal::AppCheckInternal> internal_to_destroy;
  ::firebase::App* current_app = nullptr;
  {
    MutexLock lock(GetAppCheckLock());
    if (!internal_) return;

    current_app = internal_->app();
    internal_ = nullptr;

    if (g_internal_map) {
      auto it = g_internal_map->find(this);
      if (it != g_internal_map->end()) {
        internal_to_destroy = std::move(it->second);
        g_internal_map->erase(it);
      }
      if (g_internal_map->empty()) {
        delete g_internal_map;
        g_internal_map = nullptr;
      }
    }

    // Remove it from the map.
    if (g_app_check_map) {
      g_app_check_map->erase(current_app);
      if (g_app_check_map->empty()) {
        delete g_app_check_map;
        g_app_check_map = nullptr;
      }
    }
  }

  if (current_app) {
  }

  internal_to_destroy.reset();
}

::firebase::App* AppCheck::app() {
  auto internal = LockInternal(this);
  return internal ? internal->app() : nullptr;
}

const ::firebase::App* AppCheck::app() const {
  auto internal = LockInternal(this);
  return internal ? internal->app() : nullptr;
}

void AppCheck::SetTokenAutoRefreshEnabled(bool is_token_auto_refresh_enabled) {
  auto internal = LockInternal(this);
  if (!internal) return;
  internal->SetTokenAutoRefreshEnabled(is_token_auto_refresh_enabled);
}

Future<AppCheckToken> AppCheck::GetAppCheckToken(bool force_refresh) {
  auto internal = LockInternal(this);
  return internal ? internal->GetAppCheckToken(force_refresh)
                  : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetAppCheckTokenLastResult() {
  auto internal = LockInternal(this);
  return internal ? internal->GetAppCheckTokenLastResult()
                  : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetLimitedUseAppCheckToken() {
  auto internal = LockInternal(this);
  return internal ? internal->GetLimitedUseAppCheckToken()
                  : Future<AppCheckToken>();
}

Future<AppCheckToken> AppCheck::GetLimitedUseAppCheckTokenLastResult() {
  auto internal = LockInternal(this);
  return internal ? internal->GetLimitedUseAppCheckTokenLastResult()
                  : Future<AppCheckToken>();
}

void AppCheck::AddAppCheckListener(AppCheckListener* listener) {
  auto internal = LockInternal(this);
  if (!internal) return;
  internal->AddAppCheckListener(listener);
}

void AppCheck::RemoveAppCheckListener(AppCheckListener* listener) {
  auto internal = LockInternal(this);
  if (!internal) return;
  internal->RemoveAppCheckListener(listener);
}

}  // namespace app_check
}  // namespace firebase
