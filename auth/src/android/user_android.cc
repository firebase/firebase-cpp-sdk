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

#include "app/src/time.h"
#include "auth/src/android/common_android.h"

namespace firebase {
namespace auth {

using util::CharsToJniUri;
using util::JniStringToString;
using util::JniUriToString;

// clang-format off
#define PHONE_CREDENTIAL_METHODS(X)                                            \
    X(GetSmsCode, "getSmsCode", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(phonecredential, PHONE_CREDENTIAL_METHODS)
METHOD_LOOKUP_DEFINITION(phonecredential,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/PhoneAuthCredential",
                         PHONE_CREDENTIAL_METHODS)

// clang-format off
#define TOKEN_RESULT_METHODS(X)                                                \
    X(GetToken, "getToken", "()Ljava/lang/String;")
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
    X(UpdateEmail, "updateEmail", "(Ljava/lang/String;)"                       \
      "Lcom/google/android/gms/tasks/Task;"),                                  \
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
  return phonecredential::CacheMethodIds(env, activity) &&
         tokenresult::CacheMethodIds(env, activity) &&
         user::CacheMethodIds(env, activity) &&
         userinfo::CacheMethodIds(env, activity) &&
         metadata::CacheMethodIds(env, activity) &&
         userprofilebuilder::CacheMethodIds(env, activity);
}

void ReleaseUserClasses(JNIEnv* env) {
  phonecredential::ReleaseClass(env);
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

User::~User() {}

std::string User::uid() const {
  return ValidUser(auth_data_) ? GetUID(auth_data_, UserImpl(auth_data_)) : "";
}

std::string User::email() const {
  return ValidUser(auth_data_) ? GetEmail(auth_data_, UserImpl(auth_data_))
                               : "";
}

std::string User::display_name() const {
  return ValidUser(auth_data_)
             ? GetDisplayName(auth_data_, UserImpl(auth_data_))
             : "";
}

std::string User::phone_number() const {
  return ValidUser(auth_data_)
             ? GetPhoneNumber(auth_data_, UserImpl(auth_data_))
             : "";
}

std::string User::photo_url() const {
  return ValidUser(auth_data_) ? GetPhotoUrl(auth_data_, UserImpl(auth_data_))
                               : "";
}

std::string User::provider_id() const {
  return ValidUser(auth_data_) ? GetProviderId(auth_data_, UserImpl(auth_data_))
                               : "";
}

class AndroidWrappedUserInfo : public UserInfoInterface {
 public:
  AndroidWrappedUserInfo(AuthData* auth_data, jobject user_info)
      : auth_data_(auth_data), user_info_(user_info) {
    // Convert `user_info` to a global reference.
    JNIEnv* env = Env(auth_data_);
    user_info_ = env->NewGlobalRef(user_info);
    env->DeleteLocalRef(user_info);
  }

  virtual ~AndroidWrappedUserInfo() {
    // Delete global reference.
    JNIEnv* env = Env(auth_data_);
    env->DeleteGlobalRef(user_info_);
    user_info_ = nullptr;
  }

  std::string uid() const override { return GetUID(auth_data_, user_info_); }

  std::string email() const override {
    return GetEmail(auth_data_, user_info_);
  }

  std::string display_name() const override {
    return GetDisplayName(auth_data_, user_info_);
  }

  std::string phone_number() const override {
    return GetPhoneNumber(auth_data_, user_info_);
  }

  std::string photo_url() const override {
    return GetPhotoUrl(auth_data_, user_info_);
  }

  std::string provider_id() const override {
    return GetProviderId(auth_data_, user_info_);
  }

 private:
  /// Pointer to the main class.
  /// Needed for context in implementation of virtuals.
  AuthData* auth_data_;

  /// Pointer to the main class.
  /// Needed for context in implementation of virtuals.
  jobject user_info_;
};

void ReadTokenResult(jobject result, FutureCallbackData<std::string>* d,
                     bool success, void* void_data) {
  auto data = static_cast<std::string*>(void_data);
  JNIEnv* env = Env(d->auth_data);

  // `result` is of type GetTokenResult when `success` is true.
  if (success) {
    if (d->auth_data->ShouldTriggerIdTokenListenerCallback()) {
      NotifyIdTokenListeners(d->auth_data);
    }
    // This comes from the successfully completed Task in Java, so it should
    // always be valid.
    FIREBASE_ASSERT(result != nullptr);
    jstring j_token = static_cast<jstring>(env->CallObjectMethod(
        result, tokenresult::GetMethodId(tokenresult::kGetToken)));
    assert(env->ExceptionCheck() == false);
    *data = JniStringToString(env, j_token);
  } else {
    *data = std::string();
  }
}

Future<std::string> User::GetToken(bool force_refresh) {
  if (!ValidUser(auth_data_)) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<std::string>(kUserFn_GetToken);
  JNIEnv* env = Env(auth_data_);

  auth_data_->SetExpectIdTokenListenerCallback(force_refresh);
  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kToken), force_refresh);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadTokenResult);
    env->DeleteLocalRef(pending_result);
  } else {
    // If the method failed for some reason, clear the expected callback.
    auth_data_->SetExpectIdTokenListenerCallback(false);
  }
  return MakeFuture(&futures, handle);
}

