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

#include <cstring>

#include "app/src/jni/collection.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/time.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

#define TOKEN_RESULT_METHODS(X)                             \
  X(GetToken, "getToken", "()Ljava/lang/String;")           \
  X(GetExpirationTimestamp, "getExpirationTimestamp", "()J") \
  X(GetAuthTimestamp, "getAuthTimestamp", "()J")            \
  X(GetIssuedAtTimestamp, "getIssuedAtTimestamp", "()J")    \
  X(GetSignInProvider, "getSignInProvider", "()Ljava/lang/String;") \
  X(GetSignInSecondFactor, "getSignInSecondFactor", "()Ljava/lang/String;") \
  X(GetClaims, "getClaims", "()Ljava/util/Map;")
JNI_METHOD_LOOKUP_DECLARATION(tokenresult, TOKEN_RESULT_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/GetTokenResult")

#define USER_METHODS(X)                                                      \
  X(IsAnonymous, "isAnonymous", "()Z")                                       \
  X(Token, "getIdToken", "(Z)Lcom/google/android/gms/tasks/Task;")           \
  X(ProviderData, "getProviderData", "()Ljava/util/List;")                   \
  X(VerifyBeforeUpdateEmail, "verifyBeforeUpdateEmail",                      \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")               \
  X(UpdatePassword, "updatePassword",                                        \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")               \
  X(UpdateUserProfile, "updateProfile",                                      \
    "(Lcom/google/firebase/auth/UserProfileChangeRequest;)"                  \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(LinkWithCredential, "linkWithCredential",                                \
    "(Lcom/google/firebase/auth/AuthCredential;)"                            \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(Unlink, "unlink",                                                        \
    "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;")               \
  X(UpdatePhoneNumberCredential, "updatePhoneNumber",                        \
    "(Lcom/google/firebase/auth/PhoneAuthCredential;)"                       \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(Reload, "reload", "()Lcom/google/android/gms/tasks/Task;")               \
  X(Reauthenticate, "reauthenticate",                                        \
    "(Lcom/google/firebase/auth/AuthCredential;)"                            \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(ReauthenticateAndRetrieveData, "reauthenticateAndRetrieveData",          \
    "(Lcom/google/firebase/auth/AuthCredential;)"                            \
    "Lcom/google/android/gms/tasks/Task;")                                   \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;")               \
  X(SendEmailVerification, "sendEmailVerification",                          \
    "()Lcom/google/android/gms/tasks/Task;")                                 \
  X(GetMetadata, "getMetadata",                                              \
    "()Lcom/google/firebase/auth/FirebaseUserMetadata;")
JNI_METHOD_LOOKUP_DECLARATION(user, USER_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FirebaseUser")

#define USER_INFO_METHODS(X)                                    \
  X(GetUid, "getUid", "()Ljava/lang/String;")                   \
  X(GetProviderId, "getProviderId", "()Ljava/lang/String;")     \
  X(GetDisplayName, "getDisplayName", "()Ljava/lang/String;")   \
  X(GetPhoneNumber, "getPhoneNumber", "()Ljava/lang/String;")   \
  X(GetPhotoUrl, "getPhotoUrl", "()Landroid/net/Uri;")          \
  X(GetEmail, "getEmail", "()Ljava/lang/String;")               \
  X(IsEmailVerified, "isEmailVerified", "()Z")
JNI_METHOD_LOOKUP_DECLARATION(userinfo, USER_INFO_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/UserInfo")

#define METADATA_METHODS(X)                                   \
  X(GetLastSignInTimestamp, "getLastSignInTimestamp", "()J")  \
  X(GetCreationTimestamp, "getCreationTimestamp", "()J")
JNI_METHOD_LOOKUP_DECLARATION(
    metadata, METADATA_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseUserMetadata")

#define USER_PROFILE_BUILDER_METHODS(X)                                        \
  X(Constructor, "<init>", "()V")                                              \
  X(SetDisplayName, "setDisplayName",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/auth/UserProfileChangeRequest$Builder;")             \
  X(SetPhotoUri, "setPhotoUri",                                                \
    "(Landroid/net/Uri;)"                                                      \
    "Lcom/google/firebase/auth/UserProfileChangeRequest$Builder;")             \
  X(Build, "build", "()Lcom/google/firebase/auth/UserProfileChangeRequest;")
JNI_METHOD_LOOKUP_DECLARATION(
    userprofilebuilder, USER_PROFILE_BUILDER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/UserProfileChangeRequest$Builder")

bool CacheUserMethodIds(JNIEnv* env, jobject activity) {
  return tokenresult::CacheMethodIds(env, activity) &&
         user::CacheMethodIds(env, activity) &&
         userinfo::CacheMethodIds(env, activity) &&
         metadata::CacheMethodIds(env, activity) &&
         userprofilebuilder::CacheMethodIds(env, activity);
}

void ReleaseUserClasses(JNIEnv* env) {
  tokenresult::ReleaseClass(env);
  user::ReleaseClass(env);
  userinfo::ReleaseClass(env);
  metadata::ReleaseClass(env);
  userprofilebuilder::ReleaseClass(env);
}

enum PropertyType { kPropertyTypeString, kPropertyTypeUri };

static std::string GetUserProperty(AuthData* auth_data, jobject impl,
                                   userinfo::Method method_id,
                                   PropertyType type = kPropertyTypeString) {
  JNIEnv* env = Env(auth_data);
  jobject property =
      impl ? env->CallObjectMethod(impl, userinfo::GetMethodId(method_id))
           : nullptr;
  if (::firebase::jni::CheckAndClearException(env) || !property) {
    if (property) env->DeleteLocalRef(property);
    return std::string();
  }
  return type == kPropertyTypeUri
             ? ::firebase::jni::JniUriToString(env, property)
             : jni::JniStringToStringAndReleaseLocalRef(env, property);
}

static std::string GetCurrentUserProperty(
    AuthData* auth_data, userinfo::Method method_id,
    PropertyType type = kPropertyTypeString) {
  if (!ValidUser(auth_data)) return "";
  JNIEnv* env = Env(auth_data);
  jobject user_local = AcquireUserLocalRef(env, auth_data);
  if (!user_local) return "";
  std::string result = GetUserProperty(auth_data, user_local, method_id, type);
  env->DeleteLocalRef(user_local);
  return result;
}

bool User::is_valid() const { return ValidUser(auth_data_); }
std::string User::uid() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetUid);
}
std::string User::email() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetEmail);
}
std::string User::display_name() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetDisplayName);
}
std::string User::phone_number() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetPhoneNumber);
}
std::string User::photo_url() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetPhotoUrl,
                                kPropertyTypeUri);
}
std::string User::provider_id() const {
  return GetCurrentUserProperty(auth_data_, userinfo::kGetProviderId);
}

