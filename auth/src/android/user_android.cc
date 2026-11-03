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

#include "app/src/time.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using util::CharsToJniUri;
using util::JniStringToString;
using util::JniUriToString;

// clang-format off
#define TOKEN_RESULT_METHODS(X)                                                \
    X(GetToken, "getToken", "()Ljava/lang/String;"),                           \
    X(GetExpirationTimestamp, "getExpirationTimestamp", "()J"),                \
    X(GetAuthTimestamp, "getAuthTimestamp", "()J"),                            \
    X(GetIssuedAtTimestamp, "getIssuedAtTimestamp", "()J"),                    \
    X(GetSignInProvider, "getSignInProvider", "()Ljava/lang/String;"),         \
    X(GetSignInSecondFactor, "getSignInSecondFactor", "()Ljava/lang/String;"), \
    X(GetClaims, "getClaims", "()Ljava/util/Map;")
// clang-format on
METHOD_LOOKUP_DECLARATION(tokenresult, TOKEN_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(tokenresult,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/GetTokenResult",
                         TOKEN_RESULT_METHODS)

// clang-format off
#define USER_METHODS(X)                                                        \
    X(IsAnonymous, "isAnonymous", "()Z"),                                      \
    X(Token, "getIdToken", "(Z)Lcom/google/android/gms/tasks/Task;"),          \
    X(ProviderData, "getProviderData", "()Ljava/util/List;"),                  \
    X(VerifyBeforeUpdateEmail, "verifyBeforeUpdateEmail",                      \
      "(Ljava/lang/String;)Lcom/google/android/gms/tasks/Task;"),              \
    X(UpdatePassword, "updatePassword", "(Ljava/lang/String;)"                 \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(UpdateUserProfile, "updateProfile",                                      \
      "(Lcom/google/firebase/auth/UserProfileChangeRequest;)"                  \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(LinkWithCredential, "linkWithCredential",                                \
      "(Lcom/google/firebase/auth/AuthCredential;)"                            \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(Unlink, "unlink", "(Ljava/lang/String;)"                                 \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(UpdatePhoneNumberCredential, "updatePhoneNumber",                        \
      "(Lcom/google/firebase/auth/PhoneAuthCredential;)"                       \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(Reload, "reload", "()Lcom/google/android/gms/tasks/Task;"),              \
    X(Reauthenticate, "reauthenticate",                                        \
      "(Lcom/google/firebase/auth/AuthCredential;)"                            \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(ReauthenticateAndRetrieveData, "reauthenticateAndRetrieveData",          \
      "(Lcom/google/firebase/auth/AuthCredential;)"                            \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
    X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;"),              \
    X(SendEmailVerification, "sendEmailVerification",                          \
      "()Lcom/google/android/gms/tasks/Task;"),                                \
    X(GetMetadata, "getMetadata",                                              \
      "()Lcom/google/firebase/auth/FirebaseUserMetadata;")
// clang-format on
METHOD_LOOKUP_DECLARATION(user, USER_METHODS)
METHOD_LOOKUP_DEFINITION(user,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseUser",
                         USER_METHODS)

// clang-format off
#define USER_INFO_METHODS(X)                                                   \
    X(GetUid, "getUid", "()Ljava/lang/String;"),                               \
    X(GetProviderId, "getProviderId", "()Ljava/lang/String;"),                 \
    X(GetDisplayName, "getDisplayName", "()Ljava/lang/String;"),               \
    X(GetPhoneNumber, "getPhoneNumber", "()Ljava/lang/String;"),               \
    X(GetPhotoUrl, "getPhotoUrl", "()Landroid/net/Uri;"),                      \
    X(GetEmail, "getEmail", "()Ljava/lang/String;"),                           \
    X(IsEmailVerified, "isEmailVerified", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(userinfo, USER_INFO_METHODS)
METHOD_LOOKUP_DEFINITION(userinfo,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/UserInfo",
                         USER_INFO_METHODS)

// clang-format off
#define METADATA_METHODS(X)                                                    \
    X(GetLastSignInTimestamp, "getLastSignInTimestamp", "()J"),                \
    X(GetCreationTimestamp, "getCreationTimestamp", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(metadata, METADATA_METHODS)
METHOD_LOOKUP_DEFINITION(metadata,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseUserMetadata",
                         METADATA_METHODS)

// clang-format off
#define USER_PROFILE_BUILDER_METHODS(X)                                        \
    X(Constructor, "<init>", "()V"),                                           \
    X(SetDisplayName, "setDisplayName", "(Ljava/lang/String;)"                 \
      "Lcom/google/firebase/auth/UserProfileChangeRequest$Builder;"),          \
    X(SetPhotoUri, "setPhotoUri", "(Landroid/net/Uri;)"                        \
      "Lcom/google/firebase/auth/UserProfileChangeRequest$Builder;"),          \
    X(Build, "build", "()Lcom/google/firebase/auth/UserProfileChangeRequest;")
// clang-format on
METHOD_LOOKUP_DECLARATION(userprofilebuilder, USER_PROFILE_BUILDER_METHODS)
METHOD_LOOKUP_DEFINITION(
    userprofilebuilder,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/UserProfileChangeRequest$Builder",
    USER_PROFILE_BUILDER_METHODS)

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
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    if (property) {
      env->DeleteLocalRef(property);
    }
    return std::string();
  }
  if (type == kPropertyTypeUri) {
    return JniUriToString(env, property);
  } else {
    // type == kPropertyTypeString
    return JniStringToString(env, property);
  }
}

static std::string GetUID(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetUid);
}

static std::string GetEmail(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetEmail);
}

static std::string GetDisplayName(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetDisplayName);
}

static std::string GetPhoneNumber(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetPhoneNumber);
}

static std::string GetPhotoUrl(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetPhotoUrl,
                         kPropertyTypeUri);
}

static std::string GetProviderId(AuthData* auth_data, jobject impl) {
  return GetUserProperty(auth_data, impl, userinfo::kGetProviderId);
}

static jobject AcquireUserLocalRef(JNIEnv* env, AuthData* auth_data) {
  if (!env || !auth_data) return nullptr;
  MutexLock lock(auth_data->future_impl.mutex());
  jobject user_impl = UserImpl(auth_data);
  return user_impl != nullptr ? env->NewLocalRef(user_impl) : nullptr;
}

static void UpdateUserOnSuccess(jobject /*result*/,
                                FutureCallbackData<void>* d, bool success,
                                void* /*void_data*/) {
  if (success) {
    UpdateCurrentUser(d->auth_data);
  }
}

User::User(const User& user) { auth_data_ = user.auth_data_; }

User& User::operator=(const User& user) {
  auth_data_ = user.auth_data_;
  return *this;
}

User::~User() { auth_data_ = nullptr; }

bool User::is_valid() const { return ValidUser(auth_data_); }

std::string User::uid() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetUID(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

std::string User::email() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetEmail(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

std::string User::display_name() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetDisplayName(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

std::string User::phone_number() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetPhoneNumber(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

std::string User::photo_url() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetPhotoUrl(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

std::string User::provider_id() const {
  if (!auth_data_) return "";
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return "";
  std::string result = GetProviderId(auth_data_, user_local);
  env->DeleteLocalRef(user_local);
  return result;
}

static void ReadTokenResultImpl(jobject result,
                                FutureCallbackData<std::string>* d,
                                bool success, void* void_data,
                                bool force_refresh) {
  auto data = static_cast<std::string*>(void_data);
  JNIEnv* env = Env(d->auth_data);

  // `result` is of type GetTokenResult when `success` is true.
  if (success) {
    // This comes from the successfully completed Task in Java, so it should
    // always be valid.
    FIREBASE_ASSERT(result != nullptr);
    jstring j_token = static_cast<jstring>(env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetToken)));
    if (util::CheckAndClearJniExceptions(env)) {
      if (j_token) env->DeleteLocalRef(j_token);
      j_token = nullptr;
    }
    *data = JniStringToString(env, j_token);
    if (force_refresh &&
        d->auth_data->ShouldTriggerIdTokenListenerCallback()) {
      RecordPretriggeredIdTokenCallback(d->auth_data);
      NotifyIdTokenListeners(d->auth_data);
    }
  } else {
    if (force_refresh) {
      d->auth_data->SetExpectIdTokenListenerCallback(false);
    }
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

Future<std::string> User::GetToken(bool force_refresh) {
  if (!ValidUser(auth_data_)) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<std::string>(kUserFn_GetToken);
  JNIEnv* env = Env(auth_data_);

  if (force_refresh) {
    auth_data_->SetExpectIdTokenListenerCallback(true);
  }
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    if (force_refresh) {
      auth_data_->SetExpectIdTokenListenerCallback(false);
    }
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kToken), force_refresh);
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     force_refresh ? ReadTokenResultForced : ReadTokenResult);
    env->DeleteLocalRef(pending_result);
  } else if (force_refresh) {
    // If the method failed for some reason, clear the expected callback.
    auth_data_->SetExpectIdTokenListenerCallback(false);
  }
  return MakeFuture(&futures, handle);
}

static void ReadIdTokenResultImpl(jobject result,
                                  FutureCallbackData<IdTokenResult>* d,
                                  bool success, void* void_data,
                                  bool force_refresh) {
  auto data = static_cast<IdTokenResult*>(void_data);
  JNIEnv* env = Env(d->auth_data);

  // `result` is of type GetTokenResult when `success` is true.
  if (success) {
    // This comes from the successfully completed Task in Java, so it should
    // always be valid.
    FIREBASE_ASSERT(result != nullptr);
    const jobject j_token = env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetToken));
    util::CheckAndClearJniExceptions(env);
    const jlong expiration_timestamp = env->CallLongMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetExpirationTimestamp));
    util::CheckAndClearJniExceptions(env);
    const jlong auth_timestamp = env->CallLongMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetAuthTimestamp));
    util::CheckAndClearJniExceptions(env);
    const jlong issued_at_timestamp = env->CallLongMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetIssuedAtTimestamp));
    util::CheckAndClearJniExceptions(env);
    const jobject j_sign_in_provider = env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetSignInProvider));
    util::CheckAndClearJniExceptions(env);
    const jobject j_sign_in_second_factor = env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetSignInSecondFactor));
    util::CheckAndClearJniExceptions(env);
    const jobject j_claims = env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetClaims));
    util::CheckAndClearJniExceptions(env);

    // Note that JniStringToString releases local references for us.
    data->token = JniStringToString(env, j_token);
    data->expiration_timestamp = static_cast<uint64_t>(expiration_timestamp);
    data->auth_timestamp = static_cast<uint64_t>(auth_timestamp);
    data->issued_at_timestamp = static_cast<uint64_t>(issued_at_timestamp);
    data->sign_in_provider = JniStringToString(env, j_sign_in_provider);
    data->sign_in_second_factor =
        JniStringToString(env, j_sign_in_second_factor);
    data->claims.clear();
    if (j_claims != nullptr) {
      util::JavaMapToVariantMap(env, &data->claims, j_claims);
      env->DeleteLocalRef(j_claims);
    }
    if (force_refresh &&
        d->auth_data->ShouldTriggerIdTokenListenerCallback()) {
      RecordPretriggeredIdTokenCallback(d->auth_data);
      NotifyIdTokenListeners(d->auth_data);
    }
  } else {
    if (force_refresh) {
      d->auth_data->SetExpectIdTokenListenerCallback(false);
    }
    *data = IdTokenResult();
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

Future<IdTokenResult> User::GetIdTokenResult(bool force_refresh) {
  if (!ValidUser(auth_data_)) {
    return Future<IdTokenResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<IdTokenResult>(kUserFn_GetIdTokenResult);
  JNIEnv* env = Env(auth_data_);

  if (force_refresh) {
    auth_data_->SetExpectIdTokenListenerCallback(true);
  }
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    if (force_refresh) {
      auth_data_->SetExpectIdTokenListenerCallback(false);
    }
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kToken), force_refresh);
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(
        pending_result, handle, auth_data_,
        force_refresh ? ReadIdTokenResultForced : ReadIdTokenResult);
    env->DeleteLocalRef(pending_result);
  } else if (force_refresh) {
    // If the method failed for some reason, clear the expected callback.
    auth_data_->SetExpectIdTokenListenerCallback(false);
  }
  return MakeFuture(&futures, handle);
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  if (!auth_data_) return provider_data;
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local != nullptr) {
    // getProviderData returns `List<? extends UserInfo>`
    const jobject list = env->CallObjectMethod(
        user_local, user::GetMethodId(user::kProviderData));
    env->DeleteLocalRef(user_local);
    if (util::CheckAndClearJniExceptions(env)) {
      if (list) env->DeleteLocalRef(list);
      return provider_data;
    }

    // Copy the Java UserInfo list into the returned vector.
    if (list != nullptr) {
      const int num_providers =
          env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
      if (!util::CheckAndClearJniExceptions(env) && num_providers > 0) {
        provider_data.reserve(num_providers);
        for (int i = 0; i < num_providers; ++i) {
          jobject j_user_info = env->CallObjectMethod(
              list, util::list::GetMethodId(util::list::kGet), i);
          if (util::CheckAndClearJniExceptions(env) || j_user_info == nullptr) {
            if (j_user_info) env->DeleteLocalRef(j_user_info);
            continue;
          }
          UserInfoInterface user_info;
          user_info.uid_ = GetUID(auth_data_, j_user_info);
          user_info.email_ = GetEmail(auth_data_, j_user_info);
          user_info.display_name_ = GetDisplayName(auth_data_, j_user_info);
          user_info.photo_url_ = GetPhotoUrl(auth_data_, j_user_info);
          user_info.provider_id_ = GetProviderId(auth_data_, j_user_info);
          user_info.phone_number_ = GetPhoneNumber(auth_data_, j_user_info);
          env->DeleteLocalRef(j_user_info);
          provider_data.push_back(user_info);
        }
      }
      env->DeleteLocalRef(list);
    }
  }
  return provider_data;
}

