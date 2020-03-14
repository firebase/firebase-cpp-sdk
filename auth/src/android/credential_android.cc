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

#include <algorithm>
#include <cstring>

#include "app/src/app_common.h"
#include "app/src/assert.h"
#include "app/src/build_type_generated.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using util::JniStringToString;

// clang-format off
#define CREDENTIAL_METHODS(X)                                                  \
    X(GetSignInMethod, "getSignInMethod", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(credential, CREDENTIAL_METHODS)
METHOD_LOOKUP_DEFINITION(credential,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/AuthCredential",
                         CREDENTIAL_METHODS)

// clang-format off
#define EMAIL_CRED_METHODS(X)                                                  \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", util::kMethodTypeStatic)

// clang-format on
METHOD_LOOKUP_DECLARATION(emailcred, EMAIL_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(emailcred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/EmailAuthProvider",
                         EMAIL_CRED_METHODS)

// clang-format off
#define FACEBOOK_CRED_METHODS(X)                                               \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(facebookcred, FACEBOOK_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(facebookcred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FacebookAuthProvider",
                         FACEBOOK_CRED_METHODS)

// clang-format off
#define GIT_HUB_CRED_METHODS(X)                                                \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(githubcred, GIT_HUB_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(githubcred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/GithubAuthProvider",
                         GIT_HUB_CRED_METHODS)

// clang-format off
#define GOOGLE_CRED_METHODS(X)                                                 \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(googlecred, GOOGLE_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(googlecred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/GoogleAuthProvider",
                         GOOGLE_CRED_METHODS)

// clang-format off
#define PLAY_GAMES_CRED_METHODS(X)                                             \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;)Lcom/google/firebase/auth/AuthCredential;",         \
      util::kMethodTypeStatic, util::kMethodOptional)
// clang-format on
METHOD_LOOKUP_DECLARATION(playgamescred, PLAY_GAMES_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(playgamescred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/PlayGamesAuthProvider",
                         PLAY_GAMES_CRED_METHODS)

// clang-format off
#define TWITTER_CRED_METHODS(X)                                                \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/AuthCredential;", util::kMethodTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(twittercred, TWITTER_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(twittercred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/TwitterAuthProvider",
                         TWITTER_CRED_METHODS)

// clang-format off
#define OAUTHPROVIDER_METHODS(X)                                               \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)"               \
      "Lcom/google/firebase/auth/AuthCredential;", util::kMethodTypeStatic),   \
    X(NewBuilder, "newBuilder",                                                \
      "(Ljava/lang/String;Lcom/google/firebase/auth/FirebaseAuth;)"            \
      "Lcom/google/firebase/auth/OAuthProvider$Builder;",                      \
      util::kMethodTypeStatic),                                                \
    X(NewCredentialBuilder, "newCredentialBuilder",                            \
      "(Ljava/lang/String;)"                                                   \
      "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;",            \
      util::kMethodTypeStatic)

// clang-format on
METHOD_LOOKUP_DECLARATION(oauthprovider, OAUTHPROVIDER_METHODS)
METHOD_LOOKUP_DEFINITION(oauthprovider,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/OAuthProvider",
                         OAUTHPROVIDER_METHODS)

// clang-format off
#define TIME_UNIT_METHODS(X)                                                   \
    X(ToMillis, "toMillis", "(J)J")
#define TIME_UNIT_FIELDS(X)                                                    \
    X(Milliseconds, "MILLISECONDS", "Ljava/util/concurrent/TimeUnit;",         \
      util::kFieldTypeStatic)
// clang-format on
METHOD_LOOKUP_DECLARATION(timeunit, TIME_UNIT_METHODS, TIME_UNIT_FIELDS)
METHOD_LOOKUP_DEFINITION(timeunit,
                         PROGUARD_KEEP_CLASS "java/util/concurrent/TimeUnit",
                         TIME_UNIT_METHODS, TIME_UNIT_FIELDS)
