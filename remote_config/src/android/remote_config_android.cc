#include "app/src/jni/declaration.h"
// Copyright 2016 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
#include <cstdint>
#include <cstring>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "app/src/app_android.h"
#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jni/collection.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/time.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "remote_config/remote_config_resources.h"
#include "remote_config/src/android/remote_config_android.h"
#include "remote_config/src/common.h"
#include "remote_config/src/config_update_listener_registration_internal.h"
#include "remote_config/src/value_parser.h"
namespace firebase::remote_config::internal {
DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);
#define REMOTE_CONFIG_METHODS(X)                                           \
  X(GetInstance, "getInstance",                                            \
    "(Lcom/google/firebase/FirebaseApp;)"                                  \
    "Lcom/google/firebase/remoteconfig/FirebaseRemoteConfig;",             \
    firebase::jni::kMethodTypeStatic) \
  X(EnsureInitialized, "ensureInitialized",                                \
    "()Lcom/google/android/gms/tasks/Task;") \
  X(Activate, "activate",                                                  \
    "()Lcom/google/android/gms/tasks/Task;") \
  X(FetchAndActivate, "fetchAndActivate",                                  \
    "()Lcom/google/android/gms/tasks/Task;") \
  X(SetDefaultsAsync, "setDefaultsAsync",                                  \
    "(I)Lcom/google/android/gms/tasks/Task;") \
  X(SetDefaultsUsingMapAsync, "setDefaultsAsync",                          \
    "(Ljava/util/Map;)"                                                    \
    "Lcom/google/android/gms/tasks/Task;") \
  X(SetConfigSettingsAsync, "setConfigSettingsAsync",                      \
    "(Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;)"    \
    "Lcom/google/android/gms/tasks/Task;") \
  X(SetCustomSignalsAsync, "setCustomSignals",                             \
    "(Lcom/google/firebase/remoteconfig/CustomSignals;)"                   \
    "Lcom/google/android/gms/tasks/Task;") \
  X(GetValue, "getValue",                                                  \
    "(Ljava/lang/String;)"                                                 \
    "Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigValue;") \
  X(GetAll, "getAll", "()Ljava/util/Map;") \
  X(GetKeysByPrefix, "getKeysByPrefix",                                    \
    "(Ljava/lang/String;)Ljava/util/Set;") \
  X(GetInfo, "getInfo",                                                    \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigInfo;") \
  X(Fetch, "fetch", "(J)Lcom/google/android/gms/tasks/Task;") \
  X(AddOnConfigUpdateListener, "addOnConfigUpdateListener",                \
    "(Lcom/google/firebase/remoteconfig/ConfigUpdateListener;)"            \
    "Lcom/google/firebase/remoteconfig/ConfigUpdateListenerRegistration;")
JNI_METHOD_LOOKUP_DECLARATION(config, REMOTE_CONFIG_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfig")
#define REMOTE_CONFIG_VALUE_METHODS(X)                              \
  X(AsLong, "asLong", "()J") \
  X(AsDouble, "asDouble", "()D") \
  X(AsString, "asString", "()Ljava/lang/String;") \
  X(AsByteArray, "asByteArray", "()[B") \
  X(AsBoolean,  "asBoolean", "()Z") \
  X(GetSource, "getSource", "()I")
JNI_METHOD_LOOKUP_DECLARATION(config_value, REMOTE_CONFIG_VALUE_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfigValue")
#define REMOTE_CONFIG_INFO_METHODS(X) \
  X(GetFetchTimeMillis, "getFetchTimeMillis", "()J") \
  X(GetLastFetchStatus, "getLastFetchStatus", "()I") \
  X(GetConfigSettings, "getConfigSettings", \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;")
JNI_METHOD_LOOKUP_DECLARATION(config_info, REMOTE_CONFIG_INFO_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfigInfo")
#define REMOTE_CONFIG_SETTINGS_METHODS(X) \
  X(GetFetchTimeoutInSeconds, "getFetchTimeoutInSeconds", "()J") \
  X(GetMinimumFetchIntervalInSeconds, "getMinimumFetchIntervalInSeconds", "()J")
JNI_METHOD_LOOKUP_DECLARATION(config_settings, REMOTE_CONFIG_SETTINGS_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfigSettings")
#define REMOTE_CONFIG_SETTINGS_BUILDER_METHODS(X)                         \
  X(Constructor, "<init>", "()V") \
  X(Build, "build",                                                       \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;") \
  X(SetFetchTimeoutInSeconds, "setFetchTimeoutInSeconds",                 \
    "(J)Lcom/google/firebase/remoteconfig/"                               \
    "FirebaseRemoteConfigSettings$Builder;") \
  X(SetMinimumFetchIntervalInSeconds, "setMinimumFetchIntervalInSeconds", \
    "(J)Lcom/google/firebase/remoteconfig/"                               \
    "FirebaseRemoteConfigSettings$Builder;")
JNI_METHOD_LOOKUP_DECLARATION(config_settings_builder, REMOTE_CONFIG_SETTINGS_BUILDER_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfigSettings$Builder")
#define CUSTOM_SIGNALS_BUILDER_METHODS(X)                                 \
  X(Constructor, "<init>", "()V") \
  X(PutString, "put",                                                     \
    "(Ljava/lang/String;Ljava/lang/String;)"                              \
    "Lcom/google/firebase/remoteconfig/CustomSignals$Builder;") \
  X(PutLong, "put",                                                       \
    "(Ljava/lang/String;J)"                                               \
    "Lcom/google/firebase/remoteconfig/CustomSignals$Builder;") \
  X(PutDouble, "put",                                                     \
    "(Ljava/lang/String;D)"                                               \
    "Lcom/google/firebase/remoteconfig/CustomSignals$Builder;") \
  X(Build, "build",                                                       \
    "()Lcom/google/firebase/remoteconfig/CustomSignals;")
JNI_METHOD_LOOKUP_DECLARATION(custom_signals_builder, CUSTOM_SIGNALS_BUILDER_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/CustomSignals$Builder")
#define REMOTE_CONFIG_THROTTLED_EXCEPTION_METHODS(X)     \
  X(GetThrottleEndTimeMillis, "getThrottleEndTimeMillis", "()J")
JNI_METHOD_LOOKUP_DECLARATION(throttled_exception, REMOTE_CONFIG_THROTTLED_EXCEPTION_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/FirebaseRemoteConfigFetchThrottledException")
#define JNI_CONFIG_UPDATE_LISTENER_METHODS(X)                              \
  X(Constructor, "<init>", "(J)V")
JNI_METHOD_LOOKUP_DECLARATION(jni_config_update_listener, JNI_CONFIG_UPDATE_LISTENER_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/internal/cpp/JniConfigUpdateListener")
JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnUpdate(JNIEnv* env, jobject clazz, jlong c_listener_ptr,
                                                              jobject j_config_update);
JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnError(JNIEnv* env, jobject clazz, jlong c_listener_ptr,
                                                             jint j_error_code);
static const JNINativeMethod kNativeJniConfigUpdateListenerMethods[] = {
    {const_cast<char*>("nativeOnUpdate"), const_cast<char*>("(JLcom/google/firebase/remoteconfig/ConfigUpdate;)V"),
     reinterpret_cast<void*>(JniConfigUpdateListener_nativeOnUpdate)},
    {const_cast<char*>("nativeOnError"), const_cast<char*>("(JI)V"),
     reinterpret_cast<void*>(JniConfigUpdateListener_nativeOnError)},
};
#define CONFIG_UPDATE_METHODS(X)                              \
  X(GetUpdatedKeys, "getUpdatedKeys", "()Ljava/util/Set;")
JNI_METHOD_LOOKUP_DECLARATION(config_update, CONFIG_UPDATE_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/remoteconfig/ConfigUpdate")
#define CONFIG_UPDATE_LISTENER_REGISTRATION_METHODS(X)                              \
  X(Remove, "remove", "()V")
JNI_METHOD_LOOKUP_DECLARATION(config_update_listener_registration, CONFIG_UPDATE_LISTENER_REGISTRATION_METHODS,
                              FIELD_LOOKUP_NONE, "com/google/firebase/remoteconfig/ConfigUpdateListenerRegistration")
#define RC_LAST_RESULT(RetType, MethodName)                                                           \
  Future<RetType> RemoteConfigInternal::MethodName##LastResult() {                                    \
    return static_cast<const Future<RetType>&>(future_impl_.LastResult(kRemoteConfigFn##MethodName)); \
  }
template <typename T>
Future<T> RunAsyncOp(RemoteConfigInternal* rc, ReferenceCountedFutureImpl* fimpl, int fn_idx, JNIEnv* env, jobject task,
                     firebase::util::FutureCallback cb, std::vector<std::string> keys = {}) {
  const auto handle = fimpl->template SafeAlloc<T>(fn_idx);
  if (jni::CheckAndClearException(env) || !task) {
    fimpl->Complete(handle, kFutureStatusFailure, "Native function fails");
  } else {
    auto data = new RCDataHandle<T>(fimpl, handle, rc, std::move(keys));
    firebase::util::RegisterCallbackOnTask(env, task, cb, reinterpret_cast<void*>(data), rc->jni_task_id().c_str());
    env->DeleteLocalRef(task);
  }
  return MakeFuture<T>(fimpl, handle);
}
template <typename T>
static void UpdateThrottleEndTimeFromTaskResult(JNIEnv* env, jobject result, bool success, RemoteConfigInternal* rc) {
  if (!rc) return;
  if (success) {
    rc->set_throttled_end_time(0);
    return;
  }
  if (result && env->IsInstanceOf(result, throttled_exception::GetClass())) {
    int64_t throttle_end_time = static_cast<int64_t>(
        env->CallLongMethod(result, throttled_exception::GetMethodId(throttled_exception::kGetThrottleEndTimeMillis)));
    if (!jni::CheckAndClearException(env) && throttle_end_time > 0) { rc->set_throttled_end_time(throttle_end_time); }
  }
}
void FetchCallback(JNIEnv* env, jobject result, firebase::util::FutureResult result_code, const char* status_message,
                   void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  auto* data_handle = reinterpret_cast<RCDataHandle<void>*>(callback_data);
  UpdateThrottleEndTimeFromTaskResult(env, result, success, data_handle->rc_internal);
  CompleteVoidCallback(env, result, result_code, status_message, callback_data);
}
void SetDefaultsCallback(JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  auto* data_handle = reinterpret_cast<RCDataHandle<void>*>(callback_data);
  if (result_code == util::kFutureResultSuccess) {
    data_handle->rc_internal->SaveTmpKeysToDefault(std::move(data_handle->default_keys));
  }
  CompleteVoidCallback(env, result, result_code, status_message, callback_data);
}
Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  JNIEnv* env = app_.GetJNIEnv();
  return RunAsyncOp<ConfigInfo>(this, &future_impl_, kRemoteConfigFnEnsureInitialized, env,
                                env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kEnsureInitialized)),
                                ConfigInfoCallback);
}
RC_LAST_RESULT(ConfigInfo, EnsureInitialized)
Future<bool> RemoteConfigInternal::Activate() {
  JNIEnv* env = app_.GetJNIEnv();
  return RunAsyncOp<bool>(this, &future_impl_, kRemoteConfigFnActivate, env,
                          env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kActivate)), BoolCallback);
}
RC_LAST_RESULT(bool, Activate)
Future<bool> RemoteConfigInternal::FetchAndActivate() {
  JNIEnv* env = app_.GetJNIEnv();
  return RunAsyncOp<bool>(this, &future_impl_, kRemoteConfigFnFetchAndActivate, env,
                          env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kFetchAndActivate)),
                          BoolCallback);
}
RC_LAST_RESULT(bool, FetchAndActivate)
Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  JNIEnv* env = app_.GetJNIEnv();
  return RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnFetch, env,
                          env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kFetch),
                                                static_cast<jlong>(cache_expiration_in_seconds)),
                          DefaultCallback<void>);
}
RC_LAST_RESULT(void, Fetch)
Future<void> RemoteConfigInternal::SetDefaults(int defaults_resource_id) {
  JNIEnv* env = app_.GetJNIEnv();
  return RunAsyncOp<void>(
      this, &future_impl_, kRemoteConfigFnSetDefaults, env,
      env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kSetDefaultsAsync), defaults_resource_id),
      DefaultCallback<void>);
}
Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValueVariant* defaults, size_t number_of_defaults) {
  JNIEnv* env = app_.GetJNIEnv();
  std::vector<std::string> keys;
  jobject m = ConfigKeyValueVariantArrayToHashMap(env, defaults, number_of_defaults, &keys);
  if (!m && number_of_defaults > 0)
    return RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetDefaults, env, nullptr, DefaultCallback<void>);
  auto f =
      RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetDefaults, env,
                       env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kSetDefaultsUsingMapAsync), m),
                       DefaultCallback<void>, keys);
  if (m) env->DeleteLocalRef(m);
  return f;
}
Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults) {
  JNIEnv* env = app_.GetJNIEnv();
  std::vector<std::string> keys;
  jobject m = ConfigKeyValueArrayToHashMap(env, defaults, number_of_defaults, &keys);
  if (!m && number_of_defaults > 0)
    return RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetDefaults, env, nullptr, DefaultCallback<void>);
  auto f =
      RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetDefaults, env,
                       env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kSetDefaultsUsingMapAsync), m),
                       DefaultCallback<void>, keys);
  if (m) env->DeleteLocalRef(m);
  return f;
}
RC_LAST_RESULT(void, SetDefaults)
Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  JNIEnv* env = app_.GetJNIEnv();
  jobject obj = CreateFirebaseRemoteConfigSettings(env, settings);
  if (!obj) return RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetConfigSettings, env, nullptr, nullptr);
  auto f =
      RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetConfigSettings, env,
                       env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kSetConfigSettingsAsync), obj),
                       DefaultCallback<void>);
  env->DeleteLocalRef(obj);
  return f;
}
RC_LAST_RESULT(void, SetConfigSettings)
Future<void> RemoteConfigInternal::SetCustomSignals(const std::map<std::string, Variant>& custom_signals) {
  JNIEnv* env = app_.GetJNIEnv();
  jobject obj = CustomSignalsFromMap(env, custom_signals);
  if (!obj && !custom_signals.empty())
    return RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetCustomSignals, env, nullptr, nullptr);
  auto f =
      RunAsyncOp<void>(this, &future_impl_, kRemoteConfigFnSetCustomSignals, env,
                       env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kSetCustomSignalsAsync), obj),
                       DefaultCallback<void>);
  if (obj) env->DeleteLocalRef(obj);
  return f;
}
RC_LAST_RESULT(void, SetCustomSignals)
ConfigSettings RemoteConfigInternal::GetConfigSettings() {
  ConfigSettings settings;
  JNIEnv* env = app_.GetJNIEnv();
  jobject jinfo = env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kGetInfo));
  if (jni::CheckAndClearException(env) || !jinfo) { return settings; }
  jobject config_settings_obj = env->CallObjectMethod(jinfo, config_info::GetMethodId(config_info::kGetConfigSettings));
  env->DeleteLocalRef(jinfo);
  if (jni::CheckAndClearException(env) || !config_settings_obj) { return settings; }
  int64_t fetch_timeout_sec = env->CallLongMethod(
      config_settings_obj, config_settings::GetMethodId(config_settings::kGetFetchTimeoutInSeconds));
  if (!jni::CheckAndClearException(env) && fetch_timeout_sec >= 0) {
    settings.fetch_timeout_in_milliseconds =
        static_cast<uint64_t>(fetch_timeout_sec) * ::firebase::internal::kMillisecondsPerSecond;
  }
  int64_t min_interval_sec = env->CallLongMethod(
      config_settings_obj, config_settings::GetMethodId(config_settings::kGetMinimumFetchIntervalInSeconds));
  if (!jni::CheckAndClearException(env) && min_interval_sec >= 0) {
    settings.minimum_fetch_interval_in_milliseconds =
        static_cast<uint64_t>(min_interval_sec) * ::firebase::internal::kMillisecondsPerSecond;
  }
  env->DeleteLocalRef(config_settings_obj);
  return settings;
}
#define RC_GET_TYPE_FROM_VALUE(app, obj, key, info, c_type, c_type_name, java_type)                          \
  {                                                                                                          \
    JNIEnv* env = app.GetJNIEnv();                                                                           \
    jobject value_object = GetValue(env, obj, key, info);                                                    \
    if (!value_object) return static_cast<c_type>(0);                                                        \
    c_type value =                                                                                           \
        env->Call##java_type##Method(value_object, config_value::GetMethodId(config_value::kAs##java_type)); \
    bool failed = CheckKeyRetrievalLogError(env, key, c_type_name);                                          \
    env->DeleteLocalRef(value_object);                                                                       \
    if (info) info->conversion_successful = !failed;                                                         \
    return failed ? static_cast<c_type>(0) : value;                                                          \
  }
