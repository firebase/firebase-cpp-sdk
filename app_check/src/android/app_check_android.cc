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

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/native_handle_table.h"
#include "app/src/no_destructor.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
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
  X(GetLimitedUseToken, "getLimitedUseAppCheckToken",                          \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
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
JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetLimitedUseToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source);

static const JNINativeMethod kNativeJniAppCheckProviderMethods[] = {
    {"nativeGetToken",
     "(JLcom/google/android/gms/tasks/TaskCompletionSource;)V",
     reinterpret_cast<void*>(JniAppCheckProvider_nativeGetToken)},
    {"nativeGetLimitedUseToken",
     "(JLcom/google/android/gms/tasks/TaskCompletionSource;)V",
     reinterpret_cast<void*>(JniAppCheckProvider_nativeGetLimitedUseToken)}};

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

static std::atomic<AppCheckProviderFactory*> g_provider_factory{nullptr};

struct AppCheckAndroidInitState {
  std::mutex mutex;
  int initialized_count = 0;
};

AppCheckAndroidInitState& GetAppCheckInitState() {
  static ::firebase::internal::NoDestructor<AppCheckAndroidInitState> state;
  return *state;
}

::firebase::internal::NativeHandleTable<::firebase::internal::AppState>&
GetAppStateHandleTable() {
  static ::firebase::internal::NoDestructor<
      ::firebase::internal::NativeHandleTable<::firebase::internal::AppState>>
      table;
  return *table;
}

::firebase::internal::NativeHandleTable<AppCheckProvider>&
GetProviderHandleTable() {
  static ::firebase::internal::NoDestructor<
      ::firebase::internal::NativeHandleTable<AppCheckProvider>>
      table;
  return *table;
}

::firebase::internal::NativeHandleTable<AppCheckInternal>&
GetListenerHandleTable() {
  static ::firebase::internal::NoDestructor<
      ::firebase::internal::NativeHandleTable<AppCheckInternal>>
      table;
  return *table;
}

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
  FutureDataHandle(std::weak_ptr<::firebase::internal::AppState> _weak_state,
                   ReferenceCountedFutureImpl* _future_api,
                   const SafeFutureHandle<AppCheckToken>& _future_handle)
      : weak_state(std::move(_weak_state)),
        future_api(_future_api),
        future_handle(_future_handle) {}
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<AppCheckToken> future_handle;
};

