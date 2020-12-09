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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_UTIL_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/include/google_play_services/availability.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Macros that allow Android code to easily fail initialization if Google Play
// services is unavailable.

#if FIREBASE_PLATFORM_ANDROID
// Return kInitResultFailedMissingDependency if Google Play is unavailable.
#define FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app) \
  if (::google_play_services::CheckAvailability((app).GetJNIEnv(),   \
                                                (app).activity()) != \
      ::google_play_services::kAvailabilityAvailable) {              \
    return ::firebase::kInitResultFailedMissingDependency;           \
  }

// Return null if Google Play is unavailable. Also, if output is non-null, set
// *output to kInitResultFailedMissingDependency.
#define FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(app, output) \
  if (::google_play_services::CheckAvailability((app).GetJNIEnv(),        \
                                                (app).activity()) !=      \
      ::google_play_services::kAvailabilityAvailable) {                   \
    if ((output) != nullptr)                                              \
      *(output) = ::firebase::kInitResultFailedMissingDependency;         \
    return nullptr;                                                       \
  }
#else  // !FIREBASE_PLATFORM_ANDROID
// No-ops on other platforms. Consume the variables to prevent warnings.
#define FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(x) \
  { (void)(x); }
#define FIREBASE_UTIL_RETURN_NULL_IF_GOOGLE_PLAY_UNAVAILABLE(x, y) \
  {                                                                \
    (void)(x);                                                     \
    (void)(y);                                                     \
  }
#endif  // FIREBASE_PLATFORM_ANDROID

// Macro that silences switch case fallthrough warnings when using certain
// compilers.
// For example:
// switch (foo) {
//   case YesIWantThisToFallThrough:
//     FIREBASE_CASE_FALLTHROUGH;
//   case AndDoThis:
//     // some stuff with bar
//     break;
// }
#if __clang__
#define FIREBASE_CASE_FALLTHROUGH [[clang::fallthrough]]
#else
#define FIREBASE_CASE_FALLTHROUGH
#endif  // __clang__

// Class that allows modules to register for callbacks when instances of
// firebase::App are created and destroyed.
// Callbacks are called on the thread that created / destroyed the app.
class AppCallback {
 public:
  // Method which initializes a Firebase module.
  typedef InitResult (*Created)(FIREBASE_NAMESPACE::App* app);
  // Method which terminates / shuts down a Firebase module.
  typedef void (*Destroyed)(FIREBASE_NAMESPACE::App* app);

  // Initialize a module instance.
  //
  // Right now all module auto-initialization is disabled by default.  Module
  // initialization can be enabled on a case by case basis using
  // SetEnabledByName() before creating an App object, for example:
  // SetEnabledByName("analytics", true);
  AppCallback(const char* module_name, Created created, Destroyed destroyed)
      : module_name_(module_name),
        created_(created),
        destroyed_(destroyed),
        enabled_(false) {
    AddCallback(this);
  }

  // Get the name of the module associated with this callback class.
  const char* module_name() const { return module_name_; }

  // Get whether this is enabled.
  bool enabled() const { return enabled_; }

  // Enable / disable this callback object.
  // NOTE: Use of this method is perilous!  This method should only disable
  // a callback *before* any App instances are created otherwise it's possible
  // to get into a state where a module is initialized and will never be torn
  // down.
  void set_enabled(bool enable) { enabled_ = enable; }

  // Called by firebase::App when an instance is created.
  static void NotifyAllAppCreated(
      FIREBASE_NAMESPACE::App* app,
      std::map<std::string, InitResult>* results = nullptr);

  // Called by firebase::App when an App instance is about to be destroyed.
  static void NotifyAllAppDestroyed(FIREBASE_NAMESPACE::App* app);

  // Enable a module callback by name.
  static void SetEnabledByName(const char* name, bool enable);

  // Determine whether a module callback is enabled, by name.
  static bool GetEnabledByName(const char* name);