// clang-format off
#define PHONE_CRED_METHODS(X)                                                  \
    X(GetInstance, "getInstance",                                              \
      "(Lcom/google/firebase/auth/FirebaseAuth;)"                              \
      "Lcom/google/firebase/auth/PhoneAuthProvider;",                          \
      util::kMethodTypeStatic),                                                \
    X(GetCredential, "getCredential",                                          \
      "(Ljava/lang/String;Ljava/lang/String;)"                                 \
      "Lcom/google/firebase/auth/PhoneAuthCredential;",                        \
      util::kMethodTypeStatic),                                                \
    X(VerifyPhoneNumber, "verifyPhoneNumber",                                  \
      "(Ljava/lang/String;J"                                                   \
      "Ljava/util/concurrent/TimeUnit;"                                        \
      "Landroid/app/Activity;"                                                 \
      "Lcom/google/firebase/auth/PhoneAuthProvider$"                           \
      "OnVerificationStateChangedCallbacks;"                                   \
      "Lcom/google/firebase/auth/PhoneAuthProvider$ForceResendingToken;)V")
// clang-format on
METHOD_LOOKUP_DECLARATION(phonecred, PHONE_CRED_METHODS)
METHOD_LOOKUP_DEFINITION(phonecred,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/PhoneAuthProvider",
                         PHONE_CRED_METHODS)
// clang-format off
#define OAUTHPROVIDER_BUILDER_METHODS(X)                                       \
  X(AddCustomParameters, "addCustomParameters",                                \
    "(Ljava/util/Map;)Lcom/google/firebase/auth/OAuthProvider$Builder;"),      \
  X(SetScopes, "setScopes",                                                    \
    "(Ljava/util/List;)Lcom/google/firebase/auth/OAuthProvider$Builder;"),     \
  X(Build, "build", "()Lcom/google/firebase/auth/OAuthProvider;")
// clang-format on

