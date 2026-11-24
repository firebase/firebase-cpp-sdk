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

#include "app/src/app_android.h"

#include <jni.h>
#include <string.h>

#include <cassert>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/app_state.h"
#include "app/src/assert.h"
#include "app/src/google_play_services/availability_android.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jobject_reference.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app/src/util_android.h"

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

JOBJECT_REFERENCE(AppInternal);

}  // namespace internal

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
  X(GetName, "getName", "()Ljava/lang/String;", util::kMethodTypeInstance),    \
  X(GetOptions, "getOptions", "()Lcom/google/firebase/FirebaseOptions;",       \
    util::kMethodTypeInstance),                                                \
  X(Delete, "delete", "()V", util::kMethodTypeInstance),                       \
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

// clang-format off
#define GLOBAL_LIBRARY_VERSION_REGISTAR_METHODS(X)                             \
  X(GetInstance, "getInstance",                                                \
    "()Lcom/google/firebase/platforminfo/GlobalLibraryVersionRegistrar;",      \
    util::kMethodTypeStatic),                                                  \
  X(RegisterVersion, "registerVersion",                                        \
    "(Ljava/lang/String;Ljava/lang/String;)V",                                 \
    util::kMethodTypeInstance),                                                \
  X(GetRegisteredVersions, "getRegisteredVersions", "()Ljava/util/Set;",       \
    util::kMethodTypeInstance)
// clang-format on

METHOD_LOOKUP_DECLARATION(version_registrar,
                          GLOBAL_LIBRARY_VERSION_REGISTAR_METHODS)
METHOD_LOOKUP_DEFINITION(
    version_registrar,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/platforminfo/GlobalLibraryVersionRegistrar",
    GLOBAL_LIBRARY_VERSION_REGISTAR_METHODS)

namespace {

static int g_methods_cached_count = 0;
static bool g_initialized_google_play_services = false;

void ReleaseClasses(JNIEnv* env);

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
          version_registrar::CacheMethodIds(env, activity))) {
      ReleaseClasses(env);
      return false;
    }
    if (google_play_services::Initialize(env, activity)) {
      g_initialized_google_play_services = true;
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
    version_registrar::ReleaseClass(env);
    if (g_initialized_google_play_services) {
      google_play_services::Terminate(env);
      g_initialized_google_play_services = false;
    }
    util::Terminate(env);
  }
}

static void PlatformOptionsBuilderSetString(JNIEnv* jni_env, jobject builder,
                                            const char* value,
                                            options_builder::Method setter_id) {
  jstring string_value = jni_env->NewStringUTF(value);
  jobject builder_discard = jni_env->CallObjectMethod(
      builder, options_builder::GetMethodId(setter_id), string_value);
  util::LogException(jni_env, kLogLevelWarning, "Failed to set AppOption");
  if (builder_discard) jni_env->DeleteLocalRef(builder_discard);
  jni_env->DeleteLocalRef(string_value);
}

static jobject AppOptionsToPlatformOptions(JNIEnv* jni_env,
                                           const AppOptions& app_options) {
  jobject builder = jni_env->NewObject(
      options_builder::GetClass(),
      options_builder::GetMethodId(options_builder::kConstructor));
  PlatformOptionsBuilderSetString(jni_env, builder, app_options.api_key(),
                                  options_builder::kSetApiKey);

  if (strlen(app_options.database_url())) {
    PlatformOptionsBuilderSetString(jni_env, builder,
                                    app_options.database_url(),
                                    options_builder::kSetDatabaseUrl);
  }
  if (strlen(app_options.app_id())) {
    PlatformOptionsBuilderSetString(jni_env, builder, app_options.app_id(),
                                    options_builder::kSetApplicationId);
  }
  if (strlen(app_options.messaging_sender_id())) {
    PlatformOptionsBuilderSetString(jni_env, builder,
                                    app_options.messaging_sender_id(),
                                    options_builder::kSetGcmSenderId);
  }
  if (strlen(app_options.storage_bucket())) {
    PlatformOptionsBuilderSetString(jni_env, builder,
                                    app_options.storage_bucket(),
                                    options_builder::kSetStorageBucket);
  }
  if (strlen(app_options.project_id())) {
    PlatformOptionsBuilderSetString(jni_env, builder, app_options.project_id(),
                                    options_builder::kSetProjectId);
  }
  jobject firebase_options = jni_env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  bool failed = util::LogException(jni_env, kLogLevelError,
                                   "Could not initialize Firebase App Options");
  jni_env->DeleteLocalRef(builder);
  return failed ? nullptr : firebase_options;
}

