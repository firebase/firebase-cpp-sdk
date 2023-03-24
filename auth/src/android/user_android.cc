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

bool CacheUserMethodIds(JNIEnv *env, jobject activity) {
  return phonecredential::CacheMethodIds(env, activity) &&
         tokenresult::CacheMethodIds(env, activity) &&
         user::CacheMethodIds(env, activity) &&
         userinfo::CacheMethodIds(env, activity) &&
         metadata::CacheMethodIds(env, activity) &&
         userprofilebuilder::CacheMethodIds(env, activity);
}

void ReleaseUserClasses(JNIEnv *env) {
  phonecredential::ReleaseClass(env);
  tokenresult::ReleaseClass(env);
  user::ReleaseClass(env);
  userinfo::ReleaseClass(env);
  metadata::ReleaseClass(env);
  userprofilebuilder::ReleaseClass(env);
}

///
/// AndroidWrappedUserInfo Class.
/// Queries data out of Java Android SDK UserInfo objects.
///
enum PropertyType { kPropertyTypeString, kPropertyTypeUri };

class AndroidWrappedUserInfo : public UserInfoInterface {
 public:
  AndroidWrappedUserInfo(AuthData *auth_data, jobject user_info)
      : auth_data_(auth_data), user_info_(user_info) {
    // Convert `user_info` to a global reference.
    JNIEnv *env = Env(auth_data_);
    user_info_ = env->NewGlobalRef(user_info);
    env->DeleteLocalRef(user_info);
  }

  virtual ~AndroidWrappedUserInfo() {
    // Delete global reference.
    JNIEnv *env = Env(auth_data_);
    env->DeleteGlobalRef(user_info_);
    user_info_ = nullptr;
    auth_data_ = nullptr;
  }

  std::string uid() const override {
    return GetUserProperty(userinfo::kGetUid);
  }

  std::string email() const override {
    return GetUserProperty(userinfo::kGetEmail);
  }

  std::string display_name() const override {
    return GetUserProperty(userinfo::kGetDisplayName);
  }

  std::string phone_number() const override {
    return GetUserProperty(userinfo::kGetPhoneNumber);
  }

  std::string photo_url() const override {
    return GetUserProperty(userinfo::kGetPhotoUrl, kPropertyTypeUri);
  }

  std::string provider_id() const override {
    return GetUserProperty(userinfo::kGetProviderId);
  }

