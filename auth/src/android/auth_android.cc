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

#include <assert.h>
#include <jni.h>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
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
  X(AddAuthStateListener, "addAuthStateListener",                              \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V"),           \
  X(RemoveAuthStateListener, "removeAuthStateListener",                        \
    "(Lcom/google/firebase/auth/FirebaseAuth$AuthStateListener;)V"),           \
  X(AddIdTokenListener, "addIdTokenListener",                                  \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V"),             \
  X(RemoveIdTokenListener, "removeIdTokenListener",                            \
    "(Lcom/google/firebase/auth/FirebaseAuth$IdTokenListener;)V"),             \
  X(SignOut, "signOut", "()V"),                                                \
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
  jnilistener::ReleaseClass(env);
  jni_id_token_listener::ReleaseClass(env);
}

void UpdateCurrentUser(AuthData* auth_data) {
  JNIEnv* env = Env(auth_data);

  MutexLock lock(auth_data->future_impl.mutex());

  const void* original_user_impl = auth_data->user_impl;

  // Update our pointer to the Android FirebaseUser that we're wrapping.
  jobject j_user = env->CallObjectMethod(
      AuthImpl(auth_data), auth::GetMethodId(auth::kGetCurrentUser));
  if (firebase::util::CheckAndClearJniExceptions(env)) {
    j_user = nullptr;
  }
  SetImplFromLocalRef(env, j_user, &auth_data->user_impl);

  // Log debug message when user sign-in status has changed.
  if (original_user_impl != auth_data->user_impl) {
    LogDebug("CurrentUser changed from %X to %X",
             reinterpret_cast<uintptr_t>(original_user_impl),
             reinterpret_cast<uintptr_t>(auth_data->user_impl));
  }
}

// Release cached Java classes.
static void ReleaseClasses(JNIEnv* env) {
  ReleaseAuthClasses(env);
  ReleaseUserClasses(env);
  ReleaseCredentialClasses(env);
  ReleaseCommonClasses(env);
}

void* CreatePlatformAuth(App* app) {
  // Grab varous java objects from the app.
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
  g_initialized_count++;

  // Create the FirebaseAuth class in Java.
  jobject platform_app = app->GetPlatformApp();
  jobject j_auth_impl = env->CallStaticObjectMethod(
      auth::GetClass(), auth::GetMethodId(auth::kGetInstance), platform_app);
  FIREBASE_ASSERT(!util::CheckAndClearJniExceptions(env));
  env->DeleteLocalRef(platform_app);

  // Ensure the reference hangs around.
  void* auth_impl = nullptr;
  SetImplFromLocalRef(env, j_auth_impl, &auth_impl);
  return auth_impl;
}

void Auth::InitPlatformAuth(AuthData* auth_data) {
  JNIEnv* env = Env(auth_data);

  // Create the JniAuthStateListener class to redirect the state-change
  // from Java to C++.
  jobject j_listener =
      env->NewObject(jnilistener::GetClass(),
                     jnilistener::GetMethodId(jnilistener::kConstructor),
                     reinterpret_cast<jlong>(auth_data));
  // Register the listener with the Java FirebaseAuth class.
  env->CallVoidMethod(AuthImpl(auth_data),
                      auth::GetMethodId(auth::kAddAuthStateListener),
                      j_listener);
  assert(env->ExceptionCheck() == false);
  // Convert listener from local to global ref, so it stays around.
  SetImplFromLocalRef(env, j_listener, &auth_data->listener_impl);

  // Create the JniIdTokenListener class to redirect the token changes
  // from Java to C++.
  jobject j_id_token_listener = env->NewObject(
      jni_id_token_listener::GetClass(),
      jni_id_token_listener::GetMethodId(jni_id_token_listener::kConstructor),
      reinterpret_cast<jlong>(auth_data));
  // Register the listener with the Java FirebaseAuth class.
  env->CallVoidMethod(AuthImpl(auth_data),
                      auth::GetMethodId(auth::kAddIdTokenListener),
                      j_id_token_listener);
  assert(env->ExceptionCheck() == false);
  // Convert listener from local to global ref, so it stays around.
  SetImplFromLocalRef(env, j_id_token_listener,
                      &auth_data->id_token_listener_impl);

  // Ensure our User is in-line with underlying API's user.
  // It's possible for a user to already be logged-in on start-up.
  UpdateCurrentUser(auth_data);
}

