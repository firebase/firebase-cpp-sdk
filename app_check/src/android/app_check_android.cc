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
#include <utility>
#include <vector>

#include "app/src/app_android.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "app_check/app_check_resources.h"
#include "app_check/src/android/common_android.h"
#include "app_check/src/android/debug_provider_android.h"
#include "app_check/src/android/play_integrity_provider_android.h"
#include "app_check/src/common/common.h"

namespace firebase::app_check::internal {

// clang-format off
#define APP_CHECK_METHODS(X)                                                   \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/appcheck/FirebaseAppCheck;",                         \
    firebase::jni::kMethodTypeStatic)                                          \
  X(InstallAppCheckProviderFactory, "installAppCheckProviderFactory",          \
    "(Lcom/google/firebase/appcheck/AppCheckProviderFactory;)V")               \
  X(SetTokenAutoRefreshEnabled, "setTokenAutoRefreshEnabled", "(Z)V")          \
  X(GetToken, "getAppCheckToken", "(Z)Lcom/google/android/gms/tasks/Task;")    \
  X(GetLimitedUseToken, "getLimitedUseAppCheckToken",                          \
    "()Lcom/google/android/gms/tasks/Task;")                                   \
  X(AddAppCheckListener, "addAppCheckListener",                                \
    "(Lcom/google/firebase/appcheck/FirebaseAppCheck$AppCheckListener;)V")     \
  X(RemoveAppCheckListener, "removeAppCheckListener",                          \
    "(Lcom/google/firebase/appcheck/FirebaseAppCheck$AppCheckListener;)V")

#define DEFAULT_APP_CHECK_IMPL_METHODS(X)                                      \
  X(ResetAppCheckState, "resetAppCheckState", "()V")

#define JNI_APP_CHECK_PROVIDER_FACTORY_METHODS(X)                              \
  X(Constructor, "<init>", "(JJ)V")

#define JNI_APP_CHECK_PROVIDER_METHODS(X)                                      \
  X(Constructor, "<init>", "(J)V")                                             \
  X(HandleGetTokenResult, "handleGetTokenResult",                              \
    "(Lcom/google/android/gms/tasks/TaskCompletionSource;Ljava/lang/String;"   \
    "JILjava/lang/String;)V")

#define JNI_APP_CHECK_LISTENER_METHODS(X)                                      \
  X(Constructor, "<init>", "(J)V")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(app_check, APP_CHECK_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/appcheck/FirebaseAppCheck")
JNI_METHOD_LOOKUP_DECLARATION(
    default_app_check_impl, DEFAULT_APP_CHECK_IMPL_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/internal/DefaultFirebaseAppCheck")
JNI_METHOD_LOOKUP_DECLARATION(
    jni_provider_factory, JNI_APP_CHECK_PROVIDER_FACTORY_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckProviderFactory")
JNI_METHOD_LOOKUP_DECLARATION(
    jni_provider, JNI_APP_CHECK_PROVIDER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckProvider")
JNI_METHOD_LOOKUP_DECLARATION(
    jni_app_check_listener, JNI_APP_CHECK_LISTENER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/appcheck/internal/cpp/JniAppCheckListener")

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* env, jobject clazz, jlong c_factory, jlong c_app);
JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source);
JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetLimitedUseToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source);
JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject clazz, jlong c_app_check, jobject token);

static const JNINativeMethod kNativeJniAppCheckProviderFactoryMethods[] = {
    {"nativeCreateProvider", "(JJ)J",
     reinterpret_cast<void*>(JniAppCheckProviderFactory_nativeCreateProvider)},
};
static const JNINativeMethod kNativeJniAppCheckProviderMethods[] = {
    {"nativeGetToken",
     "(JLcom/google/android/gms/tasks/TaskCompletionSource;)V",
     reinterpret_cast<void*>(JniAppCheckProvider_nativeGetToken)},
    {"nativeGetLimitedUseToken",
     "(JLcom/google/android/gms/tasks/TaskCompletionSource;)V",
     reinterpret_cast<void*>(JniAppCheckProvider_nativeGetLimitedUseToken)},
};
static const JNINativeMethod kNativeJniAppCheckListenerMethods[] = {
    {"nativeOnAppCheckTokenChanged",
     "(JLcom/google/firebase/appcheck/AppCheckToken;)V",
     reinterpret_cast<void*>(JniAppCheckListener_nativeOnAppCheckTokenChanged)},
};

bool CacheAppCheckMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<firebase::internal::EmbeddedFile>& embedded_files) {
  return jni_provider_factory::CacheClassFromFiles(env, activity,
                                                   &embedded_files) &&
         jni_provider_factory::CacheMethodIds(env, activity) &&
         jni_provider_factory::RegisterNatives(
             env, kNativeJniAppCheckProviderFactoryMethods,
             FIREBASE_ARRAYSIZE(kNativeJniAppCheckProviderFactoryMethods)) &&
         jni_provider::CacheClassFromFiles(env, activity, &embedded_files) &&
         jni_provider::CacheMethodIds(env, activity) &&
         jni_provider::RegisterNatives(
             env, kNativeJniAppCheckProviderMethods,
             FIREBASE_ARRAYSIZE(kNativeJniAppCheckProviderMethods)) &&
         jni_app_check_listener::CacheClassFromFiles(env, activity,
                                                     &embedded_files) &&
         jni_app_check_listener::CacheMethodIds(env, activity) &&
         jni_app_check_listener::RegisterNatives(
             env, kNativeJniAppCheckListenerMethods,
             FIREBASE_ARRAYSIZE(kNativeJniAppCheckListenerMethods)) &&
         app_check::CacheMethodIds(env, activity) &&
         default_app_check_impl::CacheMethodIds(env, activity);
}