const std::vector<UserInfoInterface*>& User::provider_data() const {
  ClearUserInfos(auth_data_);

  if (ValidUser(auth_data_)) {
    JNIEnv* env = Env(auth_data_);

    // getProviderData returns `List<? extends UserInfo>`
    const jobject list = env->CallObjectMethod(
        UserImpl(auth_data_), user::GetMethodId(user::kProviderData));
    assert(env->ExceptionCheck() == false);

    // Copy the list into auth_data_->user_infos.
    if (list != nullptr) {
      const int num_providers =
          env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
      assert(env->ExceptionCheck() == false);
      auth_data_->user_infos.resize(num_providers);

      for (int i = 0; i < num_providers; ++i) {
        // user_info is converted to a global reference in
        // AndroidWrappedUserInfo() and the local reference is released.
        jobject user_info = env->CallObjectMethod(
            list, util::list::GetMethodId(util::list::kGet), i);
        assert(env->ExceptionCheck() == false);
        auth_data_->user_infos[i] =
            new AndroidWrappedUserInfo(auth_data_, user_info);
      }
      env->DeleteLocalRef(list);
    }
  }

  // Return a reference to our internally-backed values.
  return auth_data_->user_infos;
}

Future<void> User::UpdateEmail(const char* email) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdateEmail);
  JNIEnv* env = Env(auth_data_);

  jstring j_email = env->NewStringUTF(email);
  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kUpdateEmail), j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::UpdatePassword(const char* password) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdatePassword);
  JNIEnv* env = Env(auth_data_);

  jstring j_password = env->NewStringUTF(password);
  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kUpdatePassword),
      j_password);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
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

  // Painfully call UserProfileChangeRequest.Builder.setDisplayName.
  if (profile.display_name != nullptr) {
    jstring j_display_name = env->NewStringUTF(profile.display_name);
    jobject j_builder_discard = env->CallObjectMethod(
        j_user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetDisplayName),
        j_display_name);

    error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
    if (j_builder_discard) {
      env->DeleteLocalRef(j_builder_discard);
    }
    env->DeleteLocalRef(j_display_name);
  }

  // Extra painfully call UserProfileChangeRequest.Builder.setPhotoUri.
  if (error == kAuthErrorNone && profile.photo_url != nullptr) {
    jobject j_uri = CharsToJniUri(env, profile.photo_url);
    jobject j_builder_discard = env->CallObjectMethod(
        j_user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetPhotoUri),
        j_uri);
    error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
    if (j_builder_discard) {
      env->DeleteLocalRef(j_builder_discard);
    }
    env->DeleteLocalRef(j_uri);
  }

  jobject j_user_profile_request = nullptr;
  if (error == kAuthErrorNone) {
    // Call UserProfileChangeRequest.Builder.build.
    j_user_profile_request = env->CallObjectMethod(
        j_user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kBuild));
    error = CheckAndClearJniAuthExceptions(env, &exception_error_message);
  }

  if (error == kAuthErrorNone) {
    // Call FirebaseUser.updateProfile.
    jobject pending_result = env->CallObjectMethod(
        UserImpl(auth_data_), user::GetMethodId(user::kUpdateUserProfile),
        j_user_profile_request);

    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterCallback(pending_result, handle, auth_data_, nullptr);
      env->DeleteLocalRef(pending_result);
    }
    return MakeFuture(&futures, handle);
  } else {
    futures.Complete(handle, error, exception_error_message.c_str());
  }
  if (j_user_profile_request) {
    env->DeleteLocalRef(j_user_profile_request);
  }
  env->DeleteLocalRef(j_user_profile_builder);
  return MakeFuture(&futures, handle);
}

