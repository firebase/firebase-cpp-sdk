// Copyright 2016 Google LLC
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

#include "remote_config/src/android/remote_config_android.h"

#include <assert.h>

#include <set>
#include <string>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/version.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/time.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "remote_config/remote_config_resources.h"
#include "remote_config/src/common.h"
#include "remote_config/src/config_update_listener_registration_internal.h"

namespace firebase {
namespace remote_config {
namespace internal {

DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);

// Methods of the FirebaseRemoteConfig class.
// clang-format off
#define REMOTE_CONFIG_METHODS(X)                                           \
  X(GetInstance, "getInstance",                                            \
    "(Lcom/google/firebase/FirebaseApp;)"                                  \
    "Lcom/google/firebase/remoteconfig/FirebaseRemoteConfig;",             \
    util::kMethodTypeStatic),                                              \
  X(EnsureInitialized, "ensureInitialized",                                \
    "()Lcom/google/android/gms/tasks/Task;"),                              \
  X(Activate, "activate",                                                  \
    "()Lcom/google/android/gms/tasks/Task;"),                              \
  X(FetchAndActivate, "fetchAndActivate",                                  \
    "()Lcom/google/android/gms/tasks/Task;"),                              \
  X(SetDefaultsAsync, "setDefaultsAsync",                                  \
    "(I)Lcom/google/android/gms/tasks/Task;"),                             \
  X(SetDefaultsUsingMapAsync, "setDefaultsAsync",                          \
    "(Ljava/util/Map;)"                                                    \
    "Lcom/google/android/gms/tasks/Task;"),                                \
  X(SetConfigSettingsAsync, "setConfigSettingsAsync",                      \
    "(Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;)"    \
    "Lcom/google/android/gms/tasks/Task;"),                                \
  X(GetLong, "getLong", "(Ljava/lang/String;)J"),                          \
  X(GetString, "getString", "(Ljava/lang/String;)Ljava/lang/String;"),     \
  X(GetBoolean, "getBoolean", "(Ljava/lang/String;)Z"),                    \
  X(GetDouble, "getDouble", "(Ljava/lang/String;)D"),                      \
  X(GetValue, "getValue",                                                  \
    "(Ljava/lang/String;)"                                                 \
    "Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigValue;"),       \
  X(GetAll, "getAll", "()Ljava/util/Map;"),                                \
  X(GetKeysByPrefix, "getKeysByPrefix",                                    \
    "(Ljava/lang/String;)Ljava/util/Set;"),                                \
  X(GetInfo, "getInfo",                                                    \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigInfo;"),      \
  X(Fetch, "fetch", "(J)Lcom/google/android/gms/tasks/Task;"),             \
  X(AddOnConfigUpdateListener, "addOnConfigUpdateListener",                \
    "(Lcom/google/firebase/remoteconfig/ConfigUpdateListener;)"            \
    "Lcom/google/firebase/remoteconfig/ConfigUpdateListenerRegistration;")
// clang-format on

METHOD_LOOKUP_DECLARATION(config, REMOTE_CONFIG_METHODS)
METHOD_LOOKUP_DEFINITION(
    config,
    PROGUARD_KEEP_CLASS "com/google/firebase/remoteconfig/FirebaseRemoteConfig",
    REMOTE_CONFIG_METHODS)

// Methods of FirebaseRemoteConfigValue
// clang-format off
#define REMOTE_CONFIG_VALUE_METHODS(X)                              \
  X(AsLong, "asLong", "()J"),                                       \
  X(AsDouble, "asDouble", "()D"),                                   \
  X(AsString, "asString", "()Ljava/lang/String;"),                  \
  X(AsByteArray, "asByteArray", "()[B"),                            \
  X(AsBoolean,  "asBoolean", "()Z"),                                \
  X(GetSource, "getSource", "()I")
// clang-format on
METHOD_LOOKUP_DECLARATION(config_value, REMOTE_CONFIG_VALUE_METHODS)
METHOD_LOOKUP_DEFINITION(
    config_value,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/remoteconfig/FirebaseRemoteConfigValue",
    REMOTE_CONFIG_VALUE_METHODS)

// Methods of FirebaseRemoteConfigInfo
// clang-format off
#define REMOTE_CONFIG_INFO_METHODS(X) \
  X(GetFetchTimeMillis, "getFetchTimeMillis", "()J"), \
  X(GetLastFetchStatus, "getLastFetchStatus", "()I"), \
  X(GetConfigSettings, "getConfigSettings", \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;")
// clang-format on
METHOD_LOOKUP_DECLARATION(config_info, REMOTE_CONFIG_INFO_METHODS)
METHOD_LOOKUP_DEFINITION(
    config_info,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/remoteconfig/FirebaseRemoteConfigInfo",
    REMOTE_CONFIG_INFO_METHODS)

// Methods of FirebaseRemoteConfigSettings
// clang-format off
#define REMOTE_CONFIG_SETTINGS_METHODS(X) \
  X(GetFetchTimeoutInSeconds, "getFetchTimeoutInSeconds", "()J"),              \
  X(GetMinimumFetchIntervalInSeconds, "getMinimumFetchIntervalInSeconds", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(config_settings, REMOTE_CONFIG_SETTINGS_METHODS)
METHOD_LOOKUP_DEFINITION(
    config_settings,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/remoteconfig/FirebaseRemoteConfigSettings",
    REMOTE_CONFIG_SETTINGS_METHODS)

// Methods of FirebaseRemoveConfigSettings.Build
// clang-format off
#define REMOTE_CONFIG_SETTINGS_BUILDER_METHODS(X)                         \
  X(Constructor, "<init>", "()V"),                                        \
  X(Build, "build",                                                       \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;"), \
  X(SetFetchTimeoutInSeconds, "setFetchTimeoutInSeconds",                 \
    "(J)Lcom/google/firebase/remoteconfig/"                               \
    "FirebaseRemoteConfigSettings$Builder;"),                             \
  X(SetMinimumFetchIntervalInSeconds, "setMinimumFetchIntervalInSeconds", \
    "(J)Lcom/google/firebase/remoteconfig/"                               \
    "FirebaseRemoteConfigSettings$Builder;")
// clang-format on
METHOD_LOOKUP_DECLARATION(config_settings_builder,
                          REMOTE_CONFIG_SETTINGS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    config_settings_builder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/remoteconfig/FirebaseRemoteConfigSettings$Builder",
    REMOTE_CONFIG_SETTINGS_BUILDER_METHODS)

// Methods of FirebaseRemoteConfigFetchThrottledException.
// clang-format off
#define REMOTE_CONFIG_THROTTLED_EXCEPTION_METHODS(X)     \
  X(GetThrottleEndTimeMillis, "getThrottleEndTimeMillis", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(throttled_exception,
                          REMOTE_CONFIG_THROTTLED_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(throttled_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/remoteconfig/"
                         "FirebaseRemoteConfigFetchThrottledException",
                         REMOTE_CONFIG_THROTTLED_EXCEPTION_METHODS)

// Methods of JniConfigUpdateListener
// clang-format off
#define JNI_CONFIG_UPDATE_LISTENER_METHODS(X)                              \
  X(Constructor, "<init>", "(J)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(jni_config_update_listener,
                          JNI_CONFIG_UPDATE_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_config_update_listener,
    "com/google/firebase/remoteconfig/internal/cpp/JniConfigUpdateListener",
    JNI_CONFIG_UPDATE_LISTENER_METHODS)

JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnUpdate(
    JNIEnv* env, jobject clazz, jlong c_listener_ptr, jobject j_config_update);

JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnError(
    JNIEnv* env, jobject clazz, jlong c_listener_ptr, jint j_error_code);

static const JNINativeMethod kNativeJniConfigUpdateListenerMethods[] = {
    {"nativeOnUpdate", "(JLcom/google/firebase/remoteconfig/ConfigUpdate;)V",
     reinterpret_cast<void*>(JniConfigUpdateListener_nativeOnUpdate)},
    {"nativeOnError", "(JI)V",
     reinterpret_cast<void*>(JniConfigUpdateListener_nativeOnError)},
};

// Methods of ConfigUpdate
// clang-format off
#define CONFIG_UPDATE_METHODS(X)                              \
  X(GetUpdatedKeys, "getUpdatedKeys", "()Ljava/util/Set;")
// clang-format on

METHOD_LOOKUP_DECLARATION(config_update, CONFIG_UPDATE_METHODS)
METHOD_LOOKUP_DEFINITION(config_update,
                         "com/google/firebase/remoteconfig/ConfigUpdate",
                         CONFIG_UPDATE_METHODS)

// Methods of ConfigUpdateRegistration
// clang-format off
#define CONFIG_UPDATE_LISTENER_REGISTRATION_METHODS(X)                              \
  X(Remove, "remove", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(config_update_listener_registration,
                          CONFIG_UPDATE_LISTENER_REGISTRATION_METHODS)
METHOD_LOOKUP_DEFINITION(
    config_update_listener_registration,
    "com/google/firebase/remoteconfig/ConfigUpdateListenerRegistration",
    CONFIG_UPDATE_LISTENER_REGISTRATION_METHODS)

using firebase::internal::ReferenceCount;
using firebase::internal::ReferenceCountLock;

// Maps FirebaseRemoteConfig.VALUE_SOURCE_* values to the ValueSource
// enumeration.
static const ValueSource kFirebaseRemoteConfigSourceToValueSourceMap[] = {
    kValueSourceStaticValue,   // FirebaseRemoteConfig.VALUE_SOURCE_STATIC (0)
    kValueSourceDefaultValue,  // FirebaseRemoteConfig.VALUE_SOURCE_DEFAULT (1)
    kValueSourceRemoteValue,   // FirebaseRemoteConfig.VALUE_SOURCE_REMOTE (2)
};

static const char* kApiIdentifier = "Remote Config";

ReferenceCount RemoteConfigInternal::initializer_;  // NOLINT

static bool CacheJNIMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  // Cache the JniRemoteConfigListener class and register the native callbacks.
  if (!(jni_config_update_listener::CacheClassFromFiles(env, activity,
                                                        &embedded_files) &&
        jni_config_update_listener::CacheMethodIds(env, activity) &&
        jni_config_update_listener::RegisterNatives(
            env, kNativeJniConfigUpdateListenerMethods,
            FIREBASE_ARRAYSIZE(kNativeJniConfigUpdateListenerMethods)))) {
    return false;
  }

  // Cache all other classes and methods
  return (config::CacheMethodIds(env, activity) &&
          config_value::CacheMethodIds(env, activity) &&
          config_info::CacheMethodIds(env, activity) &&
          config_settings::CacheMethodIds(env, activity) &&
          config_settings_builder::CacheMethodIds(env, activity) &&
          throttled_exception::CacheMethodIds(env, activity) &&
          config_update::CacheMethodIds(env, activity) &&
          config_update_listener_registration::CacheMethodIds(env, activity));
}

static void ReleaseClasses(JNIEnv* env) {
  jni_config_update_listener::ReleaseClass(env);
  config::ReleaseClass(env);
  config_value::ReleaseClass(env);
  config_info::ReleaseClass(env);
  config_settings::ReleaseClass(env);
  config_settings_builder::ReleaseClass(env);
  throttled_exception::ReleaseClass(env);
  config_update::ReleaseClass(env);
  config_update_listener_registration::ReleaseClass(env);
}

template <typename T>
static void SaveDefaultKeys(const T* defaults, std::vector<std::string>* keys,
                            size_t number_of_defaults) {
  assert(keys);
  keys->clear();
  keys->reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; i++) {
    keys->push_back(defaults[i].key);
  }
}

// Convert a ConfigKeyValue array to string into a java HashMap of strings to
// strings.
static jobject ConfigKeyValueArrayToHashMap(JNIEnv* env,
                                            const ConfigKeyValue* defaults,
                                            size_t number_of_defaults) {
  jobject hash_map =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));
  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    jstring key = env->NewStringUTF(defaults[i].key);
    jstring value = env->NewStringUTF(defaults[i].value);
    jobject previous = env->CallObjectMethod(hash_map, put_method, key, value);
    if (util::CheckAndClearJniExceptions(env)) previous = nullptr;
    if (previous) env->DeleteLocalRef(previous);
    env->DeleteLocalRef(value);
    env->DeleteLocalRef(key);
  }
  return hash_map;
}

static jobject VariantToJavaObject(JNIEnv* env, const Variant& variant) {
  if (variant.is_int64()) {
    return env->NewObject(
        util::long_class::GetClass(),
        util::long_class::GetMethodId(util::long_class::kConstructor),
        variant.int64_value());
  } else if (variant.is_double()) {
    return env->NewObject(
        util::double_class::GetClass(),
        util::double_class::GetMethodId(util::double_class::kConstructor),
        variant.double_value());
  } else if (variant.is_bool()) {
    return env->NewObject(
        util::boolean_class::GetClass(),
        util::boolean_class::GetMethodId(util::boolean_class::kConstructor),
        variant.bool_value());
  } else if (variant.is_string()) {
    return env->NewStringUTF(variant.string_value());
  } else if (variant.is_blob()) {
    // Workaround a Remote Config Android SDK bug: rather than using a byte[]
    // array, use a String containing binary data instead.
    jchar* unicode_bytes = new jchar[variant.blob_size()];
    for (int i = 0; i < variant.blob_size(); ++i) {
      unicode_bytes[i] = variant.blob_data()[i];
    }
    jobject the_string = env->NewString(unicode_bytes, variant.blob_size());
    delete[] unicode_bytes;
    return the_string;
    // TODO(b/141322200) Remote the code above and restore the code below once
    // this bug is fixed.
    // return static_cast<jobject>(util::ByteBufferToJavaByteArray(env,
    // variant.blob_data(), variant.blob_size()));
  } else {
    return nullptr;
  }
}

// Convert a ConfigKeyValueVariant array into a Java HashMap of string to
// Object.
static jobject ConfigKeyValueVariantArrayToHashMap(
    JNIEnv* env, const ConfigKeyValueVariant* defaults,
    size_t number_of_defaults) {
  jobject hash_map =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));
  jmethodID put_method = util::map::GetMethodId(util::map::kPut);
  for (size_t i = 0; i < number_of_defaults; ++i) {
    jstring key = env->NewStringUTF(defaults[i].key);
    jobject value = VariantToJavaObject(env, defaults[i].value);
    if (value != nullptr) {
      jobject previous =
          env->CallObjectMethod(hash_map, put_method, key, value);
      if (util::CheckAndClearJniExceptions(env)) previous = nullptr;
      if (previous) env->DeleteLocalRef(previous);
      env->DeleteLocalRef(value);
    } else {
      LogError("Remote Config: Invalid Variant type for SetDefaults() key %s.",
               defaults[i].key);
    }
    env->DeleteLocalRef(key);
  }
  return hash_map;
}

// Check pending exceptions following a key fetch and log an error if a
// failure occurred.  If an error occurs this method returns true, false
// otherwise.
static bool CheckKeyRetrievalLogError(JNIEnv* env, const char* key,
                                      const char* value_type) {
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    LogError("Remote Config: Failed to retrieve %s value from key %s",
             value_type, key);
    return true;
  }
  return false;
}

// Generate the logic to retrieve an integral type value and source from
// a FirebaseRemoteConfigValue interface.
// key is a string with the key to retrieve, info is a pointer to ValueInfo
// which receives the source of the retrieved value, c_type is the integral type
// being retrieved, c_type_name is a human readable string for the value being
// retrieved and java_type is the type portion of the JNI function to call.
#define CONFIG_GET_INTEGRAL_TYPE_FROM_VALUE(key, info, c_type, c_type_name, \
                                            java_type)                      \
  {                                                                         \
    JNIEnv* env = g_app->GetJNIEnv();                                       \
    jobject value_object =                                                  \
        GetValue(env, g_remote_config_class_instance, key, info);           \
    if (!value_object) return 0;                                            \
    c_type value = env->Call##java_type##Method(                            \
        value_object,                                                       \
        config_value::GetMethodId(config_value::kAs##java_type));           \
    bool failed = CheckKeyRetrievalLogError(env, key, c_type_name);         \
    env->DeleteLocalRef(value_object);                                      \
    if (info) info->conversion_successful = !failed;                        \
    return failed ? static_cast<c_type>(0) : value;                         \
  }

// Get the FirebaseRemoteConfigValue interface and the value source for a key.
static jobject GetValue(JNIEnv* env, jobject rc_obj, const char* key,
                        ValueInfo* info) {
  bool configFetchFailed = false;
  jstring key_string = env->NewStringUTF(key);
  jobject config_value = env->CallObjectMethod(
      rc_obj, config::GetMethodId(config::kGetValue), key_string);
  if (util::CheckAndClearJniExceptions(env)) {
    config_value = nullptr;
    configFetchFailed = true;
  }
  configFetchFailed |= CheckKeyRetrievalLogError(env, key, "<unknown>");
  env->DeleteLocalRef(key_string);
  if (info) {
    info->source = kValueSourceStaticValue;
    info->conversion_successful = false;
  }
  if (info && !configFetchFailed) {
    info->source = kValueSourceDefaultValue;
    int value_source = env->CallIntMethod(
        config_value, config_value::GetMethodId(config_value::kGetSource));
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      configFetchFailed = true;
    }
    if (!configFetchFailed && value_source >= 0 &&
        value_source < sizeof(kFirebaseRemoteConfigSourceToValueSourceMap)) {
      info->source = kFirebaseRemoteConfigSourceToValueSourceMap[value_source];
    } else {
      LogError(
          "Unable to convert source (%d) of key %s to a ValueSource "
          "enumeration value.",
          value_source, key);
    }
  }
  return configFetchFailed ? nullptr : config_value;
}

