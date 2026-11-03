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
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
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
  if (jni_provider_factory::GetClass() != nullptr) {
    env->UnregisterNatives(jni_provider_factory::GetClass());
  }
  if (jni_provider::GetClass() != nullptr) {
    env->UnregisterNatives(jni_provider::GetClass());
  }
  if (jni_app_check_listener::GetClass() != nullptr) {
    env->UnregisterNatives(jni_app_check_listener::GetClass());
  }
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

// Recursive mutex protecting g_provider_factory, g_initialized_count,
// active AppCheckInternal instances, and JNI class lifecycle.
Mutex& GetRegistryMutex() {
  static auto* mutex = new Mutex();
  return *mutex;
}

std::set<AppCheckInternal*>& GetActiveInstances() {
  static auto* instances = new std::set<AppCheckInternal*>();
  return *instances;
}

std::set<AppCheckInternal*>& GetInitializedInstances() {
  static auto* instances = new std::set<AppCheckInternal*>();
  return *instances;
}

AppCheckProviderFactory* g_provider_factory = nullptr;
int g_initialized_count = 0;

bool RetainJniClassesLocked() {
  if (g_initialized_count <= 0) {
    return false;
  }
  ++g_initialized_count;
  return true;
}

void ReleaseJniClassesLocked(JNIEnv* env) {
  FIREBASE_ASSERT(g_initialized_count > 0);
  --g_initialized_count;
  if (g_initialized_count == 0) {
    if (env == nullptr) {
      env = firebase::util::GetJNIEnvFromApp();
    }
    if (env != nullptr) {
      ReleaseClasses(env);
      util::Terminate(env);
    }
  }
}

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
  if (success && result) {
    result_token = CppTokenFromAndroidToken(env, result);
  } else {
    // Using error code unknown for failure because Android AppCheck doesnt have
    // an error code enum
    result_error_code = kAppCheckErrorUnknown;
  }
  // Complete the provided future.
  auto* data_handle = reinterpret_cast<FutureDataHandle*>(callback_data);
  if (data_handle != nullptr) {
    data_handle->future_api->CompleteWithResult(
        data_handle->future_handle, result_error_code,
        status_message ? status_message : "", result_token);
    delete data_handle;
  }
}

// Shared single-completion state for JNI token callbacks so global references
// and cached JNI classes are retained until the callback completes or is
// dropped, and deleted exactly once.
struct JniTokenCallbackState {
  jobject j_provider_global{nullptr};
  jobject task_completion_source_global{nullptr};
  std::atomic<bool> completed{false};
  bool retained_jni_classes{false};

  JniTokenCallbackState(JNIEnv* env, jobject j_provider,
                        jobject task_completion_source) {
    {
      MutexLock lock(GetRegistryMutex());
      retained_jni_classes = RetainJniClassesLocked();
    }
    if (env != nullptr) {
      if (j_provider != nullptr) {
        j_provider_global = env->NewGlobalRef(j_provider);
        if (util::CheckAndClearJniExceptions(env)) {
          j_provider_global = nullptr;
        }
      }
      if (task_completion_source != nullptr) {
        task_completion_source_global =
            env->NewGlobalRef(task_completion_source);
        if (util::CheckAndClearJniExceptions(env)) {
          task_completion_source_global = nullptr;
        }
      }
    }
  }