void ReleaseAppCheckClasses(JNIEnv* env) {
  if (jclass c = jni_provider_factory::GetClass()) env->UnregisterNatives(c);
  if (jclass c = jni_provider::GetClass()) env->UnregisterNatives(c);
  if (jclass c = jni_app_check_listener::GetClass()) env->UnregisterNatives(c);
  app_check::ReleaseClass(env);
  default_app_check_impl::ReleaseClass(env);
  jni_provider_factory::ReleaseClass(env);
  jni_provider::ReleaseClass(env);
  jni_app_check_listener::ReleaseClass(env);
}

static void ReleaseClasses(JNIEnv* env) {
  ReleaseAppCheckClasses(env);
  ReleaseCommonAndroidClasses(env);
  ReleaseDebugProviderClasses(env);
  ReleasePlayIntegrityProviderClasses(env);
}

namespace {

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
  if (g_initialized_count <= 0) return false;
  ++g_initialized_count;
  return true;
}

void ReleaseJniClassesLocked(JNIEnv* env) {
  FIREBASE_ASSERT(g_initialized_count > 0);
  if (--g_initialized_count == 0) {
    if (!env) env = firebase::util::GetJNIEnvFromApp();
    if (env) ReleaseClasses(env);
  }
}

struct FutureDataHandle {
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<AppCheckToken> future_handle;
};

