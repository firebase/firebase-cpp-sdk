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

#include "app/src/include/firebase/app.h"

#include <jni.h>
#include <string.h>
#include <string>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/google_play_services/availability_android.h"
#include "app/src/include/firebase/version.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app/src/util_android.h"

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

// Namespace and class for FirebaseApp.
// java/com/google/android/gmscore/*/client/firebase-common/src/com/google/\
// firebase/FirebaseApp.java
// clang-format off
#define FIREBASE_APP_METHODS(X)                                                \
  X(InitializeApp, "initializeApp",                                            \
    "(Landroid/content/Context;Lcom/google/firebase/FirebaseOptions;"          \
    "Ljava/lang/String;)Lcom/google/firebase/FirebaseApp;",                    \
    util::kMethodTypeStatic),                                                  \
  X(InitializeDefaultApp, "initializeApp",                                     \
    "(Landroid/content/Context;Lcom/google/firebase/FirebaseOptions;)"         \
    "Lcom/google/firebase/FirebaseApp;",                                       \
    util::kMethodTypeStatic),                                                  \
  X(GetInstance, "getInstance", "()Lcom/google/firebase/FirebaseApp;",         \
    util::kMethodTypeStatic),                                                  \
  X(GetInstanceByName, "getInstance",                                          \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseApp;",                   \
    util::kMethodTypeStatic),                                                  \
  X(IsDataCollectionDefaultEnabled, "isDataCollectionDefaultEnabled",          \
    "()Z", util::kMethodTypeInstance, util::kMethodOptional),                  \
  X(SetDataCollectionDefaultEnabled, "setDataCollectionDefaultEnabled",        \
    "(Z)V", util::kMethodTypeInstance, util::kMethodOptional)
// clang-format on

METHOD_LOOKUP_DECLARATION(app, FIREBASE_APP_METHODS)
METHOD_LOOKUP_DEFINITION(app,
                         PROGUARD_KEEP_CLASS "com/google/firebase/FirebaseApp",
                         FIREBASE_APP_METHODS)

// clang-format off
#define FIREBASE_OPTIONS_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V", util::kMethodTypeInstance),                  \
  X(SetApiKey, "setApiKey",                                                    \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance),                                                \
  X(SetDatabaseUrl, "setDatabaseUrl",                                          \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance),                                                \
  X(SetApplicationId, "setApplicationId",                                      \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance),                                                \
  X(SetGcmSenderId, "setGcmSenderId",                                          \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance),                                                \
  X(SetStorageBucket, "setStorageBucket",                                      \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance),                                                \
  X(SetProjectId, "setProjectId",                                              \
    "(Ljava/lang/String;)Lcom/google/firebase/FirebaseOptions$Builder;",       \
    util::kMethodTypeInstance, util::kMethodOptional),                         \
  X(Build, "build",                                                            \
    "()Lcom/google/firebase/FirebaseOptions;")
// clang-format on

METHOD_LOOKUP_DECLARATION(options_builder, FIREBASE_OPTIONS_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(options_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseOptions$Builder",
                         FIREBASE_OPTIONS_BUILDER_METHODS)

// clang-format off
#define FIREBASE_OPTIONS_METHODS(X)                                            \
  X(FromResource, "fromResource",                                              \
    "(Landroid/content/Context;)Lcom/google/firebase/FirebaseOptions;",        \
    util::kMethodTypeStatic),                                                  \
  X(GetApiKey, "getApiKey", "()Ljava/lang/String;",                            \
    util::kMethodTypeInstance),                                                \
  X(GetApplicationId, "getApplicationId", "()Ljava/lang/String;",              \
    util::kMethodTypeInstance),                                                \
  X(GetDatabaseUrl, "getDatabaseUrl", "()Ljava/lang/String;",                  \
    util::kMethodTypeInstance),                                                \
  X(GetGcmSenderId, "getGcmSenderId", "()Ljava/lang/String;",                  \
    util::kMethodTypeInstance),                                                \
  X(GetStorageBucket, "getStorageBucket", "()Ljava/lang/String;",              \
    util::kMethodTypeInstance),                                                \
  X(GetProjectId, "getProjectId", "()Ljava/lang/String;",                      \
    util::kMethodTypeInstance)
// clang-format on

METHOD_LOOKUP_DECLARATION(options, FIREBASE_OPTIONS_METHODS)
METHOD_LOOKUP_DEFINITION(options,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseOptions",
                         FIREBASE_OPTIONS_METHODS)

const char* kDefaultAppName = "__FIRAPP_DEFAULT";

namespace {

static int g_methods_cached_count = 0;

void ReleaseClasses(JNIEnv* env);

// Cache FirebaseApp and FirebaseOptions method IDs.
bool CacheMethods(JNIEnv* env, jobject activity) {
  bool initialize = g_methods_cached_count == 0;
  g_methods_cached_count++;
  if (initialize) {
    if (!util::Initialize(env, activity)) {
      g_methods_cached_count = 0;
      return false;
    }
    if (!(app::CacheMethodIds(env, activity) &&
          options_builder::CacheMethodIds(env, activity) &&
          options::CacheMethodIds(env, activity) &&
          google_play_services::Initialize(env, activity))) {
      ReleaseClasses(env);
      return false;
    }
  }
  return true;
}

void ReleaseClasses(JNIEnv* env) {
  FIREBASE_ASSERT(g_methods_cached_count);
  g_methods_cached_count--;
  if (g_methods_cached_count == 0) {
    app::ReleaseClass(env);
    options_builder::ReleaseClass(env);
    options::ReleaseClass(env);
    google_play_services::Terminate(env);
    util::Terminate(env);
  }
}

static void FirebaseOptionsBuilderSetString(JNIEnv* jni_env, jobject builder,
                                            const char* value,
                                            options_builder::Method setter_id) {
  jstring string_value = jni_env->NewStringUTF(value);
  jobject builder_discard = jni_env->CallObjectMethod(
      builder, options_builder::GetMethodId(setter_id), string_value);
  util::LogException(jni_env, kLogLevelWarning, "Failed to set AppOption");
  if (builder_discard) jni_env->DeleteLocalRef(builder_discard);
  jni_env->DeleteLocalRef(string_value);
}

// Returns a reference to a new FirebaseOptions class.
static jobject CreateFirebaseOptions(JNIEnv* jni_env,
                                     const AppOptions& app_options) {
  // Create a new android.net.Uri.Builder.
  jobject builder;
  builder = jni_env->NewObject(
      options_builder::GetClass(),
      options_builder::GetMethodId(options_builder::kConstructor));
  FirebaseOptionsBuilderSetString(jni_env, builder, app_options.api_key(),
                                  options_builder::kSetApiKey);

  if (strlen(app_options.database_url())) {
    FirebaseOptionsBuilderSetString(jni_env, builder,
                                    app_options.database_url(),
                                    options_builder::kSetDatabaseUrl);
  }
  if (strlen(app_options.app_id())) {
    FirebaseOptionsBuilderSetString(jni_env, builder, app_options.app_id(),
                                    options_builder::kSetApplicationId);
  }
  if (strlen(app_options.messaging_sender_id())) {
    FirebaseOptionsBuilderSetString(jni_env, builder,
                                    app_options.messaging_sender_id(),
                                    options_builder::kSetGcmSenderId);
  }
  if (strlen(app_options.storage_bucket())) {
    FirebaseOptionsBuilderSetString(jni_env, builder,
                                    app_options.storage_bucket(),
                                    options_builder::kSetStorageBucket);
  }
  if (strlen(app_options.project_id())) {
    FirebaseOptionsBuilderSetString(jni_env, builder, app_options.project_id(),
                                    options_builder::kSetProjectId);
  }
  // Call builder.build() and release the builder.
  jobject firebase_options = jni_env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  // An exception occurred and has been logged.
  bool failed = util::LogException(jni_env, kLogLevelError,
                                   "Could not initialize Firebase App Options");
  jni_env->DeleteLocalRef(builder);
  // Return the jobject. The caller is responsible for calling DeleteLocalRef().
  return failed ? nullptr : firebase_options;
}

// Read FirebaseOptions from the application's resources.
static bool ReadOptionsFromResources(JNIEnv* jni_env, jobject activity,
                                     AppOptions* app_options) {
  jobject firebase_options = jni_env->CallStaticObjectMethod(
      options::GetClass(), options::GetMethodId(options::kFromResource),
      activity);
  bool exception = jni_env->ExceptionCheck();
  if (!firebase_options || exception) {
    firebase_options = nullptr;
    if (exception) {
      jni_env->ExceptionClear();
    }
    FIREBASE_ASSERT_MESSAGE_RETURN(
        false, strlen(app_options->app_id()) && strlen(app_options->api_key()),
        "Failed to read Firebase options from the app's resources.  "
        "You'll need to either at least set App ID and "
        "API key or include google-services.json your app's "
        "resources.");
  }
  if (!strlen(app_options->api_key())) {
    jobject api_key = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetApiKey));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_api_key(
          util::JniStringToString(jni_env, api_key).c_str());
    }
  }
  if (!strlen(app_options->app_id())) {
    jobject app_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetApplicationId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_app_id(util::JniStringToString(jni_env, app_id).c_str());
    }
  }
  if (!strlen(app_options->database_url())) {
    jobject database_url = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetDatabaseUrl));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_database_url(
          util::JniStringToString(jni_env, database_url).c_str());
    }
  }
  if (!strlen(app_options->messaging_sender_id())) {
    jobject gcm_sender_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetGcmSenderId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_messaging_sender_id(
          util::JniStringToString(jni_env, gcm_sender_id).c_str());
    }
  }
  if (!strlen(app_options->storage_bucket())) {
    jobject storage_bucket = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetStorageBucket));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_storage_bucket(
          util::JniStringToString(jni_env, storage_bucket).c_str());
    }
  }
  if (!strlen(app_options->project_id())) {
    jobject project_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetProjectId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_project_id(
          util::JniStringToString(jni_env, project_id).c_str());
    }
  }
  jni_env->DeleteLocalRef(firebase_options);
  return true;
}

}  // namespace