  void InvokeHandleGetTokenResult(JNIEnv* env, const std::string& token_str,
                                  jlong expire_time_millis, jint error_code,
                                  const std::string& error_message) {
    if (env == nullptr || j_provider_global == nullptr ||
        task_completion_source_global == nullptr) {
      return;
    }
    jmethodID handle_result_mid =
        jni_provider::GetMethodId(jni_provider::kHandleGetTokenResult);
    if (handle_result_mid == nullptr) {
      return;
    }
    jstring error_string = env->NewStringUTF(error_message.c_str());
    if (util::CheckAndClearJniExceptions(env)) {
      error_string = nullptr;
    }
    jstring token_string = env->NewStringUTF(token_str.c_str());
    if (util::CheckAndClearJniExceptions(env)) {
      if (error_string != nullptr) {
        env->DeleteLocalRef(error_string);
      }
      return;
    }
    env->CallVoidMethod(j_provider_global, handle_result_mid,
                        task_completion_source_global, token_string,
                        expire_time_millis, error_code, error_string);
    util::CheckAndClearJniExceptions(env);
    if (token_string != nullptr) {
      env->DeleteLocalRef(token_string);
    }
    if (error_string != nullptr) {
      env->DeleteLocalRef(error_string);
    }
  }

  void Complete(const AppCheckToken& token, int error_code,
                const std::string& error_message) {
    if (completed.exchange(true)) {
      return;
    }
    JNIEnv* env = firebase::util::GetJNIEnvFromApp();
    InvokeHandleGetTokenResult(env, token.token,
                               static_cast<jlong>(token.expire_time_millis),
                               static_cast<jint>(error_code), error_message);
    ReleaseRefs(env);
  }

  void ReleaseRefs(JNIEnv* env) {
    if (env == nullptr) {
      env = firebase::util::GetJNIEnvFromApp();
    }
    if (env != nullptr) {
      if (j_provider_global != nullptr) {
        env->DeleteGlobalRef(j_provider_global);
        j_provider_global = nullptr;
      }
      if (task_completion_source_global != nullptr) {
        env->DeleteGlobalRef(task_completion_source_global);
        task_completion_source_global = nullptr;
      }
    }
    if (retained_jni_classes) {
      MutexLock lock(GetRegistryMutex());
      retained_jni_classes = false;
      ReleaseJniClassesLocked(env);
    }
  }

  ~JniTokenCallbackState() {
    JNIEnv* env = nullptr;
    if (!completed.exchange(true)) {
      env = firebase::util::GetJNIEnvFromApp();
      InvokeHandleGetTokenResult(env, "", static_cast<jlong>(0),
                                 static_cast<jint>(kAppCheckErrorUnknown),
                                 "AppCheckProvider callback was dropped.");
    }
    ReleaseRefs(env);
  }
};
}  // anonymous namespace

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* env, jobject clazz, jlong c_factory, jlong c_app) {
  (void)env;
  (void)clazz;
  App* cpp_app = reinterpret_cast<App*>(c_app);
  AppCheckProviderFactory* provider_factory =
      reinterpret_cast<AppCheckProviderFactory*>(c_factory);
  if (provider_factory == nullptr || cpp_app == nullptr) {
    return 0;
  }
  {
    MutexLock lock(GetRegistryMutex());
    if (provider_factory != g_provider_factory) {
      return 0;
    }
    bool app_active = false;
    for (AppCheckInternal* instance : GetActiveInstances()) {
      if (instance != nullptr && instance->app() == cpp_app) {
        app_active = true;
        break;
      }
    }
    if (!app_active) {
      return 0;
    }
  }
  AppCheckProvider* provider = provider_factory->CreateProvider(cpp_app);
  return reinterpret_cast<jlong>(provider);
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  auto state = std::make_shared<JniTokenCallbackState>(env, j_provider,
                                                       task_completion_source);
  AppCheckProvider* provider = reinterpret_cast<AppCheckProvider*>(c_provider);
  if (provider == nullptr) {
    state->Complete(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                    "AppCheckProvider is null.");
    return;
  }
  auto token_callback{[state](firebase::app_check::AppCheckToken token,
                              int error_code,
                              const std::string& error_message) {
    state->Complete(token, error_code, error_message);
  }};
  provider->GetToken(token_callback);
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetLimitedUseToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  auto state = std::make_shared<JniTokenCallbackState>(env, j_provider,
                                                       task_completion_source);
  AppCheckProvider* provider = reinterpret_cast<AppCheckProvider*>(c_provider);
  if (provider == nullptr) {
    state->Complete(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                    "AppCheckProvider is null.");
    return;
  }
  auto token_callback{[state](firebase::app_check::AppCheckToken token,
                              int error_code,
                              const std::string& error_message) {
    state->Complete(token, error_code, error_message);
  }};
  provider->GetLimitedUseToken(token_callback);
}

JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject clazz, jlong c_app_check, jobject token) {
  (void)clazz;
  auto* app_check_internal = reinterpret_cast<AppCheckInternal*>(c_app_check);
  if (app_check_internal == nullptr) {
    return;
  }
  AppCheckToken cpp_token = CppTokenFromAndroidToken(env, token);
  MutexLock lock(GetRegistryMutex());
  const auto& active = GetActiveInstances();
  if (active.find(app_check_internal) == active.end()) {
    return;
  }
  app_check_internal->NotifyTokenChanged(cpp_token);
}

AppCheckInternal::AppCheckInternal(App* app)
    : app_(app), app_check_impl_(nullptr), j_app_check_listener_(nullptr) {
  future_manager().AllocFutureApi(this, kAppCheckFnCount);
  if (app_ == nullptr) {
    return;
  }

  // Cache the JNI method ids so we only have to look them up by name once.
  JNIEnv* env = app_->GetJNIEnv();
  if (env == nullptr) {
    return;
  }
  {
    MutexLock lock(GetRegistryMutex());
    if (g_initialized_count == 0) {
      jobject activity = app_->activity();
      if (!util::Initialize(env, activity)) {
        return;
      }
      // Cache embedded files and load embedded classes.
      const std::vector<firebase::internal::EmbeddedFile> embedded_files =
          util::CacheEmbeddedFiles(
              env, activity,
              firebase::internal::EmbeddedFile::ToVector(
                  firebase_app_check::app_check_resources_filename,
                  firebase_app_check::app_check_resources_data,
                  firebase_app_check::app_check_resources_size));
      // Terminate and return early if unable to cache main classes.
      if (!(CacheAppCheckMethodIds(env, activity, embedded_files) &&
            CacheCommonAndroidMethodIds(env, activity))) {
        ReleaseClasses(env);
        util::Terminate(env);
        return;
      }
      // Each provider is optional as a user may or may not use it.
      CacheDebugProviderMethodIds(env, activity, embedded_files);
      CachePlayIntegrityProviderMethodIds(env, activity);
    }
    ++g_initialized_count;
    GetInitializedInstances().insert(this);
    GetActiveInstances().insert(this);
  }

  static const char* kApiIdentifier = "AppCheck";
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);

  // Create the FirebaseAppCheck class in Java.
  jobject platform_app = app_->GetPlatformApp();
  if (platform_app == nullptr) {
    return;
  }
  jobject j_app_check_local = env->CallStaticObjectMethod(
      app_check::GetClass(), app_check::GetMethodId(app_check::kGetInstance),
      platform_app);
  bool get_instance_exception = util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(platform_app);
  if (get_instance_exception || j_app_check_local == nullptr) {
    if (j_app_check_local != nullptr) {
      env->DeleteLocalRef(j_app_check_local);
    }
    return;
  }

  // Create a global reference for the app check instance. It will be
  // released in the AppCheckInternal destructor.
  app_check_impl_ = env->NewGlobalRef(j_app_check_local);
  env->DeleteLocalRef(j_app_check_local);
  if (util::CheckAndClearJniExceptions(env) || app_check_impl_ == nullptr) {
    app_check_impl_ = nullptr;
    return;
  }

  // Install the AppCheckProviderFactory for this instance of AppCheck.
  AppCheckProviderFactory* factory = nullptr;
  {
    MutexLock lock(GetRegistryMutex());
    factory = g_provider_factory;
  }
  if (factory != nullptr) {
    // Create a Java ProviderFactory to wrap the C++ ProviderFactory.
    // Since InstallAppCheckProviderFactory is done for a single instance of
    // AppCheck, this factory will only be used for this App.
    jobject j_factory = env->NewObject(
        jni_provider_factory::GetClass(),
        jni_provider_factory::GetMethodId(jni_provider_factory::kConstructor),
        reinterpret_cast<jlong>(factory), reinterpret_cast<jlong>(app_));
    if (!util::CheckAndClearJniExceptions(env) && j_factory != nullptr) {
      // Install the Java ProviderFactory in the Java AppCheck class.
      env->CallVoidMethod(
          app_check_impl_,
          app_check::GetMethodId(app_check::kInstallAppCheckProviderFactory),
          j_factory);
      util::CheckAndClearJniExceptions(env);
      env->DeleteLocalRef(j_factory);
    }
  }

  // Add a token-changed listener and give it a pointer to the C++ listeners.
  jobject j_listener = env->NewObject(
      jni_app_check_listener::GetClass(),
      jni_app_check_listener::GetMethodId(jni_app_check_listener::kConstructor),
      reinterpret_cast<jlong>(this));
  if (!util::CheckAndClearJniExceptions(env) && j_listener != nullptr) {
    env->CallVoidMethod(app_check_impl_,
                        app_check::GetMethodId(app_check::kAddAppCheckListener),
                        j_listener);
    if (!util::CheckAndClearJniExceptions(env)) {
      j_app_check_listener_ = env->NewGlobalRef(j_listener);
      if (util::CheckAndClearJniExceptions(env)) {
        j_app_check_listener_ = nullptr;
      }
    }
    env->DeleteLocalRef(j_listener);
  }
}

