#include "app/src/jni/env.h"
#include "app/src/jni/collection.h"
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

#include "app/src/jni/string.h"
#include "app/src/jni/exception.h"
#include <jni.h>
#include "app/src/jni/declaration.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using ::firebase::jni::JStringToString;

static Credential MakeErrorCredential(AuthError error, const char* msg) {
  Credential c;
  c.error_code_ = error;
  c.error_message_ = msg;
  return c;
}

template <typename Func>
static Credential GetCredentialFromJni(JNIEnv* env, Func&& func) {
  jobject j_cred = func();
  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  if (!j_cred || env->ExceptionCheck()) {
    error_code = CheckAndClearJniAuthExceptions(env, &error_message);
    if (error_code == kAuthErrorNone || error_code == kAuthErrorFailure) {
      error_code = kAuthErrorInvalidCredential;
    }
    j_cred = nullptr;
  }
  Credential cred = Credential(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = error_code;
    cred.error_message_ = error_message;
  }
  return cred;
}

#define CREDENTIAL_METHODS(X) \
    X(GetSignInMethod, "getSignInMethod", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(credential, CREDENTIAL_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/AuthCredential")

#define EMAIL_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)

JNI_METHOD_LOOKUP_DECLARATION(emailcred, EMAIL_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/EmailAuthProvider")

#define FACEBOOK_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(facebookcred, FACEBOOK_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FacebookAuthProvider")

#define GIT_HUB_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(githubcred, GIT_HUB_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/GithubAuthProvider")

#define GOOGLE_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(googlecred, GOOGLE_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/GoogleAuthProvider")

#define PLAY_GAMES_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic, jni::kMethodOptional)
JNI_METHOD_LOOKUP_DECLARATION(playgamescred, PLAY_GAMES_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PlayGamesAuthProvider")

#define TWITTER_CRED_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(twittercred, TWITTER_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/TwitterAuthProvider")

#define OAUTHPROVIDER_METHODS(X) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)"               \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic) \
    X(NewBuilder, "newBuilder",                                                \
      "(Ljava/lang/String;Lcom/google/firebase/auth/FirebaseAuth;)"            \
      "Lcom/google/firebase/auth/OAuthProvider$Builder;",                      \
      firebase::jni::kMethodTypeStatic) \
    X(NewCredentialBuilder, "newCredentialBuilder",                            \
      "(Ljava/lang/String;)"                                                   \
      "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;",            \
      firebase::jni::kMethodTypeStatic)

JNI_METHOD_LOOKUP_DECLARATION(oauthprovider, OAUTHPROVIDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider")

#define TIME_UNIT_METHODS(X) \
    X(ToMillis, "toMillis", "(J)J")
#define TIME_UNIT_FIELDS(X)                                                    \
    X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",         \
      jni::kFieldTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(timeunit, TIME_UNIT_METHODS, TIME_UNIT_FIELDS, "java/util/concurrent/TimeUnit")

#define PHONE_AUTH_OPTIONS_BUILDER_METHODS(X) \
  X(Constructor, "<init>",                                                     \
    "(Lcom/google/firebase/auth/FirebaseAuth;)V") \
  X(Build, "build",                                                            \
    "()Lcom/google/firebase/auth/PhoneAuthOptions;") \
  X(SetActivity, "setActivity", "(Landroid/app/Activity;)"                     \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;") \
  X(SetCallbacks, "setCallbacks",                                              \
    "(Lcom/google/firebase/auth/PhoneAuthProvider$"                            \
      "OnVerificationStateChangedCallbacks;)"                                  \
      "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;") \
  X(SetForceResendingToken, "setForceResendingToken",                          \
    "(Lcom/google/firebase/auth/PhoneAuthProvider$ForceResendingToken;)"       \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;") \
  X(SetPhoneNumber, "setPhoneNumber", "(Ljava/lang/String;)"                   \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;") \
  X(SetTimeout, "setTimeout",                                                  \
    "(Ljava/lang/Long;Ljava/util/concurrent/TimeUnit;)"                        \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")

JNI_METHOD_LOOKUP_DECLARATION(phone_auth_options_builder, PHONE_AUTH_OPTIONS_BUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PhoneAuthOptions$Builder")

#define PHONE_AUTH_PROVIDER_METHODS(X) \
    X(GetInstance, "getInstance",                                              \
      "(Lcom/google/firebase/auth/FirebaseAuth;)"                              \
      "Lcom/google/firebase/auth/PhoneAuthProvider;",                          \
      firebase::jni::kMethodTypeStatic) \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/PhoneAuthCredential;",                        \
      firebase::jni::kMethodTypeStatic) \
    X(VerifyPhoneNumber, "verifyPhoneNumber",                                  \
      "(Lcom/google/firebase/auth/PhoneAuthOptions;)V",                        \
      firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(phoneauthprovider, PHONE_AUTH_PROVIDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PhoneAuthProvider")

#define OAUTHPROVIDER_BUILDER_METHODS(X) \
  X(AddCustomParameters, "addCustomParameters",                                \
    "(Ljava/util/Map;)Lcom/google/firebase/auth/OAuthProvider$Builder;") \
  X(SetScopes, "setScopes",                                                    \
    "(Ljava/util/List;)Lcom/google/firebase/auth/OAuthProvider$Builder;") \
  X(Build, "build", "()Lcom/google/firebase/auth/OAuthProvider;")

JNI_METHOD_LOOKUP_DECLARATION(oauthprovider_builder, OAUTHPROVIDER_BUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider$Builder")

#define OAUTHPROVIDER_CREDENTIALBUILDER_METHODS(X) \
  X(SetAccessToken, "setAccessToken",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(SetIdToken, "setIdToken",                                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(SetIdTokenWithRawNonce, "setIdTokenWithRawNonce",                          \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(Build, "build", "()Lcom/google/firebase/auth/AuthCredential;")

JNI_METHOD_LOOKUP_DECLARATION(oauthprovider_credentialbuilder, OAUTHPROVIDER_CREDENTIALBUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider$CredentialBuilder")

#define AUTH_IDP_METHODS(X) \
  X(StartActivityForSignInWithProvider, "startActivityForSignInWithProvider",  \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")
JNI_METHOD_LOOKUP_DECLARATION(auth_idp, AUTH_IDP_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FirebaseAuth")

#define USER_IDP_METHODS(X) \
  X(StartActivityForLinkWithProvider, "startActivityForLinkWithProvider",      \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;") \
  X(StartActivityForReauthenticateWithProvider,                                \
    "startActivityForReauthenticateWithProvider",                              \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")

JNI_METHOD_LOOKUP_DECLARATION(user_idp, USER_IDP_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FirebaseUser")

#define JNI_PHONE_LISTENER_CALLBACK_METHODS(X) \
  X(Constructor, "<init>", "(J)V") \
  X(Disconnect, "disconnect", "()V")
JNI_METHOD_LOOKUP_DECLARATION(jniphone, JNI_PHONE_LISTENER_CALLBACK_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/internal/cpp/JniAuthPhoneListener")

// These static functions are wrapped in a class so that they can be "friends"
// of Credential. Only Credential's friends can create new Credentials from
// Java references to FirebaseCredentials.
class JniAuthPhoneListener {
 public:
  static JNIEXPORT void JNICALL nativeOnVerificationCompleted(
      JNIEnv* env, jobject instance, jlong c_listener, jobject j_credential);

  static JNIEXPORT void JNICALL
  nativeOnVerificationFailed(JNIEnv* env, jobject instance, jlong c_listener,
                             jstring exception_message);

  static JNIEXPORT void JNICALL
  nativeOnCodeSent(JNIEnv* env, jobject instance, jlong c_listener,
                   jstring j_verification_id, jobject j_force_resending_token);

  static JNIEXPORT void JNICALL
  nativeOnCodeAutoRetrievalTimeOut(JNIEnv* env, jobject instance,
                                   jlong c_listener, jstring j_verification_id);
};

static const JNINativeMethod kNativeJniAuthPhoneListenerMethods[] = {
    {"nativeOnVerificationCompleted",
     "(JLcom/google/firebase/auth/PhoneAuthCredential;)V",
     reinterpret_cast<void*>(
         JniAuthPhoneListener::nativeOnVerificationCompleted)},
    {"nativeOnVerificationFailed", "(JLjava/lang/String;)V",
     reinterpret_cast<void*>(JniAuthPhoneListener::nativeOnVerificationFailed)},
    {"nativeOnCodeSent",
     "(JLjava/lang/String;"
     "Lcom/google/firebase/auth/PhoneAuthProvider$ForceResendingToken;)V",
     reinterpret_cast<void*>(JniAuthPhoneListener::nativeOnCodeSent)},
    {"nativeOnCodeAutoRetrievalTimeOut", "(JLjava/lang/String;)V",
     reinterpret_cast<void*>(
         JniAuthPhoneListener::nativeOnCodeAutoRetrievalTimeOut)},
};

static std::atomic<bool> g_methods_cached{false};
static std::atomic<JavaVM*> g_jvm{nullptr};
static std::mutex g_credential_methods_mutex;

const char kMethodsNotCachedError[] =
    "Firebase Auth was not initialized, unable to create a Credential. "
    "Create an Auth instance first.";

static void CacheJavaVm(JNIEnv* env) {
  if (env != nullptr && g_jvm.load(std::memory_order_acquire) == nullptr) {
    JavaVM* jvm = nullptr;
    if (env->GetJavaVM(&jvm) == JNI_OK && jvm != nullptr) {
      g_jvm.store(jvm, std::memory_order_release);
    }
  }
}

static bool CacheCredentialClassesAndMethodsLocked(JNIEnv* env,
                                                   jobject activity) {
  if (g_methods_cached.load(std::memory_order_relaxed)) {
    return true;
  }
  const bool cached =
      credential::CacheMethodIds(env, activity) &&
      emailcred::CacheMethodIds(env, activity) &&
      facebookcred::CacheMethodIds(env, activity) &&
      githubcred::CacheMethodIds(env, activity) &&
      googlecred::CacheMethodIds(env, activity) &&
      oauthprovider::CacheMethodIds(env, activity) &&
      oauthprovider_builder::CacheMethodIds(env, activity) &&
      oauthprovider_credentialbuilder::CacheMethodIds(env, activity) &&
      auth_idp::CacheMethodIds(env, activity) &&
      user_idp::CacheMethodIds(env, activity) &&
      phone_auth_options_builder::CacheMethodIds(env, activity) &&
      phoneauthprovider::CacheMethodIds(env, activity) &&
      phonecredential::CacheMethodIds(env, activity) &&
      timeunit::CacheFieldIds(env, activity) &&
      playgamescred::CacheMethodIds(env, activity) &&
      twittercred::CacheMethodIds(env, activity);
  g_methods_cached.store(cached, std::memory_order_release);
  return cached;
}

bool CacheCredentialMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<::firebase::internal::EmbeddedFile>& embedded_files) {
  CacheJavaVm(env);
  std::lock_guard<std::mutex> lock(g_credential_methods_mutex);
  if (!(jniphone::CacheClassFromFiles(env, activity, &embedded_files) &&
        jniphone::CacheMethodIds(env, activity) &&
        jniphone::RegisterNatives(
            env, kNativeJniAuthPhoneListenerMethods,
            FIREBASE_ARRAYSIZE(kNativeJniAuthPhoneListenerMethods)))) {
    return false;
  }
  return CacheCredentialClassesAndMethodsLocked(env, activity);
}

void ReleaseCredentialClasses(JNIEnv* env) {
  CacheJavaVm(env);
  std::lock_guard<std::mutex> lock(g_credential_methods_mutex);
  auth_idp::ReleaseClass(env);
  credential::ReleaseClass(env);
  emailcred::ReleaseClass(env);
  facebookcred::ReleaseClass(env);
  githubcred::ReleaseClass(env);
  googlecred::ReleaseClass(env);
  playgamescred::ReleaseClass(env);
  if (jniphone::GetClass() != nullptr) {
    env->UnregisterNatives(jniphone::GetClass());
    jni::CheckAndClearException(env);
  }
  jniphone::ReleaseClass(env);
  oauthprovider::ReleaseClass(env);
  oauthprovider_builder::ReleaseClass(env);
  oauthprovider_credentialbuilder::ReleaseClass(env);
  phone_auth_options_builder::ReleaseClass(env);
  phoneauthprovider::ReleaseClass(env);
  phonecredential::ReleaseClass(env);
  timeunit::ReleaseClass(env);
  twittercred::ReleaseClass(env);
  user_idp::ReleaseClass(env);
  g_methods_cached.store(false, std::memory_order_release);
}

static JNIEnv* GetJniEnv() {
  // The JNI environment is the same regardless of App.
  App* app = app_common::GetAnyApp();
  if (app != nullptr) {
    JNIEnv* env = app->GetJNIEnv();
    CacheJavaVm(env);
    return env;
  }
  // Fall back to the cached JavaVM if all App instances have been destroyed,
  // ensuring Credential and ForceResendingToken destructors/operators can still
  // release global JNI references without leaking.
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  return jvm != nullptr ? util::GetThreadsafeJNIEnv(jvm) : nullptr;
}

static bool EnsureCredentialMethodsCached() {
  if (g_methods_cached.load(std::memory_order_acquire)) return true;
  std::lock_guard<std::mutex> lock(g_credential_methods_mutex);
  if (g_methods_cached.load(std::memory_order_relaxed)) return true;
  App* app = app_common::GetAnyApp();
  if (!app) return false;
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  if (!env) return false;
  CacheJavaVm(env);
  return CacheCredentialClassesAndMethodsLocked(env, activity);
}

// Remove the reference on destruction.
Credential::~Credential() {
  if (impl_ != nullptr) {
    JNIEnv* env = GetJniEnv();
    if (env != nullptr) {
      env->DeleteGlobalRef(static_cast<jobject>(impl_));
    }
    impl_ = nullptr;
  }
}

Credential::Credential(const Credential& rhs)
    : impl_(nullptr), error_code_(kAuthErrorNone) {
  *this = rhs;
}

// Increase the reference count when copying.
Credential& Credential::operator=(const Credential& rhs) {
  if (this == &rhs) return *this;
  if (impl_ != rhs.impl_) {
    JNIEnv* env = GetJniEnv();
    if (impl_ && env) {
      env->DeleteGlobalRef(static_cast<jobject>(impl_));
    }

    if (rhs.impl_ && env) {
      jobject j_cred_ref = env->NewGlobalRef(static_cast<jobject>(rhs.impl_));
      impl_ = static_cast<void*>(j_cred_ref);
    } else {
      impl_ = nullptr;
    }
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
  return *this;
}

bool Credential::operator==(const Credential& credential) const {
  if (impl_ == credential.impl_) {
    return error_code_ == credential.error_code_ &&
           error_message_ == credential.error_message_;
  }
  if (!impl_ || !credential.impl_) {
    return false;
  }
  JNIEnv* env = GetJniEnv();
  return env != nullptr &&
         env->IsSameObject(static_cast<jobject>(impl_),
                           static_cast<jobject>(credential.impl_)) &&
         error_code_ == credential.error_code_ &&
         error_message_ == credential.error_message_;
}

std::string Credential::provider() const {
  if (!impl_) return std::string();
  JNIEnv* env = GetJniEnv();
  if (!env || !EnsureCredentialMethodsCached()) return std::string();
  jobject j_provider = env->CallObjectMethod(
      CredentialFromImpl(impl_),
      credential::GetMethodId(credential::kGetSignInMethod));
  if (jni::CheckAndClearException(env) || !j_provider) {
    return std::string();
  }
  return JStringToString(env, j_provider);
}

bool Credential::is_valid() const { return impl_ != nullptr; }

static void* CredentialLocalToGlobalRef(jobject j_cred) {
  if (!j_cred) return nullptr;
  JNIEnv* env = GetJniEnv();
  if (!env) return nullptr;

  // Convert to global reference, so it stays around.
  jobject j_cred_ref = env->NewGlobalRef(j_cred);
  env->DeleteLocalRef(j_cred);
  return static_cast<void*>(j_cred_ref);
}

// static

#define FIREBASE_GET_CREDENTIAL_1(Provider, ProviderCred, Arg1, ErrMsg) \
Credential Provider::GetCredential(const char* Arg1) { \
  FIREBASE_ASSERT_RETURN(Credential(), Arg1); \
  if (Arg1[0] == '\0') return MakeErrorCredential(kAuthErrorMissingPassword, ErrMsg); \
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError); \
  JNIEnv* env = GetJniEnv(); \
  if (!env) return Credential(); \
  jstring j1 = env->NewStringUTF(Arg1); \
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(ProviderCred::GetClass(), ProviderCred::GetMethodId(ProviderCred::kGetCredential), j1); }); \
  env->DeleteLocalRef(j1); \
  return c; \
}

#define FIREBASE_GET_CREDENTIAL_2(Provider, ProviderCred, Arg1, Arg2, ErrMsg) \
Credential Provider::GetCredential(const char* Arg1, const char* Arg2) { \
  FIREBASE_ASSERT_RETURN(Credential(), Arg1 && Arg2); \
  if (Arg1[0] == '\0' || Arg2[0] == '\0') return MakeErrorCredential(kAuthErrorMissingPassword, ErrMsg); \
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError); \
  JNIEnv* env = GetJniEnv(); \
  if (!env) return Credential(); \
  jstring j1 = env->NewStringUTF(Arg1); \
  jstring j2 = env->NewStringUTF(Arg2); \
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(ProviderCred::GetClass(), ProviderCred::GetMethodId(ProviderCred::kGetCredential), j1, j2); }); \
  env->DeleteLocalRef(j1); \
  env->DeleteLocalRef(j2); \
  return c; \
}

FIREBASE_GET_CREDENTIAL_2(EmailAuthProvider, emailcred, email, password, "An email and password must be provided.")

//
// Provider methods
//
Credential InternalAuthResultProvider::GetCredential(jobject credential) {
  return Credential(CredentialLocalToGlobalRef(credential));
}

// static
FIREBASE_GET_CREDENTIAL_1(FacebookAuthProvider, facebookcred, access_token, "An access token must be provided.")

// static
FIREBASE_GET_CREDENTIAL_1(GitHubAuthProvider, githubcred, token, "A token must be provided.")

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token, const char* access_token) {
  if ((!id_token || id_token[0] == '\0') && (!access_token || access_token[0] == '\0')) return MakeErrorCredential(kAuthErrorMissingPassword, "An id_token or an access_token must be provided.");
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jstring ji = id_token ? env->NewStringUTF(id_token) : nullptr;
  jstring ja = access_token ? env->NewStringUTF(access_token) : nullptr;
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(googlecred::GetClass(), googlecred::GetMethodId(googlecred::kGetCredential), ji, ja); });
  if (ji) env->DeleteLocalRef(ji);
  if (ja) env->DeleteLocalRef(ja);
  return c;
}

// static
Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  FIREBASE_ASSERT_RETURN(Credential(), server_auth_code);
  if (server_auth_code[0] == '\0') return MakeErrorCredential(kAuthErrorMissingPassword, "A server auth code must be provided.");
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jmethodID get_cred_method = playgamescred::GetMethodId(playgamescred::kGetCredential);
  if (!playgamescred::GetClass() || !get_cred_method) return MakeErrorCredential(kAuthErrorApiNotAvailable, "PlayGamesAuthProvider is not available.");
  jstring jt = env->NewStringUTF(server_auth_code);
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(playgamescred::GetClass(), get_cred_method, jt); });
  env->DeleteLocalRef(jt);
  return c;
}

// static
FIREBASE_GET_CREDENTIAL_2(TwitterAuthProvider, twittercred, token, secret, "A token and secret must be provided.")

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token, const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id);
  if (provider_id[0] == '\0') return MakeErrorCredential(kAuthErrorMissingPassword, "A provider_id must be provided.");
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jstring jp = env->NewStringUTF(provider_id);
  jstring ji = id_token ? env->NewStringUTF(id_token) : nullptr;
  jstring ja = access_token ? env->NewStringUTF(access_token) : nullptr;
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(oauthprovider::GetClass(), oauthprovider::GetMethodId(oauthprovider::kGetCredential), jp, ji, ja); });
  env->DeleteLocalRef(jp);
  if (ji) env->DeleteLocalRef(ji);
  if (ja) env->DeleteLocalRef(ja);
  return c;
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token, const char* raw_nonce, const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id);
  if (provider_id[0] == '\0') return MakeErrorCredential(kAuthErrorMissingPassword, "A provider_id must be provided.");
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(), kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jstring jp = env->NewStringUTF(provider_id);
  jstring ji = id_token ? env->NewStringUTF(id_token) : nullptr;
  jstring jr = raw_nonce ? env->NewStringUTF(raw_nonce) : nullptr;
  jstring ja = access_token ? env->NewStringUTF(access_token) : nullptr;
  Credential c = GetCredentialFromJni(env, [&]() { return env->CallStaticObjectMethod(oauthprovider::GetClass(), oauthprovider::GetMethodId(oauthprovider::kGetCredential), jp, ji, jr, ja); });
  env->DeleteLocalRef(jp);
  if (ji) env->DeleteLocalRef(ji);
  if (jr) env->DeleteLocalRef(jr);
  if (ja) env->DeleteLocalRef(ja);
  return c;
}