  // Enable / disable all callbacks.
  static void SetEnabledAll(bool enable);

 private:
  // Called by App when an instance is created.
  InitResult NotifyAppCreated(FIREBASE_NAMESPACE::App* app) const {
    return created_ ? created_(app) : kInitResultSuccess;
  }

  // Called by App when an App instance is about to be destroyed.
  void NotifyAppDestroyed(FIREBASE_NAMESPACE::App* app) const {
    if (destroyed_) destroyed_(app);
  }

  // Add a callback class to the list.
  // NOTE: This is *not* synchronized as it's not possible to use Mutex
  // from static constructors on some platforms.
  static void AddCallback(AppCallback* callback);

 private:
  // Name of the module associated with this callback.
  const char* module_name_;
  // Called when an app has just been created.
  Created created_;
  // Called when an app is about to be destroyed.
  Destroyed destroyed_;
  // Whether this callback is enabled.
  bool enabled_;

  // Global set of callbacks.
  static std::map<std::string, AppCallback*>* callbacks_;
  // Mutex which controls access to callbacks during notification.
  // NOTE: This violates Google's C++ style guide as we do not have access to
  // module initializers in //base so this replicates part of that behavior.
  static Mutex callbacks_mutex_;
};

// Register app callbacks for a module.
//
// This can be used to initialize a module when an app is created and tear it
// down on destruction.
//
// For example:
// FIREBASE_APP_REGISTER_CALLBACKS(\
//   analytics,
//   {
//     if (app == ::firebase::App::GetInstance()) {
//       firebase::analytics::Initialize(*app);
//     }
//     return kInitResultSuccess;
//   },
//   {
//     if (app == ::firebase::App::GetInstance()) {
//       firebase::analytics::Terminate();
//     }
//   });
#define FIREBASE_APP_REGISTER_CALLBACKS(module_name, created_code,             \
                                        destroyed_code)                        \
  namespace firebase {                                                         \
  static InitResult module_name##Created(::firebase::App* app) {               \
    created_code;                                                              \
  }                                                                            \
  static void module_name##Destroyed(::firebase::App* app) { destroyed_code; } \
  static ::firebase::AppCallback module_name##_app_callback(                   \
      #module_name, module_name##Created, module_name##Destroyed);             \
  /* This is a global symbol that is referenced from all compilation units */  \
  /* that include this module. */                                              \
  void* FIREBASE_APP_REGISTER_CALLBACKS_INITIALIZER_NAME(module_name)          \
      FIREBASE_APP_KEEP_SYMBOL = &module_name##_app_callback;                  \
  } /* namespace firebase */

// Helper class to provide easy management and static access of
// ReferenceCountedFutureImpls for modules.
class StaticFutureData {
 public:
  explicit StaticFutureData(int num_functions) : api_(num_functions) {}
  ~StaticFutureData() {}

  ReferenceCountedFutureImpl* api() { return &api_; }

  // Cleanup StaticFutureData for the specified module.
  static void CleanupFutureDataForModule(const void* module_identifier);
  // Get the StaticFutureData instance for the specified module.  Creates a new
  // one if none already exists.
  static StaticFutureData* GetFutureDataForModule(const void* module_identifier,
                                                  int num_functions);

 private:
  ReferenceCountedFutureImpl api_;

  // Mutex which controls access to s_future_datas_
  static Mutex s_futures_mutex_;

  // Static storage of StaticFutureDatas per module.  Keyed on a void pointer
  // that should be declared in the module to serve as a unique identifier.
  static std::map<const void*, StaticFutureData*>* s_future_datas_;

  static StaticFutureData* CreateNewData(const void* module_identifier,
                                         int num_functions);
};

// Platform independent function to split a string based on specified character
// delimiter. Returns a vector of constituent parts.
std::vector<std::string> SplitString(const std::string& s, char delimiter);

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_UTIL_H_
