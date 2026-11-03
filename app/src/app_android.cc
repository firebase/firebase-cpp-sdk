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

#include <string>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/function_registry.h"
#include "app/src/google_play_services/availability_android.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/version.h"
#include "app/src/jobject_reference.h"
#include "app/src/log.h"
#include "app/src/util.h"
#include "app/src/util_android.h"

namespace firebase {

DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

class AppInternal : public JObjectReference {
 public:
  using JObjectReference::JObjectReference;
  explicit AppInternal(const JObjectReference& ref,
                       bool created_by_cpp_in = false)
      : JObjectReference(ref), created_by_cpp(created_by_cpp_in) {}

  static AppInternal FromLocalReference(JNIEnv* env, jobject local_ref,
                                        bool created_by_cpp_in = false) {
    return AppInternal(JObjectReference::FromLocalReference(env, local_ref),
                       created_by_cpp_in);
  }

  bool created_by_cpp = false;
  bool registered_in_app_common = false;
  FunctionRegistry function_registry;
};

}  // namespace internal

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
  X(SetGaTrackingId, "setGaTrackingId",                                        \
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
    util::kMethodTypeInstance),                                                \
  X(GetGaTrackingId, "getGaTrackingId", "()Ljava/lang/String;",                \
    util::kMethodTypeInstance, util::kMethodOptional)
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

const char* const kAndroidDefaultAppName = "[DEFAULT]";
static Mutex g_methods_mutex;
int g_methods_cached_count = 0;
bool g_initialized_google_play_services = false;

bool IsPlatformDefaultAppName(const char* name) {
  return app_common::IsDefaultAppName(name) ||
         (name != nullptr && strcmp(name, kAndroidDefaultAppName) == 0);
}

void ReleaseClassesLocked(JNIEnv* env) {
  FIREBASE_ASSERT(g_methods_cached_count > 0);
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

void ReleaseClasses(JNIEnv* env) {
  MutexLock lock(g_methods_mutex);
  ReleaseClassesLocked(env);
}

// Cache FirebaseApp and FirebaseOptions method IDs.
bool CacheMethods(JNIEnv* env, jobject activity) {
  if (!env || !activity) return false;
  MutexLock lock(g_methods_mutex);
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
      ReleaseClassesLocked(env);
      return false;
    }
    if (google_play_services::Initialize(env, activity)) {
      g_initialized_google_play_services = true;
    }
  }
  return true;
}

void PlatformOptionsBuilderSetString(JNIEnv* jni_env, jobject builder,
                                     const char* value,
                                     options_builder::Method setter_id) {
  jmethodID method_id = options_builder::GetMethodId(setter_id);
  if (!method_id || !builder || !value) return;
  jstring string_value = jni_env->NewStringUTF(value);
  if (util::LogException(jni_env, kLogLevelWarning,
                         "Failed to allocate string for AppOption") ||
      !string_value) {
    return;
  }
  jobject builder_discard =
      jni_env->CallObjectMethod(builder, method_id, string_value);
  util::LogException(jni_env, kLogLevelWarning, "Failed to set AppOption");
  if (builder_discard) jni_env->DeleteLocalRef(builder_discard);
  jni_env->DeleteLocalRef(string_value);
}

// Returns a reference to a new FirebaseOptions class.
jobject AppOptionsToPlatformOptions(JNIEnv* jni_env,
                                    const AppOptions& app_options) {
  // Create a new com.google.firebase.FirebaseOptions.Builder.
  jobject builder = jni_env->NewObject(
      options_builder::GetClass(),
      options_builder::GetMethodId(options_builder::kConstructor));
  if (util::LogException(jni_env, kLogLevelError,
                         "Could not allocate FirebaseOptions.Builder") ||
      !builder) {
    return nullptr;
  }
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
  if (strlen(app_options.ga_tracking_id())) {
    PlatformOptionsBuilderSetString(jni_env, builder,
                                    app_options.ga_tracking_id(),
                                    options_builder::kSetGaTrackingId);
  }
  // Call builder.build() and release the builder.
  jobject firebase_options = jni_env->CallObjectMethod(
      builder, options_builder::GetMethodId(options_builder::kBuild));
  // An exception occurred and has been logged.
  bool failed = util::LogException(jni_env, kLogLevelError,
                                   "Could not initialize Firebase App Options");
  jni_env->DeleteLocalRef(builder);
  if (failed) {
    if (firebase_options) jni_env->DeleteLocalRef(firebase_options);
    return nullptr;
  }
  // Return the jobject. The caller is responsible for calling DeleteLocalRef().
  return firebase_options;
}