static void PlatformOptionsToAppOptions(JNIEnv* jni_env,
                                        jobject firebase_options,
                                        AppOptions* app_options) {
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
}

static jobject GetPlatformAppByName(JNIEnv* jni_env, const char* name) {
  jobject platform_app;
  if (app_common::IsDefaultAppName(name)) {
    platform_app = jni_env->CallStaticObjectMethod(
        app::GetClass(), app::GetMethodId(app::kGetInstance));
  } else {
    jobject name_string = jni_env->NewStringUTF(name);
    platform_app = jni_env->CallStaticObjectMethod(
        app::GetClass(), app::GetMethodId(app::kGetInstanceByName),
        name_string);
    jni_env->DeleteLocalRef(name_string);
  }
  if (jni_env->ExceptionCheck()) {
    platform_app = NULL;  // NOLINT
  }
  jni_env->ExceptionClear();
  return platform_app;
}

static void GetAppOptionsFromPlatformApp(JNIEnv* jni_env, jobject platform_app,
                                         AppOptions* app_options) {
  jobject platform_options = jni_env->CallObjectMethod(
      platform_app, app::GetMethodId(app::kGetOptions));
  util::CheckAndClearJniExceptions(jni_env);
  assert(platform_options);
  PlatformOptionsToAppOptions(jni_env, platform_options, app_options);
  jni_env->DeleteLocalRef(platform_options);
}

static jobject CreatePlatformApp(JNIEnv* jni_env, const AppOptions& options,
                                 const char* name, jobject activity) {
  jobject platform_app = nullptr;
  jobject platform_options = AppOptionsToPlatformOptions(jni_env, options);
  if (platform_options != nullptr) {
    if (app_common::IsDefaultAppName(name)) {
      platform_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kInitializeDefaultApp),
          activity, platform_options);
    } else {
      jstring app_name = jni_env->NewStringUTF(name);
      platform_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kInitializeApp), activity,
          platform_options, app_name);
      jni_env->DeleteLocalRef(app_name);
    }
    jni_env->DeleteLocalRef(platform_options);
    util::CheckAndClearJniExceptions(jni_env);
  }
  return platform_app;
}

static jobject CreateOrGetPlatformApp(JNIEnv* jni_env,
                                      const AppOptions& options,
                                      const char* name, jobject activity) {
  jobject platform_app = GetPlatformAppByName(jni_env, name);
  if (platform_app) {
    AppOptions options_to_compare = options;
    options_to_compare.set_package_name("");
    AppOptions existing_options;
    GetAppOptionsFromPlatformApp(jni_env, platform_app, &existing_options);
    if (options_to_compare != existing_options) {
      LogWarning(
          "Existing instance of App %s found and options do not match "
          "the requested options. Deleting %s to attempt recreation "
          "with requested options.",
          name, name);
      jni_env->CallVoidMethod(platform_app, app::GetMethodId(app::kDelete));
      util::CheckAndClearJniExceptions(jni_env);
      jni_env->DeleteLocalRef(platform_app);
      platform_app = nullptr;
    }
  }
  if (!platform_app) {
    AppOptions options_with_defaults = options;
    if (options_with_defaults.PopulateRequiredWithDefaults(jni_env, activity)) {
      platform_app =
          CreatePlatformApp(jni_env, options_with_defaults, name, activity);
    }
  }
  return platform_app;
}

}  // namespace