App::App() : activity_(nullptr), data_(nullptr) {
  LogDebug("Creating Firebase App for %s", kFirebaseVersionString);
}

App::~App() {
  app_common::RemoveApp(this);
  JNIEnv* env = GetJNIEnv();
  if (data_) {
    env->DeleteGlobalRef(reinterpret_cast<jobject>(data_));
    data_ = nullptr;
  }
  if (activity_) {
    env->DeleteGlobalRef(reinterpret_cast<jobject>(activity_));
    activity_ = nullptr;
  }
  ReleaseClasses(env);
}

App* App::Create(const AppOptions& options, JNIEnv* jni_env, jobject activity) {
  return Create(options, kDefaultAppName, jni_env, activity);
}

App* App::Create(const AppOptions& options, const char* name, JNIEnv* jni_env,
                 jobject activity) {
  // If the app has already been initialize log an error.
  App* existing_app = GetInstance(name);
  if (existing_app) {
    LogError("firebase::App %s already created, options will not be applied.",
             name);
    return existing_app;
  }
  if (!CacheMethods(jni_env, activity)) return nullptr;

  App* new_app = new App();
  new_app->options_ = options;
  new_app->name_ = name;
  new_app->activity_ = jni_env->NewGlobalRef(activity);
  jint result = jni_env->GetJavaVM(&new_app->java_vm_);
  FIREBASE_ASSERT(result == JNI_OK);

  bool default_app = strcmp(kDefaultAppName, name) == 0;
  std::string package_name = util::GetPackageName(jni_env, activity);
  name = default_app ? package_name.c_str() : name;
  LogInfo("Firebase App initializing app %s (default %d).", name,
          default_app ? 1 : 0);
  if (default_app && app::GetMethodId(app::kInitializeDefaultApp)) {
    jobject java_app = nullptr;
    AppOptions options_with_defaults = options;
    if (ReadOptionsFromResources(jni_env, activity, &options_with_defaults)) {
      // If options can be loaded via resources then it's possible to infer the
      // default app has already been created.
      if (strlen(options.app_id()) || strlen(options.api_key()) ||
          strlen(options.messaging_sender_id())) {
        LogWarning(
            "AppOptions will be ignored as the default app has "
            "already been initialized.  To disable automatic app "
            "initialization remove or rename resources derived from "
            "google-services.json.");
      }
      java_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kGetInstance));
      if (util::CheckAndClearJniExceptions(jni_env)) java_app = nullptr;
    } else {
      // Try creating the app using the specified options.
      jobject java_options =
          CreateFirebaseOptions(jni_env, options_with_defaults);
      if (java_options != nullptr) {
        java_app = jni_env->CallStaticObjectMethod(
            app::GetClass(), app::GetMethodId(app::kInitializeDefaultApp),
            activity, java_options);
        if (util::CheckAndClearJniExceptions(jni_env)) java_app = nullptr;
        jni_env->DeleteLocalRef(java_options);
      }
    }
    if (!java_app) {
      delete new_app;
      new_app = nullptr;
    }
    FIREBASE_ASSERT_MESSAGE_RETURN(
        nullptr, new_app, "Failed to initialize the default Firebase App.");
    new_app->options_ = options_with_defaults;
    new_app->data_ = static_cast<void*>(jni_env->NewGlobalRef(java_app));
    LogDebug("App local ref (%x), global ref (%x).",
             static_cast<int>(reinterpret_cast<intptr_t>(java_app)),
             static_cast<int>(reinterpret_cast<intptr_t>(new_app->data_)));
    FIREBASE_ASSERT(new_app->data_ != nullptr);
    jni_env->DeleteLocalRef(java_app);
  } else {
    AppOptions options_with_defaults = options;
    ReadOptionsFromResources(jni_env, activity, &options_with_defaults);
    jobject java_app = nullptr;
    jobject java_options =
        CreateFirebaseOptions(jni_env, options_with_defaults);
    if (java_options != nullptr) {
      jobject java_name = jni_env->NewStringUTF(name);
      java_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kInitializeApp), activity,
          java_options, java_name);
      if (util::CheckAndClearJniExceptions(jni_env)) java_app = nullptr;
      jni_env->DeleteLocalRef(java_name);
      jni_env->DeleteLocalRef(java_options);
    }
    if (!java_app) {
      delete new_app;
      return nullptr;
    }
    new_app->options_ = options_with_defaults;
    new_app->data_ = static_cast<void*>(jni_env->NewGlobalRef(java_app));
    LogDebug("App local ref (%x), global ref (%x).",
             static_cast<int>(reinterpret_cast<intptr_t>(java_app)),
             static_cast<int>(reinterpret_cast<intptr_t>(new_app->data_)));
    FIREBASE_ASSERT(new_app->data_ != nullptr);
    jni_env->DeleteLocalRef(java_app);
  }
  return app_common::AddApp(new_app, default_app, &new_app->init_results_);
}

