// Copyright 2022 Google LLC
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

#include "app_check/src/android/app_check_android.h"

#include <string>
#include <vector>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "app_check/app_check_resources.h"
#include "app_check/src/android/common_android.h"
#include "app_check/src/android/debug_provider_android.h"
#include "app_check/src/android/play_integrity_provider_android.h"
#include "app_check/src/common/common.h"

namespace firebase {
namespace app_check {
namespace internal {

// Used to setup the cache of AppCheck class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define APP_CHECK_METHODS(X)                                                   \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/FirebaseAppCheck;",                         \
    util::kMethodTypeStatic),                                                  \
  X(InstallAppCheckProviderFactory, "installAppCheckProviderFactory",          \
    "(Lcom/google/firebase/appcheck/AppCheckProviderFactory;)V"),              \
  X(SetTokenAutoRefreshEnabled, "setTokenAutoRefreshEnabled",                  \
    "(Z)V"),                                                                   \
  X(GetToken, "getAppCheckToken",                                              \
    "(Z)Lcom/google/android/gms/tasks/Task;"),                                 \
  X(AddAppCheckListener, "addAppCheckListener",                                \
    "(Lcom/google/firebase/appcheck/FirebaseAppCheck$AppCheckListener;)V"),    \
  X(RemoveAppCheckListener, "removeAppCheckListener",                          \
    "(Lcom/google/firebase/appcheck/FirebaseAppCheck$AppCheckListener;)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(app_check, APP_CHECK_METHODS)
METHOD_LOOKUP_DEFINITION(app_check,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/appcheck/FirebaseAppCheck",
                         APP_CHECK_METHODS)

// clang-format off
#define DEFAULT_APP_CHECK_IMPL_METHODS(X)               \
  X(ResetAppCheckState, "resetAppCheckState", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(default_app_check_impl,
                          DEFAULT_APP_CHECK_IMPL_METHODS)
METHOD_LOOKUP_DEFINITION(
    default_app_check_impl,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/appcheck/internal/DefaultFirebaseAppCheck",
    DEFAULT_APP_CHECK_IMPL_METHODS)

// clang-format off
#define JNI_APP_CHECK_PROVIDER_FACTORY_METHODS(X)                              \
  X(Constructor, "<init>", "(JJ)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(jni_provider_factory,
                          JNI_APP_CHECK_PROVIDER_FACTORY_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_provider_factory,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckProviderFactory",
    JNI_APP_CHECK_PROVIDER_FACTORY_METHODS)

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* env, jobject clazz, jlong c_factory, jlong c_app);

static const JNINativeMethod kNativeJniAppCheckProviderFactoryMethods[] = {
    {"nativeCreateProvider", "(JJ)J",
     reinterpret_cast<void*>(JniAppCheckProviderFactory_nativeCreateProvider)},
};

// clang-format off
#define JNI_APP_CHECK_PROVIDER_METHODS(X)                                      \
  X(Constructor, "<init>", "(J)V"),                                             \
  X(HandleGetTokenResult, "handleGetTokenResult",                              \
  "(Lcom/google/android/gms/tasks/TaskCompletionSource;Ljava/lang/String;JILjava/lang/String;)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(jni_provider, JNI_APP_CHECK_PROVIDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_provider,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckProvider",
    JNI_APP_CHECK_PROVIDER_METHODS)

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source);

static const JNINativeMethod kNativeJniAppCheckProviderMethods[] = {
    {"nativeGetToken",
     "(JLcom/google/android/gms/tasks/TaskCompletionSource;)V",
     reinterpret_cast<void*>(JniAppCheckProvider_nativeGetToken)},
};

// clang-format off
#define JNI_APP_CHECK_LISTENER_METHODS(X)                              \
  X(Constructor, "<init>", "(J)V")
// clang-format on

METHOD_LOOKUP_DECLARATION(jni_app_check_listener,
                          JNI_APP_CHECK_LISTENER_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_app_check_listener,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckListener",
    JNI_APP_CHECK_LISTENER_METHODS)

JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject clazz, jlong c_app_check, jobject token);

static const JNINativeMethod kNativeJniAppCheckListenerMethods[] = {
    {"nativeOnAppCheckTokenChanged",
     "(JLcom/google/firebase/appcheck/AppCheckToken;)V",
     reinterpret_cast<void*>(JniAppCheckListener_nativeOnAppCheckTokenChanged)},
};

static const char* kApiIdentifier = "AppCheck";

static AppCheckProviderFactory* g_provider_factory = nullptr;
static int g_initialized_count = 0;

bool CacheAppCheckMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  // Cache the JniAppCheckProviderFactory class and register the native callback
  // methods.
  if (!(jni_provider_factory::CacheClassFromFiles(env, activity,
                                                  &embedded_files) &&
        jni_provider_factory::CacheMethodIds(env, activity) &&
        jni_provider_factory::RegisterNatives(
            env, kNativeJniAppCheckProviderFactoryMethods,
            FIREBASE_ARRAYSIZE(kNativeJniAppCheckProviderFactoryMethods)))) {
    return false;
  }
  // Cache the JniAppCheckProvider class and register the native callback
  // methods.
  if (!(jni_provider::CacheClassFromFiles(env, activity, &embedded_files) &&
        jni_provider::CacheMethodIds(env, activity) &&
        jni_provider::RegisterNatives(
            env, kNativeJniAppCheckProviderMethods,
            FIREBASE_ARRAYSIZE(kNativeJniAppCheckProviderMethods)))) {
    return false;
  }
  // Cache the JniAppCheckListener class and register the native callback
  // methods.
  if (!(jni_app_check_listener::CacheClassFromFiles(env, activity,
                                                    &embedded_files) &&
        jni_app_check_listener::CacheMethodIds(env, activity) &&
        jni_app_check_listener::RegisterNatives(
            env, kNativeJniAppCheckListenerMethods,
            FIREBASE_ARRAYSIZE(kNativeJniAppCheckListenerMethods)))) {
    return false;
  }
  return app_check::CacheMethodIds(env, activity) &&
         default_app_check_impl::CacheMethodIds(env, activity);
}