/**
 * Takes a java ConfigUpdate and returns a corresponding C++ ConfigUpdate.
 */
static ConfigUpdate ConfigUpdateFromJavaConfigUpdate(JNIEnv* env,
                                                     jobject j_config_update) {
  ConfigUpdate config_update;
  jobject key_set_java = env->CallObjectMethod(
      j_config_update,
      config_update::GetMethodId(config_update::kGetUpdatedKeys));
  if (util::CheckAndClearJniExceptions(env)) key_set_java = nullptr;
  if (key_set_java) {
    util::JavaSetToStdStringVector(env, &(config_update.updated_keys),
                                   key_set_java);
    env->DeleteLocalRef(key_set_java);
  }
  return config_update;
}

/**
 * Takes an integer that represents a FirebaseRemoteConfigException error code,
 * and returns the corresponding RemoteConfigError.
 * This mapping should be kept in sync with the enum defined in
 * FirebaseRemoteConfigException in the firebase-android-sdk.
 */
static RemoteConfigError RemoteConfigErrorFromJavaErrorCode(int error_code) {
  switch (error_code) {
    case 1:  // CONFIG_UPDATE_STREAM_ERROR
      return kRemoteConfigErrorConfigUpdateStreamError;
    case 2:  // CONFIG_UPDATE_MESSAGE_INVALID
      return kRemoteConfigErrorConfigUpdateMessageInvalid;
    case 3:  // CONFIG_UPDATE_NOT_FETCHED
      return kRemoteConfigErrorConfigUpdateNotFetched;
    case 4:  // CONFIG_UPDATE_UNAVAILABLE
      return kRemoteConfigErrorConfigUpdateUnavailable;
    case 0:  // UNKNOWN
    default:
      return kRemoteConfigErrorUnimplemented;
  }
}