 private:
  std::string GetUserProperty(userinfo::Method method_id,
                              PropertyType type = kPropertyTypeString) const {
    JNIEnv *env = Env(auth_data_);
    jobject property = user_info_
                           ? env->CallObjectMethod(
                                 user_info_, userinfo::GetMethodId(method_id))
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

  /// The AuthData context, required JNI environment data.
  AuthData *auth_data_;

  /// Pointer to the main class.
  /// Needed for context in implementation of virtuals.
  jobject user_info_;
};

///
/// User Class
/// Platform specific implementation of UserInternal.
///
User::User(AuthData *auth_data, UserInternal *user_internal) {
  assert(auth_data);
  auth_data_ = auth_data;
  if (user_internal == nullptr) {
    // Create an invalid user internal.
    // This will return is_valid() false, and operations will fail.
    user_internal_ = new UserInternal(auth_data_, nullptr);
  } else {
    user_internal_ = user_internal;
  }
}

User::~User() {
  delete user_internal_;
  user_internal_ = nullptr;
  auth_data_ = nullptr;
}

User &User::operator=(const User &user) {
  assert(user_internal_);
  delete user_internal_;

  auth_data_ = user.auth_data_;

  if (user.user_internal_ != nullptr) {
    user_internal_ = new UserInternal(auth_data_, user.user_internal_->user_);
  } else {
    user_internal_ = new UserInternal(user.auth_data_, nullptr);
  }

  return *this;
}

bool User::is_valid() const {
  assert(user_internal_);
  return user_internal_->is_valid();
}

Future<std::string> User::GetToken(bool force_refresh) {
  assert(user_internal_);
  return user_internal_->GetToken(force_refresh);
}

std::vector<UserInfoInterface> User::provider_data() const {
  assert(user_internal_);
  return user_internal_->provider_data();
}

const std::vector<UserInfoInterface *> &User::provider_data_DEPRECATED() {
  assert(user_internal_);
  return user_internal_->provider_data_DEPRECATED();
}

Future<void> User::UpdateEmail(const char *email) {
  assert(user_internal_);
  return user_internal_->UpdateEmail(email);
}

Future<void> User::UpdateEmailLastResult() const {
  assert(user_internal_);
  return user_internal_->UpdateEmailLastResult();
}

Future<void> User::UpdatePassword(const char *password) {
  assert(user_internal_);
  return user_internal_->UpdatePassword(password);
}

Future<void> User::UpdatePasswordLastResult() const {
  assert(user_internal_);
  return user_internal_->UpdatePasswordLastResult();
}

Future<void> User::Reauthenticate(const Credential &credential) {
  assert(user_internal_);
  return user_internal_->Reauthenticate(credential);
}

Future<void> User::ReauthenticateLastResult() const {
  assert(user_internal_);
  return user_internal_->ReauthenticateLastResult();
}

Future<SignInResult> User::ReauthenticateAndRetrieveData_DEPRECATED(
    const Credential &credential) {
  assert(user_internal_);
  return user_internal_->ReauthenticateAndRetrieveData_DEPRECATED(credential);
}

Future<SignInResult> User::ReauthenticateAndRetrieveDataLastResult_DEPRECATED()
    const {
  assert(user_internal_);
  return user_internal_->ReauthenticateAndRetrieveDataLastResult_DEPRECATED();
}

Future<SignInResult> User::ReauthenticateWithProvider_DEPRECATED(
    FederatedAuthProvider *provider) const {
  assert(user_internal_);
  return user_internal_->ReauthenticateWithProvider_DEPRECATED(provider);
}

Future<void> User::SendEmailVerification() {
  assert(user_internal_);
  return user_internal_->SendEmailVerification();
}

Future<void> User::SendEmailVerificationLastResult() const {
  assert(user_internal_);
  return user_internal_->SendEmailVerificationLastResult();
}

Future<void> User::UpdateUserProfile(const UserProfile &profile) {
  assert(user_internal_);
  return user_internal_->UpdateUserProfile(profile);
}

Future<void> User::UpdateUserProfileLastResult() const {
  assert(user_internal_);
  return user_internal_->UpdateUserProfileLastResult();
}

Future<User *> User::LinkWithCredential_DEPRECATED(
    const Credential &credential) {
  assert(user_internal_);
  return user_internal_->LinkWithCredential_DEPRECATED(credential);
}

Future<User *> User::LinkWithCredentialLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->LinkWithCredentialLastResult_DEPRECATED();
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential_DEPRECATED(
    const Credential &credential) {
  assert(user_internal_);
  return user_internal_->LinkAndRetrieveDataWithCredential_DEPRECATED(
      credential);
}

Future<SignInResult>
User::LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_
      ->LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED();
}

Future<SignInResult> User::LinkWithProvider_DEPRECATED(
    FederatedAuthProvider *provider) {
  assert(user_internal_);
  return user_internal_->LinkWithProvider_DEPRECATED(provider);
}

Future<User *> User::Unlink_DEPRECATED(const char *provider) {
  assert(user_internal_);
  return user_internal_->Unlink_DEPRECATED(provider);
}

Future<User *> User::UnlinkLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->UnlinkLastResult_DEPRECATED();
}

Future<User *> User::UpdatePhoneNumberCredential_DEPRECATED(
    const Credential &credential) {
  assert(user_internal_);
  return user_internal_->UpdatePhoneNumberCredential_DEPRECATED(credential);
}

Future<User *> User::UpdatePhoneNumberCredentialLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->UpdatePhoneNumberCredentialLastResult_DEPRECATED();
}

Future<void> User::Reload() {
  assert(user_internal_);
  return user_internal_->Reload();
}

