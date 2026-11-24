/*
 * Copyright 2016 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_SRC_UTIL_H_
#define FIREBASE_APP_SRC_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/google_play_services/availability.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {

#if FIREBASE_PLATFORM_ANDROID
// Return kInitResultFailedMissingDependency if Google Play is unavailable.
#define FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app) \
  if (::google_play_services::CheckAvailability((app).GetJNIEnv(),   \
                                                (app).activity()) != \
      ::google_play_services::kAvailabilityAvailable) {              \
    return ::firebase::kInitResultFailedMissingDependency;           \
  }

// Return an invalid/default value if Google Play is unavailable. Also, if
// output is non-null, set *output to kInitResultFailedMissingDependency.
#define FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(app, output) \
  if (::google_play_services::CheckAvailability((app).GetJNIEnv(),        \
                                                (app).activity()) !=      \
      ::google_play_services::kAvailabilityAvailable) {                   \
    if ((output) != nullptr)                                              \
      *(output) = ::firebase::kInitResultFailedMissingDependency;         \
    return {};                                                            \
  }
#else  // !FIREBASE_PLATFORM_ANDROID
#define FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(x) \
  { (void)(x); }
#define FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(x, y) \
  {                                                                \
    (void)(x);                                                     \
    (void)(y);                                                     \
  }
#endif  // FIREBASE_PLATFORM_ANDROID

#if __clang__
#define FIREBASE_CASE_FALLTHROUGH [[clang::fallthrough]]
#else
#define FIREBASE_CASE_FALLTHROUGH
#endif  // __clang__

// Class that allows modules to register for callbacks when instances of
// firebase::App are created (e.g., App Check auto-attaching to each AppState).
// Teardown is handled automatically by AppState::DestroyServices() when the
// last handle to AppState drops.
class AppCallback {
 public:
  typedef InitResult (*Created)(const firebase::App& app);

  AppCallback(const char* module_name, Created created, bool enabled_by_default)
      : module_name_(module_name),
        created_(created),
        enabled_(enabled_by_default) {
    AddCallback(this);
  }

  const char* module_name() const { return module_name_; }
  bool enabled() const { return enabled_; }
  void set_enabled(bool enable) { enabled_ = enable; }

  static void NotifyAllAppCreated(
      const firebase::App& app,
      std::map<std::string, InitResult>* results = nullptr);

  static void SetEnabledByName(const char* name, bool enable);
  static bool GetEnabledByName(const char* name);
  static void SetEnabledAll(bool enable);

 private:
  InitResult NotifyAppCreated(const firebase::App& app) const {
    return created_ ? created_(app) : kInitResultSuccess;
  }

  static void AddCallback(AppCallback* callback);

  const char* module_name_;
  Created created_;
  bool enabled_;
};

// Register an App creation callback for a module.
// Teardown is owned by AppState via internal::Service destructors.
#define FIREBASE_APP_REGISTER_CALLBACKS(module_name, created_code,     \
                                        enabled_by_default)            \
  namespace firebase {                                                 \
  static InitResult module_name##Created(const ::firebase::App& app) { \
    created_code;                                                      \
  }                                                                    \
  static ::firebase::AppCallback module_name##_app_callback(           \
      #module_name, module_name##Created, enabled_by_default);         \
  void* FIREBASE_APP_REGISTER_CALLBACKS_INITIALIZER_NAME(module_name)  \
      FIREBASE_APP_KEEP_SYMBOL = &module_name##_app_callback;          \
  } /* namespace firebase */

// Helper class to provide easy management and static access of
// ReferenceCountedFutureImpls for modules.
class StaticFutureData {
 public:
  explicit StaticFutureData(int num_functions) : api_(num_functions) {}
  ~StaticFutureData() = default;

  ReferenceCountedFutureImpl* api() { return &api_; }

  static void CleanupFutureDataForModule(const void* module_identifier);
  static StaticFutureData* GetFutureDataForModule(const void* module_identifier,
                                                  int num_functions);

 private:
  ReferenceCountedFutureImpl api_;

  static StaticFutureData* CreateNewData(const void* module_identifier,
                                         int num_functions);
};

std::vector<std::string> SplitString(const std::string& s, char delimiter);

std::string CreateApiIdentifier(const char* api_id, void* object);

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_UTIL_H_