// Convert Android SDK FirebaseOptions into AppOptions for all fields that are
// not populated in AppOptions.
void PlatformOptionsToAppOptions(JNIEnv* jni_env, jobject firebase_options,
                                 AppOptions* app_options) {
  if (!firebase_options || !app_options) return;
  if (!strlen(app_options->api_key())) {
    jobject api_key = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetApiKey));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_api_key(
          util::JStringToString(jni_env, api_key).c_str());
    }
    if (api_key) jni_env->DeleteLocalRef(api_key);
  }
  if (!strlen(app_options->app_id())) {
    jobject app_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetApplicationId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_app_id(util::JStringToString(jni_env, app_id).c_str());
    }
    if (app_id) jni_env->DeleteLocalRef(app_id);
  }
  if (!strlen(app_options->database_url())) {
    jobject database_url = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetDatabaseUrl));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_database_url(
          util::JStringToString(jni_env, database_url).c_str());
    }
    if (database_url) jni_env->DeleteLocalRef(database_url);
  }
  if (!strlen(app_options->messaging_sender_id())) {
    jobject gcm_sender_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetGcmSenderId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_messaging_sender_id(
          util::JStringToString(jni_env, gcm_sender_id).c_str());
    }
    if (gcm_sender_id) jni_env->DeleteLocalRef(gcm_sender_id);
  }
  if (!strlen(app_options->storage_bucket())) {
    jobject storage_bucket = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetStorageBucket));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_storage_bucket(
          util::JStringToString(jni_env, storage_bucket).c_str());
    }
    if (storage_bucket) jni_env->DeleteLocalRef(storage_bucket);
  }
  if (!strlen(app_options->project_id())) {
    jobject project_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetProjectId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_project_id(
          util::JStringToString(jni_env, project_id).c_str());
    }
    if (project_id) jni_env->DeleteLocalRef(project_id);
  }
  if (!strlen(app_options->ga_tracking_id()) &&
      options::GetMethodId(options::kGetGaTrackingId) != nullptr) {
    jobject ga_tracking_id = jni_env->CallObjectMethod(
        firebase_options, options::GetMethodId(options::kGetGaTrackingId));
    if (!util::CheckAndClearJniExceptions(jni_env)) {
      app_options->set_ga_tracking_id(
          util::JStringToString(jni_env, ga_tracking_id).c_str());
    }
    if (ga_tracking_id) jni_env->DeleteLocalRef(ga_tracking_id);
  }
}

// Find an Android SDK FirebaseApp instance by name.
// Returns a local jobject reference if successful, nullptr otherwise.
jobject GetPlatformAppByName(JNIEnv* jni_env, const char* name) {
  if (!jni_env || !name) return nullptr;
  jobject platform_app = nullptr;
  if (IsPlatformDefaultAppName(name)) {
    platform_app = jni_env->CallStaticObjectMethod(
        app::GetClass(), app::GetMethodId(app::kGetInstance));
  } else {
    jobject name_string = jni_env->NewStringUTF(name);
    if (!jni_env->ExceptionCheck() && name_string != nullptr) {
      platform_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kGetInstanceByName),
          name_string);
      jni_env->DeleteLocalRef(name_string);
    }
  }
  if (jni_env->ExceptionCheck()) {
    // Explicitly set `platform_app` to `nullptr` if an exception was thrown
    // because on KitKat (API 19) `CallStaticObjectMethod()` may return garbage
    // instead of `NULL` if an exception was thrown, and callers of this
    // function expect `nullptr` to be returned if the app was not found.
    platform_app = nullptr;
  }
  jni_env->ExceptionClear();
  return platform_app;
}

