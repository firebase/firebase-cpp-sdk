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

#include <assert.h>
#include <jni.h>

#include <cstdint>
#include <cstring>

#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util_android.h"

namespace firebase {
namespace analytics {

DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);

// Global reference to the Android Analytics class instance.
// This is initialized in analytics::Initialize() and never released
// during the lifetime of the application.
static jobject g_analytics_class_instance = nullptr;

// Used to retrieve the JNI environment in order to call methods on the
// Android Analytics class.
static const ::firebase::App* g_app = nullptr;

// Used to setup the cache of Analytics class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define ANALYTICS_METHODS(X)                                                  \
  X(SetEnabled, "setAnalyticsCollectionEnabled", "(Z)V"),                     \
  X(LogEvent, "logEvent", "(Ljava/lang/String;Landroid/os/Bundle;)V"),        \
  X(SetUserProperty, "setUserProperty",                                       \
    "(Ljava/lang/String;Ljava/lang/String;)V"),                               \
  X(SetCurrentScreen, "setCurrentScreen",                                     \
    "(Landroid/app/Activity;Ljava/lang/String;Ljava/lang/String;)V"),         \
  X(SetUserId, "setUserId", "(Ljava/lang/String;)V"),                         \
  X(SetSessionTimeoutDuration, "setSessionTimeoutDuration", "(J)V"),          \
  X(ResetAnalyticsData, "resetAnalyticsData", "()V"),                         \
  X(GetAppInstanceId, "getAppInstanceId",                                     \
    "()Lcom/google/android/gms/tasks/Task;"),                                 \
  X(GetInstance, "getInstance", "(Landroid/content/Context;)"                 \
    "Lcom/google/firebase/analytics/FirebaseAnalytics;",                      \
    firebase::util::kMethodTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(analytics, ANALYTICS_METHODS)
METHOD_LOOKUP_DEFINITION(analytics,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/analytics/FirebaseAnalytics",
                         ANALYTICS_METHODS)

// Initialize the Analytics API.
void Initialize(const ::firebase::App& app) {
  if (g_app) {
    LogWarning("%s API already initialized", internal::kAnalyticsModuleName);
    return;
  }
  LogInfo("Firebase Analytics API Initializing");
  FIREBASE_ASSERT(!g_analytics_class_instance);
  JNIEnv* env = app.GetJNIEnv();

  if (!util::Initialize(env, app.activity())) {
    return;
  }

  // Cache method pointers.
  if (!analytics::CacheMethodIds(env, app.activity())) {
    util::Terminate(env);
    return;
  }

  internal::FutureData::Create();
  g_app = &app;
  // Get / create the Analytics singleton.
  jobject analytics_class_instance_local = env->CallStaticObjectMethod(
      analytics::GetClass(), analytics::GetMethodId(analytics::kGetInstance),
      app.activity());
  util::CheckAndClearJniExceptions(env);
  // Keep a reference to the Analytics singleton.
  g_analytics_class_instance =
      env->NewGlobalRef(analytics_class_instance_local);
  FIREBASE_ASSERT(g_analytics_class_instance);

  env->DeleteLocalRef(analytics_class_instance_local);
  internal::RegisterTerminateOnDefaultAppDestroy();
  LogInfo("%s API Initialized", internal::kAnalyticsModuleName);
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() { return g_app != nullptr; }

}  // namespace internal

// Clean up the API.
void Terminate() {
  if (!g_app) {
    LogWarning("%s API already shut down", internal::kAnalyticsModuleName);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  util::CancelCallbacks(env, internal::kAnalyticsModuleName);
  internal::UnregisterTerminateOnDefaultAppDestroy();
  internal::FutureData::Destroy();
  g_app = nullptr;
  env->DeleteGlobalRef(g_analytics_class_instance);
  g_analytics_class_instance = nullptr;
  analytics::ReleaseClass(env);
  util::Terminate(env);
}

// Enable / disable analytics and reporting.
void SetAnalyticsCollectionEnabled(bool enabled) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean collection_enabled = enabled;
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetEnabled),
                      collection_enabled);
  util::CheckAndClearJniExceptions(env);
}