void TokenResultCallback(JNIEnv* env, jobject result,
                         firebase::util::FutureResult result_code,
                         const char* status_message, void* callback_data) {
  std::unique_ptr<FutureDataHandle> dh(
      static_cast<FutureDataHandle*>(callback_data));
  if (!dh) return;
  bool ok = (result_code == util::kFutureResultSuccess && result != nullptr);
  dh->future_api->CompleteWithResult(
      dh->future_handle, ok ? kAppCheckErrorNone : kAppCheckErrorUnknown,
      status_message ? status_message : "",
      ok ? CppTokenFromAndroidToken(env, result) : AppCheckToken{});
}

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
    if (env) {
      if (j_provider) {
        j_provider_global = env->NewGlobalRef(j_provider);
        if (jni::CheckAndClearException(env)) j_provider_global = nullptr;
      }
      if (task_completion_source) {
        task_completion_source_global =
            env->NewGlobalRef(task_completion_source);
        if (jni::CheckAndClearException(env)) {
          task_completion_source_global = nullptr;
        }
      }
    }
  }

  void InvokeHandleGetTokenResult(JNIEnv* env, const std::string& token_str,
                                  jlong expire_time_millis, jint error_code,
                                  const std::string& error_message) {
    if (!env || !j_provider_global || !task_completion_source_global) return;
    jmethodID mid =
        jni_provider::GetMethodId(jni_provider::kHandleGetTokenResult);
    if (!mid) return;
    jstring error_string = env->NewStringUTF(error_message.c_str());
    if (jni::CheckAndClearException(env)) error_string = nullptr;
    jstring token_string = env->NewStringUTF(token_str.c_str());
    if (jni::CheckAndClearException(env)) {
      if (error_string) env->DeleteLocalRef(error_string);
      return;
    }
    env->CallVoidMethod(j_provider_global, mid, task_completion_source_global,
                        token_string, expire_time_millis, error_code,
                        error_string);
    jni::CheckAndClearException(env);
    if (token_string) env->DeleteLocalRef(token_string);
    if (error_string) env->DeleteLocalRef(error_string);
  }

  void Complete(const AppCheckToken& token, int error_code,
                const std::string& error_message) {
    if (completed.exchange(true)) return;
    JNIEnv* env = firebase::util::GetJNIEnvFromApp();
    InvokeHandleGetTokenResult(env, token.token,
                               static_cast<jlong>(token.expire_time_millis),
                               static_cast<jint>(error_code), error_message);
    ReleaseRefs(env);
  }

  void ReleaseRefs(JNIEnv* env) {
    if (!env) env = firebase::util::GetJNIEnvFromApp();
    if (env) {
      if (j_provider_global) {
        env->DeleteGlobalRef(std::exchange(j_provider_global, nullptr));
      }
      if (task_completion_source_global) {
        env->DeleteGlobalRef(
            std::exchange(task_completion_source_global, nullptr));
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
      InvokeHandleGetTokenResult(env, "", 0, kAppCheckErrorUnknown,
                                 "AppCheckProvider callback was dropped.");
    }
    ReleaseRefs(env);
  }
};

void DispatchNativeProviderGetToken(JNIEnv* env, jobject j_provider,
                                    jlong c_provider, jobject tcs,
                                    bool limited_use) {
  auto state =
      std::make_shared<JniTokenCallbackState>(env, j_provider, tcs);
  auto* provider = reinterpret_cast<AppCheckProvider*>(c_provider);
  if (!provider) {
    state->Complete(AppCheckToken{}, kAppCheckErrorInvalidConfiguration,
                    "AppCheckProvider is null.");
    return;
  }
  auto cb = [state](AppCheckToken token, int err, const std::string& msg) {
    state->Complete(token, err, msg);
  };
  if (limited_use) {
    provider->GetLimitedUseToken(cb);
  } else {
    provider->GetToken(cb);
  }
}

}  // namespace

JNIEXPORT jlong JNICALL JniAppCheckProviderFactory_nativeCreateProvider(
    JNIEnv* /*env*/, jobject /*clazz*/, jlong c_factory, jlong c_app) {
  auto* cpp_app = reinterpret_cast<App*>(c_app);
  auto* provider_factory =
      reinterpret_cast<AppCheckProviderFactory*>(c_factory);
  if (!provider_factory || !cpp_app) return 0;
  {
    MutexLock lock(GetRegistryMutex());
    if (provider_factory != g_provider_factory) return 0;
    const auto& active = GetActiveInstances();
    if (std::none_of(active.begin(), active.end(), [cpp_app](auto* inst) {
          return inst && inst->app() == cpp_app;
        })) {
      return 0;
    }
  }
  return reinterpret_cast<jlong>(provider_factory->CreateProvider(cpp_app));
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  DispatchNativeProviderGetToken(env, j_provider, c_provider,
                                 task_completion_source, false);
}

JNIEXPORT void JNICALL JniAppCheckProvider_nativeGetLimitedUseToken(
    JNIEnv* env, jobject j_provider, jlong c_provider,
    jobject task_completion_source) {
  DispatchNativeProviderGetToken(env, j_provider, c_provider,
                                 task_completion_source, true);
}

JNIEXPORT void JNICALL JniAppCheckListener_nativeOnAppCheckTokenChanged(
    JNIEnv* env, jobject /*clazz*/, jlong c_app_check, jobject token) {
  auto* app_check_internal = reinterpret_cast<AppCheckInternal*>(c_app_check);
  if (!app_check_internal) return;
  AppCheckToken cpp_token = CppTokenFromAndroidToken(env, token);
  MutexLock lock(GetRegistryMutex());
  if (GetActiveInstances().count(app_check_internal)) {
    app_check_internal->NotifyTokenChanged(cpp_token);
  }
}

