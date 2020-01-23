#include "firestore/src/android/firebase_firestore_settings_android.h"

#include "firestore/src/android/util_android.h"

namespace firebase {
namespace firestore {

// clang-format off
#define SETTINGS_BUILDER_METHODS(X)                                            \
  X(Constructor, "<init>", "()V", util::kMethodTypeInstance),                  \
  X(SetHost, "setHost", "(Ljava/lang/String;)"                                 \
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;"),      \
  X(SetSslEnabled, "setSslEnabled", "(Z)"                                      \
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;"),      \
  X(SetPersistenceEnabled, "setPersistenceEnabled", "(Z)"                      \
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;"),      \
  X(SetTimestampsInSnapshotsEnabled, "setTimestampsInSnapshotsEnabled", "(Z)"  \
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;"),      \
  X(Build, "build",                                              \
    "()Lcom/google/firebase/firestore/FirebaseFirestoreSettings;")
// clang-format on

METHOD_LOOKUP_DECLARATION(settings_builder, SETTINGS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    settings_builder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreSettings$Builder",
    SETTINGS_BUILDER_METHODS)

// clang-format off
#define SETTINGS_METHODS(X)                                \
  X(GetHost, "getHost", "()Ljava/lang/String;"),           \
  X(IsSslEnabled, "isSslEnabled", "()Z"),                  \
  X(IsPersistenceEnabled, "isPersistenceEnabled", "()Z")
// clang-format on

METHOD_LOOKUP_DECLARATION(settings, SETTINGS_METHODS)
METHOD_LOOKUP_DEFINITION(
    settings,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreSettings",
    SETTINGS_METHODS)

/* static */
jobject FirebaseFirestoreSettingsInternal::SettingToJavaSetting(
    JNIEnv* env, const Settings& settings) {
  jobject builder = env->NewObject(
      settings_builder::GetClass(),
      settings_builder::GetMethodId(settings_builder::kConstructor));

  // Always set Timestamps-in-Snapshots enabled to true.
  jobject builder_timestamp = env->CallObjectMethod(
      builder,
      settings_builder::GetMethodId(
          settings_builder::kSetTimestampsInSnapshotsEnabled),
      static_cast<jboolean>(true));
  env->DeleteLocalRef(builder);
  builder = builder_timestamp;

  // Set host
  jstring host = env->NewStringUTF(settings.host().c_str());
  jobject builder_host = env->CallObjectMethod(
      builder, settings_builder::GetMethodId(settings_builder::kSetHost), host);
  env->DeleteLocalRef(builder);
  env->DeleteLocalRef(host);
  builder = builder_host;

  // Set SSL enabled
  jobject builder_ssl = env->CallObjectMethod(
      builder, settings_builder::GetMethodId(settings_builder::kSetSslEnabled),
      static_cast<jboolean>(settings.is_ssl_enabled()));
  env->DeleteLocalRef(builder);
  builder = builder_ssl;

  // Set Persistence enabled
  jobject builder_persistence = env->CallObjectMethod(
      builder,
      settings_builder::GetMethodId(settings_builder::kSetPersistenceEnabled),
      static_cast<jboolean>(settings.is_persistence_enabled()));
  env->DeleteLocalRef(builder);
  builder = builder_persistence;

  // Build
  jobject settings_jobj = env->CallObjectMethod(
      builder, settings_builder::GetMethodId(settings_builder::kBuild));
  env->DeleteLocalRef(builder);
  CheckAndClearJniExceptions(env);
  return settings_jobj;
}

/* static */
Settings FirebaseFirestoreSettingsInternal::JavaSettingToSetting(JNIEnv* env,
                                                                 jobject obj) {
  Settings result;

  // Set host
  jstring host = static_cast<jstring>(
      env->CallObjectMethod(obj, settings::GetMethodId(settings::kGetHost)));
  result.set_host(util::JStringToString(env, host));
  env->DeleteLocalRef(host);

  // Set SSL enabled
  jboolean ssl_enabled = env->CallBooleanMethod(
      obj, settings::GetMethodId(settings::kIsSslEnabled));
  result.set_ssl_enabled(ssl_enabled);

  // Set Persistence enabled
  jboolean persistence_enabled = env->CallBooleanMethod(
      obj, settings::GetMethodId(settings::kIsPersistenceEnabled));
  result.set_persistence_enabled(persistence_enabled);

  CheckAndClearJniExceptions(env);
  return result;
}

/* static */
bool FirebaseFirestoreSettingsInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  bool result = settings_builder::CacheMethodIds(env, activity) &&
                settings::CacheMethodIds(env, activity);
  util::CheckAndClearJniExceptions(env);
  return result;
}

/* static */
void FirebaseFirestoreSettingsInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  settings_builder::ReleaseClass(env);
  settings::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

}  // namespace firestore
}  // namespace firebase