void Auth::DestroyPlatformAuth(AuthData* auth_data) {
  JNIEnv* env = Env(auth_data);

  util::CancelCallbacks(env, auth_data->future_api_id.c_str());

  // Unregister the JniAuthStateListener and IdTokenListener from the
  // FirebaseAuth class.
  env->CallVoidMethod(static_cast<jobject>(auth_data->listener_impl),
                      jnilistener::GetMethodId(jnilistener::kDisconnect));
  assert(env->ExceptionCheck() == false);
  env->CallVoidMethod(AuthImpl(auth_data),
                      auth::GetMethodId(auth::kRemoveAuthStateListener),
                      static_cast<jobject>(auth_data->listener_impl));
  assert(env->ExceptionCheck() == false);
  env->CallVoidMethod(
      static_cast<jobject>(auth_data->id_token_listener_impl),
      jni_id_token_listener::GetMethodId(jni_id_token_listener::kDisconnect));
  assert(env->ExceptionCheck() == false);
  env->CallVoidMethod(AuthImpl(auth_data),
                      auth::GetMethodId(auth::kRemoveIdTokenListener),
                      static_cast<jobject>(auth_data->id_token_listener_impl));
  assert(env->ExceptionCheck() == false);

  // Deleting our global references should trigger the FirebaseAuth class and
  // FirebaseUser Java objects to be deleted.
  SetImplFromLocalRef(env, nullptr, &auth_data->listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->id_token_listener_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->user_impl);
  SetImplFromLocalRef(env, nullptr, &auth_data->auth_impl);

  FIREBASE_ASSERT(g_initialized_count);
  g_initialized_count--;
  if (g_initialized_count == 0) {
    ReleaseClasses(env);
    util::Terminate(env);
  }
}

JNIEXPORT void JNICALL JniAuthStateListener_nativeOnAuthStateChanged(
    JNIEnv* env, jobject clazz, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  // Update our pointer to the Android FirebaseUser that we're wrapping.
  UpdateCurrentUser(auth_data);
  NotifyAuthStateListeners(auth_data);
}

JNIEXPORT void JNICALL JniIdTokenListener_nativeOnIdTokenChanged(
    JNIEnv* env, jobject clazz, jlong callback_data) {
  AuthData* auth_data = reinterpret_cast<AuthData*>(callback_data);
  auth_data->SetExpectIdTokenListenerCallback(false);
  // Update our pointer to the Android FirebaseUser that we're wrapping.
  UpdateCurrentUser(auth_data);
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
    assert(env->ExceptionCheck() == false);
    data->providers.resize(num_providers);

    for (int i = 0; i < num_providers; ++i) {
      // provider local reference is released in JniStringToString().
      jstring provider = static_cast<jstring>(env->CallObjectMethod(
          list, util::list::GetMethodId(util::list::kGet), i));
      assert(env->ExceptionCheck() == false);
      data->providers[i] = JniStringToString(env, provider);
    }
    env->DeleteLocalRef(list);
  }
}

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(
    const char* email) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<Auth::FetchProvidersResult>(
      kAuthFn_FetchProvidersForEmail);
  JNIEnv* env = Env(auth_data_);

  jstring j_email = env->NewStringUTF(email);
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_),
      auth::GetMethodId(auth::kFetchSignInMethodsForEmail), j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadProviderResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<User*> Auth::SignInWithCustomToken(const char* token) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kAuthFn_SignInWithCustomToken);
  JNIEnv* env = Env(auth_data_);

  jstring j_token = env->NewStringUTF(token);
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_), auth::GetMethodId(auth::kSignInWithCustomToken),
      j_token);
  env->DeleteLocalRef(j_token);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<User*> Auth::SignInWithCredential(const Credential& credential) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kAuthFn_SignInWithCredential);
  JNIEnv* env = Env(auth_data_);

  // If the credential itself is in an error state, don't try signing in.
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
  } else {
    jobject pending_result = env->CallObjectMethod(
        AuthImpl(auth_data_), auth::GetMethodId(auth::kSignInWithCredential),
        CredentialFromImpl(credential.impl_));

    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterCallback(pending_result, handle, auth_data_,
                       ReadUserFromSignInResult);
      env->DeleteLocalRef(pending_result);
    }
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> Auth::SignInAndRetrieveDataWithCredential(
    const Credential& credential) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<SignInResult>(
      kAuthFn_SignInAndRetrieveDataWithCredential);
  JNIEnv* env = Env(auth_data_);

  // If the credential itself is in an error state, don't try signing in.
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
  } else {
    jobject pending_result = env->CallObjectMethod(
        AuthImpl(auth_data_), auth::GetMethodId(auth::kSignInWithCredential),
        CredentialFromImpl(credential.impl_));

    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterCallback(pending_result, handle, auth_data_, ReadSignInResult);
      env->DeleteLocalRef(pending_result);
    }
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> Auth::SignInWithProvider(FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->SignIn(auth_data_);
}