// static
Future<Credential> GameCenterAuthProvider::GetCredential() {
  // Game Center is not available on Android
  auto future_api = GetCredentialFutureImpl();
  if (!future_api) return Future<Credential>();
  const auto handle =
      future_api->SafeAlloc<Credential>(kCredentialFn_GameCenterGetCredential);

  future_api->Complete(handle, kAuthErrorInvalidCredential,
                       "GameCenter is not supported on Android.");

  return MakeFuture(future_api, handle);
}

// static
Future<Credential> GameCenterAuthProvider::GetCredentialLastResult() {
  auto future_api = GetCredentialFutureImpl();
  if (!future_api) return Future<Credential>();
  return static_cast<const Future<Credential>&>(future_api->LastResult(kCredentialFn_GameCenterGetCredential));
}

// static
bool GameCenterAuthProvider::IsPlayerAuthenticated() {
  // Game Center is not available on Android, thus the player can never be
  // authenticated.
  return false;
}

// This implementation of ForceResendingTokenData is specific to Android.
class ForceResendingTokenData {
 public:
  ForceResendingTokenData() : token_global_ref_(nullptr) {}
  explicit ForceResendingTokenData(jobject token_ref)
      : token_global_ref_(nullptr) {
    SetRef(token_ref);
  }
  ~ForceResendingTokenData() { FreeRef(); }

