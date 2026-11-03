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
#include <vector>

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

static const int kErrorCodeFailed = -1;
static const int kErrorSessionExpiredOrCollectionDisabled = -2;

// Used to setup the cache of Analytics class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define ANALYTICS_METHODS(X)                                                  \
  X(SetEnabled, "setAnalyticsCollectionEnabled", "(Z)V"),                     \
  X(SetConsent, "setConsent", "(Ljava/util/Map;)V"),                          \
  X(LogEvent, "logEvent", "(Ljava/lang/String;Landroid/os/Bundle;)V"),        \
  X(SetUserProperty, "setUserProperty",                                       \
    "(Ljava/lang/String;Ljava/lang/String;)V"),                               \
  X(SetUserId, "setUserId", "(Ljava/lang/String;)V"),                         \
  X(SetSessionTimeoutDuration, "setSessionTimeoutDuration", "(J)V"),          \
  X(ResetAnalyticsData, "resetAnalyticsData", "()V"),                         \
  X(GetAppInstanceId, "getAppInstanceId",                                     \
    "()Lcom/google/android/gms/tasks/Task;"),                                 \
  X(GetSessionId, "getSessionId",                                             \
    "()Lcom/google/android/gms/tasks/Task;"),                                 \
  X(GetInstance, "getInstance", "(Landroid/content/Context;)"                 \
    "Lcom/google/firebase/analytics/FirebaseAnalytics;",                      \
    firebase::util::kMethodTypeStatic),                                       \
  X(SetDefaultEventParameters, "setDefaultEventParameters",                   \
    "(Landroid/os/Bundle;)V")
// clang-format on

// clang-format off
#define ANALYTICS_CONSENT_TYPE_FIELDS(X)                                      \
  X(AnalyticsStorage, "ANALYTICS_STORAGE",                                    \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;",          \
      util::kFieldTypeStatic),                                                \
  X(AdStorage, "AD_STORAGE",                                                  \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;",          \
      util::kFieldTypeStatic),                                                \
  X(AdUserData, "AD_USER_DATA",                                               \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;",          \
      util::kFieldTypeStatic),                                                \
  X(AdPersonalization, "AD_PERSONALIZATION",                                  \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;",          \
      util::kFieldTypeStatic)

#define ANALYTICS_CONSENT_STATUS_FIELDS(X)                                    \
  X(Granted, "GRANTED",                                                       \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;",        \
      util::kFieldTypeStatic),                                                \
  X(Denied, "DENIED",                                                         \
    "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;",        \
      util::kFieldTypeStatic)
// clang-format on

METHOD_LOOKUP_DECLARATION(analytics, ANALYTICS_METHODS)
METHOD_LOOKUP_DEFINITION(analytics,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/analytics/FirebaseAnalytics",
                         ANALYTICS_METHODS)

METHOD_LOOKUP_DECLARATION(analytics_consent_type, METHOD_LOOKUP_NONE,
                          ANALYTICS_CONSENT_TYPE_FIELDS)
METHOD_LOOKUP_DEFINITION(
    analytics_consent_type,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/analytics/FirebaseAnalytics$ConsentType",
    METHOD_LOOKUP_NONE, ANALYTICS_CONSENT_TYPE_FIELDS)

METHOD_LOOKUP_DECLARATION(analytics_consent_status, METHOD_LOOKUP_NONE,
                          ANALYTICS_CONSENT_STATUS_FIELDS)
METHOD_LOOKUP_DEFINITION(
    analytics_consent_status,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/analytics/FirebaseAnalytics$ConsentStatus",
    METHOD_LOOKUP_NONE, ANALYTICS_CONSENT_STATUS_FIELDS)

// Initialize the Analytics API.
void Initialize(const ::firebase::App& app) {
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
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
    if (!analytics_consent_type::CacheFieldIds(env, app.activity())) {
      analytics::ReleaseClass(env);
      util::Terminate(env);
      return;
    }
    if (!analytics_consent_status::CacheFieldIds(env, app.activity())) {
      analytics_consent_type::ReleaseClass(env);
      analytics::ReleaseClass(env);
      util::Terminate(env);
      return;
    }

    // Get / create the Analytics singleton.
    jobject analytics_class_instance_local = env->CallStaticObjectMethod(
        analytics::GetClass(), analytics::GetMethodId(analytics::kGetInstance),
        app.activity());
    if (util::CheckAndClearJniExceptions(env) ||
        !analytics_class_instance_local) {
      if (analytics_class_instance_local) {
        env->DeleteLocalRef(analytics_class_instance_local);
      }
      analytics_consent_status::ReleaseClass(env);
      analytics_consent_type::ReleaseClass(env);
      analytics::ReleaseClass(env);
      util::Terminate(env);
      return;
    }
    // Keep a reference to the Analytics singleton.
    g_analytics_class_instance =
        env->NewGlobalRef(analytics_class_instance_local);
    env->DeleteLocalRef(analytics_class_instance_local);
    if (util::CheckAndClearJniExceptions(env) || !g_analytics_class_instance) {
      if (g_analytics_class_instance) {
        env->DeleteGlobalRef(g_analytics_class_instance);
        g_analytics_class_instance = nullptr;
      }
      analytics_consent_status::ReleaseClass(env);
      analytics_consent_type::ReleaseClass(env);
      analytics::ReleaseClass(env);
      util::Terminate(env);
      return;
    }

    internal::FutureData::Create();
    g_app = &app;
    LogInfo("%s API Initialized", internal::kAnalyticsModuleName);
  }
  internal::RegisterTerminateOnAppDestroy(&app);
}

