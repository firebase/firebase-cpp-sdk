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
#include "app/src/log.h"
#include "app/src/util_android.h"
#include "auth/auth_resources.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using util::JniStringToString;

// Used to setup the cache of Auth class method IDs to reduce time spent
// looking up methods by string.
// clang-format off
#define AUTH_METHODS(X)                                                        \
  X(GetInstance, "getInstance",                                                \
    "(Lcom/google/firebase/FirebaseApp;)"                                      \
    "Lcom/google/firebase/auth/FirebaseAuth;", util::kMethodTypeStatic),       \
  X(GetCurrentUser, "getCurrentUser",                                          \
    "()Lcom/google/firebase/auth/FirebaseUser;"),                              \
  X(GetLanguageCode, "getLanguageCode",                                        \
    "()Ljava/lang/String;"),                                                   \
  X(SetLanguageCode, "setLanguageCode",                                        \
    "(Ljava/lang/String;)V"),                                                  \
  X(UseAppLanguage, "useAppLanguage", "()V"),                                  \
  X(AddAuthStateListener, "addAuthStateListener",                              \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V"),           \
  X(RemoveAuthStateListener, "removeAuthStateListener",                        \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V"),           \
  X(AddIdTokenListener, "addIdTokenListener",                                  \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V"),             \
  X(RemoveIdTokenListener, "removeIdTokenListener",                            \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V"),             \
  X(SignOut, "signOut", "()V"),                                                \
  X(UseEmulator, "useEmulator", "(Ljava/lang/String;I)V"),                     \
  X(FetchSignInMethodsForEmail, "fetchSignInMethodsForEmail",                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SignInWithCustomToken, "signInWithCustomToken",                            \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SignInWithCredential, "signInWithCredential",                              \
    "(Lcom/google/firebase/auth/AuthCredential;)"                              \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SignInAnonymously, "signInAnonymously",                                    \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
  X(SignInWithEmailAndPassword, "signInWithEmailAndPassword",                  \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(CreateUserWithEmailAndPassword, "createUserWithEmailAndPassword",          \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SendPasswordResetEmail, "sendPasswordResetEmail",                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(auth, AUTH_METHODS)
METHOD_LOOKUP_DEFINITION(auth,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseAuth",
                         AUTH_METHODS)

// clang-format off
#define SIGNIN_METHOD_QUERY_RESULT_METHODS(X)                                  \
  X(GetSignInMethods, "getSignInMethods", "()Ljava/util/List;")
// clang-format on
METHOD_LOOKUP_DECLARATION(signinmethodquery, SIGNIN_METHOD_QUERY_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(signinmethodquery,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/SignInMethodQueryResult",
                         SIGNIN_METHOD_QUERY_RESULT_METHODS)

// clang-format off
#define JNI_LISTENER_CALLBACK_METHODS(X)                                       \
  X(Constructor, "<init>", "(J)V"),                                            \
  X(Disconnect, "disconnect", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(jnilistener, JNI_LISTENER_CALLBACK_METHODS)
METHOD_LOOKUP_DEFINITION(
    jnilistener, "com/google/firebase/auth/internal/cpp/JniAuthStateListener",
    JNI_LISTENER_CALLBACK_METHODS)

// clang-format off
#define JNI_ID_TOKEN_LISTENER_CALLBACK_METHODS(X)                              \
  X(Constructor, "<init>", "(J)V"),                                            \
  X(Disconnect, "disconnect", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(jni_id_token_listener,
                          JNI_ID_TOKEN_LISTENER_CALLBACK_METHODS)
METHOD_LOOKUP_DEFINITION(
    jni_id_token_listener,
    "com/google/firebase/auth/internal/cpp/JniIdTokenListener",
    JNI_ID_TOKEN_LISTENER_CALLBACK_METHODS)

static Mutex g_init_mutex;
static int g_initialized_count = 0;
static const char* kErrorEmptyEmailPassword =
    "Empty email or password are not allowed.";

JNIEXPORT void JNICALL JniAuthStateListener_nativeOnAuthStateChanged(
    JNIEnv* env, jobject clazz, jlong callback_data);

static const JNINativeMethod kNativeOnAuthStateChangedMethod = {
    "nativeOnAuthStateChanged", "(J)V",
    reinterpret_cast<void*>(JniAuthStateListener_nativeOnAuthStateChanged)};

JNIEXPORT void JNICALL JniIdTokenListener_nativeOnIdTokenChanged(
    JNIEnv* env, jobject clazz, jlong callback_data);

static const JNINativeMethod kNativeOnIdTokenChangedMethod = {
    "nativeOnIdTokenChanged", "(J)V",
    reinterpret_cast<void*>(JniIdTokenListener_nativeOnIdTokenChanged)};

bool CacheAuthMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  if (!(auth::CacheMethodIds(env, activity) &&
        signinmethodquery::CacheMethodIds(env, activity))) {
    return false;
  }

  // Cache the JniAuthStateListener and JniIdTokenListener classes.
  if (!(jnilistener::CacheClassFromFiles(env, activity, &embedded_files) &&
        jnilistener::CacheMethodIds(env, activity) &&
        jnilistener::RegisterNatives(env, &kNativeOnAuthStateChangedMethod,
                                     1) &&
        jni_id_token_listener::CacheClassFromFiles(env, activity,
                                                   &embedded_files) &&
        jni_id_token_listener::CacheMethodIds(env, activity) &&
        jni_id_token_listener::RegisterNatives(
            env, &kNativeOnIdTokenChangedMethod, 1))) {
    return false;
  }

  return true;
}

void ReleaseAuthClasses(JNIEnv* env) {
  auth::ReleaseClass(env);
  signinmethodquery::ReleaseClass(env);
  if (jnilistener::GetClass() != nullptr) {
    env->UnregisterNatives(jnilistener::GetClass());
    util::CheckAndClearJniExceptions(env);
  }
  jnilistener::ReleaseClass(env);
  if (jni_id_token_listener::GetClass() != nullptr) {
    env->UnregisterNatives(jni_id_token_listener::GetClass());
    util::CheckAndClearJniExceptions(env);
  }
  jni_id_token_listener::ReleaseClass(env);
}

void UpdateCurrentUser(AuthData* auth_data) {
  // Hold future_impl.mutex() across both the FirebaseAuth.getCurrentUser() read
  // and the auth_data->user_impl update so concurrent UpdateCurrentUser calls
  // are serialized and a slower thread cannot overwrite auth_data->user_impl
  // with a stale FirebaseUser reading. FirebaseAuth.getCurrentUser() is a
  // non-blocking field getter that neither acquires Java listener locks nor
  // invokes callbacks.
  MutexLock lock(auth_data->future_impl.mutex());
  JNIEnv* env = Env(auth_data);

  jobject j_user = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kGetCurrentUser));
  if (firebase::util::CheckAndClearJniExceptions(env)) {
    if (j_user != nullptr) {
      env->DeleteLocalRef(j_user);
    }
    j_user = nullptr;
  }

  jobject original_user_impl = static_cast<jobject>(auth_data->user_impl);
  const bool user_changed = !env->IsSameObject(original_user_impl, j_user);

  if (user_changed) {
    // Update our pointer to the Android FirebaseUser that we're wrapping.
    SetImplFromLocalRef(env, j_user, &auth_data->user_impl);
    LogDebug("CurrentUser changed from %p to %p", original_user_impl,
             auth_data->user_impl);
  } else if (j_user != nullptr) {
    env->DeleteLocalRef(j_user);
  }
}

void SetEmulatorJni(AuthData* auth_data, const char* host, uint32_t port) {
  if (!auth_data || !host || host[0] == '\0' || port == 0 || port > 65535) {
    return;
  }
  JNIEnv* env = Env(auth_data);

  jstring j_host = env->NewStringUTF(host);
  if (!firebase::util::CheckAndClearJniExceptions(env) && j_host != nullptr) {
    env->CallVoidMethod(AuthImpl(auth_data),
                        auth::GetMethodId(auth::kUseEmulator), j_host,
                        static_cast<jint>(port));
    firebase::util::CheckAndClearJniExceptions(env);
  }
  if (j_host != nullptr) {
    env->DeleteLocalRef(j_host);
  }
}

const char* const kEmulatorLocalHost = "10.0.2.2";
const uint32_t kDefaultEmulatorPort = 9099;
void CheckEmulator(AuthData* auth_data) {
  const char* use_emulator = std::getenv("USE_AUTH_EMULATOR");
  if (use_emulator == nullptr || use_emulator[0] == '\0' ||
      std::strcmp(use_emulator, "0") == 0 ||
      std::strcmp(use_emulator, "false") == 0) {
    return;
  }

  LogInfo("Using Auth Emulator for testing.");

  // Use AUTH_EMULATOR_PORT if it is set to a valid port, otherwise use the
  // default port.
  uint32_t port = kDefaultEmulatorPort;
  const char* env_port = std::getenv("AUTH_EMULATOR_PORT");
  if (env_port != nullptr && env_port[0] != '\0') {
    char* end = nullptr;
    const long parsed_port = std::strtol(env_port, &end, 10);
    if (end != env_port && *end == '\0' && parsed_port > 0 &&
        parsed_port <= 65535) {
      port = static_cast<uint32_t>(parsed_port);
    }
  }
  SetEmulatorJni(auth_data, kEmulatorLocalHost, port);
}

// Release cached Java classes.
static void ReleaseClasses(JNIEnv* env) {
  ReleaseAuthClasses(env);
  ReleaseUserClasses(env);
  ReleaseCredentialClasses(env);
  ReleaseCommonClasses(env);
}

void* CreatePlatformAuth(App* app) {
  if (app == nullptr) return nullptr;

  MutexLock init_lock(g_init_mutex);

  // Grab various java objects from the app.
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();

  // Cache the JNI method ids so we only have to look them up by name once.
  if (!g_initialized_count) {
    if (!util::Initialize(env, activity)) return nullptr;

    // Cache embedded files and load embedded classes.
    const std::vector<internal::EmbeddedFile> embedded_files =
        util::CacheEmbeddedFiles(env, activity,
                                 internal::EmbeddedFile::ToVector(
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

  // Create the FirebaseAuth class in Java.
  jobject platform_app = app->GetPlatformApp();
  jobject j_auth_impl = env->CallStaticObjectMethod(
      auth::GetClass(), auth::GetMethodId(auth::kGetInstance), platform_app);
  const bool exception_thrown = util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(platform_app);
  if (exception_thrown || j_auth_impl == nullptr) {
    if (j_auth_impl != nullptr) {
      env->DeleteLocalRef(j_auth_impl);
    }
    if (!g_initialized_count) {
      ReleaseClasses(env);
      util::Terminate(env);
    }
    return nullptr;
  }

  g_initialized_count++;

  // Ensure the reference hangs around.
  void* auth_impl = nullptr;
  SetImplFromLocalRef(env, j_auth_impl, &auth_impl);
  return auth_impl;
}

void Auth::InitPlatformAuth(AuthData* auth_data) {
  JNIEnv* env = Env(auth_data);

  // Ensure our User is in-line with underlying API's user before registering
  // listeners. It's possible for a user to already be logged-in on start-up.
  UpdateCurrentUser(auth_data);

  CheckEmulator(auth_data);

  // Create the JniAuthStateListener class to redirect the state-change
  // from Java to C++.
  jobject j_listener =
      env->NewObject(jnilistener::GetClass(),
                     jnilistener::GetMethodId(jnilistener::kConstructor),
                     reinterpret_cast<jlong>(auth_data));
  if (!util::CheckAndClearJniExceptions(env) && j_listener != nullptr) {
    // Register the listener with the Java FirebaseAuth class.
    env->CallVoidMethod(AuthImpl(auth_data),
                        auth::GetMethodId(auth::kAddAuthStateListener),
                        j_listener);
    util::CheckAndClearJniExceptions(env);
    // Convert listener from local to global ref, so it stays around.
    SetImplFromLocalRef(env, j_listener, &auth_data->listener_impl);
  } else if (j_listener != nullptr) {
    env->DeleteLocalRef(j_listener);
  }

  // Create the JniIdTokenListener class to redirect the token changes
  // from Java to C++.
  jobject j_id_token_listener = env->NewObject(
      jni_id_token_listener::GetClass(),
      jni_id_token_listener::GetMethodId(jni_id_token_listener::kConstructor),
      reinterpret_cast<jlong>(auth_data));
  if (!util::CheckAndClearJniExceptions(env) &&
      j_id_token_listener != nullptr) {
    // Register the listener with the Java FirebaseAuth class.
    env->CallVoidMethod(AuthImpl(auth_data),
                        auth::GetMethodId(auth::kAddIdTokenListener),
                        j_id_token_listener);
    util::CheckAndClearJniExceptions(env);
    // Convert listener from local to global ref, so it stays around.
    SetImplFromLocalRef(env, j_id_token_listener,
                        &auth_data->id_token_listener_impl);
  } else if (j_id_token_listener != nullptr) {
    env->DeleteLocalRef(j_id_token_listener);
  }
}

void Auth::DestroyPlatformAuth(AuthData* auth_data) {
  JNIEnv* env = Env(auth_data);

  util::CancelCallbacks(env, auth_data->future_api_id.c_str());
  ClearPretriggeredIdTokenCallbacks(auth_data);

  // Unregister the JniAuthStateListener and IdTokenListener from the
  // FirebaseAuth class.
  if (auth_data->listener_impl != nullptr) {
    env->CallVoidMethod(static_cast<jobject>(auth_data->listener_impl),
                        jnilistener::GetMethodId(jnilistener::kDisconnect));
    util::CheckAndClearJniExceptions(env);
    env->CallVoidMethod(AuthImpl(auth_data),
                        auth::GetMethodId(auth::kRemoveAuthStateListener),
                        static_cast<jobject>(auth_data->listener_impl));
    util::CheckAndClearJniExceptions(env);
  }
  if (auth_data->id_token_listener_impl != nullptr) {
    env->CallVoidMethod(
        static_cast<jobject>(auth_data->id_token_listener_impl),
        jni_id_token_listener::GetMethodId(jni_id_token_listener::kDisconnect));
    util::CheckAndClearJniExceptions(env);
    env->CallVoidMethod(
        AuthImpl(auth_data), auth::GetMethodId(auth::kRemoveIdTokenListener),
        static_cast<jobject>(auth_data->id_token_listener_impl));
    util::CheckAndClearJniExceptions(env);
  }

  // Deleting our global references should trigger the FirebaseAuth class and
  // FirebaseUser Java objects to be deleted.
  SetImplFromLocalRef(env, nullptr, &auth_data->listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->id_token_listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->user_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->auth_impl);

  MutexLock init_lock(g_init_mutex);
  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_initialized_count == 0) {
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

void LogHeartbeat(Auth* auth) { (void)auth; }

JNIEXPORT void JNICALL JniAuthStateListener_nativeOnAuthStateChanged(
    JNIEnv* env, jobject clazz, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  if (!auth_data) return;
  // Update our pointer to the Android FirebaseUser that we're wrapping.
  UpdateCurrentUser(auth_data);
  NotifyAuthStateListeners(auth_data);
}

JNIEXPORT void JNICALL JniIdTokenListener_nativeOnIdTokenChanged(
    JNIEnv* env, jobject clazz, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  if (!auth_data) return;
  // Update our pointer to the Android FirebaseUser that we're wrapping.
  UpdateCurrentUser(auth_data);
  if (!auth_data->ShouldTriggerIdTokenListenerCallback() &&
      ConsumePretriggeredIdTokenCallback(auth_data)) {
    return;
  }
  NotifyIdTokenListeners(auth_data);
}

// Record the provider data returned from Java.
static void ReadProviderResult(
    jobject result, FutureCallbackData<Auth::FetchProvidersResult>* d,
    bool success, void* void_data) {
  auto data = static_cast<Auth::FetchProvidersResult*>(void_data);
  JNIEnv* env = Env(d->auth_data);

  // `result` comes from the successfully completed Task in Java. If the Task
  // completed successfully, `result` should be valid.
  FIREBASE_ASSERT(!success || result != nullptr);
  // `result` is of type SignInMethodQueryResult when `success` is true.
  jobject list = success
                     ? env->CallObjectMethod(
                           result, signinmethodquery::GetMethodId(
                                       signinmethodquery::kGetSignInMethods))
                     : nullptr;
  if (firebase::util::CheckAndClearJniExceptions(env)) list = nullptr;

  // `list` is of type List<String>. Loop through it.
  if (list != nullptr) {
    const int num_providers =
        env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
    if (firebase::util::CheckAndClearJniExceptions(env) || num_providers <= 0) {
      env->DeleteLocalRef(list);
      return;
    }
    data->providers.resize(num_providers);

    for (int i = 0; i < num_providers; ++i) {
      // provider local reference is released in JniStringToString().
      jstring provider = static_cast<jstring>(env->CallObjectMethod(
          list, util::list::GetMethodId(util::list::kGet), i));
      if (firebase::util::CheckAndClearJniExceptions(env)) {
        provider = nullptr;
      }
      data->providers[i] = JniStringToString(env, provider);
    }
    env->DeleteLocalRef(list);
  }
}

void FetchProvidersForEmail(AuthData* auth_data, Promise<Auth::FetchProvidersResult> promise, const char* email) {
  const auto promise.handle() = futures.SafeAlloc<Auth::FetchProvidersResult>(
      kAuthFn_FetchProvidersForEmail);
  if (!email || strlen(email) == 0) {
    futures.Complete(promise.handle(), kAuthErrorMissingEmail, "Empty email address.");
  }
  JNIEnv* env = Env(auth_data);

  jstring j_email = env->NewStringUTF(email);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    if (j_email != nullptr) env->DeleteLocalRef(j_email);
  }
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data),
      auth::GetMethodId(auth::kFetchSignInMethodsForEmail), j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, ReadProviderResult);
    env->DeleteLocalRef(pending_result);
  }
}

void SignInWithCustomToken(AuthData* auth_data, Promise<AuthResult> promise, const char* token) {
  if (!token || strlen(token) == 0) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCustomToken,
                     "Empty custom token.");
  }
  JNIEnv* env = Env(auth_data);

  jstring j_token = env->NewStringUTF(token);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    if (j_token != nullptr) env->DeleteLocalRef(j_token);
  }
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kSignInWithCustomToken),
      j_token);
  env->DeleteLocalRef(j_token);

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
}

