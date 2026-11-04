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

// clang-format off
#define CREDENTIAL_METHODS(X)                                                  \
    X(GetSignInMethod, "getSignInMethod", "()Ljava/lang/String;")
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(credential, CREDENTIAL_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/AuthCredential")


// clang-format off
#define EMAIL_CRED_METHODS(X)                                                  \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)

// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(emailcred, EMAIL_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/EmailAuthProvider")


// clang-format off
#define FACEBOOK_CRED_METHODS(X)                                               \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(facebookcred, FACEBOOK_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FacebookAuthProvider")


// clang-format off
#define GIT_HUB_CRED_METHODS(X)                                                \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(githubcred, GIT_HUB_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/GithubAuthProvider")


// clang-format off
#define GOOGLE_CRED_METHODS(X)                                                 \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(googlecred, GOOGLE_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/GoogleAuthProvider")


// clang-format off
#define PLAY_GAMES_CRED_METHODS(X)                                             \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      firebase::jni::kMethodTypeStatic, jni::kMethodOptional)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(playgamescred, PLAY_GAMES_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PlayGamesAuthProvider")


// clang-format off
#define TWITTER_CRED_METHODS(X)                                                \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", firebase::jni::kMethodTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(twittercred, TWITTER_CRED_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/TwitterAuthProvider")


// clang-format off
#define OAUTHPROVIDER_METHODS(X)                                               \
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

// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(oauthprovider, OAUTHPROVIDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider")


// clang-format off
#define TIME_UNIT_FIELDS(X)                                                    \
    X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",         \
      jni::kFieldTypeStatic)
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(timeunit, OAUTHPROVIDER_METHODS, TIME_UNIT_FIELDS, "java/util/concurrent/TimeUnit")


// clang-format off
#define PHONE_AUTH_OPTIONS_BUILDER_METHODS(X)                                  \
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

// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(phone_auth_options_builder, PHONE_AUTH_OPTIONS_BUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PhoneAuthOptions$Builder")


// clang-format off
#define PHONE_AUTH_PROVIDER_METHODS(X)                                         \
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
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(phoneauthprovider, PHONE_AUTH_PROVIDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/PhoneAuthProvider")


// clang-format off
#define OAUTHPROVIDER_BUILDER_METHODS(X)                                       \
  X(AddCustomParameters, "addCustomParameters",                                \
    "(Ljava/util/Map;)Lcom/google/firebase/auth/OAuthProvider$Builder;") \
  X(SetScopes, "setScopes",                                                    \
    "(Ljava/util/List;)Lcom/google/firebase/auth/OAuthProvider$Builder;") \
  X(Build, "build", "()Lcom/google/firebase/auth/OAuthProvider;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(oauthprovider_builder, OAUTHPROVIDER_BUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider$Builder")


// clang-format off
#define OAUTHPROVIDER_CREDENTIALBUILDER_METHODS(X)                             \
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
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(oauthprovider_credentialbuilder, OAUTHPROVIDER_CREDENTIALBUILDER_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/OAuthProvider$CredentialBuilder")


// clang-format off
#define AUTH_IDP_METHODS(X)                                                    \
  X(StartActivityForSignInWithProvider, "startActivityForSignInWithProvider",  \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(auth_idp, AUTH_IDP_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FirebaseAuth")


// clang-format off
#define USER_IDP_METHODS(X)                                                    \
  X(StartActivityForLinkWithProvider, "startActivityForLinkWithProvider",      \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;") \
  X(StartActivityForReauthenticateWithProvider,                                \
    "startActivityForReauthenticateWithProvider",                              \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")

// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(user_idp, USER_IDP_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/FirebaseUser")


// clang-format off
#define JNI_PHONE_LISTENER_CALLBACK_METHODS(X)                                 \
  X(Constructor, "<init>", "(J)V") \
  X(Disconnect, "disconnect", "()V")
// clang-format on
JNI_METHOD_LOOKUP_DECLARATION(jniphone, JNI_PHONE_LISTENER_CALLBACK_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/auth/internal/cpp/JniAuthPhoneListener")


namespace phonecredential {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(
        env, ::firebase::util::FindClass(
                 env, activity,
                 "com/google/firebase/auth/PhoneAuthCredential"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetSmsCode] = env->GetMethodID(g_class.get(), "getSmsCode", "()Ljava/lang/String;");
    if (!g_method_ids[kGetSmsCode]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


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
  if (!(jniphone::CacheMethodIds(env, activity) &&
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
Credential EmailAuthProvider::GetCredential(const char* email,
                                            const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  if (strlen(email) == 0) {
    Credential cred;
    cred.error_code_ = kAuthErrorMissingEmail;
    cred.error_message_ = "An email address must be provided.";
    return cred;
  }
  if (strlen(password) == 0) {
    Credential cred;
    cred.error_code_ = kAuthErrorMissingPassword;
    cred.error_message_ = "A password must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jstring j_email = env->NewStringUTF(email);
  jstring j_password = env->NewStringUTF(password);

  jobject j_cred = env->CallStaticObjectMethod(
      emailcred::GetClass(), emailcred::GetMethodId(emailcred::kGetCredential),
      j_email, j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);
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

//
// PhoneAuthCredential methods
//
PhoneAuthCredential& PhoneAuthCredential::operator=(
    const PhoneAuthCredential& rhs) {
  if (this != &rhs) {
    Credential::operator=(rhs);
    sms_code_ = rhs.sms_code_;
  }
  return *this;
}

bool PhoneAuthCredential::operator==(
    const PhoneAuthCredential& credential) const {
  return Credential::operator==(credential) &&
         sms_code() == credential.sms_code();
}

// Gets the automatically retrieved SMS verification code if applicable.
std::string PhoneAuthCredential::sms_code() const {
  if (!sms_code_.empty()) return sms_code_;
  if (!impl_) return std::string();
  JNIEnv* env = GetJniEnv();
  if (!env || !EnsureCredentialMethodsCached()) return std::string();
  jobject j_sms_code = env->CallObjectMethod(
      CredentialFromImpl(impl_),
      phonecredential::GetMethodId(phonecredential::kGetSmsCode));
  if (jni::CheckAndClearException(env) || !j_sms_code) {
    return std::string();
  }
  return JStringToString(env, j_sms_code);
}

//
// Provider methods
//
Credential InternalAuthResultProvider::GetCredential(jobject credential) {
  return Credential(CredentialLocalToGlobalRef(credential));
}

// static
Credential FacebookAuthProvider::GetCredential(const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), access_token);
  if (access_token[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "An access token must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jstring j_access_token = env->NewStringUTF(access_token);

  jobject j_cred = env->CallStaticObjectMethod(
      facebookcred::GetClass(),
      facebookcred::GetMethodId(facebookcred::kGetCredential), j_access_token);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }
  env->DeleteLocalRef(j_access_token);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential GitHubAuthProvider::GetCredential(const char* token) {
  FIREBASE_ASSERT_RETURN(Credential(), token);
  if (token[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "A GitHub token must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jstring j_token = env->NewStringUTF(token);

  jobject j_cred = env->CallStaticObjectMethod(
      githubcred::GetClass(),
      githubcred::GetMethodId(githubcred::kGetCredential), j_token);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }
  env->DeleteLocalRef(j_token);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token,
                                             const char* access_token) {
  if ((!id_token || id_token[0] == '\0') &&
      (!access_token || access_token[0] == '\0')) {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ =
        "Must specify an id_token or an access_token, or both.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  // id_token and access_token are optional, hence the additional checks.
  jstring j_id_token =
      (id_token && id_token[0] != '\0') ? env->NewStringUTF(id_token) : nullptr;
  jstring j_access_token = (access_token && access_token[0] != '\0')
                               ? env->NewStringUTF(access_token)
                               : nullptr;

  jobject j_cred = env->CallStaticObjectMethod(
      googlecred::GetClass(),
      googlecred::GetMethodId(googlecred::kGetCredential), j_id_token,
      j_access_token);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }

  if (j_id_token) env->DeleteLocalRef(j_id_token);
  if (j_access_token) env->DeleteLocalRef(j_access_token);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  FIREBASE_ASSERT_RETURN(Credential(), server_auth_code);
  if (server_auth_code[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "A server auth code must be provided.";
    return cred;
  }

  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jmethodID get_cred_method =
      playgamescred::GetMethodId(playgamescred::kGetCredential);
  if (!playgamescred::GetClass() || !get_cred_method) {
    Credential cred;
    cred.error_code_ = kAuthErrorApiNotAvailable;
    cred.error_message_ = "PlayGamesAuthProvider is not available.";
    return cred;
  }

  jstring j_server_auth_code = env->NewStringUTF(server_auth_code);

  jobject j_cred = env->CallStaticObjectMethod(
      playgamescred::GetClass(), get_cred_method, j_server_auth_code);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }

  env->DeleteLocalRef(j_server_auth_code);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential TwitterAuthProvider::GetCredential(const char* token,
                                              const char* secret) {
  FIREBASE_ASSERT_RETURN(Credential(), token && secret);
  if (token[0] == '\0' || secret[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "A Twitter token and secret must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jstring j_token = env->NewStringUTF(token);
  jstring j_secret = env->NewStringUTF(secret);

  jobject j_cred = env->CallStaticObjectMethod(
      twittercred::GetClass(),
      twittercred::GetMethodId(twittercred::kGetCredential), j_token, j_secret);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }

  env->DeleteLocalRef(j_token);
  env->DeleteLocalRef(j_secret);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(),
                         provider_id && (id_token || access_token));
  if (provider_id[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidProviderId;
    cred.error_message_ = "A provider ID must be provided.";
    return cred;
  }
  const bool has_id_token = id_token != nullptr && id_token[0] != '\0';
  const bool has_access_token =
      access_token != nullptr && access_token[0] != '\0';
  if (!has_id_token && !has_access_token) {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "An ID token or access token must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();

  jstring j_provider_id = env->NewStringUTF(provider_id);
  jstring j_id_token = has_id_token ? env->NewStringUTF(id_token) : nullptr;
  jstring j_access_token =
      has_access_token ? env->NewStringUTF(access_token) : nullptr;

  jobject j_cred = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kGetCredential), j_provider_id,
      j_id_token, j_access_token);

  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    j_cred = nullptr;
  }

  env->DeleteLocalRef(j_provider_id);
  if (j_id_token) env->DeleteLocalRef(j_id_token);
  if (j_access_token) env->DeleteLocalRef(j_access_token);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* raw_nonce,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && raw_nonce);
  if (provider_id[0] == '\0') {
    Credential cred;
    cred.error_code_ = kAuthErrorInvalidProviderId;
    cred.error_message_ = "A provider ID must be provided.";
    return cred;
  }
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), EnsureCredentialMethodsCached(),
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  if (!env) return Credential();
  jstring j_provider_id = env->NewStringUTF(provider_id);
  jstring j_id_token = env->NewStringUTF(id_token);
  jstring j_raw_nonce = env->NewStringUTF(raw_nonce);

  jobject j_cred_builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewCredentialBuilder),
      j_provider_id);

  jobject j_cred = nullptr;
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) == kAuthErrorNone &&
      j_cred_builder != nullptr) {
    jobject builder_return_ref = env->CallObjectMethod(
        j_cred_builder,
        oauthprovider_credentialbuilder::GetMethodId(
            oauthprovider_credentialbuilder::kSetIdTokenWithRawNonce),
        j_id_token, j_raw_nonce);

    if (CheckAndClearJniAuthExceptions(env, &error_message) == kAuthErrorNone) {
      if (builder_return_ref != nullptr) {
        env->DeleteLocalRef(builder_return_ref);
      }
      // add the access token, if it exists.
      if (access_token != nullptr) {
        jobject j_access_token = env->NewStringUTF(access_token);
        builder_return_ref = env->CallObjectMethod(
            j_cred_builder,
            oauthprovider_credentialbuilder::GetMethodId(
                oauthprovider_credentialbuilder::kSetAccessToken),
            j_access_token);

        env->DeleteLocalRef(j_access_token);
        if (CheckAndClearJniAuthExceptions(env, &error_message) ==
            kAuthErrorNone) {
          if (builder_return_ref != nullptr) {
            env->DeleteLocalRef(builder_return_ref);
          }
        } else {
          if (builder_return_ref != nullptr) {
            env->DeleteLocalRef(builder_return_ref);
          }
          env->DeleteLocalRef(j_cred_builder);
          j_cred_builder = nullptr;
        }
      }
    } else {
      if (builder_return_ref != nullptr) {
        env->DeleteLocalRef(builder_return_ref);
      }
      env->DeleteLocalRef(j_cred_builder);
      j_cred_builder = nullptr;
    }

    // If we have a valid credential builder, build a credential.
    if (j_cred_builder != nullptr) {
      j_cred = env->CallObjectMethod(
          j_cred_builder, oauthprovider_credentialbuilder::GetMethodId(
                              oauthprovider_credentialbuilder::kBuild));
      if (CheckAndClearJniAuthExceptions(env, &error_message) !=
          kAuthErrorNone) {
        j_cred = nullptr;
      }

      env->DeleteLocalRef(j_cred_builder);
    }
  } else if (j_cred_builder != nullptr) {
    env->DeleteLocalRef(j_cred_builder);
  }

  env->DeleteLocalRef(j_provider_id);
  env->DeleteLocalRef(j_raw_nonce);
  env->DeleteLocalRef(j_id_token);

  Credential cred(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
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
  auto last_result =
      future_api->LastResult(kCredentialFn_GameCenterGetCredential);
  return static_cast<const Future<Credential>&>(last_result);
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

  // phone auth builder
  jobject builder =
      env->NewObject(phone_auth_options_builder::GetClass(),
                     phone_auth_options_builder::GetMethodId(
                         phone_auth_options_builder::kConstructor),
                     AuthImpl(data_->auth_data));
  if (jni::CheckAndClearException(env) || builder == nullptr) {
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable to construct PhoneAuthOptions builder.");
    return;
  }

  // activity
  jobject activity = options.ui_parent;
  if (activity == nullptr) {
    activity = data_->auth_data->app->activity();
  }
  jobject chained_ref =
      env->CallObjectMethod(builder,
                            phone_auth_options_builder::GetMethodId(
                                phone_auth_options_builder::kSetActivity),
                            activity);
  if (jni::CheckAndClearException(env)) {
    if (chained_ref) env->DeleteLocalRef(chained_ref);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable set PhoneAuthOptions activity.");
    return;
  }
  env->DeleteLocalRef(chained_ref);

  // listener callbacks
  chained_ref =
      env->CallObjectMethod(builder,
                            phone_auth_options_builder::GetMethodId(
                                phone_auth_options_builder::kSetCallbacks),
                            listener->data_ ? listener->data_->j_listener
                                            : nullptr);
  if (jni::CheckAndClearException(env)) {
    if (chained_ref) env->DeleteLocalRef(chained_ref);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable set PhoneAuthOptions listeners.");
    return;
  }
  env->DeleteLocalRef(chained_ref);

  // phone number
  jstring phone_number = env->NewStringUTF(options.phone_number.c_str());
  chained_ref =
      env->CallObjectMethod(builder,
                            phone_auth_options_builder::GetMethodId(
                                phone_auth_options_builder::kSetPhoneNumber),
                            phone_number);
  if (jni::CheckAndClearException(env)) {
    if (chained_ref) env->DeleteLocalRef(chained_ref);
    env->DeleteLocalRef(phone_number);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable set PhoneAuthOptions phone number.");
    return;
  }
  env->DeleteLocalRef(phone_number);
  env->DeleteLocalRef(chained_ref);

  // force resending token (if valid)
  if (options.force_resending_token != nullptr &&
      options.force_resending_token->is_valid() &&
      options.force_resending_token->data_ != nullptr &&
      options.force_resending_token->data_->token_global_ref() != nullptr) {
    chained_ref = env->CallObjectMethod(
        builder,
        phone_auth_options_builder::GetMethodId(
            phone_auth_options_builder::kSetForceResendingToken),
        options.force_resending_token->data_->token_global_ref());
    if (jni::CheckAndClearException(env)) {
      if (chained_ref) env->DeleteLocalRef(chained_ref);
      env->DeleteLocalRef(builder);
      listener->OnVerificationFailed(
          "VerifyPhoneNumber: unable to set force resending token.");
      return;
    }
    env->DeleteLocalRef(chained_ref);
  }

  // timeout
  jobject timeout = env->NewObject(
      jni::long_class::GetClass(),
      jni::long_class::GetMethodId(jni::long_class::kConstructor),
      static_cast<jlong>(options.timeout_milliseconds));
  if (jni::CheckAndClearException(env) || timeout == nullptr) {
    if (timeout) env->DeleteLocalRef(timeout);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: couldn't convert timeout to java.lang.Long.");
    return;
  }
  jobject milliseconds_time_unit = env->GetStaticObjectField(
      timeunit::GetClass(), timeunit::GetFieldId(timeunit::kMilliseconds));
  if (jni::CheckAndClearException(env) ||
      milliseconds_time_unit == nullptr) {
    env->DeleteLocalRef(timeout);
    if (milliseconds_time_unit) env->DeleteLocalRef(milliseconds_time_unit);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: couldn't retrieve TimeUnit.MILLISECONDS.");
    return;
  }
  chained_ref =
      env->CallObjectMethod(builder,
                            phone_auth_options_builder::GetMethodId(
                                phone_auth_options_builder::kSetTimeout),
                            timeout, milliseconds_time_unit);
  std::string error_message;
  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    if (chained_ref) env->DeleteLocalRef(chained_ref);
    env->DeleteLocalRef(timeout);
    env->DeleteLocalRef(milliseconds_time_unit);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        !error_message.empty()
            ? error_message
            : "VerifyPhoneNumber: unable set PhoneAuthOptions timeout.");
    return;
  }
  env->DeleteLocalRef(timeout);
  env->DeleteLocalRef(milliseconds_time_unit);
  env->DeleteLocalRef(chained_ref);

  // build
  jobject phone_auth_options =
      env->CallObjectMethod(builder, phone_auth_options_builder::GetMethodId(
                                         phone_auth_options_builder::kBuild));

  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone ||
      phone_auth_options == nullptr) {
    if (phone_auth_options) env->DeleteLocalRef(phone_auth_options);
    env->DeleteLocalRef(builder);
    listener->OnVerificationFailed(
        !error_message.empty()
            ? error_message
            : "VerifyPhoneNumber: builder failed to create PhoneAuthOptions");
    return;
  }
  env->DeleteLocalRef(builder);

  // verify
  env->CallStaticVoidMethod(
      phoneauthprovider::GetClass(),
      phoneauthprovider::GetMethodId(phoneauthprovider::kVerifyPhoneNumber),
      phone_auth_options);

  if (CheckAndClearJniAuthExceptions(env, &error_message) != kAuthErrorNone) {
    // If an error occurred with the call to verifyPhoneNumber, inform the
    // listener that it failed.
    listener->OnVerificationFailed(
        !error_message.empty()
            ? error_message
            : "VerifyPhoneNumber: Android failed to verify the given phone "
              "number");
  }

  env->DeleteLocalRef(phone_auth_options);
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
jobject ConstructOAuthProvider(
    AuthData* auth_data, const FederatedOAuthProviderData& provider_data,
    AuthError* out_error_code, std::string* out_error_message) {
  if (out_error_code) *out_error_code = kAuthErrorNone;
  if (out_error_message) out_error_message->clear();
  JNIEnv* env = Env(auth_data);
  FIREBASE_ASSERT_RETURN(nullptr, env != nullptr);
  jstring provider_id = env->NewStringUTF(provider_data.provider_id.c_str());
  if (env->ExceptionCheck() || provider_id == nullptr) {
    AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
    if (out_error_code) {
      *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
    }
    if (provider_id) env->DeleteLocalRef(provider_id);
    return nullptr;
  }
  jobject oauthprovider_builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewBuilder), provider_id,
      AuthImpl(auth_data));
  if (env->ExceptionCheck() || oauthprovider_builder == nullptr) {
    AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
    if (out_error_code) {
      *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
    }
    env->DeleteLocalRef(provider_id);
    if (oauthprovider_builder) env->DeleteLocalRef(oauthprovider_builder);
    return nullptr;
  }
  env->DeleteLocalRef(provider_id);

  if (!provider_data.scopes.empty()) {
    jobject scopes_list = util::StdVectorToJavaList(env, provider_data.scopes);
    if (env->ExceptionCheck() || scopes_list == nullptr) {
      AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
      if (out_error_code) {
        *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
      }
      if (scopes_list) env->DeleteLocalRef(scopes_list);
      env->DeleteLocalRef(oauthprovider_builder);
      return nullptr;
    }

    jobject builder_return_ref = env->CallObjectMethod(
        oauthprovider_builder,
        oauthprovider_builder::GetMethodId(oauthprovider_builder::kSetScopes),
        scopes_list);
    if (env->ExceptionCheck()) {
      AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
      if (out_error_code) {
        *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
      }
      env->DeleteLocalRef(scopes_list);
      if (builder_return_ref) env->DeleteLocalRef(builder_return_ref);
      env->DeleteLocalRef(oauthprovider_builder);
      return nullptr;
    }
    env->DeleteLocalRef(scopes_list);
    if (builder_return_ref) env->DeleteLocalRef(builder_return_ref);
  }

  if (!provider_data.custom_parameters.empty()) {
    jobject custom_parameters_map = env->NewObject(
        jni::hash_map::GetClass(),
        jni::hash_map::GetMethodId(jni::hash_map::kConstructor));
    if (env->ExceptionCheck() || custom_parameters_map == nullptr) {
      AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
      if (out_error_code) {
        *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
      }
      if (custom_parameters_map) env->DeleteLocalRef(custom_parameters_map);
      env->DeleteLocalRef(oauthprovider_builder);
      return nullptr;
    }
    util::StdMapToJavaMap(env, &custom_parameters_map,
                          provider_data.custom_parameters);
    if (env->ExceptionCheck()) {
      AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
      if (out_error_code) {
        *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
      }
      env->DeleteLocalRef(custom_parameters_map);
      env->DeleteLocalRef(oauthprovider_builder);
      return nullptr;
    }
    jobject builder_return_ref =
        env->CallObjectMethod(oauthprovider_builder,
                              oauthprovider_builder::GetMethodId(
                                  oauthprovider_builder::kAddCustomParameters),
                              custom_parameters_map);
    if (env->ExceptionCheck()) {
      AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
      if (out_error_code) {
        *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
      }
      env->DeleteLocalRef(custom_parameters_map);
      if (builder_return_ref) env->DeleteLocalRef(builder_return_ref);
      env->DeleteLocalRef(oauthprovider_builder);
      return nullptr;
    }
    env->DeleteLocalRef(custom_parameters_map);
    if (builder_return_ref) env->DeleteLocalRef(builder_return_ref);
  }

  jobject oauthprovider = env->CallObjectMethod(
      oauthprovider_builder,
      oauthprovider_builder::GetMethodId(oauthprovider_builder::kBuild));
  if (env->ExceptionCheck() || oauthprovider == nullptr) {
    AuthError err = CheckAndClearJniAuthExceptions(env, out_error_message);
    if (out_error_code) {
      *out_error_code = (err != kAuthErrorNone) ? err : kAuthErrorFailure;
    }
    env->DeleteLocalRef(oauthprovider_builder);
    if (oauthprovider) env->DeleteLocalRef(oauthprovider);
    return nullptr;
  }
  env->DeleteLocalRef(oauthprovider_builder);

  return oauthprovider;
}
}  // namespace

Future<AuthResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  if (!auth_data) return Future<AuthResult>();
  JNIEnv* env = Env(auth_data);

  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kAuthFn_SignInWithProvider, AuthResult());

  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }

  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(
      auth_data, provider_data_, &error_code, &error_message);
  if (oauthprovider == nullptr) {
    futures.Complete(handle,
                     error_code != kAuthErrorNone ? error_code
                                                  : kAuthErrorFailure,
                     !error_message.empty()
                         ? error_message.c_str()
                         : "Failed to construct OAuthProvider.");
  } else {
    jobject task = env->CallObjectMethod(
        AuthImpl(auth_data),
        auth_idp::GetMethodId(auth_idp::kStartActivityForSignInWithProvider),
        auth_data->app->activity(), oauthprovider);
    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      if (task != nullptr) {
        RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                              ReadAuthResult);
      } else {
        futures.Complete(handle, kAuthErrorFailure,
                         "Failed to start activity for OAuth sign-in.");
      }
    }
    if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
  }

  if (oauthprovider != nullptr) {
    env->DeleteLocalRef(oauthprovider);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  JNIEnv* env = Env(auth_data);
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kUserFn_LinkWithProvider, AuthResult());

  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }

  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(
      auth_data, provider_data_, &error_code, &error_message);
  if (oauthprovider == nullptr) {
    futures.Complete(handle,
                     error_code != kAuthErrorNone ? error_code
                                                  : kAuthErrorFailure,
                     !error_message.empty()
                         ? error_message.c_str()
                         : "Failed to construct OAuthProvider.");
  } else {
    jobject user_local = nullptr;
    {
      MutexLock lock(auth_data->future_impl.mutex());
      jobject user_impl = UserImpl(auth_data);
      if (user_impl != nullptr) {
        user_local = env->NewLocalRef(user_impl);
      }
    }
    if (user_local == nullptr) {
      futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    } else {
      jobject task = env->CallObjectMethod(
          user_local,
          user_idp::GetMethodId(user_idp::kStartActivityForLinkWithProvider),
          auth_data->app->activity(), oauthprovider);
      env->DeleteLocalRef(user_local);
      if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
        if (task != nullptr) {
          RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                                ReadAuthResult);
        } else {
          futures.Complete(handle, kAuthErrorFailure,
                           "Failed to start activity for OAuth link.");
        }
      }
      if (task != nullptr) {
        env->DeleteLocalRef(task);
      }
    }
  }

  if (oauthprovider != nullptr) {
    env->DeleteLocalRef(oauthprovider);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  JNIEnv* env = Env(auth_data);
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(
      kUserFn_ReauthenticateWithProvider, AuthResult());

  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }

  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  jobject oauthprovider = ConstructOAuthProvider(
      auth_data, provider_data_, &error_code, &error_message);
  if (oauthprovider == nullptr) {
    futures.Complete(handle,
                     error_code != kAuthErrorNone ? error_code
                                                  : kAuthErrorFailure,
                     !error_message.empty()
                         ? error_message.c_str()
                         : "Failed to construct OAuthProvider.");
  } else {
    jobject user_local = nullptr;
    {
      MutexLock lock(auth_data->future_impl.mutex());
      jobject user_impl = UserImpl(auth_data);
      if (user_impl != nullptr) {
        user_local = env->NewLocalRef(user_impl);
      }
    }
    if (user_local == nullptr) {
      futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    } else {
      jobject task = env->CallObjectMethod(
          user_local,
          user_idp::GetMethodId(
              user_idp::kStartActivityForReauthenticateWithProvider),
          auth_data->app->activity(), oauthprovider);
      env->DeleteLocalRef(user_local);
      if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
        if (task != nullptr) {
          RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                                ReadAuthResult);
        } else {
          futures.Complete(
              handle, kAuthErrorFailure,
              "Failed to start activity for OAuth reauthentication.");
        }
      }
      if (task != nullptr) {
        env->DeleteLocalRef(task);
      }
    }
  }

  if (oauthprovider != nullptr) {
    env->DeleteLocalRef(oauthprovider);
  }
  return MakeFuture(&futures, handle);
}

}  // namespace auth
}  // namespace firebase