static void ReadTokenResultImpl(jobject result,
                                FutureCallbackData<std::string>* d,
                                bool success, void* void_data,
                                bool force_refresh) {
  auto data = static_cast<std::string*>(void_data);
  JNIEnv* env = Env(d->auth_data);
  if (success) {
    FIREBASE_ASSERT(result != nullptr);
    jstring j_token = static_cast<jstring>(env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetToken)));
    if (jni::CheckAndClearException(env)) {
      if (j_token) env->DeleteLocalRef(j_token);
      j_token = nullptr;
    }
    *data = jni::JniStringToStringAndReleaseLocalRef(env, j_token);
    if (force_refresh &&
        d->auth_data->ShouldTriggerIdTokenListenerCallback()) {
      RecordPretriggeredIdTokenCallback(d->auth_data);
      NotifyIdTokenListeners(d->auth_data);
    }
  } else {
    if (force_refresh) d->auth_data->SetExpectIdTokenListenerCallback(false);
    *data = std::string();
  }
}

void ReadTokenResult(jobject result, FutureCallbackData<std::string>* d,
                     bool success, void* void_data) {
  ReadTokenResultImpl(result, d, success, void_data, false);
}
static void ReadTokenResultForced(jobject result,
                                  FutureCallbackData<std::string>* d,
                                  bool success, void* void_data) {
  ReadTokenResultImpl(result, d, success, void_data, true);
}