Future<void> User::ReloadLastResult() const {
  assert(user_internal_);
  return user_internal_->ReloadLastResult();
}

Future<void> User::Delete() {
  assert(user_internal_);
  return user_internal_->Delete();
}

Future<void> User::DeleteLastResult() const {
  assert(user_internal_);
  return user_internal_->DeleteLastResult();
}

UserMetadata User::metadata() const {
  assert(user_internal_);
  return user_internal_->metadata();
}

bool User::is_email_verified() const {
  assert(user_internal_);
  return user_internal_->is_email_verified();
}

bool User::is_anonymous() const {
  assert(user_internal_);
  return user_internal_->is_anonymous();
}

std::string User::uid() const {
  assert(user_internal_);
  return user_internal_->uid();
}

std::string User::email() const {
  assert(user_internal_);
  return user_internal_->email();
}

std::string User::display_name() const {
  assert(user_internal_);
  return user_internal_->display_name();
}

std::string User::photo_url() const {
  assert(user_internal_);
  return user_internal_->photo_url();
}

std::string User::provider_id() const {
  assert(user_internal_);
  return user_internal_->provider_id();
}

std::string User::phone_number() const {
  assert(user_internal_);
  return user_internal_->phone_number();
}

///
/// UserInternal Class
///
void assign_future_id(UserInternal *user_internal, std::string *future_api_id) {
  static const char *kApiIdentifier = "UserInternal";
  future_api_id->reserve(strlen(kApiIdentifier) +
                         16 /* hex characters in the pointer */ +
                         1 /* null terminator */);
  snprintf(&((*future_api_id)[0]), future_api_id->capacity(), "%s0x%016llx",
           kApiIdentifier,
           static_cast<unsigned long long>(  // NOLINT
               reinterpret_cast<intptr_t>(user_internal)));
}

UserInternal::UserInternal(AuthData *auth_data, jobject user)
    : auth_data_(auth_data), user_(nullptr), future_data_(kUserFnCount) {
  assert(auth_data_);
  JNIEnv *env = Env(auth_data_);
  if (user != nullptr) {
    user_ = env->NewGlobalRef(user);
    assert(env->ExceptionCheck() == false);
  }
  assign_future_id(this, &future_api_id_);
}

UserInternal::UserInternal(const UserInternal &user_internal)
    : auth_data_(user_internal.auth_data_),
      user_(user_internal.user_),
      future_data_(kUserFnCount) {
  assign_future_id(this, &future_api_id_);
}

UserInternal::~UserInternal() {
  MutexLock user_lock(user_mutex_);
  JNIEnv *env = Env(auth_data_);
  util::CancelCallbacks(env, future_api_id_.c_str());

  env->DeleteGlobalRef(user_);
  user_ = nullptr;

  {
    MutexLock user_info_lock(user_info_mutex_deprecated_);
    clear_user_infos();
  }

  // Make sure we don't have any pending futures in flight before we disappear.
  while (!future_data_.future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }
  auth_data_ = nullptr;
}

void UserInternal::set_native_user_object_deprecated(jobject user) {
  MutexLock user_lock(user_mutex_);
  user_ = user;
}

bool UserInternal::is_valid() const { return user_ != nullptr; }

void UserInternal::clear_user_infos() {
  for (size_t i = 0; i < user_infos_.size(); ++i) {
    delete user_infos_[i];
    user_infos_[i] = nullptr;
  }
  user_infos_.clear();
}

void ReadTokenResult(jobject result, FutureCallbackData<std::string> *d,
                     bool success, void *void_data) {
  auto data = static_cast<std::string *>(void_data);
  JNIEnv *env = Env(d->auth_data);

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

Future<std::string> UserInternal::GetToken(bool force_refresh) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_GetToken, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage,
                                   &future_data_, "");
  }

  SafeFutureHandle<std::string> future_handle =
      future_data_.future_impl.SafeAlloc<std::string>(kUserFn_GetToken);
  Future<std::string> future =
      MakeFuture(&future_data_.future_impl, future_handle);

  auth_data_->SetExpectIdTokenListenerCallback(force_refresh);

  JNIEnv *env = Env(auth_data_);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kToken), force_refresh);

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, ReadTokenResult);
    env->DeleteLocalRef(pending_result);
  } else {
    // If the method failed for some reason, clear the expected callback.
    auth_data_->SetExpectIdTokenListenerCallback(false);
  }
  return future;
}