  // token_ref can be a local or global reference.
  void SetRef(jobject token_ref) {
    if (token_ref == token_global_ref_) return;
    FreeRef();
    if (token_ref != nullptr) {
      JNIEnv* env = GetJniEnv();
      if (env != nullptr) {
        token_global_ref_ = env->NewGlobalRef(token_ref);
      }
    }
  }

  void FreeRef() {
    if (token_global_ref_ != nullptr) {
      JNIEnv* env = GetJniEnv();
      if (env != nullptr) {
        env->DeleteGlobalRef(token_global_ref_);
      }
      token_global_ref_ = nullptr;
    }
  }
  jobject token_global_ref() const { return token_global_ref_; }

 private:
  jobject token_global_ref_;
};

PhoneAuthProvider::ForceResendingToken::ForceResendingToken()
    : data_(nullptr) {}

PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() { delete data_; }

PhoneAuthProvider::ForceResendingToken::ForceResendingToken(
    const ForceResendingToken& rhs)
    : data_(nullptr) {
  *this = rhs;
}

PhoneAuthProvider::ForceResendingToken&
PhoneAuthProvider::ForceResendingToken::operator=(
    const ForceResendingToken& rhs) {
  if (this == &rhs) return *this;
  delete data_;
  data_ = nullptr;
  if (rhs.data_ != nullptr && rhs.data_->token_global_ref() != nullptr) {
    data_ = new ForceResendingTokenData(rhs.data_->token_global_ref());
  }
  return *this;
}