namespace internal {

// Determine whether the analytics module is initialized.
bool IsInitialized() {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  return g_app != nullptr && g_analytics_class_instance != nullptr;
}

}  // namespace internal

// Clean up the API.
void Terminate() {
  JNIEnv* env = nullptr;
  jobject analytics_instance = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (!g_app) {
      LogWarning("%s API already shut down", internal::kAnalyticsModuleName);
      return;
    }
    env = g_app->GetJNIEnv();
    analytics_instance = g_analytics_class_instance;
    g_analytics_class_instance = nullptr;
    g_app = nullptr;
    internal::FutureData::Destroy();
  }
  // Release FutureData::mutex() before CancelCallbacks and CleanupNotifier
  // unregistration to avoid AB-BA lock-order inversion with in-flight JNI task
  // completion callbacks and App destruction callbacks.
  util::CancelCallbacks(env, internal::kAnalyticsModuleName);
  internal::UnregisterTerminateOnAppDestroy();
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (analytics_instance) {
      env->DeleteGlobalRef(analytics_instance);
    }
    if (!g_app) {
      analytics_consent_status::ReleaseClass(env);
      analytics_consent_type::ReleaseClass(env);
      analytics::ReleaseClass(env);
      util::Terminate(env);
    }
  }
}

// Enable / disable analytics and reporting.
void SetAnalyticsCollectionEnabled(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean collection_enabled = enabled;
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetEnabled),
                      collection_enabled);
  util::CheckAndClearJniExceptions(env);
}

