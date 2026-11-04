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
#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/app_android.h"
#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jni/collection.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include <assert.h>
#include <cstdint>
#include <cstring>
#include <jni.h>
#include <vector>
namespace firebase {
namespace analytics {
DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);
#define CLEANUP_JNI(env)                                                                                                                                                                               \
  analytics_consent_status::ReleaseClass(env);                                                                                                                                                         \
  analytics_consent_type::ReleaseClass(env);                                                                                                                                                           \
  analytics::ReleaseClass(env);                                                                                                                                                                        \
  ::firebase::jni::ReleaseCollectionClasses(env);                                                                                                                                                      \
  util::Terminate(env);
static jobject g_analytics_class_instance = nullptr;
static const ::firebase::App *g_app = nullptr;
static const int kErrorCodeFailed = -1;
static const int kErrorSessionExpiredOrCollectionDisabled = -2;
#define ANALYTICS_METHODS(X) X(SetEnabled, "setAnalyticsCollectionEnabled", "(Z)V") X(SetConsent, "setConsent", "(Ljava/util/Map;)V") X(LogEvent, "logEvent", "(Ljava/lang/String;Landroid/os/Bundle;)V") X(SetUserProperty, "setUserProperty", "(Ljava/lang/String;Ljava/lang/String;)V") X(SetUserId, "setUserId", "(Ljava/lang/String;)V") X(SetSessionTimeoutDuration, "setSessionTimeoutDuration", "(J)V") X(ResetAnalyticsData, "resetAnalyticsData", "()V") X(GetAppInstanceId, "getAppInstanceId", "()Lcom/google/android/gms/tasks/Task;") X(GetSessionId, "getSessionId", "()Lcom/google/android/gms/tasks/Task;") X(GetInstance, "getInstance", "(Landroid/content/Context;)Lcom/google/firebase/analytics/FirebaseAnalytics;", jni::kMethodTypeStatic) X(SetDefaultEventParameters, "setDefaultEventParameters", "(Landroid/os/Bundle;)V")
#define ANALYTICS_CONSENT_TYPE_FIELDS(X) X(AnalyticsStorage, "ANALYTICS_STORAGE", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic) X(AdStorage, "AD_STORAGE", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic) X(AdUserData, "AD_USER_DATA", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic) X(AdPersonalization, "AD_PERSONALIZATION", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic)
#define ANALYTICS_CONSENT_STATUS_FIELDS(X) X(Granted, "GRANTED", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;", jni::kFieldTypeStatic) X(Denied, "DENIED", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;", jni::kFieldTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(analytics, ANALYTICS_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/analytics/FirebaseAnalytics") JNI_METHOD_LOOKUP_DECLARATION(analytics_consent_type, METHOD_LOOKUP_NONE, ANALYTICS_CONSENT_TYPE_FIELDS, "com/google/firebase/analytics/FirebaseAnalytics$ConsentType") JNI_METHOD_LOOKUP_DECLARATION(analytics_consent_status, METHOD_LOOKUP_NONE, ANALYTICS_CONSENT_STATUS_FIELDS, "com/google/firebase/analytics/FirebaseAnalytics$ConsentStatus") void Initialize(const ::firebase::App &app) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  if (g_app) return;
  JNIEnv *env = app.GetJNIEnv();
  if (!util::Initialize(env, app.activity())) return;
  if (!::firebase::jni::CacheCollectionMethodIds(env, app.activity()) ||
      !analytics::CacheMethodIds(env, app.activity()) ||
      !analytics_consent_type::CacheFieldIds(env, app.activity()) ||
      !analytics_consent_status::CacheFieldIds(env, app.activity())) { CLEANUP_JNI(env); return; }
  jobject local = env->CallStaticObjectMethod(analytics::GetClass(), analytics::GetMethodId(analytics::kGetInstance), app.activity());
  if (jni::CheckAndClearException(env) || !local) { if (local) env->DeleteLocalRef(local); CLEANUP_JNI(env); return; }
  g_analytics_class_instance = env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  if (jni::CheckAndClearException(env) || !g_analytics_class_instance) {
    if (g_analytics_class_instance) { env->DeleteGlobalRef(g_analytics_class_instance); g_analytics_class_instance = nullptr; }
    CLEANUP_JNI(env); return;
  }
  internal::FutureData::Create();
  g_app = &app;
  internal::RegisterTerminateOnAppDestroy(&app);
}
namespace internal {
  bool IsInitialized() {
    std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
    return g_app != nullptr && g_analytics_class_instance != nullptr;
  }
  }
  void Terminate() {
  JNIEnv *env = nullptr;
  jobject instance = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (!g_app) return;
    env = g_app->GetJNIEnv();
    instance = g_analytics_class_instance;
    g_analytics_class_instance = nullptr;
    g_app = nullptr;
    internal::FutureData::Destroy();
  }
  ::firebase::util::CancelCallbacks(env, internal::kAnalyticsModuleName);
  internal::UnregisterTerminateOnAppDestroy();
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (instance) env->DeleteGlobalRef(instance);
    if (!g_app) { CLEANUP_JNI(env); }
  }
}
void SetAnalyticsCollectionEnabled(bool enabled) {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
    JNIEnv *env = g_app->GetJNIEnv();
    jboolean collection_enabled = enabled;
    env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetEnabled), collection_enabled);
    jni::CheckAndClearException(env);
  }
  void SetConsent(const std::map<ConsentType, ConsentStatus> &consent_settings) {
    void SetConsent(const std::map<ConsentType, ConsentStatus> &consent_settings) {
      std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
      FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
      JNIEnv *env = g_app->GetJNIEnv();
      jobject consent_map = env->NewObject(::firebase::jni::hash_map::GetClass(), ::firebase::jni::hash_map::GetMethodId(::firebase::jni::hash_map::kConstructor));
      if (jni::CheckAndClearException(env) || !consent_map) {
        if (consent_map) env->DeleteLocalRef(consent_map);
        return;
      }
      jmethodID put_method = ::firebase::jni::hash_map::GetMethodId(::firebase::jni::hash_map::kPut);
      for (auto it = consent_settings.begin(); it != consent_settings.end(); ++it) {
        analytics_consent_type::Field t_field;
        switch (it->first) {
        case kConsentTypeAdStorage: t_field = analytics_consent_type::kAdStorage; break;
        case kConsentTypeAnalyticsStorage: t_field = analytics_consent_type::kAnalyticsStorage; break;
        case kConsentTypeAdUserData: t_field = analytics_consent_type::kAdUserData; break;
        case kConsentTypeAdPersonalization: t_field = analytics_consent_type::kAdPersonalization; break;
        default: return;
        }
        jobject consent_type = env->GetStaticObjectField(analytics_consent_type::GetClass(), analytics_consent_type::GetFieldId(t_field));
        if (::firebase::jni::CheckAndClearException(env) || !consent_type) {
          if (consent_type) env->DeleteLocalRef(consent_type);
          env->DeleteLocalRef(consent_map);
          return;
        }
        analytics_consent_status::Field s_field;
        switch (it->second) {
        case kConsentStatusGranted: s_field = analytics_consent_status::kGranted; break;
        case kConsentStatusDenied: s_field = analytics_consent_status::kDenied; break;
        default: env->DeleteLocalRef(consent_type); return;
        }
        jobject consent_status = env->GetStaticObjectField(analytics_consent_status::GetClass(), analytics_consent_status::GetFieldId(s_field));
        if (::firebase::jni::CheckAndClearException(env) || !consent_status) {
          if (consent_status) env->DeleteLocalRef(consent_status);
          env->DeleteLocalRef(consent_map);
          env->DeleteLocalRef(consent_type);
          return;
        }
        jobject previous = env->CallObjectMethod(consent_map, put_method, consent_type, consent_status);
        jni::CheckAndClearException(env);
        if (previous) env->DeleteLocalRef(previous);
        env->DeleteLocalRef(consent_type);
        env->DeleteLocalRef(consent_status);
      }
      env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetConsent), consent_map);
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(consent_map);
    }
    template <typename BuildBundleFunction> void LogEvent(JNIEnv * env, const char *name, BuildBundleFunction build_bundle) {
      if (!name || name[0] == '\0') {}
      jobject bundle = env->NewObject(::firebase::jni::bundle::GetClass(), ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
      if (jni::CheckAndClearException(env) || !bundle) { return; }
      build_bundle(bundle);
      jstring event_name_string = jni::NewRawJString(env, name);
      if (jni::CheckAndClearException(env) || !event_name_string) {
        env->DeleteLocalRef(bundle);
        return;
      }
      env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kLogEvent), event_name_string, bundle);
      env->DeleteLocalRef(bundle);
    }
    void AddToBundle(JNIEnv * env, jobject bundle, const char *key, const char *value) {
      if (!bundle || !key) return;
      jstring key_string = jni::NewRawJString(env, key);
      if (jni::CheckAndClearException(env) || !key_string) {
        if (key_string) env->DeleteLocalRef(key_string);
        return;
      }
      jstring value_string = value ? jni::NewRawJString(env, value) : nullptr;
      if (value && (jni::CheckAndClearException(env) || !value_string)) {
        if (value_string) env->DeleteLocalRef(value_string);
        env->DeleteLocalRef(key_string);
        return;
      }
      env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutString), key_string, value_string);
      jni::CheckAndClearException(env);
      if (value_string) env->DeleteLocalRef(value_string);
      env->DeleteLocalRef(key_string);
    }
    void AddToBundle(JNIEnv * env, jobject bundle, const char *key, double value) {
      if (!bundle || !key) return;
      jstring key_string = jni::NewRawJString(env, key);
      if (jni::CheckAndClearException(env) || !key_string) {
        if (key_string) env->DeleteLocalRef(key_string);
        return;
      }
      env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutDouble), key_string, static_cast<jdouble>(value));
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(key_string);
    }
    void AddToBundle(JNIEnv * env, jobject bundle, const char *key, int64_t value) {
      if (!bundle || !key) return;
      jstring key_string = jni::NewRawJString(env, key);
      if (jni::CheckAndClearException(env) || !key_string) {
        if (key_string) env->DeleteLocalRef(key_string);
        return;
      }
      env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutLong), key_string, static_cast<jlong>(value));
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(key_string);
    }
    void AddArrayListToBundle(JNIEnv * env, jobject bundle, const char *key, jobject arraylist) {
      if (!bundle || !key || !arraylist) return;
      jstring key_string = jni::NewRawJString(env, key);
      if (jni::CheckAndClearException(env) || !key_string) {
        if (key_string) env->DeleteLocalRef(key_string);
        return;
      }
      env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutParcelableArrayList), key_string, arraylist);
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(key_string);
    }
    void AddBundleToBundle(JNIEnv * env, jobject bundle, const char *key, jobject inner_bundle) {
      if (!bundle || !key || !inner_bundle) return;
      jstring key_string = jni::NewRawJString(env, key);
      if (jni::CheckAndClearException(env) || !key_string) {
        if (key_string) env->DeleteLocalRef(key_string);
        return;
      }
      env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutBundle), key_string, inner_bundle);
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(key_string);
    }
    jobject MapToBundle(JNIEnv * env, const std::map<Variant, Variant> &map);
    jobject VectorOfMapsToArrayList(JNIEnv * env, const std::vector<Variant> &vector) {
      jobject arraylist = env->NewObject(::firebase::jni::array_list::GetClass(), ::firebase::jni::array_list::GetMethodId(::firebase::jni::array_list::kConstructor));
      if (jni::CheckAndClearException(env) || !arraylist) {
        if (arraylist) env->DeleteLocalRef(arraylist);
        return nullptr;
      }
      for (const Variant &element : vector) {
        if (element.is_map()) {
          jobject bundle = MapToBundle(env, element.map());
          if (bundle) {
            env->CallBooleanMethod(arraylist, ::firebase::jni::array_list::GetMethodId(::firebase::jni::array_list::kAdd), bundle);
            jni::CheckAndClearException(env);
            env->DeleteLocalRef(bundle);
          }
        } else {
        }
        bool AddVariantToBundle(JNIEnv * env, jobject bundle, const char *key, const Variant &value, bool allow_null_clear = false) {
          if (!bundle || !key) { return false; }
          if (value.is_int64()) {
            AddToBundle(env, bundle, key, value.int64_value());
          } else if (value.is_double()) {
            AddToBundle(env, bundle, key, value.double_value());
          } else if (value.is_string()) {
            AddToBundle(env, bundle, key, value.string_value());
          } else if (value.is_bool()) {
            AddToBundle(env, bundle, key, value.bool_value() ? static_cast<int64_t>(1L) : static_cast<int64_t>(0L));
          } else if (value.is_null()) {
            if (allow_null_clear) {
              AddToBundle(env, bundle, key, static_cast<const char *>(nullptr));
            } else {
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
            return false;
          }
          return true;
        }
        jobject MapToBundle(JNIEnv * env, const std::map<Variant, Variant> &map) {
          jobject bundle = env->NewObject(::firebase::jni::bundle::GetClass(), ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
          if (jni::CheckAndClearException(env) || !bundle) {
            if (bundle) env->DeleteLocalRef(bundle);
            return nullptr;
          }
          for (const auto &pair : map) {
            if (!pair.first.is_string() || !pair.first.string_value()) { continue; }
            if (!AddVariantToBundle(env, bundle, pair.first.string_value(), pair.second)) {}
            jni::CheckAndClearException(env);
          }
          return bundle;
        }
        template <typename T> void LogEventSingleParam(const char *name, const char *parameter_name, const T parameter_value) {
          std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
          FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
          if (!name || name[0] == '\0') {}
          if (!parameter_name) {}
          JNIEnv *env = g_app->GetJNIEnv();
          LogEvent(env, name, [env, parameter_name, parameter_value](jobject bundle) { AddToBundle(env, bundle, parameter_name, parameter_value); });
        }
        void LogEvent(const char *name, const char *parameter_name, const char *parameter_value) { LogEventSingleParam(name, parameter_name, parameter_value ? parameter_value : ""); }
        void LogEvent(const char *name, const char *parameter_name, const double parameter_value) { LogEventSingleParam(name, parameter_name, parameter_value); }
        void LogEvent(const char *name, const char *parameter_name, const int64_t parameter_value) { LogEventSingleParam(name, parameter_name, parameter_value); }
        void LogEvent(const char *name, const char *parameter_name, const int parameter_value) { LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value)); }
        void LogEvent(const char *name) { LogEvent(name, nullptr, static_cast<size_t>(0)); }
        Future<void> LogAppleTransaction(const char *) { return internal::CompleteStubFuture<void>(internal::kAnalyticsFnLogAppleTransaction, nullptr); }
        Future<void> LogAppleTransactionLastResult() { return internal::GetLastResult<void>(internal::kAnalyticsFnLogAppleTransaction); }
        void LogEvent(const char *name, const Parameter *parameters, size_t number_of_parameters) {
          std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
          FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
          if (!name || name[0] == '\0') {}
          if (!parameters && number_of_parameters > 0) { return; }
          JNIEnv *env = g_app->GetJNIEnv();
          LogEvent(env, name, [env, parameters, number_of_parameters](jobject bundle) {
            for (size_t i = 0; i < number_of_parameters; ++i) {
              const Parameter &parameter = parameters[i];
              if (!parameter.name) {}
              if (!AddVariantToBundle(env, bundle, parameter.name, parameter.value)) {}
            }
          });
        }
        void LogEvent(const char *name, const std::vector<Parameter> &parameters) { LogEvent(name, parameters.data(), parameters.size()); }
        void SetDefaultEventParameters(const Parameter *parameters, size_t number_of_parameters) {
          std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
          FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
          if (!parameters && number_of_parameters > 0) { return; }
          JNIEnv *env = g_app->GetJNIEnv();
          if (!parameters || number_of_parameters == 0) { env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetDefaultEventParameters), nullptr); }
          jobject bundle = env->NewObject(::firebase::jni::bundle::GetClass(), ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
          if (jni::CheckAndClearException(env) || !bundle) { return; }
          for (size_t i = 0; i < number_of_parameters; ++i) {
            const Parameter &parameter = parameters[i];
            if (!parameter.name) {}
            if (!AddVariantToBundle(env, bundle, parameter.name, parameter.value, true)) {}
          }
          env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetDefaultEventParameters), bundle);
        }
        void SetDefaultEventParameters(const std::vector<Parameter> &parameters) { SetDefaultEventParameters(parameters.data(), parameters.size()); }