AppCheckInternal::~AppCheckInternal() {
  {
    MutexLock lock(GetRegistryMutex());
    GetActiveInstances().erase(this);
  }
  JNIEnv* env = app_ ? app_->GetJNIEnv() : util::GetJNIEnvFromApp();
  if (env != nullptr) {
    if (!jni_task_id_.empty()) {
      util::CancelCallbacks(env, jni_task_id_.c_str());
    }

    if (j_app_check_listener_ != nullptr) {
      if (app_check_impl_ != nullptr) {
        env->CallVoidMethod(
            app_check_impl_,
            app_check::GetMethodId(app_check::kRemoveAppCheckListener),
            j_app_check_listener_);
        util::CheckAndClearJniExceptions(env);
      }
      env->DeleteGlobalRef(j_app_check_listener_);
      j_app_check_listener_ = nullptr;
    }
  }

  {
    MutexLock lock(listeners_mutex_);
    listeners_.clear();
  }

  if (env != nullptr && app_check_impl_ != nullptr) {
    // The Android App Check library holds onto the provider,
    // which can be a problem if it tries to call up to C++
    // after being deleted. So we use a hidden function meant
    // for testing purposes to clear out the App Check state,
    // to prevent this.
    jclass default_cls = default_app_check_impl::GetClass();
    jmethodID reset_mid = default_app_check_impl::GetMethodId(
        default_app_check_impl::kResetAppCheckState);
    if (default_cls != nullptr && reset_mid != nullptr &&
        env->IsInstanceOf(app_check_impl_, default_cls)) {
      env->CallVoidMethod(app_check_impl_, reset_mid);
      util::CheckAndClearJniExceptions(env);
    }

    env->DeleteGlobalRef(app_check_impl_);
    app_check_impl_ = nullptr;
  }

  future_manager().ReleaseFutureApi(this);
  app_ = nullptr;

  {
    MutexLock lock(GetRegistryMutex());
    auto& initialized = GetInitializedInstances();
    auto it = initialized.find(this);
    if (it != initialized.end()) {
      initialized.erase(it);
      ReleaseJniClassesLocked(env);
    }
  }
}