void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();

  jobject consent_map =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !consent_map) {
    if (consent_map) env->DeleteLocalRef(consent_map);
    return;
  }
  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (auto it = consent_settings.begin(); it != consent_settings.end(); ++it) {
    jobject consent_type = nullptr;
    switch (it->first) {
      case kConsentTypeAdStorage:
        consent_type =
            env->GetStaticObjectField(analytics_consent_type::GetClass(),
                                      analytics_consent_type::GetFieldId(
                                          analytics_consent_type::kAdStorage));
        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentTypeAdStorage") ||
            !consent_type) {
          if (consent_type) env->DeleteLocalRef(consent_type);
          env->DeleteLocalRef(consent_map);
          return;
        }
        break;
      case kConsentTypeAnalyticsStorage:
        consent_type = env->GetStaticObjectField(
            analytics_consent_type::GetClass(),
            analytics_consent_type::GetFieldId(
                analytics_consent_type::kAnalyticsStorage));

        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentTypeAnalyticsStorage") ||
            !consent_type) {
          if (consent_type) env->DeleteLocalRef(consent_type);
          env->DeleteLocalRef(consent_map);
          return;
        }
        break;
      case kConsentTypeAdUserData:
        consent_type =
            env->GetStaticObjectField(analytics_consent_type::GetClass(),
                                      analytics_consent_type::GetFieldId(
                                          analytics_consent_type::kAdUserData));

        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentTypeAdUserData") ||
            !consent_type) {
          if (consent_type) env->DeleteLocalRef(consent_type);
          env->DeleteLocalRef(consent_map);
          return;
        }
        break;
      case kConsentTypeAdPersonalization:
        consent_type = env->GetStaticObjectField(
            analytics_consent_type::GetClass(),
            analytics_consent_type::GetFieldId(
                analytics_consent_type::kAdPersonalization));

        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentTypeAdPersonalization") ||
            !consent_type) {
          if (consent_type) env->DeleteLocalRef(consent_type);
          env->DeleteLocalRef(consent_map);
          return;
        }
        break;
      default:
        LogError("Unknown ConsentType value: %d", static_cast<int>(it->first));
        env->DeleteLocalRef(consent_map);
        return;
    }
    jobject consent_status = nullptr;
    switch (it->second) {
      case kConsentStatusGranted:
        consent_status =
            env->GetStaticObjectField(analytics_consent_status::GetClass(),
                                      analytics_consent_status::GetFieldId(
                                          analytics_consent_status::kGranted));
        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentStatusGranted") ||
            !consent_status) {
          if (consent_status) env->DeleteLocalRef(consent_status);
          env->DeleteLocalRef(consent_map);
          env->DeleteLocalRef(consent_type);
          return;
        }
        break;
      case kConsentStatusDenied:
        consent_status =
            env->GetStaticObjectField(analytics_consent_status::GetClass(),
                                      analytics_consent_status::GetFieldId(
                                          analytics_consent_status::kDenied));
        if (util::LogException(env, kLogLevelError,
                               "Failed to get ConsentStatusDenied") ||
            !consent_status) {
          if (consent_status) env->DeleteLocalRef(consent_status);
          env->DeleteLocalRef(consent_map);
          env->DeleteLocalRef(consent_type);
          return;
        }
        break;
      default:
        LogError("Unknown ConsentStatus value: %d",
                 static_cast<int>(it->second));
        env->DeleteLocalRef(consent_map);
        env->DeleteLocalRef(consent_type);
        return;
    }
    LogInfo("SetConsent: %d -> %d", static_cast<int>(it->first),
            static_cast<int>(it->second));
    jobject previous = env->CallObjectMethod(consent_map, put_method,
                                             consent_type, consent_status);
    util::CheckAndClearJniExceptions(env);
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(consent_type);
    env->DeleteLocalRef(consent_status);
  }
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetConsent),
                      consent_map);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(consent_map);
}