METHOD_LOOKUP_DECLARATION(oauthprovider_builder, OAUTHPROVIDER_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(oauthprovider_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/OAuthProvider$Builder",
                         OAUTHPROVIDER_BUILDER_METHODS)

// clang-format off
#define OAUTHPROVIDER_CREDENTIALBUILDER_METHODS(X)                             \
  X(SetAccessToken, "setAccessToken",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;"),             \
  X(SetIdToken, "setIdToken",                                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;"),             \
  X(SetIdTokenWithRawNonce, "setIdTokenWithRawNonce",                          \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/auth/OAuthProvider$CredentialBuilder;"),             \
  X(Build, "build", "()Lcom/google/firebase/auth/AuthCredential;")
// clang-format on

METHOD_LOOKUP_DECLARATION(oauthprovider_credentialbuilder,
                          OAUTHPROVIDER_CREDENTIALBUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    oauthprovider_credentialbuilder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/OAuthProvider$CredentialBuilder",
    OAUTHPROVIDER_CREDENTIALBUILDER_METHODS)

// clang-format off
#define AUTH_IDP_METHODS(X)                                                    \
  X(StartActivityForSignInWithProvider, "startActivityForSignInWithProvider",  \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")
// clang-format on
METHOD_LOOKUP_DECLARATION(auth_idp, AUTH_IDP_METHODS)
METHOD_LOOKUP_DEFINITION(auth_idp,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseAuth",
                         AUTH_IDP_METHODS)

// clang-format off
#define USER_IDP_METHODS(X)                                                    \
  X(StartActivityForLinkWithProvider, "startActivityForLinkWithProvider",      \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(StartActivityForReauthenticateWithProvider,                                \
    "startActivityForReauthenticateWithProvider",                              \
    "(Landroid/app/Activity;Lcom/google/firebase/auth/FederatedAuthProvider;)" \
    "Lcom/google/android/gms/tasks/Task;")

// clang-format on
METHOD_LOOKUP_DECLARATION(user_idp, USER_IDP_METHODS)
METHOD_LOOKUP_DEFINITION(user_idp,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseUser",
                         USER_IDP_METHODS)

// clang-format off
#define JNI_PHONE_LISTENER_CALLBACK_METHODS(X)                                 \
  X(Constructor, "<init>", "(J)V"),                                            \
  X(Disconnect, "disconnect", "()V")
// clang-format on
METHOD_LOOKUP_DECLARATION(jniphone, JNI_PHONE_LISTENER_CALLBACK_METHODS)
METHOD_LOOKUP_DEFINITION(
    jniphone, "com/google/firebase/auth/internal/cpp/JniAuthPhoneListener",
    JNI_PHONE_LISTENER_CALLBACK_METHODS)

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

static bool g_methods_cached = false;

const char kMethodsNotCachedError[] =
    "Firebase Auth was not initialized, unable to create a Credential. "
    "Create an Auth instance first.";

bool CacheCredentialMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<internal::EmbeddedFile>& embedded_files) {
  // Cache the JniAuthPhoneListener class and register the native callback
  // methods.
  if (!(jniphone::CacheClassFromFiles(env, activity, &embedded_files) &&
        jniphone::CacheMethodIds(env, activity) &&
        jniphone::RegisterNatives(
            env, kNativeJniAuthPhoneListenerMethods,
            FIREBASE_ARRAYSIZE(kNativeJniAuthPhoneListenerMethods)))) {
    return false;
  }

  g_methods_cached =
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
      phonecred::CacheMethodIds(env, activity) &&
      timeunit::CacheFieldIds(env, activity) &&
      playgamescred::CacheMethodIds(env, activity) &&
      twittercred::CacheMethodIds(env, activity);

  return g_methods_cached;
}

void ReleaseCredentialClasses(JNIEnv* env) {
  auth_idp::ReleaseClass(env);
  credential::ReleaseClass(env);
  emailcred::ReleaseClass(env);
  facebookcred::ReleaseClass(env);
  githubcred::ReleaseClass(env);
  googlecred::ReleaseClass(env);
  playgamescred::ReleaseClass(env);
  jniphone::ReleaseClass(env);
  oauthprovider::ReleaseClass(env);
  oauthprovider_builder::ReleaseClass(env);
  oauthprovider_credentialbuilder::ReleaseClass(env);
  phonecred::ReleaseClass(env);
  timeunit::ReleaseClass(env);
  twittercred::ReleaseClass(env);
  user_idp::ReleaseClass(env);
  g_methods_cached = false;
}

static JNIEnv* GetJniEnv() {
  // The JNI environment is the same regardless of App.
  App* app = app_common::GetAnyApp();
  FIREBASE_ASSERT(app != nullptr);
  return app->GetJNIEnv();
}

// Remove the reference on destruction.
Credential::~Credential() {
  if (impl_ != nullptr) {
    JNIEnv* env = GetJniEnv();
    env->DeleteGlobalRef(static_cast<jobject>(impl_));
    impl_ = nullptr;
  }
}

Credential::Credential(const Credential& rhs)
    : impl_(nullptr), error_code_(kAuthErrorNone) {
  *this = rhs;
}

// Increase the reference count when copying.
Credential& Credential::operator=(const Credential& rhs) {
  JNIEnv* env = GetJniEnv();
  if (rhs.impl_) {
    jobject j_cred_ref = env->NewGlobalRef(static_cast<jobject>(rhs.impl_));
    impl_ = static_cast<void*>(j_cred_ref);
  } else {
    impl_ = nullptr;
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
  return *this;
}

std::string Credential::provider() const {
  JNIEnv* env = GetJniEnv();
  if (!impl_) return std::string();
  jobject j_provider = env->CallObjectMethod(
      CredentialFromImpl(impl_),
      credential::GetMethodId(credential::kGetSignInMethod));
  assert(env->ExceptionCheck() == false);
  return JniStringToString(env, j_provider);
}

bool Credential::is_valid() const { return impl_ != nullptr; }

static void* CredentialLocalToGlobalRef(jobject j_cred) {
  if (!j_cred) return nullptr;
  JNIEnv* env = GetJniEnv();

  // Convert to global reference, so it stays around.
  jobject j_cred_ref = env->NewGlobalRef(j_cred);
  env->DeleteLocalRef(j_cred);
  return static_cast<void*>(j_cred_ref);
}

// static
Credential EmailAuthProvider::GetCredential(const char* email,
                                            const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_email = env->NewStringUTF(email);
  jstring j_password = env->NewStringUTF(password);

  jobject j_cred = env->CallStaticObjectMethod(
      emailcred::GetClass(), emailcred::GetMethodId(emailcred::kGetCredential),
      j_email, j_password);
  env->DeleteLocalRef(j_email);
  env->DeleteLocalRef(j_password);
  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  if (!j_cred) {
    // Read error from exception, except give more specific errors for email and
    // password being blank.
    if (strlen(email) == 0) {
      firebase::util::CheckAndClearJniExceptions(env);
      error_code = kAuthErrorMissingEmail;
      error_message = "An email address must be provided.";
    } else if (strlen(password) == 0) {
      firebase::util::CheckAndClearJniExceptions(env);
      error_code = kAuthErrorMissingPassword;
      error_message = "A password must be provided.";
    } else {
      error_code = CheckAndClearJniAuthExceptions(env, &error_message);
    }
  }

  Credential cred = Credential(CredentialLocalToGlobalRef(j_cred));
  if (!j_cred) {
    cred.error_code_ = error_code;
    cred.error_message_ = error_message;
  }

  return cred;
}

// static
Credential FacebookAuthProvider::GetCredential(const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), access_token);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_access_token = env->NewStringUTF(access_token);

  jobject j_cred = env->CallStaticObjectMethod(
      facebookcred::GetClass(),
      facebookcred::GetMethodId(facebookcred::kGetCredential), j_access_token);
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;
  env->DeleteLocalRef(j_access_token);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Credential GitHubAuthProvider::GetCredential(const char* token) {
  FIREBASE_ASSERT_RETURN(Credential(), token);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_token = env->NewStringUTF(token);

  jobject j_cred = env->CallStaticObjectMethod(
      githubcred::GetClass(),
      githubcred::GetMethodId(githubcred::kGetCredential), j_token);
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;
  env->DeleteLocalRef(j_token);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token,
                                             const char* access_token) {
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

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
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;

  if (j_id_token) env->DeleteLocalRef(j_id_token);
  if (j_access_token) env->DeleteLocalRef(j_access_token);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  FIREBASE_ASSERT_RETURN(Credential(), server_auth_code);

  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_server_auth_code = env->NewStringUTF(server_auth_code);

  jobject j_cred = env->CallStaticObjectMethod(
      playgamescred::GetClass(),
      playgamescred::GetMethodId(playgamescred::kGetCredential),
      j_server_auth_code);
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;

  env->DeleteLocalRef(j_server_auth_code);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Credential TwitterAuthProvider::GetCredential(const char* token,
                                              const char* secret) {
  FIREBASE_ASSERT_RETURN(Credential(), token && secret);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_token = env->NewStringUTF(token);
  jstring j_secret = env->NewStringUTF(secret);

  jobject j_cred = env->CallStaticObjectMethod(
      twittercred::GetClass(),
      twittercred::GetMethodId(twittercred::kGetCredential), j_token, j_secret);
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;

  env->DeleteLocalRef(j_token);
  env->DeleteLocalRef(j_secret);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && access_token);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();

  jstring j_provider_id = env->NewStringUTF(provider_id);
  jstring j_id_token = env->NewStringUTF(id_token);
  jstring j_access_token = env->NewStringUTF(access_token);

  jobject j_cred = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kGetCredential), j_provider_id,
      j_id_token, j_access_token);

  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;

  env->DeleteLocalRef(j_provider_id);
  env->DeleteLocalRef(j_id_token);
  env->DeleteLocalRef(j_access_token);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}


// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* raw_nonce,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(),
                         provider_id && id_token && raw_nonce );
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = GetJniEnv();
  jstring j_provider_id = env->NewStringUTF(provider_id);
  jstring j_id_token = env->NewStringUTF(id_token);
  jstring j_raw_nonce = env->NewStringUTF(raw_nonce);

  jobject j_cred_builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewCredentialBuilder),
      j_provider_id);

  jobject j_cred = nullptr;
  if (!firebase::util::CheckAndClearJniExceptions(env)) {
    jobject builder_return_ref = env->CallObjectMethod(
        j_cred_builder,
        oauthprovider_credentialbuilder::GetMethodId(
            oauthprovider_credentialbuilder::kSetIdTokenWithRawNonce),
        j_id_token, j_raw_nonce);

    if (!firebase::util::CheckAndClearJniExceptions(env)) {
      env->DeleteLocalRef(builder_return_ref);
      // add the access token, if it exists.
      if (access_token != nullptr) {
        jobject j_access_token = env->NewStringUTF(access_token);
        builder_return_ref = env->CallObjectMethod(
            j_cred_builder,
            oauthprovider_credentialbuilder::GetMethodId(
                oauthprovider_credentialbuilder::kSetAccessToken),
            j_access_token);

        env->DeleteLocalRef(j_access_token);
        if (!firebase::util::CheckAndClearJniExceptions(env)) {
          env->DeleteLocalRef(builder_return_ref);
        } else {
          env->DeleteLocalRef(j_cred_builder);
          j_cred_builder = nullptr;
        }
      }
    }

    // If we have a valid credential builder, build a credential.
    if (j_cred_builder != nullptr) {
      j_cred = env->CallObjectMethod(
          j_cred_builder, oauthprovider_credentialbuilder::GetMethodId(
                              oauthprovider_credentialbuilder::kBuild));
      if (firebase::util::CheckAndClearJniExceptions(env)) {
        j_cred = nullptr;
      }

      env->DeleteLocalRef(j_cred_builder);
    }
  }

  env->DeleteLocalRef(j_provider_id);
  env->DeleteLocalRef(j_raw_nonce);
  env->DeleteLocalRef(j_id_token);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