void SignInWithCredential(AuthData* auth_data, Promise<User> promise, const Credential& credential) {
  JNIEnv* env = Env(auth_data);

  // If the credential itself is in an error state, don't try signing in.
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_,
                     credential.error_message_.c_str());
  } else if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential,
                     "Invalid credential.");
  } else {
    jobject pending_result = env->CallObjectMethod(
        AuthImpl(auth_data), auth::GetMethodId(auth::kSignInWithCredential),
        CredentialFromImpl(credential.impl_));

    if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
      RegisterCallback(pending_result, promise.handle(), auth_data,
                       ReadUserFromAuthResult);
      env->DeleteLocalRef(pending_result);
    }
  }
}

void SignInAndRetrieveDataWithCredential(AuthData* auth_data, Promise<AuthResult> promise, const Credential& credential) {
  const auto promise.handle() = futures.SafeAlloc<AuthResult>(
      kAuthFn_SignInAndRetrieveDataWithCredential);
  JNIEnv* env = Env(auth_data);

  // If the credential itself is in an error state, don't try signing in.
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_,
                     credential.error_message_.c_str());
  } else if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential,
                     "Invalid credential.");
  } else {
    jobject pending_result = env->CallObjectMethod(
        AuthImpl(auth_data), auth::GetMethodId(auth::kSignInWithCredential),
        CredentialFromImpl(credential.impl_));

    if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
      RegisterCallback(pending_result, promise.handle(), auth_data, ReadAuthResult);
      env->DeleteLocalRef(pending_result);
    }
  }
}