Future<void> User::UpdatePassword(const char* password) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdatePassword);
  if (!password) {
    futures.Complete(handle, kAuthErrorMissingPassword,
                     "Password must be provided.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jstring j_password = env->NewStringUTF(password);
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kUpdatePassword), j_password);
  env->DeleteLocalRef(user_local);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, UpdateUserOnSuccess);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::UpdateUserProfile(const UserProfile& profile) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdateUserProfile);
  JNIEnv* env = Env(auth_data_);

  AuthError error = kAuthErrorNone;
  std::string exception_error_message;
  jobject j_user_profile_builder = env->NewObject(
      userprofilebuilder::GetClass(),
      userprofilebuilder::GetMethodId(userprofilebuilder::kConstructor));
  error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
  if (j_user_profile_builder == nullptr && error == kAuthErrorNone) {
    error = kAuthErrorFailure;
    exception_error_message =
        "Failed to construct UserProfileChangeRequest.Builder.";
  }

  // Call UserProfileChangeRequest.Builder.setDisplayName.
  if (error == kAuthErrorNone && profile.display_name != nullptr) {
    jstring j_display_name = env->NewStringUTF(profile.display_name);
    jobject j_builder_discard = env->CallObjectMethod(
        j_user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetDisplayName),
        j_display_name);

    error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
    if (j_builder_discard) {
      env->DeleteLocalRef(j_builder_discard);
    }
    if (j_display_name) {
      env->DeleteLocalRef(j_display_name);
    }
  }

  // Call UserProfileChangeRequest.Builder.setPhotoUri.
  if (error == kAuthErrorNone && profile.photo_url != nullptr) {
    jobject j_uri = nullptr;
    if (strlen(profile.photo_url) > 0) {
      j_uri = CharsToJniUri(env, profile.photo_url);
      error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
      if (j_uri == nullptr && error == kAuthErrorNone) {
        error = kAuthErrorFailure;
        exception_error_message = "Invalid photo URL.";
      }
    }
    if (error == kAuthErrorNone) {
      jobject j_builder_discard = env->CallObjectMethod(
          j_user_profile_builder,
          userprofilebuilder::GetMethodId(userprofilebuilder::kSetPhotoUri),
          j_uri);
      error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
      if (j_builder_discard) {
        env->DeleteLocalRef(j_builder_discard);
      }
    }
    if (j_uri) {
      env->DeleteLocalRef(j_uri);
    }
  }

  jobject j_user_profile_request = nullptr;
  if (error == kAuthErrorNone) {
    // Call UserProfileChangeRequest.Builder.build.
    j_user_profile_request = env->CallObjectMethod(
        j_user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kBuild));
    error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
    if (j_user_profile_request == nullptr && error == kAuthErrorNone) {
      error = kAuthErrorFailure;
      exception_error_message = "Failed to build UserProfileChangeRequest.";
    }
  }

  if (error == kAuthErrorNone) {
    // Call FirebaseUser.updateProfile.
    jobject user_local = AcquireUserLocalRef(env, auth_data_);
    if (user_local == nullptr) {
      futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    } else {
      jobject pending_result = env->CallObjectMethod(
          user_local, user::GetMethodId(user::kUpdateUserProfile),
          j_user_profile_request);
      env->DeleteLocalRef(user_local);
      if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
        RegisterCallback(pending_result, handle, auth_data_,
                         UpdateUserOnSuccess);
        env->DeleteLocalRef(pending_result);
      }
    }
  } else {
    futures.Complete(handle, error, exception_error_message.c_str());
  }
  if (j_user_profile_request) {
    env->DeleteLocalRef(j_user_profile_request);
  }
  if (j_user_profile_builder) {
    env->DeleteLocalRef(j_user_profile_builder);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::LinkWithCredential(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kUserFn_LinkWithCredential);
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  } else if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential,
                     "Invalid credential.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kLinkWithCredential),
      CredentialFromImpl(credential.impl_));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::LinkWithProvider(
    FederatedAuthProvider* provider) const {
  if (!ValidUser(auth_data_)) {
    return Future<AuthResult>();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data_);
}