// Get options from an Android SDK FirebaseApp instance.
void GetAppOptionsFromPlatformApp(JNIEnv* jni_env, jobject platform_app,
                                  AppOptions* app_options) {
  if (!jni_env || !platform_app || !app_options) return;
  jobject platform_options = jni_env->CallObjectMethod(
      platform_app, app::GetMethodId(app::kGetOptions));
  if (util::CheckAndClearJniExceptions(jni_env) || !platform_options) {
    return;
  }
  PlatformOptionsToAppOptions(jni_env, platform_options, app_options);
  jni_env->DeleteLocalRef(platform_options);
}

// Create an Android SDK FirebaseApp instance.
jobject CreatePlatformApp(JNIEnv* jni_env, const AppOptions& options,
                          const char* name, jobject activity) {
  jobject platform_app = nullptr;
  jobject platform_options = AppOptionsToPlatformOptions(jni_env, options);
  if (platform_options != nullptr) {
    if (IsPlatformDefaultAppName(name)) {
      platform_app = jni_env->CallStaticObjectMethod(
          app::GetClass(), app::GetMethodId(app::kInitializeDefaultApp),
          activity, platform_options);
    } else {
      jstring app_name = jni_env->NewStringUTF(name);
      if (!util::CheckAndClearJniExceptions(jni_env) && app_name != nullptr) {
        platform_app = jni_env->CallStaticObjectMethod(
            app::GetClass(), app::GetMethodId(app::kInitializeApp), activity,
            platform_options, app_name);
        jni_env->DeleteLocalRef(app_name);
      }
    }
    jni_env->DeleteLocalRef(platform_options);
    if (util::CheckAndClearJniExceptions(jni_env)) {
      platform_app = nullptr;
    }
  }
  return platform_app;
}

// Create or get an Android SDK FirebaseApp instance.
// Returns local jobject reference to the Android SDK app if successful,
// nullptr otherwise.
jobject CreateOrGetPlatformApp(JNIEnv* jni_env, const AppOptions& options,
                               const char* name, jobject activity,
                               bool* created_by_cpp_out) {
  if (created_by_cpp_out) *created_by_cpp_out = false;
  AppOptions options_with_defaults = options;
  bool populated_defaults =
      options_with_defaults.PopulateRequiredWithDefaults(jni_env, activity);
  jobject platform_app = GetPlatformAppByName(jni_env, name);
  if (platform_app) {
    AppOptions options_to_compare =
        populated_defaults ? options_with_defaults : options;
    // Package name and client ID aren't available in the platform app's options
    // so ignore them during comparison.
    options_to_compare.set_package_name("");
    options_to_compare.set_client_id("");
    // If a FirebaseApp exists, make sure it has the requested options.
    AppOptions existing_options;
    GetAppOptionsFromPlatformApp(jni_env, platform_app, &existing_options);
    if (!strlen(existing_options.ga_tracking_id())) {
      options_to_compare.set_ga_tracking_id("");
    }
    if (options_to_compare != existing_options) {
      LogWarning(
          "Existing instance of App %s found and options do not match "
          "the requested options.  Deleting %s to attempt recreation "
          "with requested options.",
          name, name);
      // Delete this FirebaseApp instance.
      jni_env->CallVoidMethod(platform_app, app::GetMethodId(app::kDelete));
      util::CheckAndClearJniExceptions(jni_env);
      jni_env->DeleteLocalRef(platform_app);
      platform_app = nullptr;
    }
  }
  if (!platform_app) {
    if (populated_defaults) {
      platform_app =
          CreatePlatformApp(jni_env, options_with_defaults, name, activity);
      if (platform_app && created_by_cpp_out) {
        *created_by_cpp_out = true;
      }
    }
  }
  return platform_app;
}

}  // namespace

