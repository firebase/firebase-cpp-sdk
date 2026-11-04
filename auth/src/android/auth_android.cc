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

#include <jni.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "auth/auth_resources.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using ::firebase::jni::JStringToString;

#define AUTH_METHODS(X)                                                      \
  X(GetInstance, "getInstance",                                              \
    "(Lcom/google/firebase/FirebaseApp;)"                                    \
    "Lcom/google/firebase/auth/FirebaseAuth;",                               \
    firebase::jni::kMethodTypeStatic)                                        \
  X(GetCurrentUser, "getCurrentUser",                                        \
    "()Lcom/google/firebase/auth/FirebaseUser;")                             \
  X(GetLanguageCode, "getLanguageCode", "()Ljava/lang/String;")              \
  X(SetLanguageCode, "setLanguageCode", "(Ljava/lang/String;)V")             \
  X(UseAppLanguage, "useAppLanguage", "()V")                                 \
  X(AddAuthStateListener, "addAuthStateListener",                            \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V")          \
  X(RemoveAuthStateListener, "removeAuthStateListener",                      \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V")          \
  X(AddIdTokenListener, "addIdTokenListener",                                \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V")            \
  X(RemoveIdTokenListener, "removeIdTokenListener",                          \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V")            \
  X(SignOut, "signOut", "()V")                                               \
  X(UseEmulator, "useEmulator", "(Ljava/lang/String;I)V")                    \
  X(FetchSignInMethodsForEmail, "fetchSignInMethodsForEmail",                \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")               \
  X(SignInWithCustomToken, "signInWithCustomToken",                          \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")               \
  X(SignInWithCredential, "signInWithCredential",                            \
    "(Lcom/google/firebase/auth/AuthCredential;)"                            \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(SignInAnonymously, "signInAnonymously",                                  \
    "()Lcom/google/android/gms/tasks/Task;")                                 \
  X(SignInWithEmailAndPassword, "signInWithEmailAndPassword",                \
    "(Ljava/lang/String;Ljava/lang/String;)"                                 \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(CreateUserWithEmailAndPassword, "createUserWithEmailAndPassword",        \
    "(Ljava/lang/String;Ljava/lang/String;)"                                 \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(SendPasswordResetEmail, "sendPasswordResetEmail",                        \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")
JNI_METHOD_LOOKUP_DECLARATION(auth, AUTH_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FirebaseAuth")

#define SIGNIN_METHOD_QUERY_RESULT_METHODS(X) \
  X(GetSignInMethods, "getSignInMethods", "()Ljava/util/List;")
JNI_METHOD_LOOKUP_DECLARATION(
    signinmethodquery, SIGNIN_METHOD_QUERY_RESULT_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/SignInMethodQueryResult")

#define JNI_LISTENER_CALLBACK_METHODS(X) \
  X(Constructor, "<init>", "(J)V")       \
  X(Disconnect, "disconnect", "()V")
JNI_METHOD_LOOKUP_DECLARATION(
    jnilistener, JNI_LISTENER_CALLBACK_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/internal/cpp/JniAuthStateListener")
JNI_METHOD_LOOKUP_DECLARATION(
    jni_id_token_listener, JNI_LISTENER_CALLBACK_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/internal/cpp/JniIdTokenListener")

static Mutex g_init_mutex;
static int g_initialized_count = 0;
static const char* kErrorEmptyEmailPassword =
    "Empty email or password are not allowed.";

JNIEXPORT void JNICALL JniAuthStateListener_nativeOnAuthStateChanged(
    JNIEnv* env, jobject clazz, jlong callback_data);
JNIEXPORT void JNICALL JniIdTokenListener_nativeOnIdTokenChanged(
    JNIEnv* env, jobject clazz, jlong callback_data);

static const JNINativeMethod kNativeOnAuthStateChangedMethod = {
    "nativeOnAuthStateChanged", "(J)V",
    reinterpret_cast<void*>(JniAuthStateListener_nativeOnAuthStateChanged)};
static const JNINativeMethod kNativeOnIdTokenChangedMethod = {
    "nativeOnIdTokenChanged", "(J)V",
    reinterpret_cast<void*>(JniIdTokenListener_nativeOnIdTokenChanged)};

bool CacheAuthMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<::firebase::internal::EmbeddedFile>& embedded_files) {
  return auth::CacheMethodIds(env, activity) &&
         signinmethodquery::CacheMethodIds(env, activity) &&
         jnilistener::CacheClassFromFiles(env, activity, &embedded_files) &&
         jnilistener::CacheMethodIds(env, activity) &&
         jnilistener::RegisterNatives(env, &kNativeOnAuthStateChangedMethod,
                                      1) &&
         jni_id_token_listener::CacheClassFromFiles(env, activity,
                                                    &embedded_files) &&
         jni_id_token_listener::CacheMethodIds(env, activity) &&
         jni_id_token_listener::RegisterNatives(
             env, &kNativeOnIdTokenChangedMethod, 1);
}

void ReleaseAuthClasses(JNIEnv* env) {
  auth::ReleaseClass(env);
  signinmethodquery::ReleaseClass(env);
  if (jnilistener::GetClass() != nullptr) {
    env->UnregisterNatives(jnilistener::GetClass());
    (void)jni::CheckAndClearException(env);
  }
  jnilistener::ReleaseClass(env);
  if (jni_id_token_listener::GetClass() != nullptr) {
    env->UnregisterNatives(jni_id_token_listener::GetClass());
    (void)jni::CheckAndClearException(env);
  }
  jni_id_token_listener::ReleaseClass(env);
}

void UpdateCurrentUser(AuthData* auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  JNIEnv* env = Env(auth_data);
  jobject j_user = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kGetCurrentUser));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (j_user) env->DeleteLocalRef(j_user);
    j_user = nullptr;
  }
  jobject original_user_impl = static_cast<jobject>(auth_data->user_impl);
  if (!env->IsSameObject(original_user_impl, j_user)) {
    SetImplFromLocalRef(env, j_user, &auth_data->user_impl);
    LogDebug("CurrentUser changed from %p to %p", original_user_impl,
             auth_data->user_impl);
  } else if (j_user) {
    env->DeleteLocalRef(j_user);
  }
}

void SetEmulatorJni(AuthData* auth_data, const char* host, uint32_t port) {
  if (!auth_data || !host || host[0] == '\0' || port == 0 || port > 65535) {
    return;
  }
  JNIEnv* env = Env(auth_data);
  jstring j_host = env->NewStringUTF(host);
  if (!::firebase::jni::CheckAndClearException(env) && j_host != nullptr) {
    env->CallVoidMethod(AuthImpl(auth_data),
                        auth::GetMethodId(auth::kUseEmulator), j_host,
                        static_cast<jint>(port));
    (void)::firebase::jni::CheckAndClearException(env);
  }
  if (j_host) env->DeleteLocalRef(j_host);
}

const char* const kEmulatorLocalHost = "10.0.2.2";
const uint32_t kDefaultEmulatorPort = 9099;
void CheckEmulator(AuthData* auth_data) {
  const char* use_emulator = std::getenv("USE_AUTH_EMULATOR");
  if (!use_emulator || use_emulator[0] == '\0' ||
      std::strcmp(use_emulator, "0") == 0 ||
      std::strcmp(use_emulator, "false") == 0) {
    return;
  }
  LogInfo("Using Auth Emulator for testing.");
  uint32_t port = kDefaultEmulatorPort;
  const char* env_port = std::getenv("AUTH_EMULATOR_PORT");
  if (env_port && env_port[0] != '\0') {
    char* end = nullptr;
    const long parsed_port = std::strtol(env_port, &end, 10);
    if (end != env_port && *end == '\0' && parsed_port > 0 &&
        parsed_port <= 65535) {
      port = static_cast<uint32_t>(parsed_port);
    }
  }
  SetEmulatorJni(auth_data, kEmulatorLocalHost, port);
}

static void ReleaseClasses(JNIEnv* env) {
  ReleaseAuthClasses(env);
  ReleaseUserClasses(env);
  ReleaseCredentialClasses(env);
  ReleaseCommonClasses(env);
}

void* CreatePlatformAuth(App* app) {
  if (app == nullptr) return nullptr;
  MutexLock init_lock(g_init_mutex);
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();

  if (!g_initialized_count) {
    if (!util::Initialize(env, activity)) return nullptr;
    const std::vector<::firebase::internal::EmbeddedFile> embedded_files =
        util::CacheEmbeddedFiles(env, activity,
                                 ::firebase::internal::EmbeddedFile::ToVector(
                                     firebase_auth::auth_resources_filename,
                                     firebase_auth::auth_resources_data,
                                     firebase_auth::auth_resources_size));
    if (!(CacheAuthMethodIds(env, activity, embedded_files) &&
          CacheUserMethodIds(env, activity) &&
          CacheCredentialMethodIds(env, activity, embedded_files) &&
          CacheCommonMethodIds(env, activity))) {
      ReleaseClasses(env);
      util::Terminate(env);
      return nullptr;
    }
  }

  jobject platform_app = app->GetPlatformApp();
  jobject j_auth_impl = env->CallStaticObjectMethod(
      auth::GetClass(), auth::GetMethodId(auth::kGetInstance), platform_app);
  const bool exception_thrown = jni::CheckAndClearException(env);
  env->DeleteLocalRef(platform_app);
  if (exception_thrown || j_auth_impl == nullptr) {
    if (j_auth_impl) env->DeleteLocalRef(j_auth_impl);
    if (!g_initialized_count) {
      ReleaseClasses(env);
      util::Terminate(env);
    }
    return nullptr;
  }

  void* auth_impl = nullptr;
  SetImplFromLocalRef(env, j_auth_impl, &auth_impl);
  if (auth_impl == nullptr) {
    if (!g_initialized_count) {
      ReleaseClasses(env);
      util::Terminate(env);
    }
    return nullptr;
  }

  g_initialized_count++;
  return auth_impl;
}

void Auth::InitPlatformAuth(AuthData* auth_data) {
  if (auth_data == nullptr || AuthImpl(auth_data) == nullptr) return;
  JNIEnv* env = Env(auth_data);
  UpdateCurrentUser(auth_data);
  CheckEmulator(auth_data);

  auto register_listener = [&](jclass cls, jmethodID ctor,
                               auth::Method add_method, void** impl_out) {
    jobject j_listener =
        env->NewObject(cls, ctor, reinterpret_cast<jlong>(auth_data));
    if (!jni::CheckAndClearException(env) && j_listener != nullptr) {
      env->CallVoidMethod(AuthImpl(auth_data), auth::GetMethodId(add_method),
                          j_listener);
      (void)jni::CheckAndClearException(env);
      SetImplFromLocalRef(env, j_listener, impl_out);
    } else if (j_listener != nullptr) {
      env->DeleteLocalRef(j_listener);
    }
  };
  register_listener(jnilistener::GetClass(),
                    jnilistener::GetMethodId(jnilistener::kConstructor),
                    auth::kAddAuthStateListener, &auth_data->listener_impl);
  register_listener(
      jni_id_token_listener::GetClass(),
      jni_id_token_listener::GetMethodId(jni_id_token_listener::kConstructor),
      auth::kAddIdTokenListener, &auth_data->id_token_listener_impl);
}

void Auth::DestroyPlatformAuth(AuthData* auth_data) {
  if (auth_data == nullptr) return;
  JNIEnv* env = Env(auth_data);
  firebase::util::CancelCallbacks(env, auth_data->future_api_id.c_str());
  ClearPretriggeredIdTokenCallbacks(auth_data);
  if (AuthImpl(auth_data) == nullptr) return;

  auto unregister_listener = [&](void* impl, jmethodID disconnect_method,
                                 auth::Method remove_method) {
    if (!impl) return;
    env->CallVoidMethod(static_cast<jobject>(impl), disconnect_method);
    (void)jni::CheckAndClearException(env);
    env->CallVoidMethod(AuthImpl(auth_data), auth::GetMethodId(remove_method),
                        static_cast<jobject>(impl));
    (void)jni::CheckAndClearException(env);
  };
  unregister_listener(auth_data->listener_impl,
                      jnilistener::GetMethodId(jnilistener::kDisconnect),
                      auth::kRemoveAuthStateListener);
  unregister_listener(
      auth_data->id_token_listener_impl,
      jni_id_token_listener::GetMethodId(jni_id_token_listener::kDisconnect),
      auth::kRemoveIdTokenListener);

  SetImplFromLocalRef(env, nullptr, &auth_data->listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->id_token_listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->user_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->auth_impl);

  MutexLock init_lock(g_init_mutex);
  FIREBASE_ASSERT(g_initialized_count);
  if (--g_initialized_count == 0) {
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

void LogHeartbeat(Auth* auth) { (void)auth; }

JNIEXPORT void JNICALL JniAuthStateListener_nativeOnAuthStateChanged(
    JNIEnv* /*env*/, jobject /*clazz*/, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  if (!auth_data) return;
  UpdateCurrentUser(auth_data);
  NotifyAuthStateListeners(auth_data);
}

JNIEXPORT void JNICALL JniIdTokenListener_nativeOnIdTokenChanged(
    JNIEnv* /*env*/, jobject /*clazz*/, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  if (!auth_data) return;
  UpdateCurrentUser(auth_data);
  if (!auth_data->ShouldTriggerIdTokenListenerCallback() &&
      ConsumePretriggeredIdTokenCallback(auth_data)) {
    return;
  }
  NotifyIdTokenListeners(auth_data);
}

static void ReadProviderResult(
    jobject result, FutureCallbackData<Auth::FetchProvidersResult>* d,
    bool success, void* void_data) {
  auto data = static_cast<Auth::FetchProvidersResult*>(void_data);
  JNIEnv* env = Env(d->auth_data);
  FIREBASE_ASSERT(!success || result != nullptr);
  jobject list = success ? env->CallObjectMethod(
                               result, signinmethodquery::GetMethodId(
                                           signinmethodquery::kGetSignInMethods))
                         : nullptr;
  if (::firebase::jni::CheckAndClearException(env) && list != nullptr) {
    env->DeleteLocalRef(list);
    list = nullptr;
  }
  if (list != nullptr) {
    util::JavaListToStdStringVector(env, &data->providers, list);
    env->DeleteLocalRef(list);
  }
}

void FetchProvidersForEmail(AuthData* auth_data,
                            Promise<Auth::FetchProvidersResult> promise,
                            const char* email) {
  if (!email || email[0] == '\0') {
    promise.Fail(kAuthErrorMissingEmail, "Empty email address.");
    return;
  }
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadProviderResult, [=](JNIEnv* env) {
        jstring j_email = env->NewStringUTF(email);
        if (env->ExceptionCheck() || !j_email) {
          if (j_email) env->DeleteLocalRef(j_email);
          return static_cast<jobject>(nullptr);
        }
        jobject task = env->CallObjectMethod(
            AuthImpl(auth_data),
            auth::GetMethodId(auth::kFetchSignInMethodsForEmail), j_email);
        env->DeleteLocalRef(j_email);
        return task;
      });
}

void SignInWithCustomToken(AuthData* auth_data, Promise<AuthResult> promise,
                           const char* token) {
  if (!token || token[0] == '\0') {
    promise.Fail(kAuthErrorInvalidCustomToken, "Empty custom token.");
    return;
  }
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
        jstring j_token = env->NewStringUTF(token);
        if (env->ExceptionCheck() || !j_token) {
          if (j_token) env->DeleteLocalRef(j_token);
          return static_cast<jobject>(nullptr);
        }
        jobject task = env->CallObjectMethod(
            AuthImpl(auth_data),
            auth::GetMethodId(auth::kSignInWithCustomToken), j_token);
        env->DeleteLocalRef(j_token);
        return task;
      });
}

void SignInWithCredential(AuthData* auth_data, Promise<User> promise,
                          const Credential& credential) {
  if (!ValidateCredentialForPromise(promise, credential)) return;
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadUserFromAuthResult, [=](JNIEnv* env) {
        return env->CallObjectMethod(
            AuthImpl(auth_data), auth::GetMethodId(auth::kSignInWithCredential),
            CredentialFromImpl(internal::CredentialAccess::GetImpl(credential)));
      });
}

void SignInAndRetrieveDataWithCredential(AuthData* auth_data,
                                         Promise<AuthResult> promise,
                                         const Credential& credential) {
  if (!ValidateCredentialForPromise(promise, credential)) return;
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
        return env->CallObjectMethod(
            AuthImpl(auth_data), auth::GetMethodId(auth::kSignInWithCredential),
            CredentialFromImpl(internal::CredentialAccess::GetImpl(credential)));
      });
}