// Build an event bundle using build_bundle and log it.
template <typename BuildBundleFunction>
void LogEvent(JNIEnv* env, const char* name, BuildBundleFunction build_bundle) {
  jobject bundle =
      env->NewObject(util::bundle::GetClass(),
                     util::bundle::GetMethodId(util::bundle::kConstructor));
  build_bundle(bundle);
  jstring event_name_string = env->NewStringUTF(name);
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kLogEvent),
                      event_name_string, bundle);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Failed to log event '%s'", name);
  }
  env->DeleteLocalRef(event_name_string);
  env->DeleteLocalRef(bundle);
}

// Add a string to a Bundle.
void AddToBundle(JNIEnv* env, jobject bundle, const char* key,
                 const char* value) {
  jstring key_string = env->NewStringUTF(key);
  jstring value_string = env->NewStringUTF(value);
  env->CallVoidMethod(bundle,
                      util::bundle::GetMethodId(util::bundle::kPutString),
                      key_string, value_string);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(value_string);
  env->DeleteLocalRef(key_string);
}

// Add a double to a Bundle.
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, double value) {
  jstring key_string = env->NewStringUTF(key);
  env->CallVoidMethod(bundle,
                      util::bundle::GetMethodId(util::bundle::kPutFloat),
                      key_string, static_cast<jfloat>(value));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Add a 64-bit integer to a Bundle.
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, int64_t value) {
  jstring key_string = env->NewStringUTF(key);
  env->CallVoidMethod(bundle, util::bundle::GetMethodId(util::bundle::kPutLong),
                      key_string, static_cast<jlong>(value));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Log an event with one string parameter.
void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) {
    AddToBundle(env, bundle, parameter_name, parameter_value);
  });
}

// Log an event with one float parameter.
void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) {
    AddToBundle(env, bundle, parameter_name, parameter_value);
  });
}

// Log an event with one 64-bit integer parameter.
void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) {
    AddToBundle(env, bundle, parameter_name, parameter_value);
  });
}

/// Log an event with one integer parameter (stored as a 64-bit integer).
void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}

/// Log an event with no parameters.
void LogEvent(const char* name) {
  LogEvent(name, nullptr, static_cast<size_t>(0));
}

// Log an event with associated parameters.
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameters, number_of_parameters](jobject bundle) {
    for (size_t i = 0; i < number_of_parameters; ++i) {
      const Parameter& parameter = parameters[i];
      if (parameter.value.is_int64()) {
        AddToBundle(env, bundle, parameter.name, parameter.value.int64_value());
      } else if (parameter.value.is_double()) {
        AddToBundle(env, bundle, parameter.name,
                    parameter.value.double_value());
      } else if (parameter.value.is_string()) {
        AddToBundle(env, bundle, parameter.name,
                    parameter.value.string_value());
      } else if (parameter.value.is_bool()) {
        // Just use integer 0 or 1.
        AddToBundle(env, bundle, parameter.name,
                    parameter.value.bool_value() ? static_cast<int64_t>(1L)
                                                 : static_cast<int64_t>(0L));
      } else if (parameter.value.is_null()) {
        // Just use integer 0 for null.
        AddToBundle(env, bundle, parameter.name, static_cast<int64_t>(0L));
      } else {
        // Vector or Map were passed in.
        LogError(
            "LogEvent(%s): %s is not a valid parameter value type. "
            "Container types are not allowed. No event was logged.",
            parameter.name, Variant::TypeName(parameter.value.type()));
      }
    }
  });
}

// Set a user property to the given value.
void SetUserProperty(const char* name, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jstring property_name = env->NewStringUTF(name);
  jstring property_value = value ? env->NewStringUTF(value) : nullptr;
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetUserProperty),
                      property_name, property_value);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Unable to set user property name='%s', value='%s'", name, value);
  }
  if (property_value) env->DeleteLocalRef(property_value);
  env->DeleteLocalRef(property_name);
}

// Sets the user ID property. This feature must be used in accordance with
// <a href="https://www.google.com/policies/privacy">
// Google's Privacy Policy</a>
void SetUserId(const char* user_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jstring user_id_value = user_id ? env->NewStringUTF(user_id) : nullptr;
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetUserId),
                      user_id_value);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Unable to set user ID '%s'", user_id);
  }
  if (user_id_value) env->DeleteLocalRef(user_id_value);
}