void SignInWithProvider(AuthData* auth_data, Promise<AuthResult> promise, FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->SignIn(auth_data);
}

void SignInAnonymously(AuthData* auth_data, Promise<AuthResult> promise) {
  JNIEnv* env = Env(auth_data);

  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kSignInAnonymously));

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
}

void SignInWithEmailAndPassword(AuthData* auth_data, Promise<AuthResult> promise, const char* email, const char* password) {

  if (!email || strlen(email) == 0 || !password || strlen(password) == 0) {
    futures.Complete(promise.handle(),
                     (!email || strlen(email) == 0) ? kAuthErrorMissingEmail
                                                    : kAuthErrorMissingPassword,
                     kErrorEmptyEmailPassword);
  }

  JNIEnv* env = Env(auth_data);
  jstring j_email = env->NewStringUTF(email);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    if (j_email != nullptr) env->DeleteLocalRef(j_email);
  }
  jstring j_password = env->NewStringUTF(password);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    env->DeleteLocalRef(j_email);
    if (j_password != nullptr) env->DeleteLocalRef(j_password);
  }
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data),
      auth::GetMethodId(auth::kSignInWithEmailAndPassword), j_email,
      j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
}

void CreateUserWithEmailAndPassword(AuthData* auth_data, Promise<AuthResult> promise, const char* email, const char* password) {

  if (!email || strlen(email) == 0 || !password || strlen(password) == 0) {
    futures.Complete(promise.handle(),
                     (!email || strlen(email) == 0) ? kAuthErrorMissingEmail
                                                    : kAuthErrorMissingPassword,
                     kErrorEmptyEmailPassword);
  }
  JNIEnv* env = Env(auth_data);

  jstring j_email = env->NewStringUTF(email);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    if (j_email != nullptr) env->DeleteLocalRef(j_email);
  }
  jstring j_password = env->NewStringUTF(password);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    env->DeleteLocalRef(j_email);
    if (j_password != nullptr) env->DeleteLocalRef(j_password);
  }
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data),
      auth::GetMethodId(auth::kCreateUserWithEmailAndPassword), j_email,
      j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
}


