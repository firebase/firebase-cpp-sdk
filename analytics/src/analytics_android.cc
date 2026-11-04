#include <assert.h>
#include <jni.h>
#include <cstdint>
#include <cstring>
#include <vector>
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
namespace firebase {
namespace analytics {
DEFINE_FIREBASE_VERSION_STRING(FirebaseAnalytics);
static jobject g_analytics_class_instance = nullptr;
static const ::firebase::App* g_app = nullptr;
static const int kErrorCodeFailed = -1;
static const int kErrorSessionExpiredOrCollectionDisabled = -2;
#define ANALYTICS_METHODS(X)                                                                                                            \
  X(SetEnabled, "setAnalyticsCollectionEnabled", "(Z)V")                                                                                \
  X(SetConsent, "setConsent", "(Ljava/util/Map;)V")                                                                                     \
  X(LogEvent, "logEvent", "(Ljava/lang/String;Landroid/os/Bundle;)V")                                                                   \
  X(SetUserProperty, "setUserProperty", "(Ljava/lang/String;Ljava/lang/String;)V")                                                      \
  X(SetUserId, "setUserId", "(Ljava/lang/String;)V")                                                                                    \
  X(SetSessionTimeoutDuration, "setSessionTimeoutDuration", "(J)V")                                                                     \
  X(ResetAnalyticsData, "resetAnalyticsData", "()V")                                                                                    \
  X(GetAppInstanceId, "getAppInstanceId", "()Lcom/google/android/gms/tasks/Task;")                                                      \
  X(GetSessionId, "getSessionId", "()Lcom/google/android/gms/tasks/Task;")                                                              \
  X(GetInstance, "getInstance", "(Landroid/content/Context;)Lcom/google/firebase/analytics/FirebaseAnalytics;", jni::kMethodTypeStatic) \
  X(SetDefaultEventParameters, "setDefaultEventParameters", "(Landroid/os/Bundle;)V")
#define ANALYTICS_CONSENT_TYPE_FIELDS(X)                                                                                           \
  X(AnalyticsStorage, "ANALYTICS_STORAGE", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic) \
  X(AdStorage, "AD_STORAGE", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic)               \
  X(AdUserData, "AD_USER_DATA", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic)            \
  X(AdPersonalization, "AD_PERSONALIZATION", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentType;", jni::kFieldTypeStatic)
#define ANALYTICS_CONSENT_STATUS_FIELDS(X)                                                                        \
  X(Granted, "GRANTED", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;", jni::kFieldTypeStatic) \
  X(Denied, "DENIED", "Lcom/google/firebase/analytics/FirebaseAnalytics$ConsentStatus;", jni::kFieldTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(analytics, ANALYTICS_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/analytics/FirebaseAnalytics")
JNI_METHOD_LOOKUP_DECLARATION(
    analytics_consent_type, METHOD_LOOKUP_NONE, ANALYTICS_CONSENT_TYPE_FIELDS,
    "com/google/firebase/analytics/FirebaseAnalytics$ConsentType")
JNI_METHOD_LOOKUP_DECLARATION(
    analytics_consent_status, METHOD_LOOKUP_NONE,
    ANALYTICS_CONSENT_STATUS_FIELDS,
    "com/google/firebase/analytics/FirebaseAnalytics$ConsentStatus")
static void ReleaseAllClasses(JNIEnv* env) {
  analytics_consent_status::ReleaseClass(env);
  analytics_consent_type::ReleaseClass(env);
  analytics::ReleaseClass(env);
  ::firebase::jni::ReleaseCollectionClasses(env);
  util::Terminate(env);
}
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
    if (!util::Initialize(env, app.activity())) return;
    if (!::firebase::jni::CacheCollectionMethodIds(env, app.activity())) {
      util::Terminate(env);
      return;
    }
    if (!analytics::CacheMethodIds(env, app.activity()) ||
        !analytics_consent_type::CacheFieldIds(env, app.activity()) ||
        !analytics_consent_status::CacheFieldIds(env, app.activity())) {
      ReleaseAllClasses(env);
      return;
    }
    jobject local_inst = env->CallStaticObjectMethod(
        analytics::GetClass(), analytics::GetMethodId(analytics::kGetInstance),
        app.activity());
    if (jni::CheckAndClearException(env) || !local_inst) {
      if (local_inst) env->DeleteLocalRef(local_inst);
      ReleaseAllClasses(env);
      return;
    }
    g_analytics_class_instance = env->NewGlobalRef(local_inst);
    env->DeleteLocalRef(local_inst);
    if (jni::CheckAndClearException(env) || !g_analytics_class_instance) {
      if (g_analytics_class_instance) {
        env->DeleteGlobalRef(g_analytics_class_instance);
        g_analytics_class_instance = nullptr;
      }
      ReleaseAllClasses(env);
      return;
    }
    internal::FutureData::Create();
    g_app = &app;
    LogInfo("%s API Initialized", internal::kAnalyticsModuleName);
  }
  internal::RegisterTerminateOnAppDestroy(&app);
}
namespace internal {
bool IsInitialized() {
  std::lock_guard<std::recursive_mutex> lock(FutureData::mutex());
  return g_app != nullptr && g_analytics_class_instance != nullptr;
}
}  // namespace internal
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
  ::firebase::util::CancelCallbacks(env, internal::kAnalyticsModuleName);
  internal::UnregisterTerminateOnAppDestroy();
  {
    std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
    if (analytics_instance) env->DeleteGlobalRef(analytics_instance);
    if (!g_app) ReleaseAllClasses(env);
  }
}
void SetAnalyticsCollectionEnabled(bool enabled) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetEnabled),
                      static_cast<jboolean>(enabled));
  jni::CheckAndClearException(env);
}
void SetConsent(const std::map<ConsentType, ConsentStatus>& consent_settings) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jobject consent_map = env->NewObject(
      ::firebase::jni::hash_map::GetClass(),
      ::firebase::jni::hash_map::GetMethodId(
          ::firebase::jni::hash_map::kConstructor));
  if (jni::CheckAndClearException(env) || !consent_map) {
    if (consent_map) env->DeleteLocalRef(consent_map);
    return;
  }
  jmethodID put_method =
      ::firebase::jni::hash_map::GetMethodId(::firebase::jni::hash_map::kPut);
  for (const auto& [type, status] : consent_settings) {
    analytics_consent_type::Field type_field;
    switch (type) {
      case kConsentTypeAdStorage:
        type_field = analytics_consent_type::kAdStorage;
        break;
      case kConsentTypeAnalyticsStorage:
        type_field = analytics_consent_type::kAnalyticsStorage;
        break;
      case kConsentTypeAdUserData:
        type_field = analytics_consent_type::kAdUserData;
        break;
      case kConsentTypeAdPersonalization:
        type_field = analytics_consent_type::kAdPersonalization;
        break;
      default:
        LogError("Unknown ConsentType value: %d", static_cast<int>(type));
        env->DeleteLocalRef(consent_map);
        return;
    }
    analytics_consent_status::Field status_field;
    switch (status) {
      case kConsentStatusGranted:
        status_field = analytics_consent_status::kGranted;
        break;
      case kConsentStatusDenied:
        status_field = analytics_consent_status::kDenied;
        break;
      default:
        LogError("Unknown ConsentStatus value: %d", static_cast<int>(status));
        env->DeleteLocalRef(consent_map);
        return;
    }
    jobject j_type = env->GetStaticObjectField(
        analytics_consent_type::GetClass(),
        analytics_consent_type::GetFieldId(type_field));
    if (::firebase::jni::CheckAndClearException(env) || !j_type) {
      if (j_type) env->DeleteLocalRef(j_type);
      env->DeleteLocalRef(consent_map);
      return;
    }
    jobject j_status = env->GetStaticObjectField(
        analytics_consent_status::GetClass(),
        analytics_consent_status::GetFieldId(status_field));
    if (::firebase::jni::CheckAndClearException(env) || !j_status) {
      if (j_status) env->DeleteLocalRef(j_status);
      env->DeleteLocalRef(j_type);
      env->DeleteLocalRef(consent_map);
      return;
    }
    LogInfo("SetConsent: %d -> %d", static_cast<int>(type),
            static_cast<int>(status));
    jobject prev = env->CallObjectMethod(consent_map, put_method, j_type, j_status);
    jni::CheckAndClearException(env);
    if (prev) env->DeleteLocalRef(prev);
    env->DeleteLocalRef(j_type);
    env->DeleteLocalRef(j_status);
  }
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetConsent),
                      consent_map);
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(consent_map);
}
template <typename BuildBundleFunction>
void LogEvent(JNIEnv* env, const char* name, BuildBundleFunction build_bundle) {
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  jobject bundle = env->NewObject(
      ::firebase::jni::bundle::GetClass(),
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
  if (jni::CheckAndClearException(env) || !bundle) {
    LogError("Failed to create Bundle for event '%s'", name);
    if (bundle) env->DeleteLocalRef(bundle);
    return;
  }
  build_bundle(bundle);
  jstring event_name_string = jni::NewRawJString(env, name);
  if (jni::CheckAndClearException(env) || !event_name_string) {
    LogError("Failed to create event name string for '%s'", name);
    if (event_name_string) env->DeleteLocalRef(event_name_string);
    env->DeleteLocalRef(bundle);
    return;
  }
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kLogEvent),
                      event_name_string, bundle);
  if (jni::CheckAndClearException(env)) {
    LogError("Failed to log event '%s'", name);
  }
  env->DeleteLocalRef(event_name_string);
  env->DeleteLocalRef(bundle);
}
void AddToBundle(JNIEnv* env, jobject bundle, const char* key,
                 const char* value) {
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
  env->CallVoidMethod(
      bundle,
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutString),
      key_string, value_string);
  jni::CheckAndClearException(env);
  if (value_string) env->DeleteLocalRef(value_string);
  env->DeleteLocalRef(key_string);
}
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, double value) {
  if (!bundle || !key) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (jni::CheckAndClearException(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(
      bundle,
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutDouble),
      key_string, static_cast<jdouble>(value));
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(key_string);
}
void AddToBundle(JNIEnv* env, jobject bundle, const char* key, int64_t value) {
  if (!bundle || !key) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (jni::CheckAndClearException(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(
      bundle,
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kPutLong),
      key_string, static_cast<jlong>(value));
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(key_string);
}
static void AddObjectToBundle(JNIEnv* env, jobject bundle, const char* key,
                              jobject obj, ::firebase::jni::bundle::Method m) {
  if (!bundle || !key || !obj) return;
  jstring key_string = jni::NewRawJString(env, key);
  if (jni::CheckAndClearException(env) || !key_string) {
    if (key_string) env->DeleteLocalRef(key_string);
    return;
  }
  env->CallVoidMethod(bundle, ::firebase::jni::bundle::GetMethodId(m),
                      key_string, obj);
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(key_string);
}
jobject MapToBundle(JNIEnv* env, const std::map<Variant, Variant>& map);
jobject VectorOfMapsToArrayList(JNIEnv* env,
                                const std::vector<Variant>& vector) {
  jobject arraylist = env->NewObject(
      ::firebase::jni::array_list::GetClass(),
      ::firebase::jni::array_list::GetMethodId(
          ::firebase::jni::array_list::kConstructor));
  if (jni::CheckAndClearException(env) || !arraylist) {
    if (arraylist) env->DeleteLocalRef(arraylist);
    return nullptr;
  }
  for (const Variant& element : vector) {
    if (element.is_map()) {
      if (jobject bundle = MapToBundle(env, element.map())) {
        env->CallBooleanMethod(arraylist,
                               ::firebase::jni::array_list::GetMethodId(
                                   ::firebase::jni::array_list::kAdd),
                               bundle);
        jni::CheckAndClearException(env);
        env->DeleteLocalRef(bundle);
      }
    } else {
      LogError("VectorOfMapsToArrayList: Unsupported type (%s) within vector.",
               Variant::TypeName(element.type()));
    }
  }
  return arraylist;
}
bool AddVariantToBundle(JNIEnv* env, jobject bundle, const char* key,
                        const Variant& value, bool allow_null_clear = false) {
  if (!bundle || !key) return false;
  if (value.is_int64()) {
    AddToBundle(env, bundle, key, value.int64_value());
  } else if (value.is_double()) {
    AddToBundle(env, bundle, key, value.double_value());
  } else if (value.is_string()) {
    AddToBundle(env, bundle, key, value.string_value());
  } else if (value.is_bool()) {
    AddToBundle(env, bundle, key, static_cast<int64_t>(value.bool_value() ? 1 : 0));
  } else if (value.is_null()) {
    if (allow_null_clear) {
      AddToBundle(env, bundle, key, static_cast<const char*>(nullptr));
    } else {
      AddToBundle(env, bundle, key, static_cast<int64_t>(0));
    }
  } else if (value.is_vector()) {
    if (jobject list = VectorOfMapsToArrayList(env, value.vector())) {
      AddObjectToBundle(env, bundle, key, list,
                        ::firebase::jni::bundle::kPutParcelableArrayList);
      env->DeleteLocalRef(list);
    }
  } else if (value.is_map()) {
    if (jobject inner = MapToBundle(env, value.map())) {
      AddObjectToBundle(env, bundle, key, inner,
                        ::firebase::jni::bundle::kPutBundle);
      env->DeleteLocalRef(inner);
    }
  } else {
    return false;
  }
  return true;
}
jobject MapToBundle(JNIEnv* env, const std::map<Variant, Variant>& map) {
  jobject bundle = env->NewObject(
      ::firebase::jni::bundle::GetClass(),
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
  if (jni::CheckAndClearException(env) || !bundle) {
    if (bundle) env->DeleteLocalRef(bundle);
    return nullptr;
  }
  for (const auto& [k, v] : map) {
    if (!k.is_string() || !k.string_value()) continue;
    if (!AddVariantToBundle(env, bundle, k.string_value(), v)) {
      LogError("MapToBundle: Unsupported type (%s) within map with key %s.",
               Variant::TypeName(v.type()), k.string_value());
    }
    jni::CheckAndClearException(env);
  }
  return bundle;
}
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
void LogEvent(const char* name, const char* parameter_name,
              const int parameter_value) {
  LogEvent(name, parameter_name, static_cast<int64_t>(parameter_value));
}
void LogEvent(const char* name) {
  LogEvent(name, nullptr, static_cast<size_t>(0));
}
Future<void> LogAppleTransaction(const char* transaction_id) {
  (void)transaction_id;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  auto* api = future_data->api();
  const auto handle =
      api->SafeAlloc<void>(internal::kAnalyticsFnLogAppleTransaction);
  api->Complete(handle, kErrorNone, "");
  return Future<void>(api, handle.get());
}
Future<void> LogAppleTransactionLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<void>();
  return static_cast<const Future<void>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnLogAppleTransaction));
}
void LogEvent(const char* name, const Parameter* parameters,
              size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("LogEvent: Event name cannot be null or empty.");
    return;
  }
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "LogEvent(%s): parameters cannot be null when "
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
        LogError(
            "LogEvent(%s): %s is not a valid parameter value type. "
            "Parameter was not added.",
            parameter.name, Variant::TypeName(parameter.value.type()));
      }
    }
  });
}
void LogEvent(const char* name, const std::vector<Parameter>& parameters) {
  LogEvent(name, parameters.data(), parameters.size());
}
void SetDefaultEventParameters(const Parameter* parameters,
                               size_t number_of_parameters) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!parameters && number_of_parameters > 0) {
    LogError(
        "SetDefaultEventParameters: parameters cannot be null when "
        "number_of_parameters > 0.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
  if (!parameters || number_of_parameters == 0) {
    env->CallVoidMethod(
        g_analytics_class_instance,
        analytics::GetMethodId(analytics::kSetDefaultEventParameters), nullptr);
    if (jni::CheckAndClearException(env)) {
      LogError("Failed to set default event parameters");
    }
    return;
  }
  jobject bundle = env->NewObject(
      ::firebase::jni::bundle::GetClass(),
      ::firebase::jni::bundle::GetMethodId(::firebase::jni::bundle::kConstructor));
  if (jni::CheckAndClearException(env) || !bundle) {
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
                            true)) {
      LogError(
          "SetDefaultEventParameters(%s): %s is not a valid parameter value"
          " type. Excluded from default parameters.",
          parameter.name, Variant::TypeName(parameter.value.type()));
    }
  }
  env->CallVoidMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kSetDefaultEventParameters), bundle);
  if (jni::CheckAndClearException(env)) {
    LogError("Failed to set default event parameters");
  }
  env->DeleteLocalRef(bundle);
}
void SetDefaultEventParameters(const std::vector<Parameter>& parameters) {
  SetDefaultEventParameters(parameters.data(), parameters.size());
}
void InitiateOnDeviceConversionMeasurementWithEmailAddress(
    const char* email_address) {
  (void)email_address;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}