// Build an event bundle using build_bundle and log it.
template <typename BuildBundleFunction>
void LogEvent(JNIEnv* env, const char* name, BuildBundleFunction build_bundle) {
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  jobject bundle =
      env->NewObject(util::bundle::GetClass(),
                     util::bundle::GetMethodId(util::bundle::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !bundle) {
    LogError("Failed to create Bundle for event '%s'", name);
    if (bundle) env->DeleteLocalRef(bundle);
    return;
  }
  build_bundle(bundle);
  jstring event_name_string = jni::NewRawJString(env, name);
  if (util::CheckAndClearJniExceptions(env) || !event_name_string) {
    LogError("Failed to create event name string for '%s'", name);
    if (event_name_string) env->DeleteLocalRef(event_name_string);
    env->DeleteLocalRef(bundle);
    return;
  }
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
  if (!bundle || !key) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (util::CheckAndClearJniExceptions(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  jstring value_string = value ? jni::NewRawJString(env, value) : nullptr;
  if (value && (util::CheckAndClearJniExceptions(env) || !value_string)) {
    if (value_string) env->DeleteLocalRef(value_string);
    env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(bundle,
                      util::bundle::GetMethodId(util::bundle::kPutString),
                      key_string, value_string);
  util::CheckAndClearJniExceptions(env);
  if (value_string) env->DeleteLocalRef(value_string);
  env->DeleteLocalRef(key_string);
}

// Add a double to a Bundle.
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, double value) {
  if (!bundle || !key) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (util::CheckAndClearJniExceptions(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(bundle,
                      util::bundle::GetMethodId(util::bundle::kPutDouble),
                      key_string, static_cast<jdouble>(value));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Add a 64-bit integer to a Bundle.
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, int64_t value) {
  if (!bundle || !key) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (util::CheckAndClearJniExceptions(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(bundle, util::bundle::GetMethodId(util::bundle::kPutLong),
                      key_string, static_cast<jlong>(value));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Add an ArrayList to the given Bundle.
void AddArrayListToBundle(JNIEnv* env, jobject bundle, const char* key,
                          jobject arraylist) {
  if (!bundle || !key || !arraylist) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (util::CheckAndClearJniExceptions(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(
      bundle, util::bundle::GetMethodId(util::bundle::kPutParcelableArrayList),
      key_string, arraylist);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Add a Bundle to the given Bundle.
void AddBundleToBundle(JNIEnv* env, jobject bundle, const char* key,
                       jobject inner_bundle) {
  if (!bundle || !key || !inner_bundle) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (util::CheckAndClearJniExceptions(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(bundle,
                      util::bundle::GetMethodId(util::bundle::kPutBundle),
                      key_string, inner_bundle);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(key_string);
}

// Declared here so that it can be used, defined below.
jobject MapToBundle(JNIEnv* env, const std::map<Variant, Variant>& map);

// Converts the given vector into a Java ArrayList. It is up to the
// caller to delete the local reference when done.
jobject VectorOfMapsToArrayList(JNIEnv* env,
                                const std::vector<Variant>& vector) {
  jobject arraylist = env->NewObject(
      util::array_list::GetClass(),
      util::array_list::GetMethodId(util::array_list::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !arraylist) {
    if (arraylist) env->DeleteLocalRef(arraylist);
    return nullptr;
  }

  for (const Variant& element : vector) {
    if (element.is_map()) {
      jobject bundle = MapToBundle(env, element.map());
      if (bundle) {
        env->CallBooleanMethod(
            arraylist, util::array_list::GetMethodId(util::array_list::kAdd),
            bundle);
        util::CheckAndClearJniExceptions(env);
        env->DeleteLocalRef(bundle);
      }
    } else {
      LogError("VectorOfMapsToArrayList: Unsupported type (%s) within vector.",
               Variant::TypeName(element.type()));
    }
  }
  return arraylist;
}

// Converts and adds the Variant to the given Bundle.
bool AddVariantToBundle(JNIEnv* env, jobject bundle, const char* key,
                        const Variant& value, bool allow_null_clear = false) {
  if (!bundle || !key) {
    return false;
  }
  if (value.is_int64()) {
    AddToBundle(env, bundle, key, value.int64_value());
  } else if (value.is_double()) {
    AddToBundle(env, bundle, key, value.double_value());
  } else if (value.is_string()) {
    AddToBundle(env, bundle, key, value.string_value());
  } else if (value.is_bool()) {
    // Just use integer 0 or 1.
    AddToBundle(env, bundle, key,
                value.bool_value() ? static_cast<int64_t>(1L)
                                   : static_cast<int64_t>(0L));
  } else if (value.is_null()) {
    if (allow_null_clear) {
      // Clear default parameter key by setting its Bundle string value to null.
      AddToBundle(env, bundle, key, static_cast<const char*>(nullptr));
    } else {
      // Just use integer 0 for null.
      AddToBundle(env, bundle, key, static_cast<int64_t>(0L));
    }
  } else if (value.is_vector()) {
    jobject arraylist = VectorOfMapsToArrayList(env, value.vector());
    if (arraylist) {
      AddArrayListToBundle(env, bundle, key, arraylist);
      env->DeleteLocalRef(arraylist);
    }
  } else if (value.is_map()) {
    jobject inner_bundle = MapToBundle(env, value.map());
    if (inner_bundle) {
      AddBundleToBundle(env, bundle, key, inner_bundle);
      env->DeleteLocalRef(inner_bundle);
    }
  } else {
    // A Variant type that couldn't be handled was passed in.
    return false;
  }
  return true;
}

// Converts the given map into a Java Bundle. It is up to the caller
// to delete the local reference when done.
jobject MapToBundle(JNIEnv* env, const std::map<Variant, Variant>& map) {
  jobject bundle =
      env->NewObject(util::bundle::GetClass(),
                     util::bundle::GetMethodId(util::bundle::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !bundle) {
    if (bundle) env->DeleteLocalRef(bundle);
    return nullptr;
  }
  for (const auto& pair : map) {
    // Only add elements that use a string key
    if (!pair.first.is_string() || !pair.first.string_value()) {
      continue;
    }
    if (!AddVariantToBundle(env, bundle, pair.first.string_value(),
                            pair.second)) {
      LogError("MapToBundle: Unsupported type (%s) within map with key %s.",
               Variant::TypeName(pair.second.type()),
               pair.first.string_value());
    }
    util::CheckAndClearJniExceptions(env);
  }
  return bundle;
}

// Log an event with one string parameter.
void LogEvent(const char* name, const char* parameter_name,
              const char* parameter_value) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent: Parameter name cannot be null.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) {
    AddToBundle(env, bundle, parameter_name,
                parameter_value ? parameter_value : "");
  });
}

// Log an event with one float parameter.
void LogEvent(const char* name, const char* parameter_name,
              const double parameter_value) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent: Parameter name cannot be null.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) {
    AddToBundle(env, bundle, parameter_name, parameter_value);
  });
}

// Log an event with one 64-bit integer parameter.
void LogEvent(const char* name, const char* parameter_name,
              const int64_t parameter_value) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameter_name) {
    LogError("LogEvent: Parameter name cannot be null.");
    return;
  }
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

/// Log an Apple StoreKit 2 transaction. This is a no-op on Android and returns
/// success.
Future<void> LogAppleTransaction(const char* transaction_id) {
  (void)transaction_id;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
  api->Complete(future_handle, kErrorNone, "");
  return Future<void>(api, future_handle.get());
}

Future<void> LogAppleTransactionLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<void>();
  }
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}

// Log an event with associated parameters.
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameters && number_of_parameters > 0) {
    LogError("LogEvent(%s): parameters cannot be null when "
             "number_of_parameters > 0.",
             name);
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  LogEvent(env, name, [env, parameters, number_of_parameters](jobject bundle) {
    for (size_t i = 0; i < number_of_parameters; ++i) {
      const Parameter& parameter = parameters[i];
      if (!parameter.name) {
        LogError("LogEvent: Parameter name cannot be null.");
        continue;
      }
      if (!AddVariantToBundle(env, bundle, parameter.name, parameter.value)) {
        // A Variant type that couldn't be handled was passed in.
        LogError(
            "LogEvent(%s): %s is not a valid parameter value type. "
            "Parameter was not added.",
            parameter.name,
            Variant::TypeName(parameter.value.type()));
      }
    }
  });
}

// log an event and the associated parameters via a vector.
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}

// Set the default event parameters.
void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!parameters && number_of_parameters > 0) {
    LogError("SetDefaultEventParameters: parameters cannot be null when "
             "number_of_parameters > 0.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();

  if (!parameters || number_of_parameters == 0) {
    env->CallVoidMethod(
        g_analytics_class_instance,
        analytics::GetMethodId(analytics::kSetDefaultEventParameters), nullptr);
    if (util::CheckAndClearJniExceptions(env)) {
      LogError("Failed to set default event parameters");
    }
    return;
  }

  jobject bundle =
      env->NewObject(util::bundle::GetClass(),
                     util::bundle::GetMethodId(util::bundle::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !bundle) {
    LogError("Failed to create bundle for SetDefaultEventParameters.");
    if (bundle) env->DeleteLocalRef(bundle);
    return;
  }

  for (size_t i = 0; i < number_of_parameters; ++i) {
    const Parameter& parameter = parameters[i];
    if (!parameter.name) {
      LogError("SetDefaultEventParameters: Parameter name cannot be null.");
      continue;
    }
    if (!AddVariantToBundle(env, bundle, parameter.name, parameter.value,
                            /*allow_null_clear=*/true)) {
      // A Variant type that couldn't be handled was passed in.
      LogError(
          "SetDefaultEventParameters(%s): %s is not a valid parameter value"
          " type. Excluded from default parameters.",
          parameter.name,
          Variant::TypeName(parameter.value.type()));
    }
  }
  env->CallVoidMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kSetDefaultEventParameters), bundle);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Failed to set default event parameters");
  }
  env->DeleteLocalRef(bundle);
}

// Set the default event parameters
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}

/// Initiates on-device conversion measurement given a user email address on iOS
/// (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  (void)email_address;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // No-op on Android
}