Future<User*> Auth::SignInAnonymously() {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kAuthFn_SignInAnonymously);
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_), auth::GetMethodId(auth::kSignInAnonymously));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }

  return MakeFuture(&futures, handle);
}

Future<User*> Auth::SignInWithEmailAndPassword(const char* email,
                                               const char* password) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<User*>(kAuthFn_SignInWithEmailAndPassword);

  if (!email || strlen(email) == 0 || !password || strlen(password) == 0) {
    futures.Complete(handle,
                     (!email || strlen(email) == 0) ? kAuthErrorMissingEmail
                                                    : kAuthErrorMissingPassword,
                     kErrorEmptyEmailPassword);
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jstring j_email = env->NewStringUTF(email);
  jstring j_password = env->NewStringUTF(password);
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_),
      auth::GetMethodId(auth::kSignInWithEmailAndPassword), j_email,
      j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }

  return MakeFuture(&futures, handle);
}

Future<User*> Auth::CreateUserWithEmailAndPassword(const char* email,
                                                   const char* password) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<User*>(kAuthFn_CreateUserWithEmailAndPassword);

  if (!email || strlen(email) == 0 || !password || strlen(password) == 0) {
    futures.Complete(handle,
                     (!email || strlen(email) == 0) ? kAuthErrorMissingEmail
                                                    : kAuthErrorMissingPassword,
                     kErrorEmptyEmailPassword);
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jstring j_email = env->NewStringUTF(email);
  jstring j_password = env->NewStringUTF(password);
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_),
      auth::GetMethodId(auth::kCreateUserWithEmailAndPassword), j_email,
      j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

// It's safe to return a direct pointer to `current_user` because that class
// holds nothing but a pointer to AuthData, which never changes.
// All User functions that require synchronization go through AuthData's mutex.
User* Auth::current_user() {
  if (!auth_data_) return nullptr;
  MutexLock lock(auth_data_->future_impl.mutex());

  // auth_data_->current_user should be available after Auth is created because
  // persistent is loaded during the constructor of Android FirebaseAuth.
  // This may change to make FirebaseAuth.getCurrentUser() to block and wait for
  // persistent loading.  However, it is safe to access auth_data_->current_user
  // here since FirebaseAuth.getCurrentUser() (Android) is called in
  // InitPlatformAuth().
  User* user =
      auth_data_->user_impl == nullptr ? nullptr : &auth_data_->current_user;
  return user;
}

void Auth::SignOut() {
  JNIEnv* env = Env(auth_data_);
  env->CallVoidMethod(AuthImpl(auth_data_), auth::GetMethodId(auth::kSignOut));
  firebase::util::CheckAndClearJniExceptions(env);

  // Release our current user implementation in Java.
  MutexLock lock(auth_data_->future_impl.mutex());
  SetImplFromLocalRef(env, nullptr, &auth_data_->user_impl);
}

Future<void> Auth::SendPasswordResetEmail(const char* email) {
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kAuthFn_SendPasswordResetEmail);

  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email address.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);
  jstring j_email = env->NewStringUTF(email);
  jobject pending_result = env->CallObjectMethod(
      AuthImpl(auth_data_), auth::GetMethodId(auth::kSendPasswordResetEmail),
      j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

// Not implemented for Android.
void EnableTokenAutoRefresh(AuthData* auth_data) {}
void DisableTokenAutoRefresh(AuthData* auth_data) {}
void InitializeTokenRefresher(AuthData* auth_data) {}
void DestroyTokenRefresher(AuthData* auth_data) {}

}  // namespace auth
}  // namespace firebase