AppOptions* AppOptions::LoadDefault(AppOptions* app_options, JNIEnv* jni_env,
                                    jobject activity) {
  if (!jni_env || !activity) return nullptr;
  AppOptions* result_options = nullptr;
  if (CacheMethods(jni_env, activity)) {
    // Read the options from the embedded resources.
    jobject platform_options = jni_env->CallStaticObjectMethod(
        options::GetClass(), options::GetMethodId(options::kFromResource),
        activity);
    if (util::CheckAndClearJniExceptions(jni_env) || !platform_options) {
      platform_options = nullptr;
    }

    if (platform_options) {
      // Get the package name.
      jobject package_name = jni_env->CallObjectMethod(
          activity, util::context::GetMethodId(util::context::kGetPackageName));
      if (!util::CheckAndClearJniExceptions(jni_env)) {
        // Populate the C++ structure.
        result_options = app_options ? app_options : new AppOptions();
        PlatformOptionsToAppOptions(jni_env, platform_options, result_options);
        result_options->set_package_name(
            util::JStringToString(jni_env, package_name).c_str());
      }
      if (package_name) jni_env->DeleteLocalRef(package_name);
      jni_env->DeleteLocalRef(platform_options);
    }
    ReleaseClasses(jni_env);
  }
  return result_options;
}

void App::Initialize() {}

App::~App() {
  if (internal_ && internal_->registered_in_app_common) {
    app_common::RemoveApp(this);
  }
  JNIEnv* env = GetJNIEnv();
  if (internal_) {
    if (internal_->created_by_cpp && env && **internal_ &&
        app::GetMethodId(app::kDelete)) {
      env->CallVoidMethod(**internal_, app::GetMethodId(app::kDelete));
      util::CheckAndClearJniExceptions(env);
    }
    delete internal_;
    internal_ = nullptr;
  }
  if (activity_) {
    if (env) {
      env->DeleteGlobalRef(activity_);
    }
    activity_ = nullptr;
  }
  if (env) {
    ReleaseClasses(env);
  }
}