Future<AuthResult> SignInWithProvider(AuthData* auth_data,
                                      FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->SignIn(auth_data);
}

void SignInAnonymously(AuthData* auth_data, Promise<AuthResult> promise) {
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
        return env->CallObjectMethod(
            AuthImpl(auth_data), auth::GetMethodId(auth::kSignInAnonymously));
      });
}

static void EmailPasswordAuthTask(AuthData* auth_data,
                                  Promise<AuthResult> promise,
                                  const char* email, const char* password,
                                  auth::Method method) {
  if (!email || email[0] == '\0' || !password || password[0] == '\0') {
    promise.Fail((!email || email[0] == '\0') ? kAuthErrorMissingEmail
                                              : kAuthErrorMissingPassword,
                 kErrorEmptyEmailPassword);
    return;
  }
  RunAndroidTaskWithPromise(
      auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
        jstring j_email = env->NewStringUTF(email);
        if (env->ExceptionCheck() || !j_email) {
          if (j_email) env->DeleteLocalRef(j_email);
          return static_cast<jobject>(nullptr);
        }
        jstring j_password = env->NewStringUTF(password);
        if (env->ExceptionCheck() || !j_password) {
          env->DeleteLocalRef(j_email);
          if (j_password) env->DeleteLocalRef(j_password);
          return static_cast<jobject>(nullptr);
        }
        jobject task = env->CallObjectMethod(
            AuthImpl(auth_data), auth::GetMethodId(method), j_email,
            j_password);
        env->DeleteLocalRef(j_email);
        env->DeleteLocalRef(j_password);
        return task;
      });
}