void InitiateOnDeviceConversionMeasurementWithPhoneNumber(
    const char* phone_number) {
  (void)phone_number;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}
void InitiateOnDeviceConversionMeasurementWithHashedEmailAddress(
    std::vector<unsigned char> hashed_email) {
  (void)hashed_email;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}
void InitiateOnDeviceConversionMeasurementWithHashedPhoneNumber(
    std::vector<unsigned char> hashed_phone) {
  (void)hashed_phone;
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
}
void SetUserProperty(const char* name, const char* value) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  if (!name || name[0] == '\0') {
    LogError("SetUserProperty: Property name cannot be null or empty.");
    return;
  }
  JNIEnv* env = g_app->GetJNIEnv();
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
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetUserProperty),
                      property_name, property_value);
  if (jni::CheckAndClearException(env)) {
    LogError("Unable to set user property name='%s', value='%s'", name,
             value ? value : "(null)");
  }
  if (property_value) env->DeleteLocalRef(property_value);
  env->DeleteLocalRef(property_name);
}
void SetUserId(const char* user_id) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jstring user_id_value = user_id ? jni::NewRawJString(env, user_id) : nullptr;
  if (user_id && (jni::CheckAndClearException(env) || !user_id_value)) {
    if (user_id_value) env->DeleteLocalRef(user_id_value);
    return;
  }
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kSetUserId),
                      user_id_value);
  if (jni::CheckAndClearException(env)) {
    LogError("Unable to set user ID '%s'", user_id ? user_id : "(null)");
  }
  if (user_id_value) env->DeleteLocalRef(user_id_value);
}
void SetSessionTimeoutDuration(int64_t milliseconds) {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kSetSessionTimeoutDuration),
      static_cast<jlong>(milliseconds));
  jni::CheckAndClearException(env);
}
void ResetAnalyticsData() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_analytics_class_instance,
                      analytics::GetMethodId(analytics::kResetAnalyticsData));
  jni::CheckAndClearException(env);
}
void SetLogCallback(const LogCallback&) {}
void NotifyAppLifecycleChange(AppLifecycleState) {}
bool IsDesktopInitialized() { return false; }
void SetDesktopDebugMode(bool) {}
Future<std::string> GetAnalyticsInstanceId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data || !g_app || !g_analytics_class_instance) {
    return Future<std::string>();
  }
  JNIEnv* env = g_app->GetJNIEnv();
  auto* api = future_data->api();
  const auto safe_handle =
      api->SafeAlloc<std::string>(internal::kAnalyticsFnGetAnalyticsInstanceId);
  jobject task = env->CallObjectMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kGetAppInstanceId));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    ::firebase::util::RegisterCallbackOnTask(
        env, task,
        [](JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
           const char* status_message, void* callback_data) {
          std::lock_guard<std::recursive_mutex> lock(
              internal::FutureData::mutex());
          if (auto future_data = internal::FutureData::GetShared()) {
            auto handle = SafeFutureHandle<std::string>(
                FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)));
            if (result_code == firebase::util::kFutureResultSuccess && result) {
              future_data->api()->CompleteWithResult(
                  handle, kErrorNone, "", jni::JStringToString(env, result));
            } else {
              std::string err = ::firebase::jni::ExtractExceptionMessage(
                  env, static_cast<jthrowable>(result));
              if (err.empty()) {
                err = (status_message && *status_message)
                          ? status_message
                          : "Unknown error occurred";
              }
              future_data->api()->CompleteWithResult(
                  handle, kErrorCodeFailed, err.c_str(), std::string(""));
            }
          }
          if (result) env->DeleteLocalRef(result);
        },
        reinterpret_cast<void*>(safe_handle.get().id()),
        internal::kAnalyticsModuleName);
    env->DeleteLocalRef(task);
  } else {
    if (error.empty()) error = "getAppInstanceId() returned null Task";
    api->CompleteWithResult(safe_handle, kErrorCodeFailed, error.c_str(),
                            std::string(""));
    if (task) env->DeleteLocalRef(task);
  }
  return Future<std::string>(api, safe_handle.get());
}
Future<std::string> GetAnalyticsInstanceIdLastResult() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<std::string>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<std::string>();
  return static_cast<const Future<std::string>&>(
      future_data->api()->LastResult(
          internal::kAnalyticsFnGetAnalyticsInstanceId));
}
Future<int64_t> GetSessionId() {
  std::lock_guard<std::recursive_mutex> lock(internal::FutureData::mutex());
  FIREBASE_ASSERT_RETURN(Future<int64_t>(), internal::IsInitialized());
  auto future_data = internal::FutureData::GetShared();
  if (!future_data || !g_app || !g_analytics_class_instance) {
    return Future<int64_t>();
  }
  auto* api = future_data->api();
  const auto future_handle =
      api->SafeAlloc<int64_t>(internal::kAnalyticsFnGetSessionId);
  JNIEnv* env = g_app->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      g_analytics_class_instance,
      analytics::GetMethodId(analytics::kGetSessionId));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && task != nullptr) {
    ::firebase::util::RegisterCallbackOnTask(
        env, task,
        [](JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
           const char* status_message, void* callback_data) {
          std::lock_guard<std::recursive_mutex> lock(
              internal::FutureData::mutex());
          if (auto future_data = internal::FutureData::GetShared()) {
            auto handle = SafeFutureHandle<int64_t>(
                FutureHandle(reinterpret_cast<FutureHandleId>(callback_data)));
            if (result_code == firebase::util::kFutureResultSuccess) {
              if (result != nullptr) {
                jmethodID long_val = ::firebase::jni::long_class::GetMethodId(
                    ::firebase::jni::long_class::kValue);
                if (!long_val) {
                  future_data->api()->CompleteWithResult(
                      handle, kErrorCodeFailed,
                      "Failed to look up Long.longValue() method", int64_t(0));
                } else {
                  int64_t session_id = env->CallLongMethod(result, long_val);
                  std::string unbox_err = jni::GetAndClearExceptionMessage(env);
                  if (!unbox_err.empty()) {
                    future_data->api()->CompleteWithResult(
                        handle, kErrorCodeFailed, unbox_err.c_str(), int64_t(0));
                  } else {
                    future_data->api()->CompleteWithResult(handle, kErrorNone,
                                                           "", session_id);
                  }
                }
              } else {
                future_data->api()->CompleteWithResult(
                    handle, kErrorSessionExpiredOrCollectionDisabled,
                    (status_message && *status_message)
                        ? status_message
                        : "AnalyticsStorage consent is set to Denied, or "
                          "session is expired.",
                    int64_t(0));
              }
            } else {
              std::string err = ::firebase::jni::ExtractExceptionMessage(
                  env, static_cast<jthrowable>(result));
              if (err.empty()) {
                err = (status_message && *status_message)
                          ? status_message
                          : "Unknown error occurred";
              }
              future_data->api()->CompleteWithResult(handle, kErrorCodeFailed,
                                                     err.c_str(), int64_t(0));
              LogError("getSessionId() returned an error: %s", err.c_str());
            }
          }
          if (result) env->DeleteLocalRef(result);
        },
        reinterpret_cast<void*>(future_handle.get().id()),
        internal::kAnalyticsModuleName);
  } else {
    if (error.empty()) error = "getSessionId() returned null Task";
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
  auto future_data = internal::FutureData::GetShared();
  if (!future_data) return Future<int64_t>();
  return static_cast<const Future<int64_t>&>(
      future_data->api()->LastResult(internal::kAnalyticsFnGetSessionId));
}
}  // namespace analytics
}  // namespace firebase