Future<Credential> GameCenterAuthProvider::GetCredential() {
  // Game Center is not available on Android
  bool is_gamecenter_available_on_android = false;

  auto future_api = GetCredentialFutureImpl();
  const auto handle =
      future_api->SafeAlloc<Credential>(kCredentialFn_GameCenterGetCredential);

  future_api->Complete(handle, kAuthErrorInvalidCredential,
                       "GameCenter is not supported on Android.");

  FIREBASE_ASSERT_RETURN(MakeFuture(future_api, handle),
                         is_gamecenter_available_on_android);

  return MakeFuture(future_api, handle);
}

// static
Future<Credential> GameCenterAuthProvider::GetCredentialLastResult() {
  auto future_api = GetCredentialFutureImpl();
  auto last_result =
      future_api->LastResult(kCredentialFn_GameCenterGetCredential);
  return static_cast<const Future<Credential>&>(last_result);
}

// static
bool GameCenterAuthProvider::IsPlayerAuthenticated() {
  // Game Center is not available on Android, thus the player can never be
  // authenticated.
  bool is_gamecenter_available_on_android = false;
  FIREBASE_ASSERT_RETURN(false, is_gamecenter_available_on_android);
  return false;
}

// This implementation of ForceResendingTokenData is specific to Android.
class ForceResendingTokenData {
 public:
  ForceResendingTokenData() : token_global_ref_(nullptr) {}
  ~ForceResendingTokenData() { FreeRef(); }