static void ReadIdTokenResultImpl(jobject result,
                                  FutureCallbackData<IdTokenResult>* d,
                                  bool success, void* void_data,
                                  bool force_refresh) {
  auto data = static_cast<IdTokenResult*>(void_data);
  JNIEnv* env = Env(d->auth_data);
  if (!success) {
    if (force_refresh) d->auth_data->SetExpectIdTokenListenerCallback(false);
    *data = IdTokenResult();
    return;
  }
  FIREBASE_ASSERT(result != nullptr);
  auto call_str = [&](tokenresult::Method m) {
    jobject s = env->CallObjectMethod(result, tokenresult::GetMethodId(m));
    (void)jni::CheckAndClearException(env);
    return jni::JniStringToStringAndReleaseLocalRef(env, s);
  };
  auto call_u64 = [&](tokenresult::Method m) {
    jlong v = env->CallLongMethod(result, tokenresult::GetMethodId(m));
    (void)jni::CheckAndClearException(env);
    return static_cast<uint64_t>(v);
  };
  data->token = call_str(tokenresult::kGetToken);
  data->expiration_timestamp = call_u64(tokenresult::kGetExpirationTimestamp);
  data->auth_timestamp = call_u64(tokenresult::kGetAuthTimestamp);
  data->issued_at_timestamp = call_u64(tokenresult::kGetIssuedAtTimestamp);
  data->sign_in_provider = call_str(tokenresult::kGetSignInProvider);
  data->sign_in_second_factor = call_str(tokenresult::kGetSignInSecondFactor);
  data->claims.clear();
  jobject j_claims = env->CallObjectMethod(
      result, tokenresult::GetMethodId(tokenresult::kGetClaims));
  (void)jni::CheckAndClearException(env);
  if (j_claims != nullptr) {
    firebase::Variant v = firebase::util::JavaObjectToVariant(env, j_claims);
    if (v.is_map()) data->claims = v.map();
    env->DeleteLocalRef(j_claims);
  }
  if (force_refresh && d->auth_data->ShouldTriggerIdTokenListenerCallback()) {
    RecordPretriggeredIdTokenCallback(d->auth_data);
    NotifyIdTokenListeners(d->auth_data);
  }
}

void ReadIdTokenResult(jobject result, FutureCallbackData<IdTokenResult>* d,
                       bool success, void* void_data) {
  ReadIdTokenResultImpl(result, d, success, void_data, false);
}
static void ReadIdTokenResultForced(jobject result,
                                    FutureCallbackData<IdTokenResult>* d,
                                    bool success, void* void_data) {
  ReadIdTokenResultImpl(result, d, success, void_data, true);
}

template <typename T>
static void GetTokenCommon(
    AuthData* auth_data, Promise<T> promise, bool force_refresh,
    typename FutureCallbackData<T>::ReadFutureResultFn* read_normal,
    typename FutureCallbackData<T>::ReadFutureResultFn* read_forced) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  JNIEnv* env = Env(auth_data);
  if (force_refresh) auth_data->SetExpectIdTokenListenerCallback(true);
  jobject user_local = AcquireUserLocalRef(env, auth_data);
  if (user_local == nullptr) {
    if (force_refresh) auth_data->SetExpectIdTokenListenerCallback(false);
    promise.Fail(kAuthErrorFailure, "User is no longer valid.");
    return;
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kToken), force_refresh);
  env->DeleteLocalRef(user_local);
  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl,
                                     promise.handle())) {
    RegisterCallback(pending_result, promise.handle(), auth_data,
                     force_refresh ? read_forced : read_normal);
    env->DeleteLocalRef(pending_result);
  } else if (force_refresh) {
    auth_data->SetExpectIdTokenListenerCallback(false);
  }
}

void GetToken(AuthData* auth_data, Promise<std::string> promise,
              bool force_refresh) {
  GetTokenCommon(auth_data, promise, force_refresh, ReadTokenResult,
                 ReadTokenResultForced);
}

void GetTokenInternal(AuthData* auth_data, Promise<std::string> promise,
                      bool force_refresh, int /*future_identifier*/) {
  GetToken(auth_data, promise, force_refresh);
}