/// Initiates on-device conversion measurement given a phone number on iOS
/// (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  (void)phone_number;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // No-op on Android
}

/// Initiates on-device conversion measurement given a hashed user email address
/// on iOS (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email) {
  (void)hashed_email;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // No-op on Android
}

/// Initiates on-device conversion measurement given a hashed phone number on
/// iOS (no-op on Android). On iOS, requires dependency
/// GoogleAppMeasurementOnDeviceConversion to be linked in, otherwise it is a
/// no-op.
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone) {
  (void)hashed_phone;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  // No-op on Android
}

// Set a user property to the given value.
void SetUserProperty(const char* name, const char* value) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("SetUserProperty: Property name cannot be null or empty.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  jstring property_name = jni::NewRawJString(env, name);
  if (util::CheckAndClearJniExceptions(env) || !property_name) {
    if (property_name) env->DeleteLocalRef(property_name);
    return;
  }
  jstring property_value = value ? jni::NewRawJString(env, value) : nullptr;
  if (value && (util::CheckAndClearJniExceptions(env) || !property_value)) {
    if (property_value) env->DeleteLocalRef(property_value);
    env->DeleteLocalRef(property_name);
    return;
  }
  env->CallVoidMethod(g_analytics_class_instance,
                       analytics::GetMethodId(analytics::kSetUserProperty),
                       property_name, property_value);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Unable to set user property name='%s', value='%s'", name,
             value ? value : "(null)");
  }
  if (property_value) env->DeleteLocalRef(property_value);
  env->DeleteLocalRef(property_name);
}