Future<AuthResult> User::Unlink(const char* provider) {
  if (!ValidUser(auth_data_)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kUserFn_Unlink);
  if (!provider || strlen(provider) == 0) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     "Provider ID must be provided.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jstring j_provider = env->NewStringUTF(provider);
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kUnlink), j_provider);
  env->DeleteLocalRef(user_local);
  env->DeleteLocalRef(j_provider);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<User> User::UpdatePhoneNumberCredential(
    const PhoneAuthCredential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<User>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<User>(kUserFn_UpdatePhoneNumberCredential);
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  } else if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential,
                     "Invalid credential.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject j_credential = CredentialFromImpl(credential.impl_);
  if (j_credential != nullptr &&
      env->IsInstanceOf(j_credential, phonecredential::GetClass())) {
    jobject user_local = AcquireUserLocalRef(env, auth_data_);
    if (user_local == nullptr) {
      futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
      return MakeFuture(&futures, handle);
    }
    jobject pending_result = env->CallObjectMethod(
        user_local, user::GetMethodId(user::kUpdatePhoneNumberCredential),
        j_credential);
    env->DeleteLocalRef(user_local);

    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterCallback(
          pending_result, handle, auth_data_,
          [](jobject /*result*/, FutureCallbackData<User>* d, bool success,
             void* void_data) {
            auto* data = static_cast<User*>(void_data);
            if (success) {
              UpdateCurrentUser(d->auth_data);
              *data = d->auth_data->auth->current_user();
            } else {
              *data = User();
            }
          });
      env->DeleteLocalRef(pending_result);
    }
  } else {
    futures.Complete(handle, kAuthErrorInvalidCredential,
                     "Credential is not a phone credential.");
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::Reload() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reload);
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result =
      env->CallObjectMethod(user_local, user::GetMethodId(user::kReload));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, UpdateUserOnSuccess);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::Reauthenticate(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reauthenticate);
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  } else if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential,
                     "Invalid credential.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kReauthenticate),
      CredentialFromImpl(credential.impl_));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, UpdateUserOnSuccess);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kUserFn_ReauthenticateAndRetrieveData);
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_,
                     credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  } else if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential,
                     "Invalid credential.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kReauthenticateAndRetrieveData),
      CredentialFromImpl(credential.impl_));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadAuthResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::ReauthenticateWithProvider(
    FederatedAuthProvider* provider) const {
  if (!ValidUser(auth_data_)) {
    return Future<AuthResult>();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data_);
}