void GetIdTokenResult(AuthData* auth_data, Promise<IdTokenResult> promise,
                      bool force_refresh) {
  GetTokenCommon(auth_data, promise, force_refresh, ReadIdTokenResult,
                 ReadIdTokenResultForced);
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  if (!ValidUser(auth_data_)) return provider_data;
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return provider_data;
  const jobject list = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kProviderData));
  env->DeleteLocalRef(user_local);
  if (jni::CheckAndClearException(env) || !list) {
    if (list) env->DeleteLocalRef(list);
    return provider_data;
  }
  const int num_providers =
      env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
  if (!jni::CheckAndClearException(env) && num_providers > 0) {
    provider_data.reserve(num_providers);
    for (int i = 0; i < num_providers; ++i) {
      jobject j_info = env->CallObjectMethod(
          list, util::list::GetMethodId(util::list::kGet), i);
      if (jni::CheckAndClearException(env) || !j_info) {
        if (j_info) env->DeleteLocalRef(j_info);
        continue;
      }
      UserInfoInterface info;
      info.uid_ = GetUserProperty(auth_data_, j_info, userinfo::kGetUid);
      info.email_ = GetUserProperty(auth_data_, j_info, userinfo::kGetEmail);
      info.display_name_ =
          GetUserProperty(auth_data_, j_info, userinfo::kGetDisplayName);
      info.photo_url_ = GetUserProperty(
          auth_data_, j_info, userinfo::kGetPhotoUrl, kPropertyTypeUri);
      info.provider_id_ =
          GetUserProperty(auth_data_, j_info, userinfo::kGetProviderId);
      info.phone_number_ =
          GetUserProperty(auth_data_, j_info, userinfo::kGetPhoneNumber);
      env->DeleteLocalRef(j_info);
      provider_data.push_back(info);
    }
  }
  env->DeleteLocalRef(list);
  return provider_data;
}

void UpdatePassword(AuthData* auth_data, Promise<void> promise,
                    const char* password) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!password) {
    promise.Fail(kAuthErrorMissingPassword, "Password must be provided.");
    return;
  }
  RunAndroidUserTaskWithPromise(
      auth_data, promise, UpdateUserOnSuccess,
      [=](JNIEnv* env, jobject user_local) {
        jstring j_password = env->NewStringUTF(password);
        jobject task = env->CallObjectMethod(
            user_local, user::GetMethodId(user::kUpdatePassword), j_password);
        env->DeleteLocalRef(j_password);
        return task;
      });
}

void UpdateUserProfile(AuthData* auth_data, Promise<void> promise,
                       const User::UserProfile& profile) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  JNIEnv* env = Env(auth_data);
  std::string err_msg;
  jobject builder = env->NewObject(
      userprofilebuilder::GetClass(),
      userprofilebuilder::GetMethodId(userprofilebuilder::kConstructor));
  AuthError error = CheckAndClearJniAuthExceptions(env, &err_msg);
  if (!builder && error == kAuthErrorNone) {
    error = kAuthErrorFailure;
    err_msg = "Failed to construct UserProfileChangeRequest.Builder.";
  }

  if (error == kAuthErrorNone && profile.display_name != nullptr) {
    jstring j_name = env->NewStringUTF(profile.display_name);
    jobject discard = env->CallObjectMethod(
        builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetDisplayName),
        j_name);
    error = CheckAndClearJniAuthExceptions(env, &err_msg);
    if (discard) env->DeleteLocalRef(discard);
    if (j_name) env->DeleteLocalRef(j_name);
  }

  if (error == kAuthErrorNone && profile.photo_url != nullptr) {
    jobject j_uri = nullptr;
    if (strlen(profile.photo_url) > 0) {
      j_uri = ::firebase::jni::CharsToJniUri(env, profile.photo_url);
      error = CheckAndClearJniAuthExceptions(env, &err_msg);
      if (!j_uri && error == kAuthErrorNone) {
        error = kAuthErrorFailure;
        err_msg = "Invalid photo URL.";
      }
    }
    if (error == kAuthErrorNone) {
      jobject discard = env->CallObjectMethod(
          builder,
          userprofilebuilder::GetMethodId(userprofilebuilder::kSetPhotoUri),
          j_uri);
      error = CheckAndClearJniAuthExceptions(env, &err_msg);
      if (discard) env->DeleteLocalRef(discard);
    }
    if (j_uri) env->DeleteLocalRef(j_uri);
  }

  jobject req = nullptr;
  if (error == kAuthErrorNone) {
    req = env->CallObjectMethod(
        builder, userprofilebuilder::GetMethodId(userprofilebuilder::kBuild));
    error = CheckAndClearJniAuthExceptions(env, &err_msg);
    if (!req && error == kAuthErrorNone) {
      error = kAuthErrorFailure;
      err_msg = "Failed to build UserProfileChangeRequest.";
    }
  }

  if (error == kAuthErrorNone) {
    RunAndroidUserTaskWithPromise(
        auth_data, promise, UpdateUserOnSuccess,
        [&](JNIEnv* e, jobject user_local) {
          return e->CallObjectMethod(
              user_local, user::GetMethodId(user::kUpdateUserProfile), req);
        });
  } else {
    promise.Fail(error, err_msg.c_str());
  }
  if (req) env->DeleteLocalRef(req);
  if (builder) env->DeleteLocalRef(builder);
}