Future<User*> User::LinkWithCredential(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kUserFn_LinkWithCredential);
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kLinkWithCredential),
      CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential(
    const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<SignInResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<SignInResult>(
      kUserFn_LinkAndRetrieveDataWithCredential);
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kLinkWithCredential),
      CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::LinkWithProvider(
    FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Link(auth_data_);
}

Future<User*> User::Unlink(const char* provider) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kUserFn_Unlink);
  JNIEnv* env = Env(auth_data_);

  jstring j_provider = env->NewStringUTF(provider);
  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kUnlink), j_provider);
  env->DeleteLocalRef(j_provider);

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<User*> User::UpdatePhoneNumberCredential(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<User*>(kUserFn_UpdatePhoneNumberCredential);
  JNIEnv* env = Env(auth_data_);

  jobject j_credential = CredentialFromImpl(credential.impl_);
  if (env->IsInstanceOf(j_credential, phonecredential::GetClass())) {
    jobject pending_result = env->CallObjectMethod(
        UserImpl(auth_data_),
        user::GetMethodId(user::kUpdatePhoneNumberCredential), j_credential);

    if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
      RegisterCallback(pending_result, handle, auth_data_,
                       ReadUserFromSignInResult);
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

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kReload));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
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
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kReauthenticate),
      CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<SignInResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<SignInResult>(kUserFn_ReauthenticateAndRetrieveData);
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_),
      user::GetMethodId(user::kReauthenticateAndRetrieveData),
      CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_, ReadSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::ReauthenticateWithProvider(
    FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Reauthenticate(auth_data_);
}


Future<void> User::SendEmailVerification() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_SendEmailVerification);
  JNIEnv* env = Env(auth_data_);

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kSendEmailVerification));

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

  jobject pending_result = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kDelete));

  if (!CheckAndCompleteFutureOnError(env, &futures, handle)) {
    RegisterCallback(pending_result, handle, auth_data_,
                     [](jobject result, FutureCallbackData<void>* d,
                        bool success, void* void_data) {
                       if (success) {
                         UpdateCurrentUser(d->auth_data);
                       }
                     });
    env->DeleteLocalRef(pending_result);
  }
  return MakeFuture(&futures, handle);
}

UserMetadata User::metadata() const {
  if (!ValidUser(auth_data_)) return UserMetadata();

  JNIEnv* env = Env(auth_data_);

  jobject userMetadata = env->CallObjectMethod(
      UserImpl(auth_data_), user::GetMethodId(user::kGetMetadata));
  util::CheckAndClearJniExceptions(env);

  if (!userMetadata) return UserMetadata();

  UserMetadata data;
  data.last_sign_in_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetLastSignInTimestamp));
  assert(env->ExceptionCheck() == false);
  data.creation_timestamp = env->CallLongMethod(
      userMetadata, metadata::GetMethodId(metadata::kGetCreationTimestamp));
  assert(env->ExceptionCheck() == false);

  env->DeleteLocalRef(userMetadata);

  return data;
}

bool User::is_email_verified() const {
  if (!ValidUser(auth_data_)) return false;

  JNIEnv* env = Env(auth_data_);
  bool result = env->CallBooleanMethod(
      UserImpl(auth_data_), userinfo::GetMethodId(userinfo::kIsEmailVerified));
  util::CheckAndClearJniExceptions(env);
  return result;
}

bool User::is_anonymous() const {
  if (!ValidUser(auth_data_)) return false;

  JNIEnv* env = Env(auth_data_);
  bool result = env->CallBooleanMethod(UserImpl(auth_data_),
                                       user::GetMethodId(user::kIsAnonymous));
  util::CheckAndClearJniExceptions(env);
  return result;
}

}  // namespace auth
}  // namespace firebase