namespace internal {

void AppState::DestroyPlatformData() {
  JNIEnv* env = nullptr;
  if (platform_data_) {
    env = util::GetThreadsafeJNIEnv(platform_data_->java_vm());
    delete platform_data_;
    platform_data_ = nullptr;
  }
  if (!env) {
    env = util::GetJNIEnvFromApp();
  }
  if (activity_ && env) {
    env->DeleteGlobalRef(activity_);
    activity_ = nullptr;
  }
  if (owns_cached_classes_ && env) {
    owns_cached_classes_ = false;
    ReleaseClasses(env);
  }
}

}  // namespace internal

AppOptions* AppOptions::LoadDefault(AppOptions* app_options, JNIEnv* jni_env,
                                    jobject activity) {
  if (CacheMethods(jni_env, activity)) {
    jobject platform_options = jni_env->CallStaticObjectMethod(
        options::GetClass(), options::GetMethodId(options::kFromResource),
        activity);
    if (jni_env->ExceptionCheck() || !platform_options) {
      jni_env->ExceptionClear();
      platform_options = nullptr;
      app_options = nullptr;
    }

    if (platform_options) {
      jobject package_name = jni_env->CallObjectMethod(
          activity, util::context::GetMethodId(util::context::kGetPackageName));
      if (util::CheckAndClearJniExceptions(jni_env)) {
        app_options = nullptr;
      } else {
        app_options = app_options ? app_options : new AppOptions();
        PlatformOptionsToAppOptions(jni_env, platform_options, app_options);
        app_options->set_package_name(
            util::JniStringToString(jni_env, package_name).c_str());
      }
      jni_env->DeleteLocalRef(platform_options);
    }
    ReleaseClasses(jni_env);
  }
  return app_options;
}

App App::Create(JNIEnv* jni_env, jobject activity) {
  App app;
  if (CacheMethods(jni_env, activity)) {
    AppOptions options;
    if (AppOptions::LoadDefault(&options, jni_env, activity)) {
      app = Create(options, jni_env, activity);
    } else {
      LogError(
          "Failed to read Firebase options from the app's resources. "
          "Either make sure google-services.json is included in your "
          "build or specify options explicitly.");
    }
    ReleaseClasses(jni_env);
  }
  return app;
}

App App::Create(const AppOptions& options, JNIEnv* jni_env, jobject activity) {
  return Create(options, kDefaultAppName, jni_env, activity);
}

App App::Create(const AppOptions& options, std::string_view name,
                JNIEnv* jni_env, jobject activity) {
  std::string app_name(name);
  LogDebug("Creating Firebase App %s for %s", app_name.c_str(),
           kFirebaseVersionString);
  if (!CacheMethods(jni_env, activity)) {
    return App();
  }

  app_common::RegisterSdkUsage(jni_env);

  jobject platform_app =
      CreateOrGetPlatformApp(jni_env, options, app_name.c_str(), activity);
  if (!platform_app) {
    ReleaseClasses(jni_env);
    return App();
  }

  AppOptions resolved_options;
  GetAppOptionsFromPlatformApp(jni_env, platform_app, &resolved_options);

  auto state = std::make_shared<internal::AppState>(
      std::move(app_name), std::move(resolved_options));
  state->set_activity(jni_env->NewGlobalRef(activity));
  state->set_owns_cached_classes(true);
  state->set_platform_data(new internal::AppInternal(
      internal::AppInternal::FromLocalReference(jni_env, platform_app)));

  App app = state->MakeAppHandle();
  AppCallback::NotifyAllAppCreated(app, &state->mutable_init_results());
  return app;
}