  // token_ref can be a local or global reference.
  void SetRef(jobject token_ref) {
    FreeRef();
    JNIEnv* env = GetJniEnv();
    token_global_ref_ =
        token_ref == nullptr ? nullptr : env->NewGlobalRef(token_ref);
  }

  void FreeRef() {
    if (token_global_ref_ != nullptr) {
      JNIEnv* env = GetJniEnv();
      env->DeleteGlobalRef(token_global_ref_);
      token_global_ref_ = nullptr;
    }
  }
  jobject token_global_ref() const { return token_global_ref_; }

 private:
  jobject token_global_ref_;
};

PhoneAuthProvider::ForceResendingToken::ForceResendingToken()
    : data_(new ForceResendingTokenData) {}

PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() { delete data_; }

PhoneAuthProvider::ForceResendingToken::ForceResendingToken(
    const ForceResendingToken& rhs)
    : data_(new ForceResendingTokenData) {
  data_->SetRef(rhs.data_->token_global_ref());
}

PhoneAuthProvider::ForceResendingToken&
PhoneAuthProvider::ForceResendingToken::operator=(
    const ForceResendingToken& rhs) {
  data_->SetRef(rhs.data_->token_global_ref());
  return *this;
}

bool PhoneAuthProvider::ForceResendingToken::operator==(
    const ForceResendingToken& rhs) const {
  JNIEnv* env = GetJniEnv();
  return env->IsSameObject(data_->token_global_ref(),
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
    JNIEnv* env = GetJniEnv();
    env->DeleteGlobalRef(data_->j_phone_auth_provider);
    delete data_;
  }
}