void ReleaseAppCheckClasses(JNIEnv* env) {
  app_check::ReleaseClass(env);
  default_app_check_impl::ReleaseClass(env);
  jni_provider_factory::ReleaseClass(env);
  jni_provider::ReleaseClass(env);
  jni_app_check_listener::ReleaseClass(env);
}

// Release cached Java classes.
static void ReleaseClasses(JNIEnv* env) {
  ReleaseAppCheckClasses(env);
  ReleaseCommonAndroidClasses(env);
  ReleaseDebugProviderClasses(env);
  ReleasePlayIntegrityProviderClasses(env);
}

// Anonymous namespace for code that is only relevant to this file.
namespace {
struct FutureDataHandle {
  FutureDataHandle(ReferenceCountedFutureImpl* _future_api,
                   const SafeFutureHandle<AppCheckToken>& _future_handle)
      : future_api(_future_api), future_handle(_future_handle) {}
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<AppCheckToken> future_handle;
};

void TokenResultCallback(JNIEnv* env, jobject result,
                         util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  int result_error_code = kAppCheckErrorNone;
  AppCheckToken result_token;
  bool success = (result_code == util::kFutureResultSuccess);
  std::string result_value = "";
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because Android AppCheck doesnt have
    // an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  // Complete the provided future.
  auto* data_handle = reinterpret_cast<FutureDataHandle*>(callback_data);
  data_handle->future_api->CompleteWithResult(data_handle->future_handle,
                                              result_error_code, status_message,
                                              result_token);
  delete data_handle;
}
}  // anonymous namespace

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* env, jobject clazz, jlong c_factory, jlong c_app) {
  App* cpp_app = reinterpret_cast<App*>(c_app);
  AppCheckProviderFactory* provider_factory =
      reinterpret_cast<AppCheckProviderFactory*>(c_factory);
  AppCheckProvider* provider = provider_factory->CreateProvider(cpp_app);
  return reinterpret_cast<jlong>(provider);
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  // Create GlobalReferences to the provider and task. These references will be
  // deleted in the completion callback.
  jobject j_provider_global = env->NewGlobalRef(j_provider);
  jobject task_completion_source_global =
      env->NewGlobalRef(task_completion_source);

  // Defines a C++ callback method to call
  // JniAppCheckProvider.HandleGetTokenResult with the resulting token
  auto token_callback{[j_provider_global, task_completion_source_global](
                          firebase::app_check::AppCheckToken token,
                          int error_code, const std::string& error_message) {
    // util::GetJNIEnvFromApp returns a threadsafe instance of JNIEnv.
    JNIEnv* env = firebase::util::GetJNIEnvFromApp();
    jstring error_string = env->NewStringUTF(error_message.c_str());
    jstring token_string = env->NewStringUTF(token.token.c_str());
    env->CallVoidMethod(
        j_provider_global,
        jni_provider::GetMethodId(jni_provider::kHandleGetTokenResult),
        task_completion_source_global, token_string, token.expire_time_millis,
        error_code, error_string);
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    env->DeleteLocalRef(token_string);
    env->DeleteLocalRef(error_string);
    env->DeleteGlobalRef(j_provider_global);
    env->DeleteGlobalRef(task_completion_source_global);
  }};
  AppCheckProvider* provider = reinterpret_cast<AppCheckProvider*>(c_provider);
  provider->GetToken(token_callback);
}

JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject clazz, jlong c_app_check, jobject token) {
  auto app_check_internal = reinterpret_cast<AppCheckInternal*>(c_app_check);
  AppCheckToken cpp_token = CppTokenFromAndroidToken(env, token);
  app_check_internal->NotifyTokenChanged(cpp_token);
}

AppCheckInternal::AppCheckInternal(App* app) : app_(app) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);

  // Cache the JNI method ids so we only have to look them up by name once.
  JNIEnv* env = app->GetJNIEnv();
  if (!g_initialized_count) {
    jobject activity = app->activity();
    if (util::Initialize(env, activity)) {
      // Cache embedded files and load embedded classes.
      const std::vector<firebase::internal::EmbeddedFile> embedded_files =
          util::CacheEmbeddedFiles(
              env, activity,
              firebase::internal::EmbeddedFile::ToVector(
                  firebase_app_check::app_check_resources_filename,
                  firebase_app_check::app_check_resources_data,
                  firebase_app_check::app_check_resources_size));
      // Terminate if unable to cache main classes
      if (!(CacheAppCheckMethodIds(env, activity, embedded_files) &&
            CacheCommonAndroidMethodIds(env, activity))) {
        ReleaseClasses(env);
        util::Terminate(env);
      } else {
        // Each provider is optional as a user may or may not use it.
        CacheDebugProviderMethodIds(env, activity, embedded_files);
        CachePlayIntegrityProviderMethodIds(env, activity);
        g_initialized_count++;
      }
    }
  } else {
    g_initialized_count++;
  }

  // Create the FirebaseAppCheck class in Java.
  jobject platform_app = app->GetPlatformApp();
  jobject j_app_check_local = env->CallStaticObjectMethod(
      app_check::GetClass(), app_check::GetMethodId(app_check::kGetInstance),
      platform_app);
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
  env->DeleteLocalRef(platform_app);

  // Create a global reference for the app check instance. It will be
  // released in the AppCheckInternal destructor.
  if (j_app_check_local != nullptr) {
    app_check_impl_ = env->NewGlobalRef(j_app_check_local);
    env->DeleteLocalRef(j_app_check_local);

    // Install the AppCheckProviderFactory for this instance of AppCheck.
    if (g_provider_factory) {
      // Create a Java ProviderFactory to wrap the C++ ProviderFactory
      // Since InstallAppCheckProviderFactory is done for a single instance of
      // AppCheck, this factory will only be used for this App.
      jobject j_factory = env->NewObject(
          jni_provider_factory::GetClass(),
          jni_provider_factory::GetMethodId(jni_provider_factory::kConstructor),
          reinterpret_cast<jlong>(g_provider_factory),
          reinterpret_cast<jlong>(app));
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
      // Install the Java ProviderFactory in the java AppCheck class.
      env->CallVoidMethod(
          app_check_impl_,
          app_check::GetMethodId(app_check::kInstallAppCheckProviderFactory),
          j_factory);
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
      env->DeleteLocalRef(j_factory);
    }

    // Add a token-changed listener and give it a pointer to the C++ listeners.
    jobject j_listener =
        env->NewObject(jni_app_check_listener::GetClass(),
                       jni_app_check_listener::GetMethodId(
                           jni_app_check_listener::kConstructor),
                       reinterpret_cast<jlong>(this));
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    env->CallVoidMethod(app_check_impl_,
                        app_check::GetMethodId(app_check::kAddAppCheckListener),
                        j_listener);
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    j_app_check_listener_ = env->NewGlobalRef(j_listener);
    env->DeleteLocalRef(j_listener);
  } else {
    app_check_impl_ = nullptr;
    j_app_check_listener_ = nullptr;
  }
}