void SignInWithEmailAndPassword(AuthData* auth_data,
                                Promise<AuthResult> promise, const char* email,
                                const char* password) {
  EmailPasswordAuthTask(auth_data, promise, email, password,
                        auth::kSignInWithEmailAndPassword);
}

void CreateUserWithEmailAndPassword(AuthData* auth_data,
                                    Promise<AuthResult> promise,
                                    const char* email, const char* password) {
  EmailPasswordAuthTask(auth_data, promise, email, password,
                        auth::kCreateUserWithEmailAndPassword);
}

std::string Auth::language_code() const {
  JNIEnv* env = Env(auth_data_);
  jstring j_language_code = static_cast<jstring>(env->CallObjectMethod(
      AuthImpl(auth_data_), auth::GetMethodId(auth::kGetLanguageCode)));
  if (::firebase::jni::CheckAndClearException(env) || !j_language_code) {
    if (j_language_code) env->DeleteLocalRef(j_language_code);
    return std::string();
  }
  std::string result = ::firebase::jni::JStringToString(env, j_language_code);
  env->DeleteLocalRef(j_language_code);
  return result;
}

void Auth::set_language_code(const char* language_code) {
  if (language_code == nullptr || language_code[0] == '\0') {
    UseAppLanguage();
    return;
  }
  JNIEnv* env = Env(auth_data_);
  jstring j_language_code = env->NewStringUTF(language_code);
  if (!::firebase::jni::CheckAndClearException(env) && j_language_code) {
    env->CallVoidMethod(AuthImpl(auth_data_),
                        auth::GetMethodId(auth::kSetLanguageCode),
                        j_language_code);
    (void)::firebase::jni::CheckAndClearException(env);
  }
  if (j_language_code) env->DeleteLocalRef(j_language_code);
}