Future<void> User::SendEmailVerification() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_SendEmailVerification);
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kSendEmailVerification));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::SendEmailVerificationBeforeUpdatingEmail(const char* email) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<void>(kUserFn_SendEmailVerificationBeforeUpdatingEmail);
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail,
                     "Email must be provided.");
    return MakeFuture(&futures, handle);
  }
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jstring j_email = env->NewStringUTF(email);
  jobject pending_result = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kVerifyBeforeUpdateEmail), j_email);
  env->DeleteLocalRef(user_local);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::Delete() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Delete);
  JNIEnv* env = Env(auth_data_);

  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (user_local == nullptr) {
    futures.Complete(handle, kAuthErrorFailure, "User is no longer valid.");
    return MakeFuture(&futures, handle);
  }
  jobject pending_result =
      env->CallObjectMethod(user_local, user::GetMethodId(user::kDelete));
  env->DeleteLocalRef(user_local);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, UpdateUserOnSuccess);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

UserMetadata User::metadata() const {
  if (!auth_data_) return UserMetadata();
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return UserMetadata();

  jobject userMetadata = env->CallObjectMethod(
      user_local, user::GetMethodId(user::kGetMetadata));
  env->DeleteLocalRef(user_local);
  if (util::CheckAndClearJniExceptions(env) || !userMetadata) {
    if (userMetadata) env->DeleteLocalRef(userMetadata);
    return UserMetadata();
  }

  UserMetadata data;
  data.last_sign_in_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetLastSignInTimestamp));
  util::CheckAndClearJniExceptions(env);
  data.creation_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetCreationTimestamp));
  util::CheckAndClearJniExceptions(env);

  env->DeleteLocalRef(userMetadata);

  return data;
}

bool User::is_email_verified() const {
  if (!auth_data_) return false;
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return false;
  bool result = env->CallBooleanMethod(
      user_local, userinfo::GetMethodId(userinfo::kIsEmailVerified));
  env->DeleteLocalRef(user_local);
  util::CheckAndClearJniExceptions(env);
  return result;
}

bool User::is_anonymous() const {
  if (!auth_data_) return false;
  JNIEnv* env = Env(auth_data_);
  jobject user_local = AcquireUserLocalRef(env, auth_data_);
  if (!user_local) return false;
  bool result = env->CallBooleanMethod(user_local,
                                       user::GetMethodId(user::kIsAnonymous));
  env->DeleteLocalRef(user_local);
  util::CheckAndClearJniExceptions(env);
  return result;
}

}  // namespace auth
}  // namespace firebase