std::vector<UserInfoInterface> UserInternal::provider_data() const {
  JNIEnv *env = Env(auth_data_);
  std::vector<UserInfoInterface> local_user_infos;
  if (is_valid()) {
    const jobject list =
        env->CallObjectMethod(user_, user::GetMethodId(user::kProviderData));
    assert(env->ExceptionCheck() == false);

    if (list != nullptr) {
      const int num_providers =
          env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
      assert(env->ExceptionCheck() == false);
      local_user_infos.resize(num_providers);

      for (int i = 0; i < num_providers; ++i) {
        // user_info is converted to a global reference in
        // AndroidWrappedUserInfo() and the local reference is released.
        jobject user_info = env->CallObjectMethod(
            list, util::list::GetMethodId(util::list::kGet), i);
        assert(env->ExceptionCheck() == false);
        local_user_infos[i] = AndroidWrappedUserInfo(auth_data_, user_info);
      }
      env->DeleteLocalRef(list);
    }
  }
  return local_user_infos;
}

const std::vector<UserInfoInterface *>
    &UserInternal::provider_data_DEPRECATED() {
  MutexLock user_info_lock(user_info_mutex_deprecated_);
  clear_user_infos();
  if (is_valid()) {
    JNIEnv *env = Env(auth_data_);
    // getProviderData returns `List<? extends UserInfo>`
    const jobject list =
        env->CallObjectMethod(user_, user::GetMethodId(user::kProviderData));
    assert(env->ExceptionCheck() == false);

    // Copy the list into user_infos_.
    if (list != nullptr) {
      const int num_providers =
          env->CallIntMethod(list, util::list::GetMethodId(util::list::kSize));
      assert(env->ExceptionCheck() == false);
      user_infos_.resize(num_providers);

      for (int i = 0; i < num_providers; ++i) {
        // user_info is converted to a global reference in
        // AndroidWrappedUserInfo() and the local reference is released.
        jobject user_info = env->CallObjectMethod(
            list, util::list::GetMethodId(util::list::kGet), i);
        assert(env->ExceptionCheck() == false);
        user_infos_[i] = new AndroidWrappedUserInfo(auth_data_, user_info);
      }
      env->DeleteLocalRef(list);
    }
  }
  // Return a reference to our internally-backed values.
  return user_infos_;
}

Future<void> UserInternal::UpdateEmail(const char *email) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_UpdateEmail, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage,
                                   &future_data_);
  }
  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdateEmail);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jstring j_email = env->NewStringUTF(email);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kUpdateEmail), j_email);
  env->DeleteLocalRef(j_email);

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::UpdateEmailLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdateEmail));
}

Future<void> UserInternal::UpdatePassword(const char *password) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(
        kUserFn_UpdatePassword, kAuthErrorInvalidUser,
        kUserNotInitializedErrorMessage, &future_data_);
  }
  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdatePassword);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jstring j_password = env->NewStringUTF(password);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kUpdatePassword), j_password);
  env->DeleteLocalRef(j_password);

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::UpdatePasswordLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdatePassword));
}

Future<void> UserInternal::UpdateUserProfile(const User::UserProfile &profile) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(
        kUserFn_UpdateUserProfile, kAuthErrorInvalidUser,
        kUserNotInitializedErrorMessage, &future_data_);
  }

  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdateUserProfile);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  AuthError error = kAuthErrorNone;
  std::string exception_error_message;
  jobject j_user_profile_builder = env->NewObject(
      userprofilebuilder::GetClass(),
      userprofilebuilder::GetMethodId(userprofilebuilder::kConstructor));

  // Call UserProfileChangeRequest.Builder.setDisplayName.
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

  // Call UserProfileChangeRequest.Builder.setPhotoUri.
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
        user_, user::GetMethodId(user::kUpdateUserProfile),
        j_user_profile_request);

    if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                       future_handle)) {
      RegisterCallback(auth_data_, pending_result, future_handle,
                       future_api_id_, &future_data_.future_impl, nullptr);
      env->DeleteLocalRef(pending_result);
    }
    return future;
  } else {
    future_data_.future_impl.Complete(future_handle, error,
                                      exception_error_message.c_str());
  }
  if (j_user_profile_request) {
    env->DeleteLocalRef(j_user_profile_request);
  }
  env->DeleteLocalRef(j_user_profile_builder);
  return future;
}

