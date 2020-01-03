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
#include "app/src/include/firebase/version.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app/src/util_android.h"
#include "remote_config/src/common.h"

namespace firebase {
namespace remote_config {

DEFINE_FIREBASE_VERSION_STRING(FirebaseRemoteConfig);

// Methods of the FirebaseRemoteConfig class.
// clang-format off
#define REMOTE_CONFIG_METHODS(X)                                           \
  X(GetInstance, "getInstance",                                            \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfig;",           \
    util::kMethodTypeStatic),                                              \
  X(ActivateFetched, "activateFetched", "()Z"),                            \
  X(SetDefaults, "setDefaults", "(I)V"),                                   \
  X(SetDefaultsUsingMap, "setDefaults", "(Ljava/util/Map;)V"),             \
  X(SetConfigSettings, "setConfigSettings",                                \
    "(Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigSettings;)V"), \
  X(GetLong, "getLong", "(Ljava/lang/String;)J"),                          \
  X(GetByteArray, "getByteArray", "(Ljava/lang/String;)[B"),               \
  X(GetString, "getString", "(Ljava/lang/String;)Ljava/lang/String;"),     \
  X(GetBoolean, "getBoolean", "(Ljava/lang/String;)Z"),                    \
  X(GetDouble, "getDouble", "(Ljava/lang/String;)D"),                      \
  X(GetValue, "getValue",                                                  \
    "(Ljava/lang/String;)"                                                 \
    "Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigValue;"),       \
  X(GetKeysByPrefix, "getKeysByPrefix",                                    \
    "(Ljava/lang/String;)Ljava/util/Set;"),                                \
  X(GetInfo, "getInfo",                                                    \
    "()Lcom/google/firebase/remoteconfig/FirebaseRemoteConfigInfo;"),      \
  X(Fetch, "fetch", "(J)Lcom/google/android/gms/tasks/Task;")              \
// clang-format on

METHOD_LOOKUP_DECLARATION(config, REMOTE_CONFIG_METHODS)
METHOD_LOOKUP_DEFINITION(
    config,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/remoteconfig/FirebaseRemoteConfig",
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
  X(IsDeveloperModeEnabled, "isDeveloperModeEnabled", "()Z")
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
  X(SetDeveloperModeEnabled, "setDeveloperModeEnabled",                   \
    "(Z)Lcom/google/firebase/remoteconfig/"                               \
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

// Global reference to the Android FirebaseRemoteConfig class instance.
// This is initialized in remote_config::Initialize() and never released
// during the lifetime of the application.
static jobject g_remote_config_class_instance = nullptr;

// Used to retrieve the JNI environment in order to call methods on the
// Android Analytics class.
static const ::firebase::App* g_app = nullptr;

// If a fetch was throttled, this is set to the time when throttling is
// finished, in milliseconds since epoch.
static int64_t g_throttled_end_time = 0;

// Maps FirebaseRemoteConfig.VALUE_SOURCE_* values to the ValueSource
// enumeration.
static const ValueSource kFirebaseRemoteConfigSourceToValueSourceMap[] = {
    kValueSourceStaticValue,   // FirebaseRemoteConfig.VALUE_SOURCE_STATIC (0)
    kValueSourceDefaultValue,  // FirebaseRemoteConfig.VALUE_SOURCE_DEFAULT (1)
    kValueSourceRemoteValue,   // FirebaseRemoteConfig.VALUE_SOURCE_REMOTE (2)
};

static const char* kApiIdentifier = "Remote Config";

// Saved default keys.
static std::vector<std::string>* g_default_keys = nullptr;

static void ReleaseClasses(JNIEnv* env) {
  config::ReleaseClass(env);
  config_value::ReleaseClass(env);
  config_info::ReleaseClass(env);
  config_settings::ReleaseClass(env);
  config_settings_builder::ReleaseClass(env);
  throttled_exception::ReleaseClass(env);
}

template <typename T>
static void SaveDefaultKeys(const T* defaults, size_t number_of_defaults) {
  assert(g_default_keys);
  g_default_keys->clear();
  g_default_keys->reserve(number_of_defaults);
  for (size_t i = 0; i < number_of_defaults; i++) {
    g_default_keys->push_back(defaults[i].key);
  }
}

InitResult Initialize(const App& app) {
  if (g_app) {
    LogWarning("%s API already initialized", kApiIdentifier);
    return kInitResultSuccess;
  }
  FIREBASE_UTIL_RETURN_FAILURE_IF_GOOGLE_PLAY_UNAVAILABLE(app);
  internal::RegisterTerminateOnDefaultAppDestroy();

  LogDebug("%s API Initializing", kApiIdentifier);
  FIREBASE_ASSERT(!g_remote_config_class_instance);
  JNIEnv* env = app.GetJNIEnv();

  // Make sure the android util library is prepped.
  jobject activity = app.activity();
  if (!util::Initialize(env, activity)) {
    return kInitResultFailedMissingDependency;
  }
  // Cache method pointers.
  if (!(config::CacheMethodIds(env, activity) &&
        config_value::CacheMethodIds(env, activity) &&
        config_info::CacheMethodIds(env, activity) &&
        config_settings::CacheMethodIds(env, activity) &&
        config_settings_builder::CacheMethodIds(env, activity) &&
        throttled_exception::CacheMethodIds(env, activity))) {
    ReleaseClasses(env);
    util::Terminate(env);
    return kInitResultFailedMissingDependency;
  }

  g_app = &app;
  // Create the remote config class.
  jclass config_class = config::GetClass();
  jobject config_instance_local = env->CallStaticObjectMethod(
      config_class, config::GetMethodId(config::kGetInstance));
  FIREBASE_ASSERT(config_instance_local);
  g_remote_config_class_instance = env->NewGlobalRef(config_instance_local);
  env->DeleteLocalRef(config_instance_local);

  FutureData::Create();
  g_default_keys = new std::vector<std::string>;
  LogInfo("%s API Initialized", kApiIdentifier);
  return kInitResultSuccess;
}

namespace internal {

bool IsInitialized() { return g_app != nullptr; }

}  // namespace internal

void Terminate() {
  if (!g_app) {
    LogWarning("Remote Config already shut down");
    return;
  }
  internal::UnregisterTerminateOnDefaultAppDestroy();
  JNIEnv* env = g_app->GetJNIEnv();
  g_app = nullptr;
  env->DeleteGlobalRef(g_remote_config_class_instance);
  g_remote_config_class_instance = nullptr;
  util::CancelCallbacks(env, kApiIdentifier);
  FutureData::Destroy();

  delete g_default_keys;
  g_default_keys = nullptr;

  ReleaseClasses(env);
  util::Terminate(env);
}

#if defined(__ANDROID__)
void SetDefaults(int defaults_resource_id) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  env->CallVoidMethod(g_remote_config_class_instance,
                      config::GetMethodId(config::kSetDefaults),
                      defaults_resource_id);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    LogError("Remote Config: Unable to set defaults from resource ID %d",
             defaults_resource_id);
  }
}
#endif  // defined(__ANDROID__)

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

void SetDefaults(const ConfigKeyValue* defaults, size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jobject hash_map =
      ConfigKeyValueArrayToHashMap(env, defaults, number_of_defaults);
  env->CallVoidMethod(g_remote_config_class_instance,
                      config::GetMethodId(config::kSetDefaultsUsingMap),
                      hash_map);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    LogError("Remote Config: Unable to set defaults using map");
  } else {
    SaveDefaultKeys(defaults, number_of_defaults);
  }
  env->DeleteLocalRef(hash_map);
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
      util::CheckAndClearJniExceptions(env);
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

std::string GetConfigSetting(ConfigSetting setting) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  std::string value;
  JNIEnv* env = g_app->GetJNIEnv();
  jobject info = env->CallObjectMethod(g_remote_config_class_instance,
                                       config::GetMethodId(config::kGetInfo));
  jobject settings_obj = env->CallObjectMethod(
      info, config_info::GetMethodId(config_info::kGetConfigSettings));
  env->DeleteLocalRef(info);
  switch (setting) {
    case kConfigSettingDeveloperMode:
      value = env->CallBooleanMethod(
                  settings_obj, config_settings::GetMethodId(
                                    config_settings::kIsDeveloperModeEnabled))
                  ? "1"
                  : "0";
      break;
  }
  env->DeleteLocalRef(settings_obj);
  return value;
}

void SetDefaults(const ConfigKeyValueVariant* defaults,
                 size_t number_of_defaults) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jobject hash_map =
      ConfigKeyValueVariantArrayToHashMap(env, defaults, number_of_defaults);
  env->CallVoidMethod(g_remote_config_class_instance,
                      config::GetMethodId(config::kSetDefaultsUsingMap),
                      hash_map);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    LogError("Remote Config: Unable to set defaults using map");
  } else {
    SaveDefaultKeys(defaults, number_of_defaults);
  }
  env->DeleteLocalRef(hash_map);
}

void SetConfigSetting(ConfigSetting setting, const char* value) {
  FIREBASE_ASSERT_RETURN_VOID(internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jobject builder = env->NewObject(config_settings_builder::GetClass(),
                                   config_settings_builder::GetMethodId(
                                       config_settings_builder::kConstructor));
  switch (setting) {
    case kConfigSettingDeveloperMode: {
      jobject newbuilder;
      newbuilder = env->CallObjectMethod(
          builder,
          config_settings_builder::GetMethodId(
              config_settings_builder::kSetDeveloperModeEnabled),
          strcmp(value, "1") == 0);
      env->DeleteLocalRef(builder);
      builder = newbuilder;
      break;
    }
  }
  jobject settings_obj = env->CallObjectMethod(
      builder,
      config_settings_builder::GetMethodId(config_settings_builder::kBuild));
  env->DeleteLocalRef(builder);
  env->CallVoidMethod(g_remote_config_class_instance,
                      config::GetMethodId(config::kSetConfigSettings),
                      settings_obj);
  env->DeleteLocalRef(settings_obj);
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
    jobject value_object = GetValue(env, key, info);                        \
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
static jobject GetValue(JNIEnv* env, const char* key, ValueInfo* info) {
  jstring key_string = env->NewStringUTF(key);
  jobject config_value =
      env->CallObjectMethod(g_remote_config_class_instance,
                            config::GetMethodId(config::kGetValue), key_string);
  bool failed = CheckKeyRetrievalLogError(env, key, "<unknown>");
  env->DeleteLocalRef(key_string);
  if (info) {
    info->source = kValueSourceStaticValue;
    info->conversion_successful = false;
  }
  if (info && !failed) {
    info->source = kValueSourceDefaultValue;
    int value_source = env->CallIntMethod(
        config_value, config_value::GetMethodId(config_value::kGetSource));
    if (env->ExceptionCheck()) {
      env->ExceptionDescribe();
      env->ExceptionClear();
      failed = true;
    }
    if (!failed && value_source >= 0 &&
        value_source < sizeof(kFirebaseRemoteConfigSourceToValueSourceMap)) {
      info->source = kFirebaseRemoteConfigSourceToValueSourceMap[value_source];
    } else {
      LogError(
          "Unable to convert source (%d) of key %s to a ValueSource "
          "enumeration value.",
          value_source, key);
    }
  }
  return failed ? nullptr : config_value;
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

int64_t GetLong(const char* key) {
  FIREBASE_ASSERT_RETURN(0, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE(key, int64_t, "long", Long);
}

int64_t GetLong(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE_FROM_VALUE(key, info, int64_t, "long", Long);
}

double GetDouble(const char* key) {
  FIREBASE_ASSERT_RETURN(0.0, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE(key, double, "double", Double);
}

double GetDouble(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(0.0, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE_FROM_VALUE(key, info, double, "double", Double);
}

bool GetBoolean(const char* key) {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE(key, bool, "boolean", Boolean);
}

bool GetBoolean(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  CONFIG_GET_INTEGRAL_TYPE_FROM_VALUE(key, info, bool, "boolean", Boolean);
}

std::string GetString(const char* key) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jstring key_string = env->NewStringUTF(key);
  jobject value_string = env->CallObjectMethod(
      g_remote_config_class_instance, config::GetMethodId(config::kGetString),
      key_string);
  bool failed = CheckKeyRetrievalLogError(env, key, "string");
  env->DeleteLocalRef(key_string);
  std::string value;
  if (!failed) value = util::JniStringToString(env, value_string);
  return value;
}

std::string GetString(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::string(), internal::IsInitialized());
  std::string value;
  JNIEnv* env = g_app->GetJNIEnv();
  jobject value_object = GetValue(env, key, info);
  if (value_object) {
    jobject value_string = env->CallObjectMethod(
        value_object, config_value::GetMethodId(config_value::kAsString));
    bool failed = CheckKeyRetrievalLogError(env, key, "string");
    env->DeleteLocalRef(value_object);
    if (!failed) value = util::JniStringToString(env, value_string);
    if (info) info->conversion_successful = !failed;
  }
  return value;
}

std::vector<unsigned char> GetData(const char* key) {
  FIREBASE_ASSERT_RETURN(std::vector<unsigned char>(),
                         internal::IsInitialized());
  std::vector<unsigned char> value;
  JNIEnv* env = g_app->GetJNIEnv();
  jstring key_string = env->NewStringUTF(key);
  jobject array = env->CallObjectMethod(
      g_remote_config_class_instance,
      config::GetMethodId(config::kGetByteArray), key_string);

  bool failed = CheckKeyRetrievalLogError(env, key, "vector");
  env->DeleteLocalRef(key_string);
  if (!failed) value = util::JniByteArrayToVector(env, array);
  return value;
}

std::vector<unsigned char> GetData(const char* key, ValueInfo* info) {
  FIREBASE_ASSERT_RETURN(std::vector<unsigned char>(),
                         internal::IsInitialized());
  std::vector<unsigned char> value;
  JNIEnv* env = g_app->GetJNIEnv();
  jobject value_object = GetValue(env, key, info);
  if (value_object) {
    jobject value_array = env->CallObjectMethod(
        value_object, config_value::GetMethodId(config_value::kAsByteArray));
    bool failed = CheckKeyRetrievalLogError(env, key, "vector");
    env->DeleteLocalRef(value_object);
    if (!failed) value = util::JniByteArrayToVector(env, value_array);
    if (info) info->conversion_successful = !failed;
  }
  return value;
}

std::vector<std::string> GetKeysByPrefix(const char* prefix) {
  FIREBASE_ASSERT_RETURN(std::vector<std::string>(), internal::IsInitialized());
  std::vector<std::string> keys;
  std::set<std::string> key_set;
  JNIEnv* env = g_app->GetJNIEnv();
  jstring prefix_string = prefix ? env->NewStringUTF(prefix) : nullptr;
  jobject key_set_java = env->CallObjectMethod(
      g_remote_config_class_instance,
      config::GetMethodId(config::kGetKeysByPrefix), prefix_string);
  if (key_set_java) {
    util::JavaSetToStdStringVector(env, &keys, key_set_java);
    env->DeleteLocalRef(key_set_java);
    for (auto i = keys.begin(); i != keys.end(); ++i) {
      key_set.insert(*i);
    }
  }
  if (prefix_string) env->DeleteLocalRef(prefix_string);

  // Add any extra keys that were previously included in defaults but not
  // returned by Get*KeysByPrefix.
  size_t prefix_length = prefix ? strlen(prefix) : 0;
  for (auto i = g_default_keys->begin(); i != g_default_keys->end(); ++i) {
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

std::vector<std::string> GetKeys() { return GetKeysByPrefix(nullptr); }

Future<void> Fetch() { return Fetch(kDefaultCacheExpiration); }

// Complete a pending future.
static void FutureCallback(JNIEnv* env, jobject result,
                           util::FutureResult result_code,
                           const char* status_message, void* callback_data) {
  bool success = (result_code == util::kFutureResultSuccess);
  if (!success && result) {
    if (env->IsInstanceOf(result, throttled_exception::GetClass())) {
      g_throttled_end_time = static_cast<int64_t>(env->CallLongMethod(
          result, throttled_exception::GetMethodId(
                      throttled_exception::kGetThrottleEndTimeMillis)));
    }
  }
  // If the API has being shut down this will be nullptr.
  FutureData* future_data = FutureData::Get();
  auto* future_handle =
      reinterpret_cast<SafeFutureHandle<void>*>(callback_data);
  if (future_data) {
    future_data->api()->Complete(
        *future_handle,
        success ? kFetchFutureStatusSuccess : kFetchFutureStatusFailure);
  }
  delete future_handle;
}

Future<void> Fetch(uint64_t cache_expiration_in_seconds) {
  FIREBASE_ASSERT_RETURN(FetchLastResult(), internal::IsInitialized());
  // Create the future.
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  const auto handle = api->SafeAlloc<void>(kRemoteConfigFnFetch);
  JNIEnv* env = g_app->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      g_remote_config_class_instance, config::GetMethodId(config::kFetch),
      static_cast<jlong>(cache_expiration_in_seconds));

  util::RegisterCallbackOnTask(env, task, FutureCallback,
                               new SafeFutureHandle<void>(handle),
                               kApiIdentifier);

  env->DeleteLocalRef(task);
  return MakeFuture<void>(api, handle);
}

Future<void> FetchLastResult() {
  FIREBASE_ASSERT_RETURN(Future<void>(), internal::IsInitialized());
  ReferenceCountedFutureImpl* api = FutureData::Get()->api();
  return static_cast<const Future<void>&>(
      api->LastResult(kRemoteConfigFnFetch));
}

bool ActivateFetched() {
  FIREBASE_ASSERT_RETURN(false, internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jboolean result =
      env->CallBooleanMethod(g_remote_config_class_instance,
                             config::GetMethodId(config::kActivateFetched));
  return result != 0;
}

const ConfigInfo& GetInfo() {
  static ConfigInfo config_info;
  FIREBASE_ASSERT_RETURN(config_info, internal::IsInitialized());
  JNIEnv* env = g_app->GetJNIEnv();
  jobject info = env->CallObjectMethod(g_remote_config_class_instance,
                                       config::GetMethodId(config::kGetInfo));

  config_info.fetch_time = env->CallLongMethod(
      info, config_info::GetMethodId(config_info::kGetFetchTimeMillis));
  config_info.throttled_end_time = g_throttled_end_time;

  switch (env->CallIntMethod(
      info, config_info::GetMethodId(config_info::kGetLastFetchStatus))) {
    case -1:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_SUCCESS
      config_info.last_fetch_status = kLastFetchStatusSuccess;
      config_info.last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case 0:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_NO_FETCH_YET
      config_info.last_fetch_status = kLastFetchStatusPending;
      config_info.last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
    case 1:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_FAILURE
      config_info.last_fetch_status = kLastFetchStatusFailure;
      config_info.last_fetch_failure_reason = kFetchFailureReasonError;
      break;
    case 2:  // FirebaseRemoteConfig.LAST_FETCH_STATUS_THROTTLED
      config_info.last_fetch_status = kLastFetchStatusFailure;
      config_info.last_fetch_failure_reason = kFetchFailureReasonThrottled;
      break;
    default:
      config_info.last_fetch_status = kLastFetchStatusFailure;
      config_info.last_fetch_failure_reason = kFetchFailureReasonInvalid;
      break;
  }
  env->DeleteLocalRef(info);
  return config_info;
}

namespace internal {
RemoteConfigInternal::RemoteConfigInternal(const firebase::App& app)
    : app_(app), future_impl_(kRemoteConfigFnCount) {
  LogInfo("%s API Initialized", kApiIdentifier);
}

RemoteConfigInternal::~RemoteConfigInternal() {
  // TODO(cynthiajiang) android clean up
}

bool RemoteConfigInternal::Initialized() const{
  // TODO(cynthiajiang) implement
  return true;
}

void RemoteConfigInternal::Cleanup() {
  // TODO(cynthiajiang) implement
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitialized() {
  const auto handle =
      future_impl_.SafeAlloc<ConfigInfo>(kRemoteConfigFnEnsureInitialized);
  // TODO(cynthiajiang) implement
  return MakeFuture<ConfigInfo>(&future_impl_, handle);
}

Future<ConfigInfo> RemoteConfigInternal::EnsureInitializedLastResult() {
  return static_cast<const Future<ConfigInfo>&>(
      future_impl_.LastResult(kRemoteConfigFnEnsureInitialized));
}

Future<bool> RemoteConfigInternal::Activate() {
  const auto handle = future_impl_.SafeAlloc<bool>(kRemoteConfigFnActivate);
  // TODO(cynthiajiang) implement
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::ActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnActivate));
}

Future<bool> RemoteConfigInternal::FetchAndActivate() {
  const auto handle =
      future_impl_.SafeAlloc<bool>(kRemoteConfigFnFetchAndActivate);
  // TODO(cynthiajiang) implement
  return MakeFuture<bool>(&future_impl_, handle);
}

Future<bool> RemoteConfigInternal::FetchAndActivateLastResult() {
  return static_cast<const Future<bool>&>(
      future_impl_.LastResult(kRemoteConfigFnFetchAndActivate));
}

Future<void> RemoteConfigInternal::Fetch(uint64_t cache_expiration_in_seconds) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnFetch);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::FetchLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnFetch));
}

Future<void> RemoteConfigInternal::SetDefaults(int defaults_resource_id) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaults(
    const ConfigKeyValueVariant* defaults, size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaults(const ConfigKeyValue* defaults,
                                               size_t number_of_defaults) {
  const auto handle = future_impl_.SafeAlloc<void>(kRemoteConfigFnSetDefaults);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}

Future<void> RemoteConfigInternal::SetDefaultsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetDefaults));
}

#ifdef FIREBASE_EARLY_ACCESS_PREVIEW
Future<void> RemoteConfigInternal::SetConfigSettings(ConfigSettings settings) {
  const auto handle =
      future_impl_.SafeAlloc<void>(kRemoteConfigFnSetConfigSettings);
  // TODO(cynthiajiang) implement
  return MakeFuture<void>(&future_impl_, handle);
}
#endif  // FIREBASE_EARLY_ACCESS_PREVIEW

Future<void> RemoteConfigInternal::SetConfigSettingsLastResult() {
  return static_cast<const Future<void>&>(
      future_impl_.LastResult(kRemoteConfigFnSetConfigSettings));
}

bool RemoteConfigInternal::GetBoolean(const char* key, ValueInfo* info) {
  // TODO(cynthiajiang) implement
  return true;
}

int64_t RemoteConfigInternal::GetLong(const char* key, ValueInfo* info) {
  // TODO(cynthiajiang) implement
  return 0;
}

double RemoteConfigInternal::GetDouble(const char* key, ValueInfo* info) {
  // TODO(cynthiajiang) implement
  return 0.0f;
}

std::string RemoteConfigInternal::GetString(const char* key, ValueInfo* info) {
  // TODO(cynthiajiang) implement
  return "";
}

std::vector<unsigned char> RemoteConfigInternal::GetData(const char* key,
                                                         ValueInfo* info) {
  std::vector<unsigned char> value;
  // TODO(cynthiajiang) implement
  return value;
}

std::vector<std::string> RemoteConfigInternal::GetKeysByPrefix(
    const char* prefix) {
  std::vector<std::string> value;
  // TODO(cynthiajiang) implement
  return value;
}

std::vector<std::string> RemoteConfigInternal::GetKeys() {
  std::vector<std::string> value;
  // TODO(cynthiajiang) implement
  return value;
}

std::map<std::string, Variant> RemoteConfigInternal::GetAll() {
  std::map<std::string, Variant> value;
  // TODO(cynthiajiang) implement
  return value;
}

const ConfigInfo& RemoteConfigInternal::GetInfo() const {
  static ConfigInfo config_info;
  // TODO(cynthiajiang) implement
  return config_info;
}
}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
