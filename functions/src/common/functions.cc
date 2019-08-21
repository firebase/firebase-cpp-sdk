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

#include <assert.h>

#include <map>
#include <string>

#include "app/src/assert.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

// QueryInternal is defined in these 3 files, one implementation for each OS.
#if FIREBASE_PLATFORM_ANDROID
#include "functions/src/android/functions_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "functions/src/ios/functions_ios.h"
#else
#include "functions/src/desktop/functions_desktop.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

// Register the module initializer.
FIREBASE_APP_REGISTER_CALLBACKS(
    functions,
    {
      FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(*app);
      return ::firebase::kInitResultSuccess;
    },
    {
        // Nothing to tear down.
    });

namespace firebase {
namespace functions {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFunctions);

constexpr const char* kDefaultRegion = "us-central1";

Mutex g_functions_lock;  // NOLINT
static std::map<std::pair<App*, std::string>, Functions*>* g_functions =
    nullptr;

Functions* Functions::GetInstance(::firebase::App* app,
                                  InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Functions* Functions::GetInstance(::firebase::App* app, const char* region,
                                  InitResult* init_result_out) {
  MutexLock lock(g_functions_lock);
  if (!g_functions) {
    g_functions = new std::map<std::pair<App*, std::string>, Functions*>();
  }

  // Region we use for our global index of Functions instances.
  // If a null region is given, we use the default region.
  std::string region_idx;
  if (region != nullptr && strlen(region) > 0) {
    region_idx = region;
  } else {
    region_idx = std::string(kDefaultRegion);
  }

  auto it = g_functions->find(std::make_pair(app, region_idx));
  if (it != g_functions->end()) {
    if (init_result_out != nullptr) *init_result_out = kInitResultSuccess;
    return it->second;
  }
  FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(*app, init_result_out);

  Functions* functions = new Functions(app, region_idx.c_str());
  if (!functions->internal_->initialized()) {
    if (init_result_out) *init_result_out = kInitResultFailedMissingDependency;
    delete functions;
    return nullptr;
  }
  g_functions->insert(
      std::make_pair(std::make_pair(app, region_idx), functions));
  if (init_result_out) *init_result_out = kInitResultSuccess;
  return functions;
}

Functions::Functions(::firebase::App* app, const char* region) {
  internal_ = new internal::FunctionsInternal(app, region);

  if (internal_->initialized()) {
    CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app);
    assert(app_notifier);
    app_notifier->RegisterObject(this, [](void* object) {
      Functions* functions = reinterpret_cast<Functions*>(object);
      LogWarning(
          "Functions object 0x%08x should be deleted before the App 0x%08x "
          "it depends upon.",
          static_cast<int>(reinterpret_cast<intptr_t>(functions)),
          static_cast<int>(reinterpret_cast<intptr_t>(functions->app())));
      functions->DeleteInternal();
    });
  }
}

Functions::~Functions() { DeleteInternal(); }

void Functions::DeleteInternal() {
  MutexLock lock(g_functions_lock);

  if (!internal_) return;

  CleanupNotifier* app_notifier = CleanupNotifier::FindByOwner(app());
  assert(app_notifier);
  app_notifier->UnregisterObject(this);
  // Force cleanup to happen first.
  internal_->cleanup().CleanupAll();
  // If a Functions is explicitly deleted, remove it from our cache.
  std::string region = internal_->region();
  std::string region_idx = region.empty() ? kDefaultRegion : region;
  g_functions->erase(std::make_pair(app(), region_idx));
  delete internal_;
  internal_ = nullptr;
  // If it's the last one, delete the map.
  if (g_functions->empty()) {
    delete g_functions;
    g_functions = nullptr;
  }
}

::firebase::App* Functions::app() {
  return internal_ ? internal_->app() : nullptr;
}

HttpsCallableReference Functions::GetHttpsCallable(const char* name) const {
  if (!internal_) return HttpsCallableReference();
  return HttpsCallableReference(internal_->GetHttpsCallable(name));
}

void Functions::UseFunctionsEmulator(const char* origin) {
  if (!internal_) return;
  internal_->UseFunctionsEmulator(origin);
}

}  // namespace functions
}  // namespace firebase