// Generate the logic to retrieve an integral type value from the
// FirebaseRemoteConfig class.
// key is a string with the key to retrieve, c_type is the integral type being
// retrieved, c_type_name is a human readable string for the value being
// retrieved and java_type is the type portion of the JNI function to call.
#define CONFIG_GET_INTEGRAL_TYPE(key, c_type, c_type_name, java_type) \
  {                                                                   \
    JNIEnv* env = g_app->GetJNIEnv();                                 \
    jstring key_string = env->NewStringUTF(key);                      \
    c_type value = env->Call##java_type##Method(                      \
        g_remote_config_class_instance,                               \
        config::GetMethodId(config::kGet##java_type), key_string);    \
    bool failed = CheckKeyRetrievalLogError(env, key, c_type_name);   \
    env->DeleteLocalRef(key_string);                                  \
    return failed ? static_cast<c_type>(0) : value;                   \
  }

static void JConfigInfoToConfigInfo(JNIEnv* env, jobject jinfo,
                                    ConfigInfo* info) {
  FIREBASE_DEV_ASSERT(env->IsInstanceOf(jinfo, config_info::GetClass()));

  info->fetch_time = env->CallLongMethod(
      jinfo, config_info::GetMethodId(config_info::kGetFetchTimeMillis));
  int64_t status_code = env->CallIntMethod(
      jinfo, config_info::GetMethodId(config_info::kGetLastFetchStatus));
  switch (status_code) {
    case -1:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_SUCCESS
      info->last_fetch_status = kLastFetchStatusSuccess;
      info->last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case 0:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_NO_FETCH_YET
      info->last_fetch_status = kLastFetchStatusPending;
      info->last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case 1:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_FAILURE
      info->last_fetch_status = kLastFetchStatusFailure;
      info->last_fetch_failure_reason = kFetchFailureReasonError;
      break;
    case 2:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_THROTTLED
      info->last_fetch_status = kLastFetchStatusFailure;
      info->last_fetch_failure_reason = kFetchFailureReasonThrottled;
      break;
    default:
      LogWarning("Unknown last fetch status %d.", status_code);
      info->last_fetch_status = kLastFetchStatusFailure;
      info->last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
  }
  util::CheckAndClearJniExceptions(env);
}

template <typename T>
struct RCDataHandle {
  RCDataHandle(
      ReferenceCountedFutureImpl* _future_api,
      const SafeFutureHandle<T>& _future_handle,
      RemoteConfigInternal* _rc_internal,
      std::vector<std::string> _default_keys = std::vector<std::string>())
      : future_api(_future_api),
        future_handle(_future_handle),
        rc_internal(_rc_internal),
        default_keys(_default_keys) {}
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
  RemoteConfigInternal* rc_internal;
  std::vector<std::string> default_keys;
};

// An object wrapping a ConfigUpdateListener callback method
struct ConfigUpdateListenerWrapper {
  ConfigUpdateListenerWrapper(
      std::function<void(ConfigUpdate&&, RemoteConfigError)> listener_)
      : listener(listener_) {}
  std::function<void(ConfigUpdate&&, RemoteConfigError)> listener;
};

RemoteConfigInternal::RemoteConfigInternal(const firebase::App& app)
    : app_(app), future_impl_(kRemoteConfigFnCount) {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  LogDebug("Firebase RemoteConfig API Initializing");
  JNIEnv* env = app_.GetJNIEnv();
  if (lock.AddReference() == 0) {
    // Initialize
    jobject activity = app_.activity();
    if (!util::Initialize(env, activity)) {
      lock.RemoveReference();
      return;
    }

    // Cache embedded files and load embedded classes.
    const std::vector<firebase::internal::EmbeddedFile> embedded_files =
        util::CacheEmbeddedFiles(
            env, activity,
            firebase::internal::EmbeddedFile::ToVector(
                firebase_remote_config::remote_config_resources_filename,
                firebase_remote_config::remote_config_resources_data,
                firebase_remote_config::remote_config_resources_size));
    // Cache method pointers.
    if (!CacheJNIMethodIds(env, activity, embedded_files)) {
      ReleaseClasses(env);
      util::Terminate(env);
      lock.RemoveReference();
      return;
    }
  }

  // Create the remote config class.
  jclass config_class = config::GetClass();
  jobject platform_app = app_.GetPlatformApp();
  jobject config_instance_local = env->CallStaticObjectMethod(
      config_class, config::GetMethodId(config::kGetInstance), platform_app);
  env->DeleteLocalRef(platform_app);
  if (util::CheckAndClearJniExceptions(env)) config_instance_local = nullptr;
  FIREBASE_ASSERT(config_instance_local);
  internal_obj_ = env->NewGlobalRef(config_instance_local);
  env->DeleteLocalRef(config_instance_local);
  LogDebug("%s API Initialized", kApiIdentifier);
}

RemoteConfigInternal::~RemoteConfigInternal() {
  // Trigger CleanupNotifier Cleanup. This will delete
  // ConfigUpdateListenerRegistrationInternal instances and it will update
  // ConfigUpdateListenerRegistration instances to no longer point to the
  // corresponding internal objects.
  cleanup_notifier().CleanupAll();
}

bool RemoteConfigInternal::Initialized() const {
  return internal_obj_ != nullptr;
}

void RemoteConfigInternal::Cleanup() {
  ReferenceCountLock<ReferenceCount> lock(&initializer_);
  if (lock.RemoveReference() == 1) {
    JNIEnv* env = app_.GetJNIEnv();
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

void EnsureInitializedCallback(JNIEnv* env, jobject result,
                               util::FutureResult result_code,
                               const char* status_message,
                               void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  ConfigInfo info;
  if (success && result) {
    JConfigInfoToConfigInfo(env, result, &info);
  }
  auto* data_handle =
      reinterpret_cast<RCDataHandle<ConfigInfo>*>(callback_data);

  data_handle->future_api->CompleteWithResult(
      data_handle->future_handle,
      success ? kFutureStatusSuccess : kFutureStatusFailure, status_message,
      info);

  delete data_handle;
}

void BoolResultCallback(JNIEnv* env, jobject result,
                        util::FutureResult result_code,
                        const char* status_message, void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  bool result_value = false;
  if (success && result) {
    result_value = util::JBooleanToBool(env, result);
  }

  auto* data_handle = reinterpret_cast<RCDataHandle<bool>*>(callback_data);
  data_handle->future_api->CompleteWithResult(
      data_handle->future_handle,
      success ? kFutureStatusSuccess : kFutureStatusFailure, status_message,
      result_value);

  delete data_handle;
}

void CompleteVoidCallback(JNIEnv* env, jobject result,
                          util::FutureResult result_code,
                          const char* status_message, void* callback_data) {
  auto* data_handle = reinterpret_cast<RCDataHandle<void>*>(callback_data);
  data_handle->future_api->Complete(data_handle->future_handle,
                                    result_code == util::kFutureResultSuccess
                                        ? kFutureStatusSuccess
                                        : kFutureStatusFailure);
  delete data_handle;
}

void FetchCallback(JNIEnv* env, jobject result, util::FutureResult result_code,
                   const char* status_message, void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  int64_t throttle_end_time = 0;
  if (!success && result) {
    if (env->IsInstanceOf(result, throttled_exception::GetClass())) {
      throttle_end_time = static_cast<int64_t>(env->CallLongMethod(
          result, throttled_exception::GetMethodId(
                      throttled_exception::kGetThrottleEndTimeMillis)));
    }
  }
  auto data_handle = reinterpret_cast<RCDataHandle<void>*>(callback_data);
  if (throttle_end_time > 0) {
    data_handle->rc_internal->set_throttled_end_time(throttle_end_time);
  }

  CompleteVoidCallback(env, result, result_code, status_message, callback_data);
}

void SetDefaultsCallback(JNIEnv* env, jobject result,
                         util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  auto data_handle = reinterpret_cast<RCDataHandle<void>*>(callback_data);
  if (result_code == util::kFutureResultSuccess &&
      !data_handle->default_keys.empty()) {
    data_handle->rc_internal->SaveTmpKeysToDefault(data_handle->default_keys);
  }
  CompleteVoidCallback(env, result, result_code, status_message, callback_data);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle =
      future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kEnsureInitialized));
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "EnsureInitialized native function fails");
  } else {
    auto data_handle =
        new RCDataHandle<ConfigInfo>(&future_impl_, handle, this);

    util::RegisterCallbackOnTask(env, task, EnsureInitializedCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }
  env->DeleteLocalRef(task);
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo>&>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(internal_obj_,
                                       config::GetMethodId(config::kActivate));
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "Activate native function fails");
  } else {
    auto data_handle = new RCDataHandle<bool>(&future_impl_, handle, this);
    util::RegisterCallbackOnTask(env, task, BoolResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }

  env->DeleteLocalRef(task);
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnActivate));
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto handle =
      future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kFetchAndActivate));
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "FetchAndActivate native function fails");
  } else {
    auto data_handle = new RCDataHandle<bool>(&future_impl_, handle, this);
    util::RegisterCallbackOnTask(env, task, BoolResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }

  env->DeleteLocalRef(task);
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnFetchAndActivate));
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task =
      env->CallObjectMethod(internal_obj_, config::GetMethodId(config::kFetch),
                            static_cast<jlong>(cache_expiration_in_seconds));
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "Fetch native function fails");
  } else {
    auto data_handle = new RCDataHandle<void>(&future_impl_, handle, this);

    util::RegisterCallbackOnTask(env, task, FetchCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }
  env->DeleteLocalRef(task);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::FetchLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnFetch));
}