bool PhoneAuthProvider::ForceResendingToken::operator==(
    const ForceResendingToken& rhs) const {
  if (data_ == nullptr && rhs.data_ == nullptr) return true;
  if (data_ == nullptr || rhs.data_ == nullptr) return false;
  if (data_->token_global_ref() == rhs.data_->token_global_ref()) return true;
  if (data_->token_global_ref() == nullptr ||
      rhs.data_->token_global_ref() == nullptr) {
    return false;
  }
  JNIEnv* env = GetJniEnv();
  return env != nullptr &&
         env->IsSameObject(data_->token_global_ref(),
                           rhs.data_->token_global_ref());
}

bool PhoneAuthProvider::ForceResendingToken::operator!=(
    const ForceResendingToken& rhs) const {
  return !operator==(rhs);
}

// This implementation of PhoneAuthProviderData is specific to Android.
struct PhoneAuthProviderData {
  PhoneAuthProviderData()
      : auth_data(nullptr), j_phone_auth_provider(nullptr) {}

  // Back-pointer to structure that holds this one.
  AuthData* auth_data;

  // The Java PhoneAuthProvider class for this Auth instance.
  jobject j_phone_auth_provider;
};

// The `data_` pimpl is created lazily in @ref PhoneAuthProvider::GetInstance.
// This is necessary since the Java Auth class must be fully created to get
// `j_phone_auth_provider`.
PhoneAuthProvider::PhoneAuthProvider() : data_(nullptr) {}
PhoneAuthProvider::~PhoneAuthProvider() {
  if (data_ != nullptr) {
    if (data_->j_phone_auth_provider != nullptr) {
      JNIEnv* env = GetJniEnv();
      if (env != nullptr) {
        env->DeleteGlobalRef(data_->j_phone_auth_provider);
      }
      data_->j_phone_auth_provider = nullptr;
    }
    delete data_;
    data_ = nullptr;
  }
}