void TokenResultCallback(JNIEnv* env, jobject result,
                         util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  std::unique_ptr<FutureDataHandle> data_handle(
      reinterpret_cast<FutureDataHandle*>(callback_data));
  auto locked_state = data_handle->weak_state.lock();
  if (!locked_state) {
    return;
  }
  int result_error_code = kAppCheckErrorNone;
  AppCheckToken result_token;
  bool success = (result_code == util::kFutureResultSuccess);
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because Android AppCheck doesnt have
    // an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  // Complete the provided future.
  data_handle->future_api->CompleteWithResult(data_handle->future_handle,
                                              result_error_code, status_message,
                                              result_token);
}
}  // anonymous namespace

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* env, jobject clazz, jlong c_factory, jlong c_app) {
  auto locked_state =
      GetAppStateHandleTable().Lock(static_cast<int64_t>(c_app));
  if (!locked_state) return 0;
  App cpp_app = locked_state->MakeAppHandle();
  if (!cpp_app.is_valid()) return 0;
  AppCheckProviderFactory* provider_factory =
      reinterpret_cast<AppCheckProviderFactory*>(c_factory);
  if (!provider_factory) return 0;
  AppCheckProvider* provider = provider_factory->CreateProvider(cpp_app);
  if (!provider) return 0;
  // Alias the provider pointer to AppState's control block so locking the
  // handle keeps both AppState and the AppState-owned provider service alive.
  std::shared_ptr<AppCheckProvider> aliasing_provider(locked_state, provider);
  return static_cast<jlong>(
      GetProviderHandleTable().Register(aliasing_provider));
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  auto locked_provider =
      GetProviderHandleTable().Lock(static_cast<int64_t>(c_provider));
  if (!locked_provider) return;
  std::weak_ptr<AppCheckProvider> weak_provider = locked_provider;

  // Create GlobalReferences to the provider and task. These references will be
  // deleted in the completion callback.
  jobject j_provider_global = env->NewGlobalRef(j_provider);
  jobject task_completion_source_global =
      env->NewGlobalRef(task_completion_source);

  // Defines a C++ callback method to call
  // JniAppCheckProvider.HandleGetTokenResult with the resulting token
  auto token_callback{
      [weak_provider, j_provider_global, task_completion_source_global](
          firebase::app_check::AppCheckToken token, int error_code,
          const std::string& error_message) {
        JNIEnv* cb_env = firebase::util::GetJNIEnvFromApp();
        if (!cb_env) return;
        // Pin AppState/AppCheckInternal while invoking cached jni_provider
        // methods.
        auto pinned = weak_provider.lock();
        if (pinned) {
          jstring error_string = cb_env->NewStringUTF(error_message.c_str());
          jstring token_string = cb_env->NewStringUTF(token.token.c_str());
          cb_env->CallVoidMethod(
              j_provider_global,
              jni_provider::GetMethodId(jni_provider::kHandleGetTokenResult),
              task_completion_source_global, token_string,
              token.expire_time_millis, error_code, error_string);
          FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(cb_env));
          cb_env->DeleteLocalRef(token_string);
          cb_env->DeleteLocalRef(error_string);
        }
        cb_env->DeleteGlobalRef(j_provider_global);
        cb_env->DeleteGlobalRef(task_completion_source_global);
      }};
  locked_provider->GetToken(token_callback);
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetLimitedUseToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  auto locked_provider =
      GetProviderHandleTable().Lock(static_cast<int64_t>(c_provider));
  if (!locked_provider) return;
  std::weak_ptr<AppCheckProvider> weak_provider = locked_provider;

  // Create GlobalReferences to the provider and task. These references will be
  // deleted in the completion callback.
  jobject j_provider_global = env->NewGlobalRef(j_provider);
  jobject task_completion_source_global =
      env->NewGlobalRef(task_completion_source);

  // Defines a C++ callback method to call
  // JniAppCheckProvider.HandleGetTokenResult with the resulting token
  auto token_callback{
      [weak_provider, j_provider_global, task_completion_source_global](
          firebase::app_check::AppCheckToken token, int error_code,
          const std::string& error_message) {
        JNIEnv* cb_env = firebase::util::GetJNIEnvFromApp();
        if (!cb_env) return;
        auto pinned = weak_provider.lock();
        if (pinned) {
          jstring error_string = cb_env->NewStringUTF(error_message.c_str());
          jstring token_string = cb_env->NewStringUTF(token.token.c_str());
          cb_env->CallVoidMethod(
              j_provider_global,
              jni_provider::GetMethodId(jni_provider::kHandleGetTokenResult),
              task_completion_source_global, token_string,
              token.expire_time_millis, error_code, error_string);
          FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(cb_env));
          cb_env->DeleteLocalRef(token_string);
          cb_env->DeleteLocalRef(error_string);
        }
        cb_env->DeleteGlobalRef(j_provider_global);
        cb_env->DeleteGlobalRef(task_completion_source_global);
      }};
  locked_provider->GetLimitedUseToken(token_callback);
}

JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject clazz, jlong c_app_check, jobject token) {
  auto app_check_internal =
      GetListenerHandleTable().Lock(static_cast<int64_t>(c_app_check));
  if (!app_check_internal) return;
  AppCheckToken cpp_token = CppTokenFromAndroidToken(env, token);
  app_check_internal->NotifyTokenChanged(cpp_token);
}