Future<void> UserInternal::UpdateUserProfileLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdateUserProfile));
}

Future<void> UserInternal::SendEmailVerification() {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(
        kUserFn_SendEmailVerification, kAuthErrorInvalidUser,
        kUserNotInitializedErrorMessage, &future_data_);
  }

  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_SendEmailVerification);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kSendEmailVerification));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::SendEmailVerificationLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_SendEmailVerification));
}

Future<User *> UserInternal::LinkWithCredential_DEPRECATED(
    const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  SafeFutureHandle<User *> future_handle =
      future_data_.future_impl.SafeAlloc<User *>(
          kUserFn_LinkWithCredential_DEPRECATED);
  Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (!is_valid()) {
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                   future_handle, &future_data_, (User *)nullptr);
    return future;
  }

  JNIEnv *env = Env(auth_data_);
  jobject pending_result =
      env->CallObjectMethod(user_, user::GetMethodId(user::kLinkWithCredential),
                            CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<User *> UserInternal::LinkWithCredentialLastResult_DEPRECATED() const {
  return static_cast<const Future<User *> &>(
      future_data_.future_impl.LastResult(
          kUserFn_LinkWithCredential_DEPRECATED));
}

Future<SignInResult> UserInternal::LinkAndRetrieveDataWithCredential_DEPRECATED(
    const Credential &credential) {
  SafeFutureHandle<SignInResult> future_handle =
      future_data_.future_impl.SafeAlloc<SignInResult>(
          kUserFn_LinkAndRetrieveDataWithCredential_DEPRECATED);
  Future<SignInResult> future =
      MakeFuture(&future_data_.future_impl, future_handle);
  if (!is_valid()) {
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                   future_handle, &future_data_, SignInResult());
    return future;
  }

  JNIEnv *env = Env(auth_data_);
  jobject pending_result =
      env->CallObjectMethod(user_, user::GetMethodId(user::kLinkWithCredential),
                            CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, ReadSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<SignInResult>
UserInternal::LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED() const {
  return static_cast<const Future<SignInResult> &>(
      future_data_.future_impl.LastResult(
          kUserFn_LinkAndRetrieveDataWithCredential_DEPRECATED));
}

Future<SignInResult> UserInternal::LinkWithProvider_DEPRECATED(
    FederatedAuthProvider *provider) {
  if (!is_valid() || provider == nullptr) {
    SafeFutureHandle<SignInResult> future_handle =
        future_data_.future_impl.SafeAlloc<SignInResult>(
            kUserFn_LinkWithProvider_DEPRECATED);
    Future<SignInResult> future =
        MakeFuture(&future_data_.future_impl, future_handle);
    if (!is_valid()) {
      CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                     future_handle, &future_data_, SignInResult());
    } else {
      CompleteFuture(kAuthErrorInvalidParameter,
                     kUserNotInitializedErrorMessage, future_handle,
                     &future_data_, SignInResult());
    }
    return future;
  }
  return provider->Link(auth_data_, this);
}

Future<User *> UserInternal::Unlink_DEPRECATED(const char *provider) {
  MutexLock user_info_lock(user_mutex_);
  SafeFutureHandle<User *> future_handle =
      future_data_.future_impl.SafeAlloc<User *>(kUserFn_Unlink_DEPRECATED);
  Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (!is_valid()) {
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                   future_handle, &future_data_, (User *)nullptr);
    return future;
  }

  JNIEnv *env = Env(auth_data_);
  jstring j_provider = env->NewStringUTF(provider);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kUnlink), j_provider);
  env->DeleteLocalRef(j_provider);

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, ReadUserFromSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<User *> UserInternal::UnlinkLastResult_DEPRECATED() const {
  return static_cast<const Future<User *> &>(
      future_data_.future_impl.LastResult(kUserFn_Unlink_DEPRECATED));
}