#define NOOP_CONVERSION_MEASUREMENT(func_name, type)                                                                                                                                                   \
  void func_name(type arg) {                                                                                                                                                                           \
    (void)arg;                                                                                                                                                                                         \
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());                                                                                                                         \
    FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());                                                                                                                                            \
  }
        NOOP_CONVERSION_MEASUREMENT(InitiateOnDeviceConversionMeasurementWithEmailAddress, const char *)
        NOOP_CONVERSION_MEASUREMENT(InitiateOnDeviceConversionMeasurementWithPhoneNumber, const char *)
        NOOP_CONVERSION_MEASUREMENT(InitiateOnDeviceConversionMeasurementWithHashedEmailAddress, std::vector<unsigned char>)
        NOOP_CONVERSION_MEASUREMENT(InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber, std::vector<unsigned char>)
#define SIMPLE_JNI_METHOD(name, method_id)                                                                                                                                                             \
  void name() {                                                                                                                                                                                        \
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());                                                                                                                         \
    FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());                                                                                                                                            \
    JNIEnv *env = g_app->GetJNIEnv();                                                                                                                                                                  \
    env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::method_id));                                                                                                     \
    jni::CheckAndClearException(env);                                                                                                                                                                  \
  }
#define LONG_JNI_METHOD(name, method_id)                                                                                                                                                               \
  void name(int64_t val) {                                                                                                                                                                             \
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());                                                                                                                         \
    FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());                                                                                                                                            \
    JNIEnv *env = g_app->GetJNIEnv();                                                                                                                                                                  \
    env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::method_id), static_cast<jlong>(val));                                                                            \
    jni::CheckAndClearException(env);                                                                                                                                                                  \
  }