App* App::Create(JNIEnv* jni_env, jobject activity) {
  App* app = nullptr;
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

App* App::Create(const AppOptions& options, JNIEnv* jni_env, jobject activity) {
  return Create(options, kDefaultAppName, jni_env, activity);
}

App* App::Create(const AppOptions& options, const char* name, JNIEnv* jni_env,
                 jobject activity) {
  if (!name || name[0] == '\0') {
    LogError("App::Create: App name cannot be null or empty.");
    return nullptr;
  }
  if (!jni_env || !activity) {
    LogError("App::Create: JNIEnv and Activity cannot be null.");
    return nullptr;
  }
  const char* normalized_name =
      IsPlatformDefaultAppName(name) ? kDefaultAppName : name;
  // If the app has already been initialized log an error.
  App* app = GetInstance(normalized_name);
  if (app) {
    LogError("App %s already created, options will not be applied.",
             normalized_name);
    return app;
  }
  LogDebug("Creating Firebase App %s for %s", normalized_name,
           kFirebaseVersionString);
  if (CacheMethods(jni_env, activity)) {
    // Register C++ user-agents before creating Android app.
    app_common::RegisterSdkUsage(jni_env);

    // Try to get or create a new FirebaseApp object.
    bool created_by_cpp = false;
    jobject platform_app = CreateOrGetPlatformApp(
        jni_env, options, normalized_name, activity, &created_by_cpp);
    if (platform_app) {
      app = new App();
      app->name_ = normalized_name;
      app->activity_ = jni_env->NewGlobalRef(activity);
      app->options_ = options;
      GetAppOptionsFromPlatformApp(jni_env, platform_app, &app->options_);
      if (!strlen(app->options_.package_name()) &&
          util::context::GetMethodId(util::context::kGetPackageName)) {
        jobject package_name = jni_env->CallObjectMethod(
            activity,
            util::context::GetMethodId(util::context::kGetPackageName));
        if (!util::CheckAndClearJniExceptions(jni_env)) {
          app->options_.set_package_name(
              util::JStringToString(jni_env, package_name).c_str());
        }
        if (package_name) jni_env->DeleteLocalRef(package_name);
      }
      app->internal_ = new internal::AppInternal(
          internal::AppInternal::FromLocalReference(jni_env, platform_app,
                                                    created_by_cpp));
      App* added_app = app_common::AddApp(app, &app->init_results_);
      if (added_app != app) {
        if (added_app && added_app->internal_ && created_by_cpp) {
          added_app->internal_->created_by_cpp = true;
        }
        app->internal_->created_by_cpp = false;
        delete app;
        app = added_app;
      } else {
        app->internal_->registered_in_app_common = true;
      }
    } else {
      ReleaseClasses(jni_env);
    }
  }
  return app;
}

App* App::GetInstance() { return app_common::GetDefaultApp(); }

App* App::GetInstance(const char* name) {
  if (IsPlatformDefaultAppName(name)) {
    return app_common::GetDefaultApp();
  }
  return app_common::FindAppByName(name);
}

std::vector<App*> App::GetApps() { return app_common::GetAllApps(); }

#ifdef INTERNAL_EXPERIMENTAL
internal::FunctionRegistry* App::function_registry() {
  return internal_ ? &internal_->function_registry : nullptr;
}
#endif  // INTERNAL_EXPERIMENTAL

JNIEnv* App::GetJNIEnv() const { return util::GetThreadsafeJNIEnv(java_vm()); }

static void RegisterLibraryWithVersionRegistrar(JNIEnv* env,
                                                const char* library,
                                                const char* version) {
  if (!env || !library || !version) return;
  if (!version_registrar::GetClass() ||
      !version_registrar::GetMethodId(version_registrar::kGetInstance)) {
    return;
  }
  jobject registrar = env->CallStaticObjectMethod(
      version_registrar::GetClass(),
      version_registrar::GetMethodId(version_registrar::kGetInstance));
  if (util::CheckAndClearJniExceptions(env) || !registrar) {
    return;
  }

  jstring library_string = env->NewStringUTF(library);
  if (util::CheckAndClearJniExceptions(env) || !library_string) {
    env->DeleteLocalRef(registrar);
    return;
  }

  jstring version_string = env->NewStringUTF(version);
  if (!util::CheckAndClearJniExceptions(env) && version_string) {
    env->CallVoidMethod(
        registrar,
        version_registrar::GetMethodId(version_registrar::kRegisterVersion),
        library_string, version_string);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(version_string);
  }
  env->DeleteLocalRef(library_string);
  env->DeleteLocalRef(registrar);
}

void App::RegisterLibrary(const char* library, const char* version,
                          void* platform_resource) {
  if (!library || !version) return;
  if (platform_resource) {
    // Uses platform_resource to get reference to JNIEnv* when provided.
    RegisterLibraryWithVersionRegistrar(
        reinterpret_cast<JNIEnv*>(platform_resource), library, version);
  }
  app_common::RegisterLibrary(library, version);
}

void App::SetDefaultConfigPath(const char* /* path */) {}

void App::SetDataCollectionDefaultEnabled(bool enabled) {
  if (!internal_ || !**internal_) return;
  if (!app::GetMethodId(app::kSetDataCollectionDefaultEnabled)) {
    LogError(
        "App::SetDataCollectionDefaultEnabled() is not supported by this "
        "version of the Firebase Android library. Please update your project's "
        "Firebase Android dependencies to firebase-core:16.0.0 or higher and "
        "try again.");
    return;
  }
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  env->CallVoidMethod(**internal_,
                      app::GetMethodId(app::kSetDataCollectionDefaultEnabled),
                      enabled ? JNI_TRUE : JNI_FALSE);
  util::CheckAndClearJniExceptions(env);
}

bool App::IsDataCollectionDefaultEnabled() const {
  if (!internal_ || !**internal_) return true;
  if (!app::GetMethodId(app::kIsDataCollectionDefaultEnabled)) {
    // By default, if this feature isn't supported, data collection must be
    // enabled.
    return true;
  }
  JNIEnv* env = GetJNIEnv();
  if (!env) return true;
  jboolean result = env->CallBooleanMethod(
      **internal_, app::GetMethodId(app::kIsDataCollectionDefaultEnabled));
  util::CheckAndClearJniExceptions(env);
  return result != JNI_FALSE;
}

const char* App::GetUserAgent() { return app_common::GetUserAgent(); }

JavaVM* App::java_vm() const {
  return internal_ ? internal_->java_vm() : nullptr;
}

jobject App::GetPlatformApp() const {
  return internal_ ? internal_->GetLocalRef() : nullptr;
}

void CallAfterEnsureMethodsCached(JNIEnv* env, jobject activity,
                                  std::function<void()> callback) {
  if (CacheMethods(env, activity)) {
    callback();
    ReleaseClasses(env);
  }
}

}  // namespace firebase