AppCheckInternal::~AppCheckInternal() {
  future_manager().ReleaseFutureApi(this);
  JNIEnv* env = app_->GetJNIEnv();
  app_ = nullptr;
  listeners_.clear();

  if (j_app_check_listener_ != nullptr) {
    env->CallVoidMethod(
        app_check_impl_,
        app_check::GetMethodId(app_check::kRemoveAppCheckListener),
        j_app_check_listener_);
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
    env->DeleteGlobalRef(j_app_check_listener_);
  }
  if (app_check_impl_ != nullptr) {
    // The Android App Check library holds onto the provider,
    // which can be a problem if it tries to call up to C++
    // after being deleted. So we use a hidden function meant
    // for testing purposes to clear out the App Check state,
    // to prevent this. Note, this assumes that the java object
    // is a DefaultFirebaseAppCheck (instead of a FirebaseAppCheck)
    // which is currently true, but may not be in the future.
    // We will have to rely on tests to detect if this changes.
    env->CallVoidMethod(app_check_impl_,
                        default_app_check_impl::GetMethodId(
                            default_app_check_impl::kResetAppCheckState));
    FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));

    env->DeleteGlobalRef(app_check_impl_);
  }

  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_initialized_count == 0) {
    util::CancelCallbacks(env, kApiIdentifier);
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  // Store the C++ factory in a static variable.
  // Whenever an instance of AppCheck is created, it will read this variable
  // and install the factory as it is initialized.
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(
      app_check_impl_,
      app_check::GetMethodId(app_check::kSetTokenAutoRefreshEnabled),
      static_cast<jboolean>(is_token_auto_refresh_enabled));
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  JNIEnv* env = app_->GetJNIEnv();
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  jobject j_task = env->CallObjectMethod(
      app_check_impl_, app_check::GetMethodId(app_check::kGetToken),
      static_cast<jboolean>(force_refresh));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    auto data_handle = new FutureDataHandle(future(), handle);
    util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 kApiIdentifier);
  } else {
    AppCheckToken empty_token;
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, error.c_str(),
                                 empty_token);
  }
  env->DeleteLocalRef(j_task);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  MutexLock lock(listeners_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it == listeners_.end()) {
    listeners_.push_back(listener);
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  MutexLock lock(listeners_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
}

void AppCheckInternal::NotifyTokenChanged(AppCheckToken token) {
  MutexLock lock(listeners_mutex_);
  for (AppCheckListener* listener : listeners_) {
    listener->OnAppCheckTokenChanged(token);
  }
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