void LinkWithCredential(AuthData* auth_data, Promise<AuthResult> promise,
                        const Credential& credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateCredentialForPromise(promise, credential)) return;
  RunAndroidUserTaskWithPromise(
      auth_data, promise, ReadAuthResult,
      [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(
            user_local, user::GetMethodId(user::kLinkWithCredential),
            CredentialFromImpl(internal::CredentialAccess::GetImpl(credential)));
      });
}

Future<AuthResult> LinkWithProvider(AuthData* auth_data,
                                    FederatedAuthProvider* provider) {
  if (!ValidUser(auth_data)) {
    if (!auth_data) return Future<AuthResult>();
    Promise<AuthResult> promise(&auth_data->future_impl,
                                kUserFn_LinkWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data);
}

void Unlink(AuthData* auth_data, Promise<AuthResult> promise,
            const char* provider) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!provider || provider[0] == '\0') {
    promise.Fail(kAuthErrorInvalidProviderId, "Provider ID must be provided.");
    return;
  }
  RunAndroidUserTaskWithPromise(
      auth_data, promise, ReadAuthResult,
      [=](JNIEnv* env, jobject user_local) {
        jstring j_provider = env->NewStringUTF(provider);
        jobject task = env->CallObjectMethod(
            user_local, user::GetMethodId(user::kUnlink), j_provider);
        env->DeleteLocalRef(j_provider);
        return task;
      });
}

void UpdatePhoneNumberCredential(AuthData* auth_data, Promise<User> promise,
                                 const PhoneAuthCredential& credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateCredentialForPromise(promise, credential)) return;
  JNIEnv* env = Env(auth_data);
  jobject j_credential =
      CredentialFromImpl(internal::CredentialAccess::GetImpl(credential));
  if (!j_credential ||
      !env->IsInstanceOf(j_credential, phonecredential::GetClass())) {
    promise.Fail(kAuthErrorInvalidCredential,
                 "Credential is not a phone credential.");
    return;
  }
  RunAndroidUserTaskWithPromise(
      auth_data, promise,
      [](jobject /*result*/, FutureCallbackData<User>* d, bool success,
         void* void_data) {
        auto* data = static_cast<User*>(void_data);
        if (success) {
          UpdateCurrentUser(d->auth_data);
          *data = d->auth_data->auth->current_user();
        } else {
          *data = User();
        }
      },
      [=](JNIEnv* e, jobject user_local) {
        return e->CallObjectMethod(
            user_local, user::GetMethodId(user::kUpdatePhoneNumberCredential),
            j_credential);
      });
}

void Reload(AuthData* auth_data, Promise<void> promise) {
  RunAndroidUserTaskWithPromise(
      auth_data, promise, UpdateUserOnSuccess,
      [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(user_local,
                                     user::GetMethodId(user::kReload));
      });
}