void Auth::UseAppLanguage() {
  JNIEnv* env = Env(auth_data_);
  env->CallVoidMethod(AuthImpl(auth_data_),
                      auth::GetMethodId(auth::kUseAppLanguage));
  (void)::firebase::jni::CheckAndClearException(env);
}

void SendPasswordResetEmail(AuthData* auth_data, Promise<void> promise,
                            const char* email) {
  if (!email || email[0] == '\0') {
    promise.Fail(kAuthErrorMissingEmail, "Empty email address.");
    return;
  }
  RunAndroidTaskWithPromise(auth_data, promise, nullptr, [=](JNIEnv* env) {
    jstring j_email = env->NewStringUTF(email);
    if (env->ExceptionCheck() || !j_email) {
      if (j_email) env->DeleteLocalRef(j_email);
      return static_cast<jobject>(nullptr);
    }
    jobject task = env->CallObjectMethod(
        AuthImpl(auth_data), auth::GetMethodId(auth::kSendPasswordResetEmail),
        j_email);
    env->DeleteLocalRef(j_email);
    return task;
  });
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  SetEmulatorJni(auth_data_, host.c_str(), port);
}

void EnableTokenAutoRefresh(AuthData* /*auth_data*/) {}
void DisableTokenAutoRefresh(AuthData* /*auth_data*/) {}

void SignOut(AuthData* auth_data) {
  if (auth_data == nullptr || AuthImpl(auth_data) == nullptr) return;
  JNIEnv* env = Env(auth_data);
  env->CallVoidMethod(AuthImpl(auth_data), auth::GetMethodId(auth::kSignOut));
  if (!::firebase::jni::CheckAndClearException(env)) {
    UpdateCurrentUser(auth_data);
  }
}

}  // namespace auth
}  // namespace firebase