// This implementation of PhoneListenerData is specific to Android.
struct PhoneListenerData {
  PhoneListenerData() : j_listener(nullptr) {}

  // The JniAuthStateListener class that has the same lifespan as the C++ class.
  jobject j_listener;
};

PhoneAuthProvider::Listener::Listener() : data_(new PhoneListenerData) {
  JNIEnv* env = GetJniEnv();

  // Create the JniAuthStateListener class to redirect the state-change
  // from Java to C++. Make it a global reference so it sticks around.
  jobject j_listener_local = env->NewObject(
      jniphone::GetClass(), jniphone::GetMethodId(jniphone::kConstructor),
      reinterpret_cast<jlong>(this));

  // The C++ Listener keeps a global reference to the Java Listener,
  // and the Java Listener keeps a pointer to the C++ Listener.
  // Destruction of the C++ Listener triggers destruction of the Java
  // Listener.
  data_->j_listener = env->NewGlobalRef(j_listener_local);
}

PhoneAuthProvider::Listener::~Listener() {
  JNIEnv* env = GetJniEnv();

  // Disable the Java Listener by nulling its pointer to the C++ Listener
  // (which is being destroyed).
  env->CallVoidMethod(data_->j_listener,
                      jniphone::GetMethodId(jniphone::kDisconnect));
  assert(env->ExceptionCheck() == false);

  // Remove our reference to the Java Listener. Once Auth Java is done with it,
  // it will be garbage collected.
  env->DeleteGlobalRef(data_->j_listener);

  // Delete the Android-specific pimpl.
  delete data_;
}

void PhoneAuthProvider::VerifyPhoneNumber(
    const char* phone_number, uint32_t auto_verify_time_out_ms,
    const ForceResendingToken* force_resending_token, Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  JNIEnv* env = GetJniEnv();

  // Convert parameters to Java equivalents.
  jstring j_phone_number = env->NewStringUTF(phone_number);
  jobject j_milliseconds = env->GetStaticObjectField(
      timeunit::GetClass(), timeunit::GetFieldId(timeunit::kMilliseconds));
  jlong j_time_out =
      static_cast<jlong>(std::min(auto_verify_time_out_ms, kMaxTimeoutMs));
  jobject j_token = force_resending_token == nullptr
                        ? nullptr
                        : force_resending_token->data_->token_global_ref();

  // Call PhoneAuthProvider.verifyPhoneNumber in Java.
  env->CallVoidMethod(data_->j_phone_auth_provider,
                      phonecred::GetMethodId(phonecred::kVerifyPhoneNumber),
                      j_phone_number, j_time_out, j_milliseconds,
                      data_->auth_data->app->activity(),
                      listener->data_->j_listener, j_token);

  if (firebase::util::CheckAndClearJniExceptions(env)) {
    // If an error occurred with the call to verifyPhoneNumber, inform the
    // listener that it failed.
    if (listener) {
      if (!phone_number || !strlen(phone_number)) {
        listener->OnVerificationFailed(
            "Unable to verify with empty phone number");
      } else {
        listener->OnVerificationFailed(
            "Unable to verify the given phone number");
      }
    }
  }

  env->DeleteLocalRef(j_phone_number);
  env->DeleteLocalRef(j_milliseconds);
}