Future<User *> UserInternal::UpdatePhoneNumberCredential_DEPRECATED(
    const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  SafeFutureHandle<User *> future_handle =
      future_data_.future_impl.SafeAlloc<User *>(
          kUserFn_UpdatePhoneNumberCredential_DEPRECATED);
  Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (!is_valid()) {
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                   future_handle, &future_data_, (User *)nullptr);
    return future;
  }

  JNIEnv *env = Env(auth_data_);
  jobject j_credential = CredentialFromImpl(credential.impl_);
  if (env->IsInstanceOf(j_credential, phonecredential::GetClass())) {
    jobject pending_result = env->CallObjectMethod(
        user_, user::GetMethodId(user::kUpdatePhoneNumberCredential),
        j_credential);

    if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                       future_handle)) {
      RegisterCallback(auth_data_, pending_result, future_handle,
                       future_api_id_, &future_data_.future_impl,
                       ReadUserFromSignInResult);
      env->DeleteLocalRef(pending_result);
    }
  } else {
    CompleteFuture(kAuthErrorInvalidCredential, kInvalidCredentialErrorMessage,
                   future_handle, &future_data_, (User *)nullptr);
  }
  return future;
}

Future<User *> UserInternal::UpdatePhoneNumberCredentialLastResult_DEPRECATED()
    const {
  return static_cast<const Future<User *> &>(
      future_data_.future_impl.LastResult(
          kUserFn_UpdatePhoneNumberCredential_DEPRECATED));
}

Future<void> UserInternal::Reload() {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_Reload, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage,
                                   &future_data_);
  }

  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_Reload);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jobject pending_result =
      env->CallObjectMethod(user_, user::GetMethodId(user::kReload));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::ReloadLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_Reload));
}

Future<void> UserInternal::Reauthenticate(const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(
        kUserFn_Reauthenticate, kAuthErrorInvalidUser,
        kUserNotInitializedErrorMessage, &future_data_);
  }

  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_Reload);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jobject pending_result =
      env->CallObjectMethod(user_, user::GetMethodId(user::kReauthenticate),
                            CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, nullptr);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::ReauthenticateLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_Reauthenticate));
}

Future<SignInResult> UserInternal::ReauthenticateAndRetrieveData_DEPRECATED(
    const Credential &credential) {
  SafeFutureHandle<SignInResult> future_handle =
      future_data_.future_impl.SafeAlloc<SignInResult>(
          kUserFn_ReauthenticateAndRetrieveData_DEPRECATED);
  Future<SignInResult> future =
      MakeFuture(&future_data_.future_impl, future_handle);
  if (!is_valid()) {
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                   future_handle, &future_data_, SignInResult());
    return future;
  }

  JNIEnv *env = Env(auth_data_);
  jobject pending_result = env->CallObjectMethod(
      user_, user::GetMethodId(user::kReauthenticateAndRetrieveData),
      CredentialFromImpl(credential.impl_));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl, ReadSignInResult);
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<SignInResult>
UserInternal::ReauthenticateAndRetrieveDataLastResult_DEPRECATED() const {
  return static_cast<const Future<SignInResult> &>(
      future_data_.future_impl.LastResult(
          kUserFn_ReauthenticateAndRetrieveData_DEPRECATED));
}

Future<SignInResult> UserInternal::ReauthenticateWithProvider_DEPRECATED(
    FederatedAuthProvider *provider) {
  if (!is_valid() || provider == nullptr) {
    SafeFutureHandle<SignInResult> future_handle =
        future_data_.future_impl.SafeAlloc<SignInResult>(
            kUserFn_ReauthenticateWithProvider_DEPRECATED);
    Future<SignInResult> future =
        MakeFuture(&future_data_.future_impl, future_handle);
    if (!is_valid()) {
      CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage,
                     future_handle, &future_data_, SignInResult());
    } else {
      CompleteFuture(kAuthErrorInvalidParameter,
                     kUserNotInitializedErrorMessage, future_handle,
                     &future_data_, SignInResult());
    }
    return future;
  }
  return provider->Reauthenticate(auth_data_, this);
}