Future<void> RemoteConfigInternal::SetDefaults(int defaults_resource_id) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  JNIEnv* env = app_.GetJNIEnv();
  jobject task = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kSetDefaultsAsync),
      defaults_resource_id);

  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "SetDefaults native function fails");
  } else {
    auto data_handle = new RCDataHandle<void>(&future_impl_, handle, this);
    util::RegisterCallbackOnTask(env, task, SetDefaultsCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }
  env->DeleteLocalRef(task);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaults(
    const ConfigKeyValueVariant* defaults, size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  JNIEnv* env = app_.GetJNIEnv();
  jobject hash_map =
      ConfigKeyValueVariantArrayToHashMap(env, defaults, number_of_defaults);
  std::vector<std::string> tmp_keys;
  SaveDefaultKeys(defaults, &tmp_keys, number_of_defaults);
  jobject task = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kSetDefaultsUsingMapAsync),
      hash_map);
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "SetDefaults native function fails");
  } else {
    auto data_handle =
        new RCDataHandle<void>(&future_impl_, handle, this, tmp_keys);

    util::RegisterCallbackOnTask(env, task, SetDefaultsCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(hash_map);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  JNIEnv* env = app_.GetJNIEnv();
  jobject hash_map =
      ConfigKeyValueArrayToHashMap(env, defaults, number_of_defaults);
  std::vector<std::string> tmp_keys;
  SaveDefaultKeys(defaults, &tmp_keys, number_of_defaults);
  jobject task = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kSetDefaultsUsingMapAsync),
      hash_map);
  if (util::CheckAndClearJniExceptions(env)) {
    task = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "SetDefaults native function fails");
  } else {
    auto data_handle =
        new RCDataHandle<void>(&future_impl_, handle, this, tmp_keys);

    util::RegisterCallbackOnTask(env, task, SetDefaultsCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  }
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(hash_map);
  util::CheckAndClearJniExceptions(env);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetDefaults));
}

Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  JNIEnv* env = app_.GetJNIEnv();

  jobject builder = env->NewObject(config_settings_builder::GetClass(),
                                   config_settings_builder::GetMethodId(
                                       config_settings_builder::kConstructor));
  uint64_t fetch_timeout_in_sec = settings.fetch_timeout_in_milliseconds /
                                  ::firebase::internal::kMillisecondsPerSecond;
  uint64_t minimum_fetch_interval_in_sec =
      settings.minimum_fetch_interval_in_milliseconds /
      ::firebase::internal::kMillisecondsPerSecond;

  // FirebaseRemoteConfigSettings.Builder.setFetchTimeoutInSeconds(long)
  env->CallObjectMethod(builder,
                        config_settings_builder::GetMethodId(
                            config_settings_builder::kSetFetchTimeoutInSeconds),
                        fetch_timeout_in_sec);
  util::CheckAndClearJniExceptions(env);

  // FirebaseRemoteConfigSettings.Builder.setMinimumFetchIntervalInSeconds(long)
  env->CallObjectMethod(
      builder,
      config_settings_builder::GetMethodId(
          config_settings_builder::kSetMinimumFetchIntervalInSeconds),
      minimum_fetch_interval_in_sec);
  util::CheckAndClearJniExceptions(env);

  // FirebaseRemoteConfigSettings.Builder.build()
  jobject settings_obj = env->CallObjectMethod(
      builder,
      config_settings_builder::GetMethodId(config_settings_builder::kBuild));
  if (util::CheckAndClearJniExceptions(env)) {
    settings_obj = nullptr;
    future_impl_.Complete(handle, kFutureStatusFailure,
                          "SetConfigSettings native function fails");
  } else {
    jobject task = env->CallObjectMethod(
        internal_obj_, config::GetMethodId(config::kSetConfigSettingsAsync),
        settings_obj);
    if (util::CheckAndClearJniExceptions(env)) {
      task = nullptr;
      future_impl_.Complete(handle, kFutureStatusFailure,
                            "SetConfigSettings native function fails");
    } else {
      auto data_handle = new RCDataHandle<void>(&future_impl_, handle, this);

      util::RegisterCallbackOnTask(env, task, CompleteVoidCallback,
                                   reinterpret_cast<void*>(data_handle),
                                   kApiIdentifier);
    }
    env->DeleteLocalRef(task);
  }

  env->DeleteLocalRef(settings_obj);
  env->DeleteLocalRef(builder);
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