#define RC_GET(RetType, MethName, JniMeth, ErrName, DefaultVal)                                              \
  RetType RemoteConfigInternal::MethName(const char* key, ValueInfo* info) {                                 \
    JNIEnv* env = app_.GetJNIEnv();                                                                          \
    jobject val = GetValue(env, internal_obj_, key, info);                                                   \
    if (!val) return DefaultVal;                                                                             \
    RetType res = jni::Call##MethName##Method(env, val, config_value::GetMethodId(config_value::k##JniMeth), \
                                              CheckKeyRetrievalLogError, key, ErrName);                      \
    env->DeleteLocalRef(val);                                                                                \
    return res;                                                                                              \
  }
RC_GET(bool, GetBoolean, AsBoolean, "boolean", false)
RC_GET(int64_t, GetLong, AsLong, "long", 0)
RC_GET(double, GetDouble, AsDouble, "double", 0.0)
std::string RemoteConfigInternal::GetString(const char* key, ValueInfo* info) {
  std::string value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject value_object = GetValue(env, internal_obj_, key, info);
  if (value_object) {
    jobject value_string = env->CallObjectMethod(value_object, config_value::GetMethodId(config_value::kAsString));
    bool failed = CheckKeyRetrievalLogError(env, key, "string");
    env->DeleteLocalRef(value_object);
    if (!failed && value_string != nullptr) {
      value = ::firebase::jni::JStringToString(env, value_string);
    } else if (value_string != nullptr) {
      env->DeleteLocalRef(value_string);
    }
    if (info) info->conversion_successful = (!failed && value_string != nullptr);
  }
  return value;
}
std::vector<unsigned char> RemoteConfigInternal::GetData(const char* key, ValueInfo* info) {
  std::vector<unsigned char> value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject value_object = GetValue(env, internal_obj_, key, info);
  if (value_object) {
    jobject value_array = env->CallObjectMethod(value_object, config_value::GetMethodId(config_value::kAsByteArray));
    bool failed = CheckKeyRetrievalLogError(env, key, "vector");
    env->DeleteLocalRef(value_object);
    if (!failed && value_array != nullptr) {
      value = util::JniByteArrayToVector(env, value_array);
    } else if (value_array != nullptr) {
      env->DeleteLocalRef(value_array);
    }
    if (info) info->conversion_successful = (!failed && value_array != nullptr);
  }
  return value;
}
std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(const char* prefix) {
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  JNIEnv* env = app_.GetJNIEnv();
  jstring prefix_string = nullptr;
  if (prefix) {
    prefix_string = env->NewStringUTF(prefix);
    if (jni::CheckAndClearException(env) || !prefix_string) { return keys; }
  }
  jobject key_set_java =
      env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kGetKeysByPrefix), prefix_string);
  if (jni::CheckAndClearException(env)) key_set_java = nullptr;
  if (key_set_java) {
    util::JavaSetToStdStringVector(env, &keys, key_set_java);
    env->DeleteLocalRef(key_set_java);
    for (auto i = keys.begin(); i != keys.end(); ++i) { key_set.insert(*i); }
  }
  if (prefix_string) env->DeleteLocalRef(prefix_string);
  MutexLock lock(default_key_mutex_);
  size_t prefix_length = prefix ? std::strlen(prefix) : 0;
  for (auto i = default_keys_.begin(); i != default_keys_.end(); ++i) {
    if (key_set.find(*i) != key_set.end()) {
      continue;
    }
    if (prefix_length == 0 || std::strncmp(prefix, i->c_str(), prefix_length) == 0) {
      keys.push_back(*i);
      key_set.insert(*i);  // In case the defaults vector has duplicate keys.
    }
  }
  return keys;
}
std::vector<std::string> RemoteConfigInternal::GetKeys() { return GetKeysByPrefix(nullptr); }
static Variant RemoteConfigValueToVariant(JNIEnv* env, jobject from) {
  if (!from) return Variant::Null();
  if (!env->IsInstanceOf(from, config_value::GetClass())) return Variant::Null();
  jobject value_string = env->CallObjectMethod(from, config_value::GetMethodId(config_value::kAsString));
  if (!CheckKeyRetrievalLogError(env, "", "string") && value_string != nullptr) {
    std::string str = ::firebase::jni::JStringToString(env, value_string);
    return ValueParser::CoerceToVariant(str);
  }
  if (value_string != nullptr) { env->DeleteLocalRef(value_string); }
  jobject value_array = env->CallObjectMethod(from, config_value::GetMethodId(config_value::kAsByteArray));
  if (!CheckKeyRetrievalLogError(env, "", "vector") && value_array != nullptr) {
    std::vector<unsigned char> blob = util::JniByteArrayToVector(env, value_array);
    return Variant::FromMutableBlob(blob.data(), blob.size());
  }
  if (value_array != nullptr) { env->DeleteLocalRef(value_array); }
  LogError(
      "Remote Config: Unable to convert a FirebaseRemoteConfigValue to "
      "Variant.");
  return Variant::Null();
}
static void JavaMapToStringVariantMap(JNIEnv* env, std::map<std::string, Variant>* to, jobject from) {
  if (!from || !to) return;
  jobject key_set = env->CallObjectMethod(from, jni::map::GetMethodId(jni::map::kKeySet));
  if (jni::CheckAndClearException(env) || !key_set) { return; }
  jobject iter = env->CallObjectMethod(key_set, jni::set::GetMethodId(jni::set::kIterator));
  if (jni::CheckAndClearException(env) || !iter) {
    env->DeleteLocalRef(key_set);
    return;
  }
  while (env->CallBooleanMethod(iter, jni::iterator::GetMethodId(jni::iterator::kHasNext))) {
    if (jni::CheckAndClearException(env)) { break; }
    jobject key_object = env->CallObjectMethod(iter, jni::iterator::GetMethodId(jni::iterator::kNext));
    if (jni::CheckAndClearException(env)) { key_object = nullptr; }
    jobject value_object = nullptr;
    if (key_object) {
      value_object = env->CallObjectMethod(from, jni::map::GetMethodId(jni::map::kGet), key_object);
      if (jni::CheckAndClearException(env)) {
        value_object = nullptr;
      } else {
        std::string key = jni::JStringToString(env, key_object);
        Variant v_value = RemoteConfigValueToVariant(env, value_object);
        to->insert(std::make_pair(std::move(key), std::move(v_value)));
      }
    }
    if (key_object) env->DeleteLocalRef(key_object);
    if (value_object) env->DeleteLocalRef(value_object);
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}
std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject key_value_map = env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kGetAll));
  if (jni::CheckAndClearException(env)) key_value_map = nullptr;
  if (key_value_map) {
    JavaMapToStringVariantMap(env, &value, key_value_map);
    env->DeleteLocalRef(key_value_map);
  }
  return value;
}
ConfigInfo RemoteConfigInternal::GetInfo() const {
  JNIEnv* env = app_.GetJNIEnv();
  ConfigInfo info{};
  int64_t throttled_end = throttled_end_time();
  info.throttled_end_time = throttled_end > 0 ? static_cast<uint64_t>(throttled_end) : 0;
  jobject jinfo = env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kGetInfo));
  if (jni::CheckAndClearException(env) || !jinfo) { return info; }
  JConfigInfoToConfigInfo(env, jinfo, &info);
  env->DeleteLocalRef(jinfo);
  return info;
}
ConfigUpdateListenerRegistration RemoteConfigInternal::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)> config_update_listener) {
  JNIEnv* env = app_.GetJNIEnv();
  ConfigUpdateListenerWrapper* listener_wrapper = new ConfigUpdateListenerWrapper(std::move(config_update_listener));
  jlong listener_id = RegisterListenerWrapper(listener_wrapper);
  jobject j_listener =
      env->NewObject(jni_config_update_listener::GetClass(),
                     jni_config_update_listener::GetMethodId(jni_config_update_listener::kConstructor), listener_id);
  if (jni::CheckAndClearException(env) || !j_listener) {
    UnregisterAndDeleteListenerWrapper(listener_id);
    return ConfigUpdateListenerRegistration();
  }
  jobject j_local_registration =
      env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kAddOnConfigUpdateListener), j_listener);
  env->DeleteLocalRef(j_listener);
  if (jni::CheckAndClearException(env) || !j_local_registration) {
    UnregisterAndDeleteListenerWrapper(listener_id);
    return ConfigUpdateListenerRegistration();
  }
  jobject j_registration = env->NewGlobalRef(j_local_registration);
  env->DeleteLocalRef(j_local_registration);
  ConfigUpdateListenerRegistrationInternal* registration_internal =
      new ConfigUpdateListenerRegistrationInternal(this, [j_registration, listener_id]() {
        JNIEnv* env = firebase::util::GetJNIEnvFromApp();
        if (env && j_registration) {
          env->CallVoidMethod(j_registration, config_update_listener_registration::GetMethodId(
                                                  config_update_listener_registration::kRemove));
          jni::CheckAndClearException(env);
          env->DeleteGlobalRef(j_registration);
        }
        UnregisterAndDeleteListenerWrapper(listener_id);
      });
  CleanupNotifier::FindByOwner(const_cast<firebase::App*>(&app_))
      ->RegisterObject(registration_internal, [](void* registration) {
        delete reinterpret_cast<ConfigUpdateListenerRegistrationInternal*>(registration);
      });
  return ConfigUpdateListenerRegistration(registration_internal);
}
JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnUpdate(JNIEnv* env, jobject clazz, jlong c_listener_ptr,
                                                              jobject j_config_update) {
  (void)clazz;
  std::function<void(ConfigUpdate&&, RemoteConfigError)> listener_copy;
  {
    MutexLock lock(g_listener_wrappers_mutex);
    if (!g_active_listener_wrappers) { return; }
    auto it = g_active_listener_wrappers->find(c_listener_ptr);
    if (it == g_active_listener_wrappers->end() || !it->second) { return; }
    listener_copy = it->second->listener;
  }
  if (listener_copy) { listener_copy(ConfigUpdateFromJavaConfigUpdate(env, j_config_update), kRemoteConfigErrorNone); }
}
JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnError(JNIEnv* env, jobject clazz, jlong c_listener_ptr,
                                                             jint j_error_code) {
  (void)env;
  (void)clazz;
  std::function<void(ConfigUpdate&&, RemoteConfigError)> listener_copy;
  {
    MutexLock lock(g_listener_wrappers_mutex);
    if (!g_active_listener_wrappers) { return; }
    auto it = g_active_listener_wrappers->find(c_listener_ptr);
    if (it == g_active_listener_wrappers->end() || !it->second) { return; }
    listener_copy = it->second->listener;
  }
  if (listener_copy) { listener_copy({}, RemoteConfigErrorFromJavaErrorCode(static_cast<int>(j_error_code))); }
}
}  // namespace firebase::remote_config::internal
