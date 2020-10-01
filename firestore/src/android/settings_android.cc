#include "firestore/src/android/settings_android.h"

#include "firestore/src/jni/env.h"
#include "firestore/src/jni/loader.h"

namespace firebase {
namespace firestore {
namespace {

using jni::Constructor;
using jni::Env;
using jni::Local;
using jni::Method;
using jni::Object;
using jni::String;

// class FirebaseFirestoreSettings.Builder
constexpr char kSettingsBuilderClass[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreSettings$Builder";
Constructor<Object> kNewBuilder("()V");
Method<Object> kSetHost(
    "setHost",
    "(Ljava/lang/String;)"
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;");
Method<Object> kSetSslEnabled(
    "setSslEnabled",
    "(Z)"
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;");
Method<Object> kSetPersistenceEnabled(
    "setPersistenceEnabled",
    "(Z)"
    "Lcom/google/firebase/firestore/FirebaseFirestoreSettings$Builder;");
Method<SettingsInternal> kBuild(
    "build", "()Lcom/google/firebase/firestore/FirebaseFirestoreSettings;");

// class FirebaseFirestoreSettings
constexpr char kSettingsClass[] = PROGUARD_KEEP_CLASS
    "com/google/firebase/firestore/FirebaseFirestoreSettings";
Method<String> kGetHost("getHost", "()Ljava/lang/String;");
Method<bool> kIsSslEnabled("isSslEnabled", "()Z");
Method<bool> kIsPersistenceEnabled("isPersistenceEnabled", "()Z");

}  // namespace

void SettingsInternal::Initialize(jni::Loader& loader) {
  loader.LoadClass(kSettingsBuilderClass, kNewBuilder, kSetHost, kSetSslEnabled,
                   kSetPersistenceEnabled, kBuild);

  loader.LoadClass(kSettingsClass, kGetHost, kIsSslEnabled,
                   kIsPersistenceEnabled);
}

Local<SettingsInternal> SettingsInternal::Create(Env& env,
                                                 const Settings& settings) {
  Local<Object> builder = env.New(kNewBuilder);

  Local<String> host = env.NewStringUtf(settings.host());
  builder = env.Call(builder, kSetHost, host);

  builder = env.Call(builder, kSetSslEnabled, settings.is_ssl_enabled());

  builder = env.Call(builder, kSetPersistenceEnabled,
                     settings.is_persistence_enabled());

  return env.Call(builder, kBuild);
}

Settings SettingsInternal::ToPublic(Env& env) const {
  Settings result;

  // Set host
  Local<String> host = env.Call(*this, kGetHost);
  result.set_host(host.ToString(env));

  // Set SSL enabled
  bool ssl_enabled = env.Call(*this, kIsSslEnabled);
  result.set_ssl_enabled(ssl_enabled);

  // Set Persistence enabled
  bool persistence_enabled = env.Call(*this, kIsPersistenceEnabled);
  result.set_persistence_enabled(persistence_enabled);

  return result;
}

}  // namespace firestore
}  // namespace firebase