Credential PhoneAuthProvider::GetCredential(const char* verification_id,
                                            const char* verification_code) {
  FIREBASE_ASSERT_RETURN(Credential(), verification_id && verification_code);
  FIREBASE_ASSERT_MESSAGE_RETURN(Credential(), g_methods_cached,
                                 kMethodsNotCachedError);

  JNIEnv* env = Env(data_->auth_data);

  jstring j_verification_id = env->NewStringUTF(verification_id);
  jstring j_verification_code = env->NewStringUTF(verification_code);

  jobject j_cred = env->CallStaticObjectMethod(
      phonecred::GetClass(), phonecred::GetMethodId(phonecred::kGetCredential),
      j_verification_id, j_verification_code);
  if (firebase::util::CheckAndClearJniExceptions(env)) j_cred = nullptr;

  env->DeleteLocalRef(j_verification_id);
  env->DeleteLocalRef(j_verification_code);

  return Credential(CredentialLocalToGlobalRef(j_cred));
}

// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (provider.data_ == nullptr) {
    JNIEnv* env = Env(auth->auth_data_);

    // Get a global reference to the Java PhoneAuthProvider for this Auth.
    jobject j_phone_auth_provider_local = env->CallStaticObjectMethod(
        phonecred::GetClass(), phonecred::GetMethodId(phonecred::kGetInstance),
        AuthImpl(auth->auth_data_));

    // Create the implementation class that holds the global references.
    // The global references will be freed when provider is destroyed
    // (during the Auth destructor).
    provider.data_ = new PhoneAuthProviderData();
    provider.data_->j_phone_auth_provider =
        env->NewGlobalRef(j_phone_auth_provider_local);
    provider.data_->auth_data = auth->auth_data_;
  }
  return provider;
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnVerificationCompleted().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationCompleted(
    JNIEnv* env, jobject j_listener, jlong c_listener, jobject j_credential) {
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  listener->OnVerificationCompleted(
      Credential(CredentialLocalToGlobalRef(j_credential)));
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnVerificationFailed().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnVerificationFailed(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring exception_message) {
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  listener->OnVerificationFailed(util::JStringToString(env, exception_message));
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnCodeSent().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeSent(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring j_verification_id, jobject j_force_resending_token) {
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);

  // Change the passed-in local reference to a global reference that has the
  // lifespan of the ForceResendingToken.
  PhoneAuthProvider::ForceResendingToken token;
  token.data_->SetRef(j_force_resending_token);
  listener->OnCodeSent(util::JniStringToString(env, j_verification_id), token);
}

// Redirect JniAuthPhoneListener.java callback to C++
// PhoneAuthProvider::Listener::OnCodeAutoRetrievalTimeOut().
JNIEXPORT void JNICALL JniAuthPhoneListener::nativeOnCodeAutoRetrievalTimeOut(
    JNIEnv* env, jobject j_listener, jlong c_listener,
    jstring j_verification_id) {
  auto listener = reinterpret_cast<PhoneAuthProvider::Listener*>(c_listener);
  listener->OnCodeAutoRetrievalTimeOut(
      util::JniStringToString(env, j_verification_id));
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

namespace {
// Pulls data out of the C++ FederatedOAuthProviderData structure and
// constructs a com.google.firebase.OAuthProvider object.
// @note This function detects but does not clear jni exceptions.
// @return A pointer to an OAuthProvider, or nullptr on failure.
jobject ConstructOAuthProvider(
    AuthData* auth_data, const FederatedOAuthProviderData& provider_data) {
  JNIEnv* env = Env(auth_data);
  assert(env);
  jstring provider_id = env->NewStringUTF(provider_data.provider_id.c_str());
  jobject oauthprovider_builder = env->CallStaticObjectMethod(
      oauthprovider::GetClass(),
      oauthprovider::GetMethodId(oauthprovider::kNewBuilder), provider_id,
      AuthImpl(auth_data));
  env->DeleteLocalRef(provider_id);
  if (env->ExceptionCheck()) {
    return nullptr;
  }

  jobject scopes_list = util::StdVectorToJavaList(env, provider_data.scopes);
  if (env->ExceptionCheck()) {
    return nullptr;
  }

  jobject builder_return_ref = env->CallObjectMethod(
      oauthprovider_builder,
      oauthprovider_builder::GetMethodId(oauthprovider_builder::kSetScopes),
      scopes_list);
  env->DeleteLocalRef(scopes_list);
  if (env->ExceptionCheck()) {
    env->DeleteLocalRef(oauthprovider_builder);
    return nullptr;
  }
  env->DeleteLocalRef(builder_return_ref);

  jobject custom_parameters_map =
      env->NewObject(util::hash_map::GetClass(),
                     util::hash_map::GetMethodId(util::hash_map::kConstructor));
  util::StdMapToJavaMap(env, &custom_parameters_map,
                        provider_data.custom_parameters);
  builder_return_ref =
      env->CallObjectMethod(oauthprovider_builder,
                            oauthprovider_builder::GetMethodId(
                                oauthprovider_builder::kAddCustomParameters),
                            custom_parameters_map);
  env->DeleteLocalRef(custom_parameters_map);
  if (env->ExceptionCheck()) {
    env->DeleteLocalRef(oauthprovider_builder);
    return nullptr;
  }
  env->DeleteLocalRef(builder_return_ref);

  jobject oauthprovider = env->CallObjectMethod(
      oauthprovider_builder,
      oauthprovider_builder::GetMethodId(oauthprovider_builder::kBuild));
  env->DeleteLocalRef(oauthprovider_builder);
  if (env->ExceptionCheck()) {
    return nullptr;
  }

  return oauthprovider;
}
}  // namespace

Future<SignInResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  assert(auth_data);
  JNIEnv* env = Env(auth_data);

  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle = futures.SafeAlloc<SignInResult>(
      kAuthFn_SignInWithProvider, SignInResult());

  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_);
  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    jobject task = env->CallObjectMethod(
        AuthImpl(auth_data),
        auth_idp::GetMethodId(auth_idp::kStartActivityForSignInWithProvider),
        auth_data->app->activity(), oauthprovider);
    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                            ReadSignInResult);
    }
    env->DeleteLocalRef(task);
  }

  env->DeleteLocalRef(oauthprovider);
  return MakeFuture(&futures, handle);
}