#define STRING_JNI_METHOD(name, method_id, err_msg)                                                                                                                                                    \
  void name(const char *val) {                                                                                                                                                                         \
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());                                                                                                                         \
    FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());                                                                                                                                            \
    jstring jval = jni::NewRawJString(env, val);                                                                                                                                                       \
    if (jni::CheckAndClearException(env) || !jval) {                                                                                                                                                   \
      if (jval) env->DeleteLocalRef(jval);                                                                                                                                                             \
      return;                                                                                                                                                                                          \
    }                                                                                                                                                                                                  \
    env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::method_id), jval);                                                                                               \
    jni::CheckAndClearException(env);                                                                                                                                                                  \
    env->DeleteLocalRef(jval);                                                                                                                                                                         \
  }
        void SetUserProperty(const char *name, const char *value) {
          std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
          FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
          if (!name || name[0] == '\0') {}
          JNIEnv *env = g_app->GetJNIEnv();
          jstring property_name = jni::NewRawJString(env, name);
          if (jni::CheckAndClearException(env) || !property_name) {
            if (property_name) env->DeleteLocalRef(property_name);
            return;
          }
          jstring property_value = value ? jni::NewRawJString(env, value) : nullptr;
          if (value && (jni::CheckAndClearException(env) || !property_value)) {
            if (property_value) env->DeleteLocalRef(property_value);
            env->DeleteLocalRef(property_name);
            return;
          }
          env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetUserProperty), property_name, property_value);
          if (jni::CheckAndClearException(env)) { env->DeleteLocalRef(property_name); }
          void SetUserId(const char *user_id) {
            std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
            FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
            JNIEnv *env = g_app->GetJNIEnv();
            jstring user_id_value = user_id ? jni::NewRawJString(env, user_id) : nullptr;
            if (user_id && (jni::CheckAndClearException(env) || !user_id_value)) {
              if (user_id_value) env->DeleteLocalRef(user_id_value);
              return;
            }
            env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetUserId), user_id_value);
          }
          void SetSessionTimeoutDuration(int64_t milliseconds) {
            std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
            FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
            JNIEnv *env = g_app->GetJNIEnv();
            env->CallVoidMethod(g_analytics_class_instance, analytics::GetMethodId(analytics::kSetSessionTimeoutDuration), static_cast<jlong>(milliseconds));
            jni::CheckAndClearException(env);
          }
          SIMPLE_JNI_METHOD(ResetAnalyticsData, kResetAnalyticsData)
          void SetLogCallback(const LogCallback &) {}
          void NotifyAppLifecycleChange(AppLifecycleState) {}
          bool IsDesktopInitialized() { return false; }
          void SetDesktopDebugMode(bool) {}
          template <typename T> struct JniTaskCallback {
            static void OnCompletion(JNIEnv *env, jobject result, firebase::util::FutureResult result_code, const char *status_message, void *callback_data) {
              std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
              auto future_data = internal::FutureData::GetShared();
              if (!future_data) {
                if (result) env->DeleteLocalRef(result);
                return;
              }
              auto handle = SafeFutureHandle<T>(FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)));
              if (result_code == firebase::util::kFutureResultSuccess) {
                T val = T();
                std::string err;
                int err_code = kErrorNone;
                if (Parse(env, result, status_message, val, err, err_code)) {
                  future_data->api()->CompleteWithResult(handle, err_code, "", val);
                } else {
                  future_data->api()->CompleteWithResult(handle, err_code, err.c_str(), val);
                }
              } else {
                std::string err = ::firebase::jni::ExtractExceptionMessage(env, static_cast<jthrowable>(result));
                if (err.empty()) err = (status_message && *status_message) ? status_message : "Unknown error";
                future_data->api()->CompleteWithResult(handle, kErrorCodeFailed, err.c_str(), T());
              }
              if (result) env->DeleteLocalRef(result);
            }
            static bool Parse(JNIEnv *env, jobject result, const char *status_message, T &out_val, std::string &out_err, int &out_code);
          };
          template <> bool JniTaskCallback<std::string>::Parse(JNIEnv * env, jobject result, const char *status_message, std::string &out_val, std::string &out_err, int &out_code) {
            if (!result) {
              out_err = "Unknown error occurred";
              out_code = kErrorCodeFailed;
              return false;
            }
            out_val = jni::JStringToString(env, result);
            return true;
          }
          template <> bool JniTaskCallback<int64_t>::Parse(JNIEnv * env, jobject result, const char *status_message, int64_t &out_val, std::string &out_err, int &out_code) {
            if (!result) {
              out_err = (status_message && *status_message) ? status_message : "AnalyticsStorage consent is set to Denied, or session is expired.";
              out_code = kErrorSessionExpiredOrCollectionDisabled;
              return false;
            }
            jmethodID mid = ::firebase::jni::long_class::GetMethodId(::firebase::jni::long_class::kValue);
            if (!mid) {
              out_err = "Failed to look up Long.longValue() method";
              out_code = kErrorCodeFailed;
              return false;
            }
            out_val = env->CallLongMethod(result, mid);
            out_err = jni::GetAndClearExceptionMessage(env);
            if (!out_err.empty()) {
              out_code = kErrorCodeFailed;
              out_val = 0;
              return false;
            }
            return true;
          }
          template <typename T> Future<T> ExecuteJniTask(internal::AnalyticsFn fn, analytics::Method method_id, const char *method_name) {
            std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
            FIREBASE_ASSERT_RETURN(Future<T>(), internal::IsInitialized());
            auto future_data = internal::FutureData::GetShared();
            if (!future_data || !g_app || !g_analytics_class_instance) { return Future<T>(); }
            JNIEnv *env = g_app->GetJNIEnv();
            auto *api = future_data->api();
            const auto safe_future_handle = api->SafeAlloc<T>(fn);
            jobject task = env->CallObjectMethod(g_analytics_class_instance, analytics::GetMethodId(method_id));
            std::string error = jni::GetAndClearExceptionMessage(env);
            if (error.empty() && task != nullptr) {
              ::firebase::util::RegisterCallbackOnTask(env, task, JniTaskCallback<T>::OnCompletion, reinterpret_cast<void *>(safe_future_handle.get().id()), internal::kAnalyticsModuleName);
            } else {
              if (error.empty()) error = std::string(method_name) + " returned null Task";
              else
            }
            if (task) env->DeleteLocalRef(task);
            return Future<T>(api, safe_future_handle.get());
          }
          Future<std::string> GetAnalyticsInstanceId() { return ExecuteJniTask<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId, analytics::kGetAppInstanceId, "getAppInstanceId()"); }
          Future<std::string> GetAnalyticsInstanceIdLastResult() { return internal::GetLastResult<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId); }
          Future<int64_t> GetSessionId() { return ExecuteJniTask<int64_t>(internal::kAnalyticsFnGetSessionId, analytics::kGetSessionId, "GetSessionId()"); }
          Future<int64_t> GetSessionIdLastResult() { return internal::GetLastResult<int64_t>(internal::kAnalyticsFnGetSessionId); }
        }
      }