void Reauthenticate(AuthData* auth_data, Promise<void> promise,
                    const Credential& credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateCredentialForPromise(promise, credential)) return;
  RunAndroidUserTaskWithPromise(
      auth_data, promise, UpdateUserOnSuccess,
      [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(
            user_local, user::GetMethodId(user::kReauthenticate),
            CredentialFromImpl(internal::CredentialAccess::GetImpl(credential)));
      });
}

void ReauthenticateAndRetrieveData(AuthData* auth_data,
                                   Promise<AuthResult> promise,
                                   const Credential& credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateCredentialForPromise(promise, credential)) return;
  RunAndroidUserTaskWithPromise(
      auth_data, promise, ReadAuthResult,
      [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(
            user_local, user::GetMethodId(user::kReauthenticateAndRetrieveData),
            CredentialFromImpl(internal::CredentialAccess::GetImpl(credential)));
      });
}

Future<AuthResult> ReauthenticateWithProvider(AuthData* auth_data,
                                              FederatedAuthProvider* provider) {
  if (!ValidUser(auth_data)) {
    if (!auth_data) return Future<AuthResult>();
    Promise<AuthResult> promise(&auth_data->future_impl,
                                kUserFn_ReauthenticateWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data);
}

void SendEmailVerification(AuthData* auth_data, Promise<void> promise) {
  RunAndroidUserTaskWithPromise(
      auth_data, promise, nullptr, [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(
            user_local, user::GetMethodId(user::kSendEmailVerification));
      });
}

void SendEmailVerificationBeforeUpdatingEmail(AuthData* auth_data,
                                              Promise<void> promise,
                                              const char* email) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!email || email[0] == '\0') {
    promise.Fail(kAuthErrorMissingEmail, "Email must be provided.");
    return;
  }
  RunAndroidUserTaskWithPromise(
      auth_data, promise, nullptr, [=](JNIEnv* env, jobject user_local) {
        jstring j_email = env->NewStringUTF(email);
        jobject task = env->CallObjectMethod(
            user_local, user::GetMethodId(user::kVerifyBeforeUpdateEmail),
            j_email);
        env->DeleteLocalRef(j_email);
        return task;
      });
}

void Delete(AuthData* auth_data, Promise<void> promise) {
  RunAndroidUserTaskWithPromise(
      auth_data, promise, UpdateUserOnSuccess,
      [=](JNIEnv* env, jobject user_local) {
        return env->CallObjectMethod(user_local,
                                     user::GetMethodId(user::kDelete));
      });
}

UserMetadata User::metadata() const {
  if (!ValidUser(auth_data_)) return UserMetadata();
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return UserMetadata();
  jobject userMetadata = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kGetMetadata));
  env->DeleteLocalRef(user_local);
  if (jni::CheckAndClearException(env) || !userMetadata) {
    if (userMetadata) env->DeleteLocalRef(userMetadata);
    return UserMetadata();
  }
  UserMetadata data;
  data.last_sign_in_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetLastSignInTimestamp));
  (void)jni::CheckAndClearException(env);
  data.creation_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetCreationTimestamp));
  (void)jni::CheckAndClearException(env);
  env->DeleteLocalRef(userMetadata);
  return data;
}

static bool GetUserBoolProperty(AuthData* auth_data, jmethodID method_id) {
  if (!ValidUser(auth_data)) return false;
  JNIEnv* env = Env(auth_data);
  jobject user_local = AcquireUserLocalRef(env, auth_data);
  if (!user_local) return false;
  bool result = env->CallBooleanMethod(user_local, method_id);
  env->DeleteLocalRef(user_local);
  (void)jni::CheckAndClearException(env);
  return result;
}

bool User::is_email_verified() const {
  return GetUserBoolProperty(
      auth_data_, userinfo::GetMethodId(userinfo::kIsEmailVerified));
}

bool User::is_anonymous() const {
  return GetUserBoolProperty(auth_data_, user::GetMethodId(user::kIsAnonymous));
}

}  // namespace auth
}  // namespace firebase