App App::CreateRegistered(JNIEnv* jni_env, jobject activity) {
  App app;
  if (CacheMethods(jni_env, activity)) {
    AppOptions options;
    if (AppOptions::LoadDefault(&options, jni_env, activity)) {
      app = CreateRegistered(options, jni_env, activity);
    } else {
      LogError(
          "Failed to read Firebase options from the app's resources. "
          "Either make sure google-services.json is included in your "
          "build or specify options explicitly.");
    }
    ReleaseClasses(jni_env);
  }
  return app;
}

App App::CreateRegistered(const AppOptions& options, JNIEnv* jni_env,
                          jobject activity) {
  return CreateRegistered(options, kDefaultAppName, jni_env, activity);
}

App App::CreateRegistered(const AppOptions& options, std::string_view name,
                          JNIEnv* jni_env, jobject activity) {
  if (auto existing = GetInstance(name)) {
    LogError("App %.*s already registered, options will not be applied.",
             static_cast<int>(name.size()), name.data());
    return App();
  }
  App app = Create(options, name, jni_env, activity);
  if (!app || !app.Register()) {
    return App();
  }
  return app;
}

JNIEnv* App::GetJNIEnv() const {
  JavaVM* vm = java_vm();
  return vm ? util::GetThreadsafeJNIEnv(vm) : nullptr;
}

jobject App::activity() const { return state_ ? state_->activity() : nullptr; }

static void RegisterLibraryWithVersionRegistrar(JNIEnv* env,
                                                const char* library,
                                                const char* version) {
  jobject registrar = env->CallStaticObjectMethod(
      version_registrar::GetClass(),
      version_registrar::GetMethodId(version_registrar::kGetInstance));
  util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(registrar != nullptr);

  jstring library_string = env->NewStringUTF(library);
  jstring version_string = env->NewStringUTF(version);
  env->CallVoidMethod(
      registrar,
      version_registrar::GetMethodId(version_registrar::kRegisterVersion),
      library_string, version_string);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(version_string);
  env->DeleteLocalRef(library_string);
  env->DeleteLocalRef(registrar);
}

void App::RegisterLibrary(const char* library, const char* version,
                          void* platform_resource) {
  FIREBASE_ASSERT(platform_resource);
  RegisterLibraryWithVersionRegistrar(
      reinterpret_cast<JNIEnv*>(platform_resource), library, version);
  app_common::RegisterLibrary(library, version);
}

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  if (!state_ || !state_->platform_data()) return;
  if (!app::GetMethodId(app::kSetDataCollectionDefaultEnabled)) {
    LogError(
        "App::SetDataCollectionDefaultEnabled() is not supported by this "
        "version of the Firebase Android library. Please update your project's "
        "Firebase Android dependencies to firebase-core:16.0.0 or higher and "
        "try again.");
    return;
  }
  JNIEnv* env = GetJNIEnv();
  env->CallVoidMethod(**state_->platform_data(),
                      app::GetMethodId(app::kSetDataCollectionDefaultEnabled),
                      enabled ? JNI_TRUE : JNI_FALSE);
  util::CheckAndClearJniExceptions(env);
}

bool App::IsDataCollectionDefaultEnabled() const {
  if (!state_ || !state_->platform_data()) return true;
  if (!app::GetMethodId(app::kIsDataCollectionDefaultEnabled)) {
    return true;
  }
  JNIEnv* env = GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      **state_->platform_data(),
      app::GetMethodId(app::kIsDataCollectionDefaultEnabled));
  util::CheckAndClearJniExceptions(env);
  return result != JNI_FALSE;
}

JavaVM* App::java_vm() const {
  return (state_ && state_->platform_data())
             ? state_->platform_data()->java_vm()
             : nullptr;
}

jobject App::GetPlatformApp() const {
  return (state_ && state_->platform_data())
             ? state_->platform_data()->GetLocalRef()
             : nullptr;
}

void CallAfterEnsureMethodsCached(JNIEnv* env, jobject activity,
                                  std::function<void()> callback) {
  if (CacheMethods(env, activity)) {
    callback();
    ReleaseClasses(env);
  }
}

}  // namespace firebase