ConfigSettings RemoteConfigInternal::GetConfigSettings() {
  ConfigSettings settings;
  JNIEnv* env = app_.GetJNIEnv();

  // FirebaseRemoteConfig.getInfo()
  jobject jinfo = env->CallObjectMethod(internal_obj_,
                                        config::GetMethodId(config::kGetInfo));
  util::CheckAndClearJniExceptions(env);

  // FirebaseRemoteConfigInfo.getConfigSettings()
  jobject config_settings_obj = env->CallObjectMethod(
      jinfo, config_info::GetMethodId(config_info::kGetConfigSettings));
  util::CheckAndClearJniExceptions(env);

  // FirebaseRemoteConfigSettings.getFetchTimeoutInSeconds()
  settings.fetch_timeout_in_milliseconds =
      env->CallLongMethod(config_settings_obj,
                          config_settings::GetMethodId(
                              config_settings::kGetFetchTimeoutInSeconds)) *
      ::firebase::internal::kMillisecondsPerSecond;
  util::CheckAndClearJniExceptions(env);

  // FirebaseRemoteConfigSettings.getMinimumFetchIntervalInSeconds()
  settings.minimum_fetch_interval_in_milliseconds =
      env->CallLongMethod(
          config_settings_obj,
          config_settings::GetMethodId(
              config_settings::kGetMinimumFetchIntervalInSeconds)) *
      ::firebase::internal::kMillisecondsPerSecond;
  util::CheckAndClearJniExceptions(env);

  env->DeleteLocalRef(jinfo);
  env->DeleteLocalRef(config_settings_obj);
  return settings;
}