// Sets the duration of inactivity that terminates the current session.
void SetSessionTimeoutDuration(int64_t milliseconds) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kSetSessionTimeoutDuration),
      milliseconds);
  util::CheckAndClearJniExceptions(env);
}

namespace {
// Data passed into SetCurrentScreen, stored here because it is needed on a
// different thread.
struct SetCurrentScreenData {
  SetCurrentScreenData() : screen_name(nullptr), screen_class(nullptr) {}
  SetCurrentScreenData(const char* screen_name_, const char* screen_class_)
      : screen_name(screen_name_ ? new std::string(screen_name_) : nullptr),
        screen_class(screen_class_ ? new std::string(screen_class_) : nullptr) {
  }
  ~SetCurrentScreenData() {
    if (screen_name) delete screen_name;
    screen_name = nullptr;
    if (screen_class) delete screen_class;
    screen_class = nullptr;
  }
  // These are pointers to std::string because these can be null.
  std::string* screen_name;
  std::string* screen_class;
};
}  // namespace

static void SetCurrentScreenReal(void* raw_data) {
  SetCurrentScreenData* data =
      reinterpret_cast<SetCurrentScreenData*>(raw_data);
  raw_data = nullptr;
  const char* screen_name =
      data->screen_name ? data->screen_name->c_str() : nullptr;
  const char* screen_class =
      data->screen_class ? data->screen_class->c_str() : nullptr;

  JNIEnv* env = g_app->GetJNIEnv();
  jstring jni_screen_name =
      screen_name ? env->NewStringUTF(screen_name) : nullptr;
  jstring jni_screen_class =
      screen_class ? env->NewStringUTF(screen_class) : nullptr;
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetCurrentScreen),
                      g_app->activity(), jni_screen_name, jni_screen_class);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Unable to set current screen name='%s', class='%s'", screen_name,
             screen_class);
  }
  if (jni_screen_name) env->DeleteLocalRef(jni_screen_name);
  if (jni_screen_class) env->DeleteLocalRef(jni_screen_class);

  delete data;
}

void SetCurrentScreen(const char* screen_name, const char* screen_class) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // Run SetCurrentScreen on the main thread.
  SetCurrentScreenData* data =
      new SetCurrentScreenData(screen_name, screen_class);
  // The callback will delete `data`.
  util::RunOnMainThread(g_app->GetJNIEnv(), g_app->activity(),
                        SetCurrentScreenReal, data);
}

void ResetAnalyticsData() {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kResetAnalyticsData));
  util::CheckAndClearJniExceptions(env);
}

Future<std::string> GetAnalyticsInstanceId() {
  FIREBASE_ASSERT_RETURN(GetAnalyticsInstanceIdLastResult(),
                         internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  auto* api = internal::FutureData::Get()->api();
  const auto safe_future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  const auto future_handle = safe_future_handle.get();
  jobject task = env->CallObjectMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kGetAppInstanceId));
  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    util::RegisterCallbackOnTask(
        env, task,
        [](JNIEnv* env, jobject result, util::FutureResult result_code,
           const char* status_message, void* callback_data) {
          auto* future_data = internal::FutureData::Get();
          if (future_data) {
            bool success =
                result_code == util::kFutureResultSuccess && result != nullptr;
            FutureHandleId future_id =
                reinterpret_cast<FutureHandleId>(callback_data);
            FutureHandle handle(future_id);
            future_data->api()->CompleteWithResult(
                handle, success ? 0 : -1,
                success ? ""
                        : status_message ? status_message
                                         : "Unknown error occurred",
                // Both JStringToString and GetMessageFromException are
                // able to handle a nullptr being passed in, and neither
                // deletes the object passed in (so delete it below).
                success ? util::JStringToString(env, result)
                        : util::GetMessageFromException(env, result));
          }
          if (result) env->DeleteLocalRef(result);
        },
        reinterpret_cast<void*>(safe_future_handle.get().id()),
        internal::kAnalyticsModuleName);
    env->DeleteLocalRef(task);
  } else {
    api->CompleteWithResult(safe_future_handle, -1, error.c_str(),
                            std::string());
  }
  return Future<std::string>(api, future_handle);
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  return static_cast<const Future<std::string>&>(
      internal::FutureData::Get()->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

}  // namespace analytics
}  // namespace firebase