// This implementation of PhoneListenerData is specific to Android.
struct PhoneListenerData {
  PhoneListenerData() : j_listener(nullptr) {}

  // The JniAuthPhoneListener class that has the same lifespan as the C++ class.
  jobject j_listener;
};

PhoneAuthProvider::Listener::Listener() : data_(new PhoneListenerData) {
  JNIEnv* env = GetJniEnv();
  if (!env || !jniphone::GetClass()) return;

  // Create the JniAuthPhoneListener class to redirect the callbacks
  // from Java to C++. Make it a global reference so it sticks around.
  jobject j_listener_local = env->NewObject(
      jniphone::GetClass(), jniphone::GetMethodId(jniphone::kConstructor),
      reinterpret_cast<jlong>(this));
  if (!jni::CheckAndClearException(env) && j_listener_local != nullptr) {
    // The C++ Listener keeps a global reference to the Java Listener,
    // and the Java Listener keeps a pointer to the C++ Listener.
    // Destruction of the C++ Listener triggers disconnection of the Java
    // Listener.
    data_->j_listener = env->NewGlobalRef(j_listener_local);
  }
  if (j_listener_local != nullptr) {
    env->DeleteLocalRef(j_listener_local);
  }
}

PhoneAuthProvider::Listener::~Listener() {
  if (data_ != nullptr) {
    JNIEnv* env = GetJniEnv();
    if (env != nullptr && data_->j_listener != nullptr) {
      if (jniphone::GetClass() != nullptr) {
        // Disable the Java Listener by nulling its pointer to the C++ Listener
        // (which is being destroyed).
        env->CallVoidMethod(data_->j_listener,
                            jniphone::GetMethodId(jniphone::kDisconnect));
        jni::CheckAndClearException(env);
      }
      // Remove our reference to the Java Listener. Once Auth Java is done with
      // it, it will be garbage collected.
      env->DeleteGlobalRef(data_->j_listener);
      data_->j_listener = nullptr;
    }

    // Delete the Android-specific pimpl.
    delete data_;
    data_ = nullptr;
  }
}

