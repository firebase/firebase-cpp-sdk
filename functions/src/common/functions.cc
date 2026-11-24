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

#include <cstring>
#include <memory>
#include <string>
#include <utility>

#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"

// FunctionsInternal is defined in these 3 files, one implementation for each OS.
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
    functions, { return ::firebase::kInitResultSuccess; }, false);

namespace firebase {
namespace functions {

DEFINE_FIREBASE_VERSION_STRING(FirebaseFunctions);

namespace {

constexpr const char* kDefaultRegion = "us-central1";
constexpr int kFunctionsServiceTag = 0;

}  // namespace

Functions::Functions(std::shared_ptr<internal::FunctionsInternal> internal)
    : internal_(std::move(internal)) {}

Functions Functions::GetInstance(const ::firebase::App& app,
                                 InitResult* init_result_out) {
  return GetInstance(app, nullptr, init_result_out);
}

Functions Functions::GetInstance(const ::firebase::App& app, const char* region,
                                 InitResult* init_result_out) {
  auto state = ::firebase::internal::AppState::FromApp(app);
  if (!state) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Functions();
  }

  // Region we use as the per-App service key.
  // If a null or empty region is given, we use the default region.
  std::string region_idx;
  if (region != nullptr && strlen(region) > 0) {
    region_idx = region;
  } else {
    region_idx = std::string(kDefaultRegion);
  }

  auto internal = state->GetOrCreateService<internal::FunctionsInternal>(
      &kFunctionsServiceTag, region_idx,
      [&app, &region_idx](::firebase::internal::AppState& app_state)
          -> std::shared_ptr<internal::FunctionsInternal> {
        auto inst = std::make_shared<internal::FunctionsInternal>(
            app_state, app, region_idx.c_str());
        if (!inst->initialized()) {
          return nullptr;
        }
        return inst;
      });

  if (!internal) {
    if (init_result_out != nullptr) {
      *init_result_out = kInitResultFailedMissingDependency;
    }
    return Functions();
  }

  if (init_result_out != nullptr) {
    *init_result_out = kInitResultSuccess;
  }
  return Functions(std::move(internal));
}

::firebase::App Functions::app() const {
  return internal_ ? internal_->app() : ::firebase::App();
}

HttpsCallableReference Functions::GetHttpsCallable(const char* name) const {
  if (!internal_) return HttpsCallableReference();
  return HttpsCallableReference(internal_->GetHttpsCallable(internal_, name));
}

HttpsCallableReference Functions::GetHttpsCallable(
    const char* name, const HttpsCallableOptions& options) const {
  if (!internal_) return HttpsCallableReference();
  return HttpsCallableReference(
      internal_->GetHttpsCallable(internal_, name, options));
}

HttpsCallableReference Functions::GetHttpsCallableFromURL(
    const char* url) const {
  if (!internal_) return HttpsCallableReference();
  return HttpsCallableReference(
      internal_->GetHttpsCallableFromURL(internal_, url));
}

HttpsCallableReference Functions::GetHttpsCallableFromURL(
    const char* url, const HttpsCallableOptions& options) const {
  if (!internal_) return HttpsCallableReference();
  return HttpsCallableReference(
      internal_->GetHttpsCallableFromURL(internal_, url, options));
}

void Functions::UseFunctionsEmulator(const char* origin) {
  if (!internal_) return;
  internal_->UseFunctionsEmulator(origin);
}

}  // namespace functions
}  // namespace firebase