::firebase::App* AppCheckInternal::app() const { return app_; }

ReferenceCountedFutureImpl* AppCheckInternal::future() {
  return future_manager().GetFutureApi(this);
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  // Store the C++ factory in a static variable protected by GetRegistryMutex().
  // Whenever an instance of AppCheck is created, it will read this variable
  // and install the factory as it is initialized.
  MutexLock lock(GetRegistryMutex());
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  if (!app_ || !app_check_impl_) return;
  JNIEnv* env = app_->GetJNIEnv();
  if (env == nullptr) return;
  env->CallVoidMethod(
      app_check_impl_,
      app_check::GetMethodId(app_check::kSetTokenAutoRefreshEnabled),
      static_cast<jboolean>(is_token_auto_refresh_enabled));
  util::CheckAndClearJniExceptions(env);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetAppCheckToken);
  if (app_ == nullptr || app_check_impl_ == nullptr) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "FirebaseAppCheck is not initialized.",
                                 AppCheckToken{});
    return MakeFuture(future(), handle);
  }
  JNIEnv* env = app_->GetJNIEnv();
  if (env == nullptr) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "JNIEnv is not available.", AppCheckToken{});
    return MakeFuture(future(), handle);
  }
  jobject j_task = env->CallObjectMethod(
      app_check_impl_, app_check::GetMethodId(app_check::kGetToken),
      static_cast<jboolean>(force_refresh));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task != nullptr) {
    auto data_handle = new FutureDataHandle(future(), handle);
    util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 jni_task_id_.c_str());
  } else {
    AppCheckToken empty_token;
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, error.c_str(),
                                 empty_token);
  }
  if (j_task != nullptr) {
    env->DeleteLocalRef(j_task);
  }
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  auto handle =
      future()->SafeAlloc<AppCheckToken>(kAppCheckFnGetLimitedUseAppCheckToken);
  if (app_ == nullptr || app_check_impl_ == nullptr) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "FirebaseAppCheck is not initialized.",
                                 AppCheckToken{});
    return MakeFuture(future(), handle);
  }
  JNIEnv* env = app_->GetJNIEnv();
  if (env == nullptr) {
    future()->CompleteWithResult(handle, kAppCheckErrorInvalidConfiguration,
                                 "JNIEnv is not available.", AppCheckToken{});
    return MakeFuture(future(), handle);
  }
  jobject j_task = env->CallObjectMethod(
      app_check_impl_, app_check::GetMethodId(app_check::kGetLimitedUseToken));

  std::string error = util::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task != nullptr) {
    auto data_handle = new FutureDataHandle(future(), handle);
    util::RegisterCallbackOnTask(env, j_task, TokenResultCallback,
                                 reinterpret_cast<void*>(data_handle),
                                 jni_task_id_.c_str());
  } else {
    AppCheckToken empty_token;
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, error.c_str(),
                                 empty_token);
  }
  if (j_task != nullptr) {
    env->DeleteLocalRef(j_task);
  }
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it == listeners_.end()) {
    listeners_.push_back(listener);
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) {
    listeners_.erase(it);
  }
}

void AppCheckInternal::NotifyTokenChanged(AppCheckToken token) {
  // listeners_mutex_ is a recursive firebase::Mutex, so holding it across
  // listener dispatch prevents concurrent RemoveAppCheckListener() + listener
  // destruction on another thread while still allowing same-thread re-entrant
  // AddAppCheckListener/RemoveAppCheckListener calls.
  MutexLock lock(listeners_mutex_);
  std::vector<AppCheckListener*> snapshot = listeners_;
  for (AppCheckListener* listener : snapshot) {
    if (listener != nullptr &&
        std::find(listeners_.begin(), listeners_.end(), listener) !=
            listeners_.end()) {
      listener->OnAppCheckTokenChanged(token);
    }
  }
}

}  // namespace internal
}  // namespace app_check
}  // namespace firebase