Future<SignInResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  assert(auth_data);
  JNIEnv* env = Env(auth_data);
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle =
      futures.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider, SignInResult());

  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_);
  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    jobject task = env->CallObjectMethod(
        UserImpl(auth_data),
        user_idp::GetMethodId(user_idp::kStartActivityForLinkWithProvider),
        auth_data->app->activity(), oauthprovider);
    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                            ReadSignInResult);
    }
    env->DeleteLocalRef(task);
  }

  env->DeleteLocalRef(oauthprovider);
  return MakeFuture(&futures, handle);
}

Future<SignInResult> FederatedOAuthProvider::Reauthenticate(
    AuthData* auth_data) {
  assert(auth_data);
  JNIEnv* env = Env(auth_data);
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle = futures.SafeAlloc<SignInResult>(
      kUserFn_ReauthenticateWithProvider, SignInResult());

  jobject oauthprovider = ConstructOAuthProvider(auth_data, provider_data_);
  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    jobject task = env->CallObjectMethod(
        UserImpl(auth_data),
        user_idp::GetMethodId(
            user_idp::kStartActivityForReauthenticateWithProvider),
        auth_data->app->activity(), oauthprovider);
    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterFederatedAuthProviderCallback(task, handle, auth_data,
                                            ReadSignInResult);
    }
    env->DeleteLocalRef(task);
  }

  env->DeleteLocalRef(oauthprovider);
  return MakeFuture(&futures, handle);
}

}  // namespace auth
}  // namespace firebase