AppCheckInternal::AppCheckInternal(::firebase::internal::AppState& app_state)
    : app_state_(app_state) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);

  App app_handle = app_state_.MakeAppHandle();
  // Cache the JNI method ids so we only have to look them up by name once.
  JNIEnv* env = app_handle.GetJNIEnv();
  {
    AppCheckAndroidInitState& init_state = GetAppCheckInitState();
    std::lock_guard<std::mutex> lock(init_state.mutex);
    if (init_state.initialized_count == 0) {
      jobject activity = app_state_.activity();
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
          init_state.initialized_count++;
          initialized_classes_ = true;
        }
      }
    } else {
      init_state.initialized_count++;
      initialized_classes_ = true;
    }
  }

  static const char* kApiIdentifier = "AppCheck";
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);

  if (!initialized_classes_) {
    return;
  }

  // Create the FirebaseAppCheck class in Java.
  jobject platform_app = app_handle.GetPlatformApp();
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

    auto self_state = app_state_.shared_from_this();
    app_state_handle_id_ = GetAppStateHandleTable().Register(self_state);
    std::shared_ptr<AppCheckInternal> aliasing_self(self_state, this);
    listener_handle_id_ = GetListenerHandleTable().Register(aliasing_self);

    // Install the AppCheckProviderFactory for this instance of AppCheck.
    AppCheckProviderFactory* factory = g_provider_factory.load();
    if (factory) {
      // Create a Java ProviderFactory to wrap the C++ ProviderFactory
      // Since InstallAppCheckProviderFactory is done for a single instance of
      // AppCheck, this factory will only be used for this App.
      jobject j_factory = env->NewObject(
          jni_provider_factory::GetClass(),
          jni_provider_factory::GetMethodId(jni_provider_factory::kConstructor),
          reinterpret_cast<jlong>(factory),
          static_cast<jlong>(app_state_handle_id_));
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
      // Install the Java ProviderFactory in the java AppCheck class.
      env->CallVoidMethod(
          app_check_impl_,
          app_check::GetMethodId(app_check::kInstallAppCheckProviderFactory),
          j_factory);
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
      env->DeleteLocalRef(j_factory);
    }

    // Add a token-changed listener and pass an opaque handle ID to Java.
    jobject j_listener =
        env->NewObject(jni_app_check_listener::GetClass(),
                       jni_app_check_listener::GetMethodId(
                           jni_app_check_listener::kConstructor),
                       static_cast<jlong>(listener_handle_id_));
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
  GetListenerHandleTable().Unregister(listener_handle_id_);
  listener_handle_id_ = 0;
  GetAppStateHandleTable().Unregister(app_state_handle_id_);
  app_state_handle_id_ = 0;

  future_manager().ReleaseFutureApi(this);
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  if (env != nullptr) {
    util::CancelCallbacks(env, jni_task_id_.c_str());

    if (j_app_check_listener_ != nullptr) {
      env->CallVoidMethod(
          app_check_impl_,
          app_check::GetMethodId(app_check::kRemoveAppCheckListener),
          j_app_check_listener_);
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
      env->DeleteGlobalRef(j_app_check_listener_);
      j_app_check_listener_ = nullptr;
    }
    if (app_check_impl_ != nullptr) {
      env->CallVoidMethod(app_check_impl_,
                          default_app_check_impl::GetMethodId(
                              default_app_check_impl::kResetAppCheckState));
      FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));

      env->DeleteGlobalRef(app_check_impl_);
      app_check_impl_ = nullptr;
    }
  }
  {
    MutexLock lock(listeners_mutex_);
    listeners_.clear();
  }

  if (initialized_classes_) {
    initialized_classes_ = false;
    AppCheckAndroidInitState& init_state = GetAppCheckInitState();
    std::lock_guard<std::mutex> lock(init_state.mutex);
    FIREBASE_ASSERT(init_state.initialized_count > 0);
    init_state.initialized_count--;
    if (init_state.initialized_count == 0 && env != nullptr) {
      ReleaseClasses(env);
      util::Terminate(env);
    }
  }
}

::firebase::App AppCheckInternal::app() const {
  return app_state_.TryMakeAppHandle();
}

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  // Store the C++ factory in a static variable.
  // Whenever an instance of AppCheck is created, it will read this variable
  // and install the factory as it is initialized.
  g_provider_factory.store(factory);
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  env->CallVoidMethod(
      app_check_impl_,
      app_check::GetMethodId(app_check::kSetTokenAutoRefreshEnabled),
      static_cast<jboolean>(is_token_auto_refresh_enabled));
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  jobject j_task = env->CallObjectMethod(
      app_check_impl_, app_check::GetMethodId(app_check::kGetToken),
      static_cast<jboolean>(force_refresh));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    auto data_handle =
        new FutureDataHandle(app_state_.weak_from_this(), future(), handle);
    util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 jni_task_id_.c_str());
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

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  JNIEnv* env = firebase::util::GetJNIEnvFromApp();
  auto handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetLimitedUseAppCheckToken);
  jobject j_task = env->CallObjectMethod(
      app_check_impl_, app_check::GetMethodId(app_check::kGetLimitedUseToken));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty()) {
    auto data_handle =
        new FutureDataHandle(app_state_.weak_from_this(), future(), handle);
    util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 jni_task_id_.c_str());
  } else {
    AppCheckToken empty_token;
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, error.c_str(),
                                 empty_token);
  }
  env->DeleteLocalRef(j_task);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
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
  std::vector<AppCheckListener*> listeners_copy;
  {
    MutexLock lock(listeners_mutex_);
    listeners_copy = listeners_;
  }
  for (AppCheckListener* listener : listeners_copy) {
    listener->OnAppCheckTokenChanged(token);
  }
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