void PhoneAuthProvider::VerifyPhoneNumber(const PhoneAuthOptions& options, PhoneAuthProvider::Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  if (!data_ || !data_->auth_data || !EnsureCredentialMethodsCached()) { listener->OnVerificationFailed("VerifyPhoneNumber: PhoneAuthProvider is not initialized."); return; }
  if (options.phone_number.empty()) { listener->OnVerificationFailed("VerifyPhoneNumber: unable to verify with empty phone number"); return; }
  JNIEnv* env = Env(data_->auth_data);
  if (!env) { listener->OnVerificationFailed("VerifyPhoneNumber: unable to acquire JNIEnv."); return; }
  auto set_err = [&](const char* m) { listener->OnVerificationFailed(m); };
  jobject b = env->NewObject(phone_auth_options_builder::GetClass(), phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kConstructor), AuthImpl(data_->auth_data));
  if (jni::CheckAndClearException(env) || !b) return set_err("VerifyPhoneNumber: unable to construct PhoneAuthOptions builder.");
  jobject act = options.ui_parent ? options.ui_parent : data_->auth_data->app->activity();
  jobject r = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kSetActivity), act);
  if (r) env->DeleteLocalRef(r);
  if (jni::CheckAndClearException(env)) { env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: unable set PhoneAuthOptions activity."); }
  r = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kSetCallbacks), listener->data_ ? listener->data_->j_listener : nullptr);
  if (r) env->DeleteLocalRef(r);
  if (jni::CheckAndClearException(env)) { env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: unable set PhoneAuthOptions listeners."); }
  jstring pn = env->NewStringUTF(options.phone_number.c_str());
  r = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kSetPhoneNumber), pn);
  if (r) env->DeleteLocalRef(r);
  env->DeleteLocalRef(pn);
  if (jni::CheckAndClearException(env)) { env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: unable set PhoneAuthOptions phone number."); }
  if (options.force_resending_token && options.force_resending_token->is_valid() && options.force_resending_token->data_ && options.force_resending_token->data_->token_global_ref()) {
    r = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kSetForceResendingToken), options.force_resending_token->data_->token_global_ref());
    if (r) env->DeleteLocalRef(r);
    if (jni::CheckAndClearException(env)) { env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: unable to set force resending token."); }
  }
  jobject timeout = env->NewObject(jni::long_class::GetClass(), jni::long_class::GetMethodId(jni::long_class::kConstructor), static_cast<jlong>(options.timeout_milliseconds));
  if (jni::CheckAndClearException(env) || !timeout) { env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: couldn't convert timeout to java.lang.Long."); }
  jobject tu = env->GetStaticObjectField(timeunit::GetClass(), timeunit::GetFieldId(timeunit::kMilliseconds));
  if (jni::CheckAndClearException(env) || !tu) { env->DeleteLocalRef(timeout); env->DeleteLocalRef(b); return set_err("VerifyPhoneNumber: couldn't retrieve TimeUnit.MILLISECONDS."); }
  r = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kSetTimeout), timeout, tu);
  std::string e_msg;
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone) {
    if (r) env->DeleteLocalRef(r); env->DeleteLocalRef(timeout); env->DeleteLocalRef(tu); env->DeleteLocalRef(b);
    return set_err(!e_msg.empty() ? e_msg.c_str() : "VerifyPhoneNumber: unable set PhoneAuthOptions timeout.");
  }
  if (r) env->DeleteLocalRef(r); env->DeleteLocalRef(timeout); env->DeleteLocalRef(tu);
  jobject po = env->CallObjectMethod(b, phone_auth_options_builder::GetMethodId(phone_auth_options_builder::kBuild));
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone || !po) {
    if (po) env->DeleteLocalRef(po); env->DeleteLocalRef(b);
    return set_err(!e_msg.empty() ? e_msg.c_str() : "VerifyPhoneNumber: builder failed to create PhoneAuthOptions");
  }
  env->DeleteLocalRef(b);
  env->CallStaticVoidMethod(phoneauthprovider::GetClass(), phoneauthprovider::GetMethodId(phoneauthprovider::kVerifyPhoneNumber), po);
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone) set_err(!e_msg.empty() ? e_msg.c_str() : "VerifyPhoneNumber: Android failed to verify phone number");
  env->DeleteLocalRef(po);
}