AppCheckInternal::AppCheckInternal(App* app)
    : future_impl_(kAppCheckFnCount),
      app_(app),
      app_check_impl_(nullptr),
      j_app_check_listener_(nullptr) {
  if (!app_) return;
  JNIEnv* env = app_->GetJNIEnv();
  if (!env) return;
  {
    MutexLock lock(GetRegistryMutex());
    if (g_initialized_count == 0) {
      jobject activity = app_->activity();
      const auto embedded_files = jni::CacheEmbeddedFiles(
          env, activity,
          firebase::internal::EmbeddedFile::ToVector(
              firebase_app_check::app_check_resources_filename,
              firebase_app_check::app_check_resources_data,
              firebase_app_check::app_check_resources_size));
      if (!(CacheAppCheckMethodIds(env, activity, embedded_files) &&
            CacheCommonAndroidMethodIds(env, activity))) {
        ReleaseClasses(env);
        return;
      }
      CacheDebugProviderMethodIds(env, activity, embedded_files);
      CachePlayIntegrityProviderMethodIds(env, activity);
    }
    ++g_initialized_count;
    GetInitializedInstances().insert(this);
    GetActiveInstances().insert(this);
  }

  jni_task_id_ = CreateApiIdentifier("AppCheck", this);

  jobject platform_app = app_->GetPlatformApp();
  if (!platform_app) return;
  jobject j_app_check_local = env->CallStaticObjectMethod(
      app_check::GetClass(), app_check::GetMethodId(app_check::kGetInstance),
      platform_app);
  bool ex = jni::CheckAndClearException(env);
  env->DeleteLocalRef(platform_app);
  if (ex || !j_app_check_local) {
    if (j_app_check_local) env->DeleteLocalRef(j_app_check_local);
    return;
  }

  app_check_impl_ = env->NewGlobalRef(j_app_check_local);
  env->DeleteLocalRef(j_app_check_local);
  if (jni::CheckAndClearException(env) || !app_check_impl_) {
    app_check_impl_ = nullptr;
    return;
  }

  AppCheckProviderFactory* factory = nullptr;
  {
    MutexLock lock(GetRegistryMutex());
    factory = g_provider_factory;
  }
  if (factory) {
    jobject j_factory = env->NewObject(
        jni_provider_factory::GetClass(),
        jni_provider_factory::GetMethodId(jni_provider_factory::kConstructor),
        reinterpret_cast<jlong>(factory), reinterpret_cast<jlong>(app_));
    if (!jni::CheckAndClearException(env) && j_factory) {
      env->CallVoidMethod(
          app_check_impl_,
          app_check::GetMethodId(app_check::kInstallAppCheckProviderFactory),
          j_factory);
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(j_factory);
    }
  }

  jobject j_listener = env->NewObject(
      jni_app_check_listener::GetClass(),
      jni_app_check_listener::GetMethodId(jni_app_check_listener::kConstructor),
      reinterpret_cast<jlong>(this));
  if (!jni::CheckAndClearException(env) && j_listener) {
    env->CallVoidMethod(app_check_impl_,
                        app_check::GetMethodId(app_check::kAddAppCheckListener),
                        j_listener);
    if (!jni::CheckAndClearException(env)) {
      j_app_check_listener_ = env->NewGlobalRef(j_listener);
      if (jni::CheckAndClearException(env)) j_app_check_listener_ = nullptr;
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
  if (env) {
    if (!jni_task_id_.empty()) {
      firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
    }
    if (j_app_check_listener_) {
      if (app_check_impl_) {
        env->CallVoidMethod(
            app_check_impl_,
            app_check::GetMethodId(app_check::kRemoveAppCheckListener),
            j_app_check_listener_);
        jni::CheckAndClearException(env);
      }
      env->DeleteGlobalRef(std::exchange(j_app_check_listener_, nullptr));
    }
  }

  {
    MutexLock lock(listeners_mutex_);
    listeners_.clear();
  }

  if (env && app_check_impl_) {
    jclass default_cls = default_app_check_impl::GetClass();
    jmethodID reset_mid = default_app_check_impl::GetMethodId(
        default_app_check_impl::kResetAppCheckState);
    if (default_cls && reset_mid &&
        env->IsInstanceOf(app_check_impl_, default_cls)) {
      env->CallVoidMethod(app_check_impl_, reset_mid);
      jni::CheckAndClearException(env);
    }
    env->DeleteGlobalRef(std::exchange(app_check_impl_, nullptr));
  }
  app_ = nullptr;

  {
    MutexLock lock(GetRegistryMutex());
    if (GetInitializedInstances().erase(this)) {
      ReleaseJniClassesLocked(env);
    }
  }
}

void AppCheckInternal::SetAppCheckProviderFactory(
    AppCheckProviderFactory* factory) {
  MutexLock lock(GetRegistryMutex());
  g_provider_factory = factory;
}

void AppCheckInternal::SetTokenAutoRefreshEnabled(
    bool is_token_auto_refresh_enabled) {
  if (!app_ || !app_check_impl_) return;
  if (JNIEnv* env = app_->GetJNIEnv()) {
    env->CallVoidMethod(
        app_check_impl_,
        app_check::GetMethodId(app_check::kSetTokenAutoRefreshEnabled),
        static_cast<jboolean>(is_token_auto_refresh_enabled));
    jni::CheckAndClearException(env);
  }
}

Future<AppCheckToken> AppCheckInternal::DispatchTokenTask(AppCheckFn fn,
                                                          bool force_refresh) {
  auto handle = future()->SafeAlloc<AppCheckToken>(fn);
  JNIEnv* env = (app_ && app_check_impl_) ? app_->GetJNIEnv() : nullptr;
  if (!env) {
    future()->CompleteWithResult(
        handle, kAppCheckErrorInvalidConfiguration,
        app_ && app_check_impl_ ? "JNIEnv is not available."
                                : "FirebaseAppCheck is not initialized.",
        AppCheckToken{});
    return MakeFuture(future(), handle);
  }
  jobject j_task =
      (fn == kAppCheckFnGetAppCheckToken)
          ? env->CallObjectMethod(
                app_check_impl_, app_check::GetMethodId(app_check::kGetToken),
                static_cast<jboolean>(force_refresh))
          : env->CallObjectMethod(
                app_check_impl_,
                app_check::GetMethodId(app_check::kGetLimitedUseToken));
  std::string error = jni::GetAndClearExceptionMessage(env);
  if (error.empty() && j_task) {
    firebase::util::RegisterCallbackOnTask(
        env, j_task, TokenResultCallback,
        new FutureDataHandle{future(), handle}, jni_task_id_.c_str());
  } else {
    future()->CompleteWithResult(handle, kAppCheckErrorUnknown, error.c_str(),
                                 AppCheckToken{});
  }
  if (j_task) env->DeleteLocalRef(j_task);
  return MakeFuture(future(), handle);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckToken(bool force_refresh) {
  return DispatchTokenTask(kAppCheckFnGetAppCheckToken, force_refresh);
}

Future<AppCheckToken> AppCheckInternal::GetAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetAppCheckToken));
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckToken() {
  return DispatchTokenTask(kAppCheckFnGetLimitedUseAppCheckToken, false);
}

Future<AppCheckToken> AppCheckInternal::GetLimitedUseAppCheckTokenLastResult() {
  return static_cast<const Future<AppCheckToken>&>(
      future()->LastResult(kAppCheckFnGetLimitedUseAppCheckToken));
}

void AppCheckInternal::AddAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex_);
  if (std::find(listeners_.begin(), listeners_.end(), listener) ==
      listeners_.end()) {
    listeners_.push_back(listener);
  }
}

void AppCheckInternal::RemoveAppCheckListener(AppCheckListener* listener) {
  if (!listener) return;
  MutexLock lock(listeners_mutex_);
  auto it = std::find(listeners_.begin(), listeners_.end(), listener);
  if (it != listeners_.end()) listeners_.erase(it);
}

void AppCheckInternal::NotifyTokenChanged(AppCheckToken token) {
  MutexLock lock(listeners_mutex_);
  std::vector<AppCheckListener*> snapshot = listeners_;
  for (AppCheckListener* listener : snapshot) {
    if (listener && std::find(listeners_.begin(), listeners_.end(), listener) !=
                        listeners_.end()) {
      listener->OnAppCheckTokenChanged(token);
    }
  }
}

}  // namespace firebase::app_check::internal