// Sets the user ID property. This feature must be used in accordance with
// <a href="https://www.google.com/policies/privacy">
// Google's Privacy Policy</a>
void SetUserId(const char* user_id) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jstring user_id_value = user_id ? jni::NewRawJString(env, user_id) : nullptr;
  if (user_id && (util::CheckAndClearJniExceptions(env) || !user_id_value)) {
    if (user_id_value) env->DeleteLocalRef(user_id_value);
    return;
  }
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetUserId),
                      user_id_value);
  if (util::CheckAndClearJniExceptions(env)) {
    LogError("Unable to set user ID '%s'", user_id ? user_id : "(null)");
  }
  if (user_id_value) env->DeleteLocalRef(user_id_value);
}

// Sets the duration of inactivity that terminates the current session.
void SetSessionTimeoutDuration(int64_t milliseconds) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kSetSessionTimeoutDuration),
      static_cast<jlong>(milliseconds));
  util::CheckAndClearJniExceptions(env);
}

void ResetAnalyticsData() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kResetAnalyticsData));
  util::CheckAndClearJniExceptions(env);
}

// NO-OP in Android and iOS. Only used in Windows.
void SetLogCallback(const LogCallback&) {}

// NO-OP in Android and iOS. Only used in Windows.
void NotifyAppLifecycleChange(AppLifecycleState) {}

// NO-OP in Android and iOS. Only used in Windows.
bool IsDesktopInitialized() { return false; }

// NO-OP in Android and iOS. Only used in Windows.
void SetDesktopDebugMode(bool) {}

