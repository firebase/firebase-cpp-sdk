// Copyright 2016 Google LLC
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

#include <jni.h>

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
#include "app/src/jni/collection.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using ::firebase::jni::JStringToString;

#define CREDENTIAL_METHODS(X) \
  X(GetSignInMethod, "getSignInMethod", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(credential, CREDENTIAL_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/AuthCredential")

#define EMAIL_CRED_METHODS(X)                    \
  X(GetCredential, "getCredential",              \
    "(Ljava/lang/String;Ljava/lang/String;)"     \
    "Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(emailcred, EMAIL_CRED_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/EmailAuthProvider")

#define FACEBOOK_CRED_METHODS(X)                                     \
  X(GetCredential, "getCredential",                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(facebookcred, FACEBOOK_CRED_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FacebookAuthProvider")

#define GIT_HUB_CRED_METHODS(X)                                      \
  X(GetCredential, "getCredential",                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(githubcred, GIT_HUB_CRED_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/GithubAuthProvider")

#define GOOGLE_CRED_METHODS(X)                   \
  X(GetCredential, "getCredential",              \
    "(Ljava/lang/String;Ljava/lang/String;)"     \
    "Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(googlecred, GOOGLE_CRED_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/GoogleAuthProvider")

#define PLAY_GAMES_CRED_METHODS(X)                                   \
  X(GetCredential, "getCredential",                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic, jni::kMethodOptional)
JNI_METHOD_LOOKUP_DECLARATION(playgamescred, PLAY_GAMES_CRED_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/PlayGamesAuthProvider")

#define TWITTER_CRED_METHODS(X)                  \
  X(GetCredential, "getCredential",              \
    "(Ljava/lang/String;Ljava/lang/String;)"     \
    "Lcom/google/firebase/auth/AuthCredential;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(twittercred, TWITTER_CRED_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/TwitterAuthProvider")

#define OAUTHPROVIDER_METHODS(X)                                  \
  X(GetCredential, "getCredential",                               \
    "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)"    \
    "Lcom/google/firebase/auth/AuthCredential;",                  \
    firebase::jni::kMethodTypeStatic)                             \
  X(NewBuilder, "newBuilder",                                     \
    "(Ljava/lang/String;Lcom/google/firebase/auth/FirebaseAuth;)" \
    "Lcom/google/firebase/auth/OAuthProvider$Builder;",           \
    firebase::jni::kMethodTypeStatic)                             \
  X(NewCredentialBuilder, "newCredentialBuilder",                 \
    "(Ljava/lang/String;)"                                        \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;", \
    firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(oauthprovider, OAUTHPROVIDER_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/OAuthProvider")

#define TIME_UNIT_METHODS(X) X(ToMillis, "toMillis", "(J)J")
#define TIME_UNIT_FIELDS(X)                                          \
  X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;", \
    jni::kFieldTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(timeunit, TIME_UNIT_METHODS, TIME_UNIT_FIELDS,
                              "java/util/concurrent/TimeUnit")

#define PHONE_AUTH_OPTIONS_BUILDER_METHODS(X)                                  \
  X(Constructor, "<init>", "(Lcom/google/firebase/auth/FirebaseAuth;)V")       \
  X(Build, "build", "()Lcom/google/firebase/auth/PhoneAuthOptions;")           \
  X(SetActivity, "setActivity",                                                \
    "(Landroid/app/Activity;)"                                                 \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")                     \
  X(SetCallbacks, "setCallbacks",                                              \
    "(Lcom/google/firebase/auth/PhoneAuthProvider$"                            \
    "OnVerificationStateChangedCallbacks;)"                                    \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")                     \
  X(SetForceResendingToken, "setForceResendingToken",                          \
    "(Lcom/google/firebase/auth/PhoneAuthProvider$ForceResendingToken;)"       \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")                     \
  X(SetPhoneNumber, "setPhoneNumber",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")                     \
  X(SetTimeout, "setTimeout",                                                  \
    "(Ljava/lang/Long;Ljava/util/concurrent/TimeUnit;)"                        \
    "Lcom/google/firebase/auth/PhoneAuthOptions$Builder;")
JNI_METHOD_LOOKUP_DECLARATION(
    phone_auth_options_builder, PHONE_AUTH_OPTIONS_BUILDER_METHODS,
    FIELD_LOOKUP_NONE, "com/google/firebase/auth/PhoneAuthOptions$Builder")

#define PHONE_AUTH_PROVIDER_METHODS(X)                 \
  X(GetInstance, "getInstance",                        \
     "(Lcom/google/firebase/auth/FirebaseAuth;)"       \
     "Lcom/google/firebase/auth/PhoneAuthProvider;",   \
     firebase::jni::kMethodTypeStatic)                 \
  X(GetCredential, "getCredential",                    \
     "(Ljava/lang/String;Ljava/lang/String;)"          \
     "Lcom/google/firebase/auth/PhoneAuthCredential;", \
     firebase::jni::kMethodTypeStatic)                 \
  X(VerifyPhoneNumber, "verifyPhoneNumber",            \
     "(Lcom/google/firebase/auth/PhoneAuthOptions;)V", \
     firebase::jni::kMethodTypeStatic)
JNI_METHOD_LOOKUP_DECLARATION(phoneauthprovider, PHONE_AUTH_PROVIDER_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/PhoneAuthProvider")

#define OAUTHPROVIDER_BUILDER_METHODS(X)                                  \
  X(AddCustomParameters, "addCustomParameters",                           \
    "(Ljava/util/Map;)Lcom/google/firebase/auth/OAuthProvider$Builder;")  \
  X(SetScopes, "setScopes",                                               \
    "(Ljava/util/List;)Lcom/google/firebase/auth/OAuthProvider$Builder;") \
  X(Build, "build", "()Lcom/google/firebase/auth/OAuthProvider;")
JNI_METHOD_LOOKUP_DECLARATION(
    oauthprovider_builder, OAUTHPROVIDER_BUILDER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/OAuthProvider$Builder")

#define OAUTHPROVIDER_CREDENTIALBUILDER_METHODS(X)                \
  X(SetAccessToken, "setAccessToken",                             \
    "(Ljava/lang/String;)"                                        \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(SetIdToken, "setIdToken",                                     \
    "(Ljava/lang/String;)"                                        \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(SetIdTokenWithRawNonce, "setIdTokenWithRawNonce",             \
    "(Ljava/lang/String;Ljava/lang/String;)"                      \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;") \
  X(Build, "build", "()Lcom/google/firebase/auth/AuthCredential;")
JNI_METHOD_LOOKUP_DECLARATION(
    oauthprovider_credentialbuilder, OAUTHPROVIDER_CREDENTIALBUILDER_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/OAuthProvider$CredentialBuilder")

#define AUTH_IDP_METHODS(X)                                                    \
  X(StartActivityForSignInWithProvider, "startActivityForSignInWithProvider",  \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")
JNI_METHOD_LOOKUP_DECLARATION(auth_idp, AUTH_IDP_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FirebaseAuth")

#define USER_IDP_METHODS(X)                                                    \
  X(StartActivityForLinkWithProvider, "startActivityForLinkWithProvider",      \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")                                     \
  X(StartActivityForReauthenticateWithProvider,                                \
    "startActivityForReauthenticateWithProvider",                              \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")
JNI_METHOD_LOOKUP_DECLARATION(user_idp, USER_IDP_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FirebaseUser")

#define JNI_PHONE_LISTENER_CALLBACK_METHODS(X) \
  X(Constructor, "<init>", "(J)V")             \
  X(Disconnect, "disconnect", "()V")
JNI_METHOD_LOOKUP_DECLARATION(
    jniphone, JNI_PHONE_LISTENER_CALLBACK_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/internal/cpp/JniAuthPhoneListener")

class JniAuthPhoneListener {
 public:
  static JNIEXPORT void JNICALL nativeOnVerificationCompleted(
      JNIEnv* env, jobject instance, jlong c_listener, jobject j_credential);
  static JNIEXPORT void JNICALL nativeOnVerificationFailed(
      JNIEnv* env, jobject instance, jlong c_listener, jstring exception_msg);
  static JNIEXPORT void JNICALL nativeOnCodeSent(
      JNIEnv* env, jobject instance, jlong c_listener, jstring j_ver_id,
      jobject j_force_resending_token);
  static JNIEXPORT void JNICALL nativeOnCodeAutoRetrievalTimeOut(
      JNIEnv* env, jobject instance, jlong c_listener, jstring j_ver_id);
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
  if (env && !g_jvm.load(std::memory_order_acquire)) {
    JavaVM* jvm = nullptr;
    if (env->GetJavaVM(&jvm) == JNI_OK && jvm) {
      g_jvm.store(jvm, std::memory_order_release);
    }
  }
}

static bool CacheCredentialClassesAndMethodsLocked(JNIEnv* env,
                                                   jobject activity) {
  if (g_methods_cached.load(std::memory_order_relaxed)) return true;
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
    (void)jni::CheckAndClearException(env);
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
  if (App* app = app_common::GetAnyApp()) {
    JNIEnv* env = app->GetJNIEnv();
    CacheJavaVm(env);
    return env;
  }
  JavaVM* jvm = g_jvm.load(std::memory_order_acquire);
  return jvm ? util::GetThreadsafeJNIEnv(jvm) : nullptr;
}

static bool EnsureCredentialMethodsCached() {
  if (g_methods_cached.load(std::memory_order_acquire)) return true;
  std::lock_guard<std::mutex> lock(g_credential_methods_mutex);
  if (g_methods_cached.load(std::memory_order_relaxed)) return true;
  App* app = app_common::GetAnyApp();
  if (!app) return false;
  JNIEnv* env = app->GetJNIEnv();
  if (!env) return false;
  CacheJavaVm(env);
  return CacheCredentialClassesAndMethodsLocked(env, app->activity());
}

Credential::~Credential() {
  if (impl_) {
    if (JNIEnv* env = GetJniEnv()) {
      env->DeleteGlobalRef(static_cast<jobject>(impl_));
    }
    impl_ = nullptr;
  }
}

Credential::Credential(const Credential& rhs)
    : impl_(nullptr), error_code_(kAuthErrorNone) {
  *this = rhs;
}

Credential& Credential::operator=(const Credential& rhs) {
  if (this == &rhs) return *this;
  if (impl_ != rhs.impl_) {
    JNIEnv* env = GetJniEnv();
    if (impl_ && env) env->DeleteGlobalRef(static_cast<jobject>(impl_));
    impl_ = (rhs.impl_ && env)
                ? static_cast<void*>(
                      env->NewGlobalRef(static_cast<jobject>(rhs.impl_)))
                : nullptr;
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
  if (!impl_ || !credential.impl_) return false;
  JNIEnv* env = GetJniEnv();
  return env &&
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
  if (jni::CheckAndClearException(env) || !j_provider) return std::string();
  return JStringToString(env, j_provider);
}

bool Credential::is_valid() const { return impl_ != nullptr; }

std::string PhoneAuthCredential::sms_code() const {
  if (!sms_code_.empty()) return sms_code_;
  if (!impl_) return std::string();
  JNIEnv* env = GetJniEnv();
  if (!env || !EnsureCredentialMethodsCached()) return std::string();
  jobject j_sms_code = env->CallObjectMethod(
      CredentialFromImpl(impl_),
      phonecredential::GetMethodId(phonecredential::kGetSmsCode));
  if (jni::CheckAndClearException(env) || !j_sms_code) return std::string();
  return JStringToString(env, j_sms_code);
}

static void* CredentialLocalToGlobalRef(jobject j_cred) {
  if (!j_cred) return nullptr;
  JNIEnv* env = GetJniEnv();
  if (!env) return nullptr;
  jobject j_cred_ref = env->NewGlobalRef(j_cred);
  env->DeleteLocalRef(j_cred);
  return static_cast<void*>(j_cred_ref);
}

static Credential MakeErrorCredential(AuthError error, const char* msg) {
  return InternalAuthResultProvider::GetCredential(nullptr, error, msg);
}

struct JStr {
  JNIEnv* env;
  jstring s;
  JStr(JNIEnv* e, const char* c)
      : env(e), s((c && c[0] != '\0') ? e->NewStringUTF(c) : nullptr) {}
  ~JStr() {
    if (s) env->DeleteLocalRef(s);
  }
};

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
  return j_cred ? InternalAuthResultProvider::GetCredential(j_cred)
                : InternalAuthResultProvider::GetCredential(nullptr, error_code,
                                                            error_message);
}

template <typename Func>
static Credential CallCredJni(Func&& func) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  return env ? GetCredentialFromJni(env, [&]() { return func(env); })
             : Credential();
}

Credential InternalAuthResultProvider::GetCredential(jobject credential) {
  return Credential(CredentialLocalToGlobalRef(credential));
}

Credential EmailAuthProvider::GetCredential(const char* email,
                                            const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  if (!email[0])
    return MakeErrorCredential(kAuthErrorMissingEmail,
                               "An email address must be provided.");
  if (!password[0])
    return MakeErrorCredential(kAuthErrorMissingPassword,
                               "A password must be provided.");
  return CallCredJni([&](JNIEnv* env) {
    JStr j1(env, email), j2(env, password);
    return env->CallStaticObjectMethod(
        emailcred::GetClass(),
        emailcred::GetMethodId(emailcred::kGetCredential), j1.s, j2.s);
  });
}

#define FIREBASE_GET_CREDENTIAL_1(Provider, ProviderCred, Arg1, ErrMsg)      \
  Credential Provider::GetCredential(const char* Arg1) {                     \
    FIREBASE_ASSERT_RETURN(Credential(), Arg1);                              \
    if (!Arg1[0])                                                            \
      return MakeErrorCredential(kAuthErrorInvalidCredential, ErrMsg);       \
    return CallCredJni([&](JNIEnv* env) {                                    \
      JStr j1(env, Arg1);                                                    \
      return env->CallStaticObjectMethod(                                    \
          ProviderCred::GetClass(),                                          \
          ProviderCred::GetMethodId(ProviderCred::kGetCredential), j1.s);    \
    });                                                                      \
  }

FIREBASE_GET_CREDENTIAL_1(FacebookAuthProvider, facebookcred, access_token,
                          "An access token must be provided.")
FIREBASE_GET_CREDENTIAL_1(GitHubAuthProvider, githubcred, token,
                          "A token must be provided.")

Credential GoogleAuthProvider::GetCredential(const char* id_token,
                                             const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), id_token || access_token);
  if ((!id_token || !id_token[0]) && (!access_token || !access_token[0])) {
    return MakeErrorCredential(
        kAuthErrorInvalidCredential,
        "Either an ID token or an access token must be supplied.");
  }
  return CallCredJni([&](JNIEnv* env) {
    JStr ji(env, id_token), ja(env, access_token);
    return env->CallStaticObjectMethod(
        googlecred::GetClass(),
        googlecred::GetMethodId(googlecred::kGetCredential), ji.s, ja.s);
  });
}

Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  FIREBASE_ASSERT_RETURN(Credential(), server_auth_code);
  if (!server_auth_code[0]) {
    return MakeErrorCredential(kAuthErrorInvalidCredential,
                               "A server auth code must be provided.");
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jmethodID get_cred_method =
      playgamescred::GetMethodId(playgamescred::kGetCredential);
  if (!playgamescred::GetClass() || !get_cred_method) {
    return MakeErrorCredential(kAuthErrorApiNotAvailable,
                               "PlayGamesAuthProvider is not available.");
  }
  return GetCredentialFromJni(env, [&]() {
    JStr jt(env, server_auth_code);
    return env->CallStaticObjectMethod(playgamescred::GetClass(),
                                       get_cred_method, jt.s);
  });
}

Credential TwitterAuthProvider::GetCredential(const char* token,
                                              const char* secret) {
  FIREBASE_ASSERT_RETURN(Credential(), token && secret);
  if (!token[0] || !secret[0]) {
    return MakeErrorCredential(kAuthErrorInvalidCredential,
                               "A Twitter token and secret must be provided.");
  }
  return CallCredJni([&](JNIEnv* env) {
    JStr j1(env, token), j2(env, secret);
    return env->CallStaticObjectMethod(
        twittercred::GetClass(),
        twittercred::GetMethodId(twittercred::kGetCredential), j1.s, j2.s);
  });
}

Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(),
                         provider_id && (id_token || access_token));
  if (!provider_id[0]) {
    return MakeErrorCredential(kAuthErrorInvalidProviderId,
                               "A provider ID must be provided.");
  }
  if ((!id_token || !id_token[0]) && (!access_token || !access_token[0])) {
    return MakeErrorCredential(
        kAuthErrorInvalidCredential,
        "An ID token or access token must be provided.");
  }
  return CallCredJni([&](JNIEnv* env) {
    JStr jp(env, provider_id), ji(env, id_token), ja(env, access_token);
    return env->CallStaticObjectMethod(
        oauthprovider::GetClass(),
        oauthprovider::GetMethodId(oauthprovider::kGetCredential), jp.s, ji.s,
        ja.s);
  });
}

Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* raw_nonce,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && raw_nonce);
  if (!provider_id[0]) {
    return MakeErrorCredential(kAuthErrorInvalidProviderId,
                               "A provider ID must be provided.");
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);
  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  JStr jp(env, provider_id), ji(env, id_token), jr(env, raw_nonce);
  jobject builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewCredentialBuilder), jp.s);
  jobject j_cred = nullptr;
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) == kAuthErrorNone &&
      builder) {
    jobject r = env->CallObjectMethod(
        builder,
        oauthprovider_credentialbuilder::GetMethodId(
            oauthprovider_credentialbuilder::kSetIdTokenWithRawNonce),
        ji.s, jr.s);
    if (r) env->DeleteLocalRef(r);
    if (CheckAndClearJniAuthExceptions(env, &error_message) == kAuthErrorNone) {
      if (access_token) {
        JStr ja(env, access_token);
        r = env->CallObjectMethod(
            builder,
            oauthprovider_credentialbuilder::GetMethodId(
                oauthprovider_credentialbuilder::kSetAccessToken),
            ja.s);
        if (r) env->DeleteLocalRef(r);
      }
      if (CheckAndClearJniAuthExceptions(env, &error_message) ==
          kAuthErrorNone) {
        j_cred = env->CallObjectMethod(
            builder, oauthprovider_credentialbuilder::GetMethodId(
                         oauthprovider_credentialbuilder::kBuild));
        if (CheckAndClearJniAuthExceptions(env, &error_message) !=
            kAuthErrorNone) {
          if (j_cred) env->DeleteLocalRef(j_cred);
          j_cred = nullptr;
        }
      }
    }
    env->DeleteLocalRef(builder);
  }
  return j_cred ? InternalAuthResultProvider::GetCredential(j_cred)
                : InternalAuthResultProvider::GetCredential(
                      nullptr, kAuthErrorInvalidCredential, error_message);
}

Future<Credential> GameCenterAuthProvider::GetCredential() {
  auto future_api = GetCredentialFutureImpl();
  if (!future_api) return Future<Credential>();
  const auto handle =
      future_api->SafeAlloc<Credential>(kCredentialFn_GameCenterGetCredential);
  future_api->Complete(handle, kAuthErrorInvalidCredential,
                       "GameCenter is not supported on Android.");
  return MakeFuture(future_api, handle);
}

Future<Credential> GameCenterAuthProvider::GetCredentialLastResult() {
  auto future_api = GetCredentialFutureImpl();
  if (!future_api) return Future<Credential>();
  return static_cast<const Future<Credential>&>(
      future_api->LastResult(kCredentialFn_GameCenterGetCredential));
}

bool GameCenterAuthProvider::IsPlayerAuthenticated() { return false; }

class ForceResendingTokenData {
 public:
  ForceResendingTokenData() : token_global_ref_(nullptr) {}
  explicit ForceResendingTokenData(jobject token_ref)
      : token_global_ref_(nullptr) {
    SetRef(token_ref);
  }
  ~ForceResendingTokenData() { FreeRef(); }
  void SetRef(jobject token_ref) {
    if (token_ref == token_global_ref_) return;
    FreeRef();
    if (token_ref) {
      if (JNIEnv* env = GetJniEnv()) {
        token_global_ref_ = env->NewGlobalRef(token_ref);
      }
    }
  }
  void FreeRef() {
    if (token_global_ref_) {
      if (JNIEnv* env = GetJniEnv()) {
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
  data_ = (rhs.data_ && rhs.data_->token_global_ref())
              ? new ForceResendingTokenData(rhs.data_->token_global_ref())
              : nullptr;
  return *this;
}
bool PhoneAuthProvider::ForceResendingToken::operator==(
    const ForceResendingToken& rhs) const {
  if (!data_ && !rhs.data_) return true;
  if (!data_ || !rhs.data_) return false;
  if (data_->token_global_ref() == rhs.data_->token_global_ref()) return true;
  if (!data_->token_global_ref() || !rhs.data_->token_global_ref())
    return false;
  JNIEnv* env = GetJniEnv();
  return env &&
         env->IsSameObject(data_->token_global_ref(),
                           rhs.data_->token_global_ref());
}
bool PhoneAuthProvider::ForceResendingToken::operator!=(
    const ForceResendingToken& rhs) const {
  return !operator==(rhs);
}

struct PhoneAuthProviderData {
  AuthData* auth_data = nullptr;
  jobject j_phone_auth_provider = nullptr;
};

PhoneAuthProvider::PhoneAuthProvider() : data_(nullptr) {}
PhoneAuthProvider::~PhoneAuthProvider() {
  if (data_) {
    if (data_->j_phone_auth_provider) {
      if (JNIEnv* env = GetJniEnv()) {
        env->DeleteGlobalRef(data_->j_phone_auth_provider);
      }
    }
    delete data_;
    data_ = nullptr;
  }
}

struct PhoneListenerData {
  jobject j_listener = nullptr;
};

PhoneAuthProvider::Listener::Listener() : data_(new PhoneListenerData) {
  JNIEnv* env = GetJniEnv();
  if (!env || !jniphone::GetClass()) return;
  jobject j_listener_local = env->NewObject(
      jniphone::GetClass(), jniphone::GetMethodId(jniphone::kConstructor),
      reinterpret_cast<jlong>(this));
  if (!jni::CheckAndClearException(env) && j_listener_local) {
    data_->j_listener = env->NewGlobalRef(j_listener_local);
  }
  if (j_listener_local) env->DeleteLocalRef(j_listener_local);
}

PhoneAuthProvider::Listener::~Listener() {
  if (data_) {
    JNIEnv* env = GetJniEnv();
    if (env && data_->j_listener) {
      if (jniphone::GetClass()) {
        env->CallVoidMethod(data_->j_listener,
                            jniphone::GetMethodId(jniphone::kDisconnect));
        (void)jni::CheckAndClearException(env);
      }
      env->DeleteGlobalRef(data_->j_listener);
    }
    delete data_;
    data_ = nullptr;
  }
}

void PhoneAuthProvider::VerifyPhoneNumber(
    const PhoneAuthOptions& options, PhoneAuthProvider::Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  if (!data_ || !data_->auth_data || !EnsureCredentialMethodsCached()) {
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: PhoneAuthProvider is not initialized.");
    return;
  }
  if (options.phone_number.empty()) {
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable to verify with empty phone number");
    return;
  }
  JNIEnv* env = Env(data_->auth_data);
  if (!env) {
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable to acquire JNIEnv.");
    return;
  }
  auto set_err = [&](const char* m) { listener->OnVerificationFailed(m); };
  jobject b = env->NewObject(phone_auth_options_builder::GetClass(),
                             phone_auth_options_builder::GetMethodId(
                                 phone_auth_options_builder::kConstructor),
                             AuthImpl(data_->auth_data));
  if (jni::CheckAndClearException(env) || !b) {
    return set_err(
        "VerifyPhoneNumber: unable to construct PhoneAuthOptions builder.");
  }
  auto call_builder_obj = [&](phone_auth_options_builder::Method m, jobject arg,
                              const char* err_msg) {
    jobject r = env->CallObjectMethod(
        b, phone_auth_options_builder::GetMethodId(m), arg);
    if (r) env->DeleteLocalRef(r);
    if (jni::CheckAndClearException(env)) {
      env->DeleteLocalRef(b);
      set_err(err_msg);
      return false;
    }
    return true;
  };
  if (!call_builder_obj(
          phone_auth_options_builder::kSetActivity,
          options.ui_parent ? options.ui_parent
                            : data_->auth_data->app->activity(),
          "VerifyPhoneNumber: unable set PhoneAuthOptions activity.") ||
      !call_builder_obj(
          phone_auth_options_builder::kSetCallbacks,
          listener->data_ ? listener->data_->j_listener : nullptr,
          "VerifyPhoneNumber: unable set PhoneAuthOptions listeners.") ||
      !call_builder_obj(
          phone_auth_options_builder::kSetPhoneNumber,
          JStr(env, options.phone_number.c_str()).s,
          "VerifyPhoneNumber: unable set PhoneAuthOptions phone number.")) {
    return;
  }
  if (options.force_resending_token &&
      options.force_resending_token->is_valid() &&
      options.force_resending_token->data_ &&
      options.force_resending_token->data_->token_global_ref() &&
      !call_builder_obj(
          phone_auth_options_builder::kSetForceResendingToken,
          options.force_resending_token->data_->token_global_ref(),
          "VerifyPhoneNumber: unable to set force resending token.")) {
    return;
  }
  jobject timeout = env->NewObject(
      jni::long_class::GetClass(),
      jni::long_class::GetMethodId(jni::long_class::kConstructor),
      static_cast<jlong>(options.timeout_milliseconds));
  if (jni::CheckAndClearException(env) || !timeout) {
    env->DeleteLocalRef(b);
    return set_err(
        "VerifyPhoneNumber: couldn't convert timeout to java.lang.Long.");
  }
  jobject tu = env->GetStaticObjectField(
      timeunit::GetClass(), timeunit::GetFieldId(timeunit::kMilliseconds));
  if (jni::CheckAndClearException(env) || !tu) {
    env->DeleteLocalRef(timeout);
    env->DeleteLocalRef(b);
    return set_err(
        "VerifyPhoneNumber: couldn't retrieve TimeUnit.MILLISECONDS.");
  }
  jobject r = env->CallObjectMethod(
      b,
      phone_auth_options_builder::GetMethodId(
          phone_auth_options_builder::kSetTimeout),
      timeout, tu);
  if (r) env->DeleteLocalRef(r);
  env->DeleteLocalRef(timeout);
  env->DeleteLocalRef(tu);
  std::string e_msg;
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone) {
    env->DeleteLocalRef(b);
    return set_err(
        !e_msg.empty()
            ? e_msg.c_str()
            : "VerifyPhoneNumber: unable set PhoneAuthOptions timeout.");
  }
  jobject po = env->CallObjectMethod(
      b, phone_auth_options_builder::GetMethodId(
             phone_auth_options_builder::kBuild));
  env->DeleteLocalRef(b);
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone || !po) {
    if (po) env->DeleteLocalRef(po);
    return set_err(
        !e_msg.empty()
            ? e_msg.c_str()
            : "VerifyPhoneNumber: builder failed to create PhoneAuthOptions");
  }
  env->CallStaticVoidMethod(
      phoneauthprovider::GetClass(),
      phoneauthprovider::GetMethodId(phoneauthprovider::kVerifyPhoneNumber),
      po);
  if (CheckAndClearJniAuthExceptions(env, &e_msg) != kAuthErrorNone) {
    set_err(!e_msg.empty()
                ? e_msg.c_str()
                : "VerifyPhoneNumber: Android failed to verify phone number");
  }
  env->DeleteLocalRef(po);
}

PhoneAuthCredential PhoneAuthProvider::GetCredential(
    const char* verification_id, const char* verification_code) {
  FIREBASE_ASSERT_RETURN(PhoneAuthCredential(),
                         verification_id && verification_code);
  auto make_err = [](AuthError code, const char* msg) {
    PhoneAuthCredential cred;
    cred.error_code_ = code;
    cred.error_message_ = msg;
    return cred;
  };
  if (!verification_id[0]) {
    return make_err(kAuthErrorMissingVerificationId,
                    "A verification ID must be provided.");
  }
  if (!verification_code[0]) {
    return make_err(kAuthErrorMissingVerificationCode,
                    "A verification code must be provided.");
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(PhoneAuthCredential(),
                                 EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);
  JNIEnv* env =
      (data_ && data_->auth_data) ? Env(data_->auth_data) : GetJniEnv();
  if (!env) return PhoneAuthCredential();

  JStr j_vid(env, verification_id), j_vcode(env, verification_code);
  jobject j_cred = env->CallStaticObjectMethod(
      phoneauthprovider::GetClass(),
      phoneauthprovider::GetMethodId(phoneauthprovider::kGetCredential),
      j_vid.s, j_vcode.s);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }
  return j_cred ? PhoneAuthCredential(CredentialLocalToGlobalRef(j_cred))
                : make_err(kAuthErrorInvalidCredential, error_message.c_str());
}

PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  static PhoneAuthProvider kInvalidPhoneAuthProvider;
  FIREBASE_ASSERT_RETURN(kInvalidPhoneAuthProvider,
                         auth != nullptr && auth->auth_data_ != nullptr);
  MutexLock lock(auth->auth_data_->future_impl.mutex());
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (!provider.data_) {
    JNIEnv* env = Env(auth->auth_data_);
    jobject local = env->CallStaticObjectMethod(
        phoneauthprovider::GetClass(),
        phoneauthprovider::GetMethodId(phoneauthprovider::kGetInstance),
        AuthImpl(auth->auth_data_));
    if (jni::CheckAndClearException(env)) local = nullptr;
    provider.data_ = new PhoneAuthProviderData();
    if (local) {
      provider.data_->j_phone_auth_provider = env->NewGlobalRef(local);
      env->DeleteLocalRef(local);
    }
    provider.data_->auth_data = auth->auth_data_;
  }
  return provider;
}

JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationCompleted(
    JNIEnv* env, jobject /*j_listener*/, jlong c_listener,
    jobject j_credential) {
  if (!c_listener) return;
  reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener)
      ->OnVerificationCompleted(PhoneAuthCredential(
          j_credential ? env->NewGlobalRef(j_credential) : nullptr));
}

JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationFailed(
    JNIEnv* env, jobject /*j_listener*/, jlong c_listener,
    jstring exception_message) {
  if (!c_listener) return;
  reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener)
      ->OnVerificationFailed(
          exception_message ? jni::JStringToString(env, exception_message)
                            : "Phone verification failed.");
}

JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeSent(
    JNIEnv* env, jobject /*j_listener*/, jlong c_listener,
    jstring j_verification_id, jobject j_force_resending_token) {
  if (!c_listener) return;
  PhoneAuthProvider::ForceResendingToken token;
  if (j_force_resending_token) {
    token.data_ = new ForceResendingTokenData(j_force_resending_token);
  }
  reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener)
      ->OnCodeSent(
          j_verification_id ? jni::JStringToString(env, j_verification_id) : "",
          token);
}

JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeAutoRetrievalTimeOut(
    JNIEnv* env, jobject /*j_listener*/, jlong c_listener,
    jstring j_verification_id) {
  if (!c_listener) return;
  reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener)
      ->OnCodeAutoRetrievalTimeOut(j_verification_id
                                       ? jni::JStringToString(env,
                                                              j_verification_id)
                                       : "");
}

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
jobject ConstructOAuthProvider(AuthData* auth_data,
                               const FederatedOAuthProviderData& pd,
                               AuthError* err, std::string* msg) {
  if (err) *err = kAuthErrorNone;
  if (msg) msg->clear();
  JNIEnv* env = Env(auth_data);
  if (!env) return nullptr;
  auto set_err = [&](AuthError e) {
    AuthError jni_err = CheckAndClearJniAuthExceptions(env, msg);
    if (err) *err = (jni_err != kAuthErrorNone) ? jni_err : e;
  };
  JStr pid(env, pd.provider_id.c_str());
  if (!pid.s) {
    set_err(kAuthErrorFailure);
    return nullptr;
  }
  jobject builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewBuilder), pid.s,
      AuthImpl(auth_data));
  if (env->ExceptionCheck() || !builder) {
    set_err(kAuthErrorFailure);
    return nullptr;
  }
  if (!pd.scopes.empty()) {
    jobject scopes = util::StdVectorToJavaList(env, pd.scopes);
    if (!scopes || env->ExceptionCheck()) {
      set_err(kAuthErrorFailure);
      env->DeleteLocalRef(builder);
      return nullptr;
    }
    jobject r = env->CallObjectMethod(
        builder,
        oauthprovider_builder::GetMethodId(oauthprovider_builder::kSetScopes),
        scopes);
    env->DeleteLocalRef(scopes);
    if (r) env->DeleteLocalRef(r);
    if (env->ExceptionCheck()) {
      set_err(kAuthErrorFailure);
      env->DeleteLocalRef(builder);
      return nullptr;
    }
  }
  if (!pd.custom_parameters.empty()) {
    jobject map = env->NewObject(
        jni::hash_map::GetClass(),
        jni::hash_map::GetMethodId(jni::hash_map::kConstructor));
    if (!map || env->ExceptionCheck()) {
      set_err(kAuthErrorFailure);
      env->DeleteLocalRef(builder);
      return nullptr;
    }
    util::StdMapToJavaMap(env, &map, pd.custom_parameters);
    jobject r = env->CallObjectMethod(
        builder,
        oauthprovider_builder::GetMethodId(
            oauthprovider_builder::kAddCustomParameters),
        map);
    env->DeleteLocalRef(map);
    if (r) env->DeleteLocalRef(r);
    if (env->ExceptionCheck()) {
      set_err(kAuthErrorFailure);
      env->DeleteLocalRef(builder);
      return nullptr;
    }
  }
  jobject prov = env->CallObjectMethod(
      builder,
      oauthprovider_builder::GetMethodId(oauthprovider_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (!prov || env->ExceptionCheck()) {
    set_err(kAuthErrorFailure);
    return nullptr;
  }
  return prov;
}

Future<AuthResult> RunFederatedOAuthTask(
    AuthData* auth_data, const FederatedOAuthProviderData& provider_data,
    int fn_id, bool require_user,
    user_idp::Method user_method = user_idp::kStartActivityForLinkWithProvider) {
  if (!auth_data || (require_user && !ValidUser(auth_data))) {
    return Future<AuthResult>();
  }
  Promise<AuthResult> promise(&auth_data->future_impl, fn_id);
  if (provider_data.provider_id.empty()) {
    promise.Fail(kAuthErrorInvalidProviderId, "Invalid provider ID.");
    return promise.future();
  }
  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data,
                                                 &error_code, &error_message);
  if (!oauthprovider) {
    promise.Fail(error_code != kAuthErrorNone ? error_code : kAuthErrorFailure,
                 !error_message.empty() ? error_message.c_str()
                                        : "Failed to construct OAuthProvider.");
    return promise.future();
  }
  JNIEnv* env = Env(auth_data);
  jobject target = AuthImpl(auth_data);
  if (require_user) {
    target = AcquireUserLocalRef(env, auth_data);
    if (!target) {
      env->DeleteLocalRef(oauthprovider);
      promise.Fail(kAuthErrorFailure, "User is no longer valid.");
      return promise.future();
    }
  }
  jobject task = env->CallObjectMethod(
      target,
      require_user
          ? user_idp::GetMethodId(user_method)
          : auth_idp::GetMethodId(auth_idp::kStartActivityForSignInWithProvider),
      auth_data->app->activity(), oauthprovider);
  if (require_user) env->DeleteLocalRef(target);
  env->DeleteLocalRef(oauthprovider);
  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl,
                                     promise.handle())) {
    if (task) {
      RegisterFederatedAuthProviderCallback(task, promise.handle(), auth_data,
                                            ReadAuthResult);
    } else {
      promise.Fail(kAuthErrorFailure, "Failed to start activity for OAuth.");
    }
  }
  if (task) env->DeleteLocalRef(task);
  return promise.future();
}
}  // namespace

Future<AuthResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  return RunFederatedOAuthTask(auth_data, provider_data_,
                               kAuthFn_SignInWithProvider, false);
}

Future<AuthResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  return RunFederatedOAuthTask(auth_data, provider_data_,
                               kUserFn_LinkWithProvider, true,
                               user_idp::kStartActivityForLinkWithProvider);
}

Future<AuthResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  return RunFederatedOAuthTask(
      auth_data, provider_data_, kUserFn_ReauthenticateWithProvider, true,
      user_idp::kStartActivityForReauthenticateWithProvider);
}

}  // namespace auth
}  // namespace firebase
