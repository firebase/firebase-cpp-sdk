// Copyright 2017 Google LLC
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

#include "functions/src/include/firebase/functions.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "functions/src/include/firebase/functions/callable_reference.h"

// Include the header that matches the platform being used.
#if FIREBASE_PLATFORM_ANDROID
#include "functions/src/android/functions_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "functions/src/ios/functions_ios.h"
#else
#include "functions/src/desktop/functions_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    functions,
    {
      if (app == ::firebase::App::GetInstance()) {
        firebase::InitResult init_result;
        (void)firebase::functions::Functions::GetInstance(app, &init_result);
        return init_result;
      }
      return kInitResultSuccess;
    },
    {
      // Cleanup for any Functions instances associated with this App is
      // performed by CleanupNotifier.
    },
    false);

namespace firebase {
namespace functions {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFunctions);

namespace {
using FunctionsMapKey = std::pair<App*, std::string>;
using FunctionsMap = std::map<FunctionsMapKey, Functions*, std::less<>>;

Mutex& GetFunctionsLock() {
  static auto* lock = new Mutex();
  return *lock;
}

FunctionsMap* g_functions = nullptr;

FunctionsMap& GetFunctionsMap() {
  if (!g_functions) {
    g_functions = new FunctionsMap();
  }
  return *g_functions;
}
}  // namespace

Functions* Functions::GetInstance(::firebase::App* app,
                                  InitResult* init_result_out) {
  return GetInstance(app, "us-central1", init_result_out);
}

Functions* Functions::GetInstance(::firebase::App* app, const char* region,
                                  InitResult* init_result_out) {
  if (!app) {
    LogError("Functions::GetInstance: App cannot be null.");
    if (init_result_out) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return nullptr;
  }
  const char* effective_region =
      (region && region[0] != '\0') ? region : "us-central1";

  Functions* failed_functions = nullptr;
  {
    MutexLock lock(GetFunctionsLock());
    auto& functions_map = GetFunctionsMap();
    std::pair key{app, std::string(effective_region)};
    auto it = functions_map.find(key);
    if (it != functions_map.end()) {
      if (init_result_out != nullptr) {
        *init_result_out = kInitResultSuccess;
      }
      return it->second;
    }

    Functions* functions = new Functions(app, effective_region);
    if (!functions->internal_ || !functions->internal_->is_valid()) {
      if (init_result_out) {
        *init_result_out = kInitResultFailedMissingDependency;
      }
      failed_functions = functions;
    } else {
      functions_map[key] = functions;
      if (init_result_out) {
        *init_result_out = kInitResultSuccess;
      }
      return functions;
    }
  }
  delete failed_functions;
  return nullptr;
}

Functions::Functions(::firebase::App* app, const char* region) {
  FIREBASE_ASSERT(app != nullptr);
  const char* effective_region =
      (region && region[0] != '\0') ? region : "us-central1";
  internal_ = new internal::FunctionsInternal(app, effective_region);
  if (!internal_->is_valid()) {
    delete internal_;
    internal_ = nullptr;
    return;
  }
  CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app);
  FIREBASE_ASSERT(notifier);
  notifier->RegisterObject(this, [](void* object) {
    Functions* functions = reinterpret_cast<Functions*>(object);
    LogWarning(
        "Functions object 0x%08x should be deleted before the App 0x%08x "
        "it depends upon.",
        static_cast<int>(reinterpret_cast<intptr_t>(functions)),
        static_cast<int>(reinterpret_cast<intptr_t>(functions->app())));
    functions->DeleteInternal();
  });
}

Functions::~Functions() { DeleteInternal(); }

void Functions::DeleteInternal() {
  App* my_app = nullptr;
  internal::FunctionsInternal* internal_to_delete = nullptr;
  {
    MutexLock lock(GetFunctionsLock());
    if (!internal_) return;

    my_app = app();
    std::string my_region = region();

    // If a Functions is explicitly deleted, remove it from our cache inside
    // the lock so concurrent GetInstance calls do not return a half-deleted
    // instance.
    if (g_functions) {
      g_functions->erase({my_app, my_region});
      for (auto it = g_functions->begin(); it != g_functions->end();) {
        if (it->second == this) {
          it = g_functions->erase(it);
        } else {
          ++it;
        }
      }
      if (g_functions->empty()) {
        delete g_functions;
        g_functions = nullptr;
      }
    }

    internal_to_delete = std::exchange(internal_, nullptr);
  }

  if (my_app) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(my_app);
    if (notifier) {
      notifier->UnregisterObject(this);
    }
  }

  // Force cleanup of any live HttpsCallableReference instances first.
  internal_to_delete->cleanup().CleanupAll();
  delete internal_to_delete;
}

::firebase::App* Functions::app() {
  return internal_ ? internal_->app() : nullptr;
}

::firebase::App* Functions::app() const {
  return internal_ ? internal_->app() : nullptr;
}

const char* Functions::region() const {
  return internal_ ? internal_->region() : "";
}

HttpsCallableReference Functions::GetHttpsCallable(const char* name) const {
  if (!internal_ || !name) return HttpsCallableReference();
  return HttpsCallableReference(internal_->GetHttpsCallable(name));
}

HttpsCallableReference Functions::GetHttpsCallable(
    const char* name, const HttpsCallableOptions& options) const {
  return GetHttpsCallableWithOptions(name, options);
}

HttpsCallableReference Functions::GetHttpsCallableWithOptions(
    const char* name, const HttpsCallableOptions& options) const {
  if (!internal_ || !name) return HttpsCallableReference();
  return HttpsCallableReference(
      internal_->GetHttpsCallableWithOptions(name, options));
}

HttpsCallableReference Functions::GetHttpsCallableFromURL(
    const char* url) const {
  if (!internal_ || !url) return HttpsCallableReference();
  return HttpsCallableReference(internal_->GetHttpsCallableFromURL(url));
}

HttpsCallableReference Functions::GetHttpsCallableFromURL(
    const char* url, const HttpsCallableOptions& options) const {
  return GetHttpsCallableFromURLWithOptions(url, options);
}

HttpsCallableReference Functions::GetHttpsCallableFromURLWithOptions(
    const char* url, const HttpsCallableOptions& options) const {
  if (!internal_ || !url) return HttpsCallableReference();
  return HttpsCallableReference(
      internal_->GetHttpsCallableFromURLWithOptions(url, options));
}

void Functions::UseFunctionsEmulator(const char* origin) {
  if (!internal_ || !origin) return;
  internal_->UseFunctionsEmulator(origin);
}

void Functions::UseEmulator(const char* host, int port) {
  if (!internal_ || !host || host[0] == '\0' || port <= 0) return;
  std::string origin = std::string(host) + ":" + std::to_string(port);
  internal_->UseFunctionsEmulator(origin.c_str());
}

}  // namespace functions
}  // namespace firebase