Future<std::string> GetAnalyticsInstanceId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data || !g_app || !g_analytics_class_instance) {
    return Future<std::string>();
  }
  JNIEnv* env = g_app->GetJNIEnv();
  auto* api = future_data->api();
  const auto safe_future_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  const auto future_handle = safe_future_handle.get();
  jobject task = env->CallObjectMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kGetAppInstanceId));
  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    util::RegisterCallbackOnTask(
        env, task,
        [](JNIEnv* env, jobject result, util::FutureResult result_code,
           const char* status_message, void* callback_data) {
          std::lock_guard<std::recursive_mutex> lock(
              internal::FutureData::mutex());
          std::shared_ptr<internal::FutureData> future_data =
              internal::FutureData::GetShared();
          if (future_data) {
            bool success =
                result_code == util::kFutureResultSuccess && result != nullptr;
            FutureHandleId future_id =
                reinterpret_cast<FutureHandleId>(callback_data);
            auto handle = SafeFutureHandle<std::string>(FutureHandle(future_id));
            if (success) {
              future_data->api()->CompleteWithResult(
                  handle, kErrorNone, "", util::JStringToString(env, result));
            } else {
              std::string error_string =
                  util::GetMessageFromException(env, result);
              if (error_string.empty()) {
                error_string = (status_message && *status_message)
                                   ? status_message
                                   : "Unknown error occurred";
              }
              future_data->api()->CompleteWithResult(
                  handle, kErrorCodeFailed, error_string.c_str(),
                  std::string(""));
            }
          }
          if (result) env->DeleteLocalRef(result);
        },
        reinterpret_cast<void*>(safe_future_handle.get().id()),
        internal::kAnalyticsModuleName);
    env->DeleteLocalRef(task);
  } else {
    if (error.empty()) {
      error = "getAppInstanceId() returned null Task";
    }
    api->CompleteWithResult(safe_future_handle, kErrorCodeFailed, error.c_str(),
                            std::string(""));
    if (task) env->DeleteLocalRef(task);
  }
  return Future<std::string>(api, future_handle);
}

Future<std::string> GetAnalyticsInstanceIdLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<std::string>();
  }
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}

Future<int64_t> GetSessionId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data || !g_app || !g_analytics_class_instance) {
    return Future<int64_t>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  JNIEnv* env = g_app->GetJNIEnv();
  jobject task =
      env->CallObjectMethod(g_analytics_class_instance,
                            analytics::GetMethodId(analytics::kGetSessionId));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    util::RegisterCallbackOnTask(
        env, task,
        [](JNIEnv* env, jobject result, util::FutureResult result_code,
           const char* status_message, void* callback_data) {
          std::lock_guard<std::recursive_mutex> lock(
              internal::FutureData::mutex());
          std::shared_ptr<internal::FutureData> future_data =
              internal::FutureData::GetShared();
          if (future_data) {
            FutureHandleId future_id =
                reinterpret_cast<FutureHandleId>(callback_data);
            auto handle = SafeFutureHandle<int64_t>(FutureHandle(future_id));

            if (result_code == util::kFutureResultSuccess) {
              if (result != nullptr) {
                // result is a Long class type, unbox it.
                // It'll get deleted below.
                int64_t session_id = util::JLongToInt64(env, result);
                util::CheckAndClearJniExceptions(env);
                future_data->api()->CompleteWithResult(handle, kErrorNone, "",
                                                       session_id);
              } else {
                // Succeeded, but with a nullptr result.
                // This occurs when AnalyticsStorage consent is set to Denied or
                // the session is expired.
                future_data->api()->CompleteWithResult(
                    handle, kErrorSessionExpiredOrCollectionDisabled,
                    (status_message && *status_message)
                        ? status_message
                        : "AnalyticsStorage consent is set to "
                          "Denied, or session is expired.",
                    int64_t(0));
              }
            } else {
              // Failed, result may be an exception.
              std::string error_string =
                  util::GetMessageFromException(env, result);
              if (error_string.empty()) {
                error_string = (status_message && *status_message)
                                   ? status_message
                                   : "Unknown error occurred";
              }
              future_data->api()->CompleteWithResult(
                  handle, kErrorCodeFailed, error_string.c_str(), int64_t(0));
              LogError("getSessionId() returned an error: %s",
                       error_string.c_str());
            }
          }
          if (result) env->DeleteLocalRef(result);
        },
        reinterpret_cast<void*>(future_handle.get().id()),
        internal::kAnalyticsModuleName);
  } else {
    if (error.empty()) {
      error = "getSessionId() returned null Task";
    }
    LogError("GetSessionId() threw an exception: %s", error.c_str());
    api->CompleteWithResult(future_handle, kErrorCodeFailed, error.c_str(),
                            int64_t(0));
  }
  if (task) env->DeleteLocalRef(task);

  return Future<int64_t>(api, future_handle.get());
}

Future<int64_t> GetSessionIdLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  std::shared_ptr<internal::FutureData> future_data =
      internal::FutureData::GetShared();
  if (!future_data) {
    return Future<int64_t>();
  }
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetSessionId));
}

}  // namespace analytics
}  // namespace firebase