Future<void> UserInternal::Delete() {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_Delete, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage,
                                   &future_data_);
  }

  SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kUserFn_Reload);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  JNIEnv *env = Env(auth_data_);
  jobject pending_result =
      env->CallObjectMethod(user_, user::GetMethodId(user::kDelete));

  if (!CheckAndCompleteFutureOnError(env, &future_data_.future_impl,
                                     future_handle)) {
    RegisterCallback(auth_data_, pending_result, future_handle, future_api_id_,
                     &future_data_.future_impl,
                     [](jobject result, FutureCallbackData<void> *d,
                        bool success, void *void_data) {
                       if (success) {
                         UpdateCurrentUser(Env(d->auth_data), d->auth_data);
                       }
                     });
    env->DeleteLocalRef(pending_result);
  }
  return future;
}

Future<void> UserInternal::DeleteLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_Delete));
}

UserMetadata UserInternal::metadata() const {
  UserMetadata user_metadata;
  if (!is_valid()) return user_metadata;

  JNIEnv *env = Env(auth_data_);
  jobject jUserMetadata =
      env->CallObjectMethod(user_, user::GetMethodId(user::kGetMetadata));
  util::CheckAndClearJniExceptions(env);

  if (!jUserMetadata) return user_metadata;

  user_metadata.last_sign_in_timestamp = env->CallLongMethod(
      jUserMetadata, metadata::GetMethodId(metadata::kGetLastSignInTimestamp));
  assert(env->ExceptionCheck() == false);
  user_metadata.creation_timestamp = env->CallLongMethod(
      jUserMetadata, metadata::GetMethodId(metadata::kGetCreationTimestamp));
  assert(env->ExceptionCheck() == false);

  env->DeleteLocalRef(jUserMetadata);

  return user_metadata;
}

bool UserInternal::is_email_verified() const {
  if (!is_valid()) return false;

  JNIEnv *env = Env(auth_data_);
  bool result = env->CallBooleanMethod(
      user_, userinfo::GetMethodId(userinfo::kIsEmailVerified));
  util::CheckAndClearJniExceptions(env);
  return result;
}

bool UserInternal::is_anonymous() const {
  if (!is_valid()) return false;

  JNIEnv *env = Env(auth_data_);
  bool result =
      env->CallBooleanMethod(user_, user::GetMethodId(user::kIsAnonymous));
  util::CheckAndClearJniExceptions(env);
  return result;
}

std::string UserInternal::uid() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property =
      env->CallObjectMethod(user_, userinfo::GetMethodId(userinfo::kGetUid));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniStringToString(env, property);
}

std::string UserInternal::email() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property =
      env->CallObjectMethod(user_, userinfo::GetMethodId(userinfo::kGetEmail));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniStringToString(env, property);
}

std::string UserInternal::display_name() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property = env->CallObjectMethod(
      user_, userinfo::GetMethodId(userinfo::kGetDisplayName));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniStringToString(env, property);
}

std::string UserInternal::photo_url() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property = env->CallObjectMethod(
      user_, userinfo::GetMethodId(userinfo::kGetPhotoUrl));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniUriToString(env, property);
}

std::string UserInternal::provider_id() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property = env->CallObjectMethod(
      user_, userinfo::GetMethodId(userinfo::kGetProviderId));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniStringToString(env, property);
}

std::string UserInternal::phone_number() const {
  if (!is_valid()) return "";
  JNIEnv *env = Env(auth_data_);
  jobject property = env->CallObjectMethod(
      user_, userinfo::GetMethodId(userinfo::kGetPhoneNumber));
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  return JniStringToString(env, property);
}

}  // namespace auth
}  // namespace firebase