PhoneAuthCredential PhoneAuthProvider::GetCredential(
    const char* verification_id, const char* verification_code) {
  FIREBASE_ASSERT_RETURN(PhoneAuthCredential(),
                         verification_id && verification_code);
  if (strlen(verification_id) == 0) {
    PhoneAuthCredential cred;
    cred.error_code_ = kAuthErrorMissingVerificationId;
    cred.error_message_ = "A verification ID must be provided.";
    return cred;
  }
  if (strlen(verification_code) == 0) {
    PhoneAuthCredential cred;
    cred.error_code_ = kAuthErrorMissingVerificationCode;
    cred.error_message_ = "A verification code must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(PhoneAuthCredential(),
                                 EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env =
      (data_ && data_->auth_data) ? Env(data_->auth_data) : GetJniEnv();
  if (!env) return PhoneAuthCredential();

  jstring j_verification_id = env->NewStringUTF(verification_id);
  jstring j_verification_code = env->NewStringUTF(verification_code);

  jobject j_cred = env->CallStaticObjectMethod(
      phoneauthprovider::GetClass(),
      phoneauthprovider::GetMethodId(phoneauthprovider::kGetCredential),
      j_verification_id, j_verification_code);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }

  env->DeleteLocalRef(j_verification_id);
  env->DeleteLocalRef(j_verification_code);

  PhoneAuthCredential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  static PhoneAuthProvider kInvalidPhoneAuthProvider;
  FIREBASE_ASSERT_RETURN(kInvalidPhoneAuthProvider,
                         auth != nullptr && auth->auth_data_ != nullptr);
  MutexLock lock(auth->auth_data_->future_impl.mutex());
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (provider.data_ == nullptr) {
    JNIEnv* env = Env(auth->auth_data_);

    // Get a global reference to the Java PhoneAuthProvider for this Auth.
    jobject j_phone_auth_provider_local = env->CallStaticObjectMethod(
        phoneauthprovider::GetClass(),
        phoneauthprovider::GetMethodId(phoneauthprovider::kGetInstance),
        AuthImpl(auth->auth_data_));
    if (jni::CheckAndClearException(env)) {
      j_phone_auth_provider_local = nullptr;
    }

    // Create the implementation class that holds the global references.
    // The global references will be freed when provider is destroyed
    // (during the Auth destructor).
    provider.data_ = new PhoneAuthProviderData();
    if (j_phone_auth_provider_local != nullptr) {
      provider.data_->j_phone_auth_provider =
          env->NewGlobalRef(j_phone_auth_provider_local);
      env->DeleteLocalRef(j_phone_auth_provider_local);
    }
    provider.data_->auth_data = auth->auth_data_;
  }
  return provider;
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnVerificationCompleted().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationCompleted(
    JNIEnv* env, jobject j_listener, jlong c_listener, jobject j_credential) {
  if (!c_listener) return;
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  jobject global_cred =
      (j_credential != nullptr) ? env->NewGlobalRef(j_credential) : nullptr;
  listener->OnVerificationCompleted(PhoneAuthCredential(global_cred));
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnVerificationFailed().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationFailed(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring exception_message) {
  if (!c_listener) return;
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  std::string message =
      (exception_message != nullptr)
          ? jni::JStringToString(env, exception_message)
          : "Phone verification failed.";
  listener->OnVerificationFailed(message);
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnCodeSent().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeSent(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring j_verification_id, jobject j_force_resending_token) {
  if (!c_listener) return;
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);

  // Change the passed-in reference to a global reference that has the
  // lifespan of the ForceResendingToken.
  PhoneAuthProvider::ForceResendingToken token;
  if (j_force_resending_token != nullptr) {
    token.data_ = new ForceResendingTokenData(j_force_resending_token);
  }
  std::string verification_id =
      (j_verification_id != nullptr)
          ? jni::JStringToString(env, j_verification_id)
          : std::string();
  listener->OnCodeSent(verification_id, token);
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnCodeAutoRetrievalTimeOut().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeAutoRetrievalTimeOut(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring j_verification_id) {
  if (!c_listener) return;
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  std::string verification_id =
      (j_verification_id != nullptr)
          ? jni::JStringToString(env, j_verification_id)
          : std::string();
  listener->OnCodeAutoRetrievalTimeOut(verification_id);
}

// FederatedAuthHandlers
FederatedOAuthProvider::FederatedOAuthProvider() {}

FederatedOAuthProvider::FederatedOAuthProvider(
    const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

FederatedOAuthProvider::~FederatedOAuthProvider() {}

void FederatedOAuthProvider::SetProviderData(
    const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

#ifdef INTERNAL_EXPERIMENTAL
void FederatedOAuthProvider::SetAuthHandler(AuthHandler* handler) {
  handler_ = handler;
}
#endif  // INTERNAL_EXPERIMENTAL

namespace {
// Pulls data out of the C++ FederatedOAuthProviderData structure and
// constructs a com.google.firebase.OAuthProvider object.
// @note Any JNI exceptions encountered are cleared and recorded into
// `out_error_code` and `out_error_message`.
// @return A local reference to an OAuthProvider, or nullptr on failure.
jobject ConstructOAuthProvider(AuthData* auth_data, const FederatedOAuthProviderData& pd, AuthError* err, std::string* msg) {
  if (err) *err = kAuthErrorNone;
  if (msg) msg->clear();
  JNIEnv* env = Env(auth_data);
  if (!env) return nullptr;
  auto set_err = [&](AuthError e) {
    AuthError jni_err = CheckAndClearJniAuthExceptions(env, msg);
    if (err) *err = (jni_err != kAuthErrorNone) ? jni_err : e;
  };
  jstring pid = env->NewStringUTF(pd.provider_id.c_str());
  if (!pid) { set_err(kAuthErrorFailure); return nullptr; }
  jobject builder = env->CallStaticObjectMethod(oauthprovider::GetClass(), oauthprovider::GetMethodId(oauthprovider::kNewBuilder), pid, AuthImpl(auth_data));
  env->DeleteLocalRef(pid);
  if (env->ExceptionCheck() || !builder) { set_err(kAuthErrorFailure); return nullptr; }
  if (!pd.scopes.empty()) {
    jobject scopes = util::StdVectorToJavaList(env, pd.scopes);
    if (!scopes || env->ExceptionCheck()) { set_err(kAuthErrorFailure); env->DeleteLocalRef(builder); return nullptr; }
    jobject r = env->CallObjectMethod(builder, oauthprovider_builder::GetMethodId(oauthprovider_builder::kSetScopes), scopes);
    env->DeleteLocalRef(scopes);
    if (r) env->DeleteLocalRef(r);
    if (env->ExceptionCheck()) { set_err(kAuthErrorFailure); env->DeleteLocalRef(builder); return nullptr; }
  }
  if (!pd.custom_parameters.empty()) {
    jobject map = env->NewObject(jni::hash_map::GetClass(), jni::hash_map::GetMethodId(jni::hash_map::kConstructor));
    if (!map || env->ExceptionCheck()) { set_err(kAuthErrorFailure); env->DeleteLocalRef(builder); return nullptr; }
    util::StdMapToJavaMap(env, &map, pd.custom_parameters);
    jobject r = env->CallObjectMethod(builder, oauthprovider_builder::GetMethodId(oauthprovider_builder::kAddCustomParameters), map);
    env->DeleteLocalRef(map);
    if (r) env->DeleteLocalRef(r);
    if (env->ExceptionCheck()) { set_err(kAuthErrorFailure); env->DeleteLocalRef(builder); return nullptr; }
  }
  jobject prov = env->CallObjectMethod(builder, oauthprovider_builder::GetMethodId(oauthprovider_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (!prov || env->ExceptionCheck()) { set_err(kAuthErrorFailure); return nullptr; }
  return prov;
}
}  // namespace

Future<AuthResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data->future_impl, kUserFn_SignInWithProvider);
  if (provider_data_.provider_id.empty()) { promise.Fail(kAuthErrorInvalidProviderId, "Invalid provider ID."); return promise.future(); }
  AuthError error_code = kAuthErrorNone; std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_, &error_code, &error_message);
  if (!oauthprovider) { promise.Fail(error_code != kAuthErrorNone ? error_code : kAuthErrorFailure, error_message.c_str()); return promise.future(); }
  RunAndroidTaskWithPromise(auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
    jobject task = env->CallObjectMethod(AuthImpl(auth_data), auth_idp::GetMethodId(auth_idp::kStartActivityForSignInWithProvider), auth_data->app->activity(), oauthprovider);
    env->DeleteLocalRef(oauthprovider);
    return task;
  });
  return promise.future();
}

Future<AuthResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data->future_impl, kUserFn_LinkWithProvider);
  if (provider_data_.provider_id.empty()) { promise.Fail(kAuthErrorInvalidProviderId, "Invalid provider ID."); return promise.future(); }
  AuthError error_code = kAuthErrorNone; std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_, &error_code, &error_message);
  if (!oauthprovider) { promise.Fail(error_code != kAuthErrorNone ? error_code : kAuthErrorFailure, error_message.c_str()); return promise.future(); }
  RunAndroidTaskWithPromise(auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
    jobject user_local = AcquireUserLocalRef(env, auth_data);
    if (!user_local) { env->DeleteLocalRef(oauthprovider); return (jobject)nullptr; }
    jobject task = env->CallObjectMethod(user_local, user_idp::GetMethodId(user_idp::kStartActivityForLinkWithProvider), auth_data->app->activity(), oauthprovider);
    env->DeleteLocalRef(user_local); env->DeleteLocalRef(oauthprovider);
    return task;
  });
  return promise.future();
}

Future<AuthResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  Promise<AuthResult> promise(&auth_data->future_impl, kUserFn_ReauthenticateWithProvider);
  if (provider_data_.provider_id.empty()) { promise.Fail(kAuthErrorInvalidProviderId, "Invalid provider ID."); return promise.future(); }
  AuthError error_code = kAuthErrorNone; std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_, &error_code, &error_message);
  if (!oauthprovider) { promise.Fail(error_code != kAuthErrorNone ? error_code : kAuthErrorFailure, error_message.c_str()); return promise.future(); }
  RunAndroidTaskWithPromise(auth_data, promise, ReadAuthResult, [=](JNIEnv* env) {
    jobject user_local = AcquireUserLocalRef(env, auth_data);
    if (!user_local) { env->DeleteLocalRef(oauthprovider); return (jobject)nullptr; }
    jobject task = env->CallObjectMethod(user_local, user_idp::GetMethodId(user_idp::kStartActivityForReauthenticateWithProvider), auth_data->app->activity(), oauthprovider);
    env->DeleteLocalRef(user_local); env->DeleteLocalRef(oauthprovider);
    return task;
  });
  return promise.future();
}