std::string Auth::language_code() const {
  JNIEnv* env = Env(auth_data_);
  jobject j_pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_), auth::GetMethodId(auth::kGetLanguageCode));
  if (firebase::util::CheckAndClearJniExceptions(env) ||
      j_pending_result == nullptr) {
    return std::string();
  }
  return util::JniStringToString(env, j_pending_result);
}

void Auth::set_language_code(const char* language_code) {
  if (language_code == nullptr || language_code[0] == '\0') {
    UseAppLanguage();
    return;
  }
  JNIEnv* env = Env(auth_data_);
  jstring j_language_code = env->NewStringUTF(language_code);
  if (!firebase::util::CheckAndClearJniExceptions(env) &&
      j_language_code != nullptr) {
    env->CallVoidMethod(AuthImpl(auth_data_),
                        auth::GetMethodId(auth::kSetLanguageCode),
                        j_language_code);
    firebase::util::CheckAndClearJniExceptions(env);
  }
  if (j_language_code != nullptr) {
    env->DeleteLocalRef(j_language_code);
  }
}

void Auth::UseAppLanguage() {
  JNIEnv* env = Env(auth_data_);
  env->CallVoidMethod(AuthImpl(auth_data_),
                      auth::GetMethodId(auth::kUseAppLanguage));
  firebase::util::CheckAndClearJniExceptions(env);
}


void SendPasswordResetEmail(AuthData* auth_data, Promise<void> promise, const char* email) {

  if (!email || strlen(email) == 0) {
    futures.Complete(promise.handle(), kAuthErrorMissingEmail, "Empty email address.");
  }
  JNIEnv* env = Env(auth_data);
  jstring j_email = env->NewStringUTF(email);
  if (CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    if (j_email != nullptr) env->DeleteLocalRef(j_email);
  }
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kSendPasswordResetEmail),
      j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl, promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data, nullptr);
    env->DeleteLocalRef(pending_result);
  }
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  SetEmulatorJni(auth_data_, host.c_str(), port);
}

// Not implemented for Android.
void EnableTokenAutoRefresh(AuthData* auth_data) {}
void DisableTokenAutoRefresh(AuthData* auth_data) {}

}  // namespace auth
}  // namespace firebase