// Generate the logic to retrieve an certain type value and source from
// a FirebaseRemoteConfigValue interface.
// key is a string with the key to retrieve, info is a pointer to ValueInfo
// which receives the source of the retrieved value, c_type is the integral type
// being retrieved, c_type_name is a human readable string for the value being
// retrieved and java_type is the type portion of the JNI function to call.
#define RC_GET_TYPE_FROM_VALUE(app, obj, key, info, c_type, c_type_name, \
                               java_type)                                \
  {                                                                      \
    JNIEnv* env = app.GetJNIEnv();                                       \
    jobject value_object = GetValue(env, obj, key, info);                \
    if (!value_object) return static_cast<c_type>(0);                    \
    c_type value = env->Call##java_type##Method(                         \
        value_object,                                                    \
        config_value::GetMethodId(config_value::kAs##java_type));        \
    bool failed = CheckKeyRetrievalLogError(env, key, c_type_name);      \
    env->DeleteLocalRef(value_object);                                   \
    if (info) info->conversion_successful = !failed;                     \
    return failed ? static_cast<c_type>(0) : value;                      \
  }

bool RemoteConfigInternal::GetBoolean(const char* key, ValueInfo* info) {
  RC_GET_TYPE_FROM_VALUE(app_, internal_obj_, key, info, bool, "boolean",
                         Boolean);
}

int64_t RemoteConfigInternal::GetLong(const char* key, ValueInfo* info) {
  RC_GET_TYPE_FROM_VALUE(app_, internal_obj_, key, info, int64_t, "long", Long);
}

double RemoteConfigInternal::GetDouble(const char* key, ValueInfo* info) {
  RC_GET_TYPE_FROM_VALUE(app_, internal_obj_, key, info, double, "double",
                         Double);
}

std::string RemoteConfigInternal::GetString(const char* key, ValueInfo* info) {
  std::string value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject value_object = GetValue(env, internal_obj_, key, info);
  if (value_object) {
    jobject value_string = env->CallObjectMethod(
        value_object, config_value::GetMethodId(config_value::kAsString));
    if (util::CheckAndClearJniExceptions(env)) value_string = nullptr;
    bool failed = CheckKeyRetrievalLogError(env, key, "string");
    env->DeleteLocalRef(value_object);
    if (!failed) value = util::JniStringToString(env, value_string);
    if (info) info->conversion_successful = !failed;
  }
  return value;
}

std::vector<unsigned char> RemoteConfigInternal::GetData(const char* key,
                                                         ValueInfo* info) {
  std::vector<unsigned char> value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject value_object = GetValue(env, internal_obj_, key, info);
  if (value_object) {
    jobject value_array = env->CallObjectMethod(
        value_object, config_value::GetMethodId(config_value::kAsByteArray));
    if (util::CheckAndClearJniExceptions(env)) value_array = nullptr;
    bool failed = CheckKeyRetrievalLogError(env, key, "vector");
    env->DeleteLocalRef(value_object);
    if (!failed) value = util::JniByteArrayToVector(env, value_array);
    if (info) info->conversion_successful = !failed;
  }
  return value;
}

std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(
    const char* prefix) {
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  JNIEnv* env = app_.GetJNIEnv();
  jstring prefix_string = prefix ? env->NewStringUTF(prefix) : nullptr;
  jobject key_set_java = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kGetKeysByPrefix),
      prefix_string);
  if (util::CheckAndClearJniExceptions(env)) key_set_java = nullptr;
  if (key_set_java) {
    util::JavaSetToStdStringVector(env, &keys, key_set_java);
    env->DeleteLocalRef(key_set_java);
    for (auto i = keys.begin(); i != keys.end(); ++i) {
      key_set.insert(*i);
    }
  }
  if (prefix_string) env->DeleteLocalRef(prefix_string);

  MutexLock lock(default_key_mutex_);
  // Add any extra keys that were previously included in defaults but not
  // returned by Get*KeysByPrefix.
  size_t prefix_length = prefix ? strlen(prefix) : 0;
  for (auto i = default_keys_.begin(); i != default_keys_.end(); ++i) {
    if (key_set.find(*i) != key_set.end()) {
      // Already in the list of keys, no need to add it.
      continue;
    }
    // If the prefix matches (or we have no prefix to compare), add it to the
    // defaults list.
    if (prefix_length == 0 || strncmp(prefix, i->c_str(), prefix_length) == 0) {
      keys.push_back(*i);
      key_set.insert(*i);  // In case the defaults vector has duplicate keys.
    }
  }
  return keys;
}