App* App::GetInstance() { return app_common::GetDefaultApp(); }

App* App::GetInstance(const char* name) {
  return app_common::FindAppByName(name);
}

JNIEnv* App::GetJNIEnv() const { return util::GetThreadsafeJNIEnv(java_vm()); }

void App::RegisterLibrary(const char* library, const char* version) {
  app_common::RegisterLibrary(library, version);
}

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  if (!app::GetMethodId(app::kSetDataCollectionDefaultEnabled)) {
    LogError(
        "App::SetDataCollectionDefaultEnabled() is not supported by this "
        "version of the Firebase Android library. Please update your project's "
        "Firebase Android dependencies to firebase-core:16.0.0 or higher and "
        "try again.");
    return;
  }
  JNIEnv* env = GetJNIEnv();
  jobject obj = reinterpret_cast<jobject>(data_);
  env->CallVoidMethod(obj,
                      app::GetMethodId(app::kSetDataCollectionDefaultEnabled),
                      enabled ? JNI_TRUE : JNI_FALSE);
  util::CheckAndClearJniExceptions(env);
}

bool App::IsDataCollectionDefaultEnabled() const {
  if (!app::GetMethodId(app::kIsDataCollectionDefaultEnabled)) {
    // By default, if this feature isn't supported, data collection must be
    // enabled.
    return true;
  }
  JNIEnv* env = GetJNIEnv();
  jobject obj = reinterpret_cast<jobject>(data_);
  jboolean result = env->CallBooleanMethod(
      obj, app::GetMethodId(app::kIsDataCollectionDefaultEnabled));
  util::CheckAndClearJniExceptions(env);
  return result != JNI_FALSE;
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

}  // namespace firebase