std::vector<std::string> RemoteConfigInternal::GetKeys() {
  return GetKeysByPrefix(nullptr);
}

// Generate the logic to convert an certain type value from a
// FirebaseRemoteConfigValue interface to Variant. key is a string with the key
// to retrieve, ic_type is the cpp type being retrieved, variant_fn is the
// function to construct the Variant. java_type is the type portion of the JNI
// function to call.
#define RC_SET_VALUE_TO_VARIANT(env, from_obj, c_type, variant_fn, java_type) \
  {                                                                           \
    c_type value = env->Call##java_type##Method(                              \
        from_obj, config_value::GetMethodId(config_value::kAs##java_type));   \
    if (!CheckKeyRetrievalLogError(env, "", "c_type")) {                      \
      return Variant::variant_fn(value);                                      \
    }                                                                         \
  }

// FirebaseRemoteConfigValue -> Variant
static Variant RemoteConfigValueToVariant(JNIEnv* env, jobject from) {
  if (!from) return Variant::Null();
  if (!env->IsInstanceOf(from, config_value::GetClass()))
    return Variant::Null();
  // Try int
  RC_SET_VALUE_TO_VARIANT(env, from, int64_t, FromInt64, Long)
  // Not int, try double
  RC_SET_VALUE_TO_VARIANT(env, from, double, FromDouble, Double)
  // Not double, try bool
  RC_SET_VALUE_TO_VARIANT(env, from, bool, FromBool, Boolean)

  // Not bool, try string
  jobject value_string = env->CallObjectMethod(
      from, config_value::GetMethodId(config_value::kAsString));
  if (util::CheckAndClearJniExceptions(env)) value_string = nullptr;
  if (!CheckKeyRetrievalLogError(env, "", "string")) {
    return Variant::FromMutableString(
        util::JniStringToString(env, value_string));
  }

  // Not string, try byte array
  jobject value_array = env->CallObjectMethod(
      from, config_value::GetMethodId(config_value::kAsByteArray));
  if (util::CheckAndClearJniExceptions(env)) value_array = nullptr;
  if (!CheckKeyRetrievalLogError(env, "", "vector")) {
    std::vector<unsigned char> blob =
        util::JniByteArrayToVector(env, value_array);
    return Variant::FromMutableBlob(&blob, blob.size());
  }
  // If get here, all conversion fails.
  LogError(
      "Remote Config: Unable to convert a FirebaseRemoteConfigValue to "
      "Variant.");
  return Variant::Null();
}

static void JavaMapToStringVariantMap(JNIEnv* env,
                                      std::map<std::string, Variant>* to,
                                      jobject from) {
  jobject key_set =
      env->CallObjectMethod(from, util::map::GetMethodId(util::map::kKeySet));
  if (util::CheckAndClearJniExceptions(env)) {
    env->DeleteLocalRef(key_set);
    return;
  }
  jobject iter = env->CallObjectMethod(
      key_set, util::set::GetMethodId(util::set::kIterator));
  if (util::CheckAndClearJniExceptions(env)) {
    env->DeleteLocalRef(iter);
    return;
  }
  while (env->CallBooleanMethod(
      iter, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    jobject key_object = env->CallObjectMethod(
        iter, util::iterator::GetMethodId(util::iterator::kNext));
    bool call_native_success = !util::CheckAndClearJniExceptions(env);
    jobject value_object = env->CallObjectMethod(
        from, util::map::GetMethodId(util::map::kGet), key_object);
    call_native_success =
        call_native_success && !util::CheckAndClearJniExceptions(env);
    if (call_native_success) {
      std::string key = util::JStringToString(env, key_object);
      Variant v_value = RemoteConfigValueToVariant(env, value_object);
      to->insert(std::make_pair(key, v_value));
    }
    env->DeleteLocalRef(key_object);
    env->DeleteLocalRef(value_object);
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  JNIEnv* env = app_.GetJNIEnv();
  jobject key_value_map = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kGetAll));
  if (util::CheckAndClearJniExceptions(env)) key_value_map = nullptr;
  if (key_value_map) {
    JavaMapToStringVariantMap(env, &value, key_value_map);
    env->DeleteLocalRef(key_value_map);
  }
  return value;
}

const ConfigInfo RemoteConfigInternal::GetInfo() const {
  JNIEnv* env = app_.GetJNIEnv();
  ConfigInfo info;
  info.throttled_end_time = throttled_end_time_;

  jobject jinfo = env->CallObjectMethod(internal_obj_,
                                        config::GetMethodId(config::kGetInfo));
  if (util::CheckAndClearJniExceptions(env)) jinfo = nullptr;
  JConfigInfoToConfigInfo(env, jinfo, &info);
  env->DeleteLocalRef(jinfo);
  return info;
}

ConfigUpdateListenerRegistration
RemoteConfigInternal::AddOnConfigUpdateListener(
    std::function<void(ConfigUpdate&&, RemoteConfigError)>
        config_update_listener) {
  JNIEnv* env = app_.GetJNIEnv();
  // Create a struct to wrap the listener function.
  ConfigUpdateListenerWrapper* listener_wrapper =
      new ConfigUpdateListenerWrapper(config_update_listener);
  // Create a Java Listener and give it a pointer to the listener struct.
  jobject j_listener =
      env->NewObject(jni_config_update_listener::GetClass(),
                     jni_config_update_listener::GetMethodId(
                         jni_config_update_listener::kConstructor),
                     reinterpret_cast<jlong>(listener_wrapper));
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));

  jobject j_local_registration = env->CallObjectMethod(
      internal_obj_, config::GetMethodId(config::kAddOnConfigUpdateListener),
      j_listener);
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));

  // Create a global reference to the registration
  jobject j_registration = env->NewGlobalRef(j_local_registration);
  env->DeleteLocalRef(j_local_registration);

  // Create a C++ registration to wrap the native registration
  ConfigUpdateListenerRegistrationInternal* registration_internal =
      new ConfigUpdateListenerRegistrationInternal(this, [j_registration]() {
        // util::GetJNIEnvFromApp returns a threadsafe instance of JNIEnv.
        JNIEnv* env = firebase::util::GetJNIEnvFromApp();
        env->CallVoidMethod(j_registration,
                            config_update_listener_registration::GetMethodId(
                                config_update_listener_registration::kRemove));
        FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
        env->DeleteGlobalRef(j_registration);
      });
  // Delete the internal registration when RemoteConfigInternal is cleaned up.
  cleanup_notifier().RegisterObject(
      registration_internal, [](void* registration) {
        delete reinterpret_cast<ConfigUpdateListenerRegistrationInternal*>(
            registration);
      });

  ConfigUpdateListenerRegistration registration_wrapper(registration_internal);
  return registration_wrapper;
}

JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnUpdate(
    JNIEnv* env, jobject clazz, jlong c_listener_ptr, jobject j_config_update) {
  auto config_update_listener =
      reinterpret_cast<ConfigUpdateListenerWrapper*>(c_listener_ptr);
  config_update_listener->listener(
      ConfigUpdateFromJavaConfigUpdate(env, j_config_update),
      kRemoteConfigErrorNone);
}

JNIEXPORT void JNICALL JniConfigUpdateListener_nativeOnError(
    JNIEnv* env, jobject clazz, jlong c_listener_ptr, jint j_error_code) {
  auto config_update_listener =
      reinterpret_cast<ConfigUpdateListenerWrapper*>(c_listener_ptr);
  config_update_listener->listener(
      {}, RemoteConfigErrorFromJavaErrorCode(static_cast<int>(j_error_code)));
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
