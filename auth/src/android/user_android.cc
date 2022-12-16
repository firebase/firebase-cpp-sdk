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

#include "auth/src/android/user_android.h"

#include <assert.h>

#include "app/src/time.h"
#include "app/src/util.h"
#include "auth/src/android/auth_android.h"
#include "auth/src/android/common_android.h"
#include "auth/src/credential_internal.h"
#include "firebase/auth.h"
#include "firebase/auth/user.h"

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

UserInternal::UserInternal()
    : auth_internal_(nullptr), platform_user_(nullptr) {}

UserInternal::UserInternal(AuthData* auth_internal, jobject platform_user)
    : auth_internal_(nullptr), platform_user_(nullptr) {
  SetPlatformUser(auth_internal, platform_user);
}

UserInternal::UserInternal(UserInternal& user)
    : auth_internal_(nullptr), platform_user_(nullptr) {
  this->operator=(user);
}

UserInternal::~UserInternal() { Invalidate(); }

UserInternal& UserInternal::operator=(const UserInternal& user) {
  Invalidate();
  SetPlatformUser(user.auth_internal_, user.platform_user_);
  return *this;
}

void UserInternal::SetPlatformUser(AuthData* auth_internal,
                                   jobject platform_user) {
  Invalidate();
  if (auth_internal && platform_user) {
    auth_internal_ = auth_internal;
    auth_internal_->cleanup.RegisterObject(
        this, [](void* obj) { static_cast<UserInternal*>(obj)->Invalidate(); });
    JNIEnv* env = GetJNIEnv();
    platform_user_ = util::LocalToGlobalReference(env, platform_user);
  }
}

std::vector<UniquePtr<UserInfoInterface>> UserInternal::provider_data() const {
  std::vector<UniquePtr<UserInfoInterface>> user_infos;
  if (!is_valid()) return user_infos;

  JNIEnv* env = GetJNIEnv();
  // getProviderData returns `List<? extends UserInfo>`
  jobject providers_list = env->CallObjectMethod(
      platform_user_, user::GetMethodId(user::kProviderData));
  if (util::CheckAndClearJniExceptions(env) || providers_list == nullptr) {
    return user_infos;
  }

  // Copy the list into user_infos.
  int num_providers = env->CallIntMethod(
      providers_list, util::list::GetMethodId(util::list::kSize));
  if (util::CheckAndClearJniExceptions(env)) return user_infos;

  user_infos.resize(num_providers);
  for (int i = 0; i < num_providers; ++i) {
    jobject user_info = env->CallObjectMethod(
        providers_list, util::list::GetMethodId(util::list::kGet), i);
    if (firebase::util::CheckAndClearJniExceptions(env)) {
      user_infos.clear();
      break;
    }
    // user_info is converted to a global reference in UserInternal and the
    // local reference is released.
    user_infos[i].reset(new UserInternal(auth_internal_, user_info));
  }
  env->DeleteLocalRef(providers_list);
  return user_infos;
}

UserMetadata UserInternal::metadata() const {
  if (!is_valid()) return UserMetadata();

  JNIEnv* env = GetJNIEnv();
  jobject user_metadata = env->CallObjectMethod(
      platform_user_, user::GetMethodId(user::kGetMetadata));
  if (util::CheckAndClearJniExceptions(env) || !user_metadata) {
    return UserMetadata();
  }

  UserMetadata data;
  data.last_sign_in_timestamp = env->CallLongMethod(
      user_metadata, metadata::GetMethodId(metadata::kGetLastSignInTimestamp));
  util::CheckAndClearJniExceptions(env);
  data.creation_timestamp = env->CallLongMethod(
      user_metadata, metadata::GetMethodId(metadata::kGetCreationTimestamp));
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(user_metadata);
  return data;
}

bool UserInternal::is_email_verified() const {
  if (!is_valid()) return false;

  JNIEnv* env = GetJNIEnv();
  bool result = env->CallBooleanMethod(
      platform_user_, userinfo::GetMethodId(userinfo::kIsEmailVerified));
  util::CheckAndClearJniExceptions(env);
  return result;
}

bool UserInternal::is_anonymous() const {
  if (!is_valid()) return false;

  JNIEnv* env = GetJNIEnv();
  bool result = env->CallBooleanMethod(platform_user_,
                                       user::GetMethodId(user::kIsAnonymous));
  util::CheckAndClearJniExceptions(env);
  return result;
}

Future<void> UserInternal::Delete() {
  FIREBASE_ASSERT(is_valid());

  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_Delete);
  JNIEnv* env = GetJNIEnv();
  jobject task =
      env->CallObjectMethod(platform_user_, user::GetMethodId(user::kDelete));

  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(env, task, handle, futures, future_api_id(),
                     auth_internal_,
                     [](jobject /*task_result*/,
                        FutureCallbackData<void, AuthData>* callback_data,
                        bool success, void* /*result_data*/) {
                       if (success) callback_data->context->UpdateCurrentUser();
                     });
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

Future<std::string> UserInternal::GetToken(bool force_refresh) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<std::string>(kUserFn_GetToken);
  JNIEnv* env = GetJNIEnv();

  SetExpectIdTokenListenerCallback(force_refresh);
  jobject task = env->CallObjectMethod(
      platform_user_, user::GetMethodId(user::kToken), force_refresh);

  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(
        env, task, handle, futures, future_api_id(), this,
        [](jobject task_result,
           FutureCallbackData<std::string, UserInternal>* callback_data,
           bool success, std::string* result_data) {
          if (!success || task_result == nullptr) {
            *result_data = std::string();
            return;
          }
          UserInternal* user_internal = callback_data->context;
          if (user_internal->ShouldTriggerIdTokenListenerCallback()) {
            NotifyIdTokenListeners(user_internal->auth_internal());
          }
          JNIEnv* env = user_internal->GetJNIEnv();
          jstring token = static_cast<jstring>(env->CallObjectMethod(
              task_result, tokenresult::GetMethodId(tokenresult::kGetToken)));
          util::CheckAndClearJniExceptions(env);
          *result_data = JniStringToString(env, token);
        });
  } else {
    // If the method failed for some reason, clear the expected callback.
    SetExpectIdTokenListenerCallback(false);
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

void UserInternal::SetExpectIdTokenListenerCallback(bool expect) {
  MutexLock lock(expect_id_token_mutex_);
  expect_id_token_listener_callback_ = expect;
}

bool UserInternal::ShouldTriggerIdTokenListenerCallback() {
  MutexLock lock(expect_id_token_mutex_);
  bool result = expect_id_token_listener_callback_;
  expect_id_token_listener_callback_ = false;
  return result;
}

Future<void> UserInternal::UpdateEmail(const char* email) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_UpdateEmail);
  JNIEnv* env = GetJNIEnv();

  jstring email_string = env->NewStringUTF(email);
  jobject task = env->CallObjectMethod(
      platform_user_, user::GetMethodId(user::kUpdateEmail), email_string);
  env->DeleteLocalRef(email_string);

  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(env, task, handle, futures, future_api_id());
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

Future<void> UserInternal::UpdatePassword(const char* password) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_UpdatePassword);
  JNIEnv* env = GetJNIEnv();

  jstring password_string = env->NewStringUTF(password);
  jobject task = env->CallObjectMethod(platform_user_,
                                       user::GetMethodId(user::kUpdatePassword),
                                       password_string);
  env->DeleteLocalRef(password_string);

  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(env, task, handle, futures, future_api_id());
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

Future<void> UserInternal::UpdateUserProfile(const User::UserProfile& profile) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_UpdateUserProfile);
  JNIEnv* env = GetJNIEnv();

  jobject user_profile_builder = env->NewObject(
      userprofilebuilder::GetClass(),
      userprofilebuilder::GetMethodId(userprofilebuilder::kConstructor));

  if (CheckAndCompleteFutureOnError(env, futures, handle)) {
    return MakeFuture(futures, handle);
  }

  // UserProfileChangeRequest.Builder.setDisplayName().
  if (profile.display_name) {
    jstring display_name = env->NewStringUTF(profile.display_name);
    jobject builder = env->CallObjectMethod(
        user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetDisplayName),
        display_name);
    bool failed = CheckAndCompleteFutureOnError(env, futures, handle);
    if (builder) env->DeleteLocalRef(builder);
    env->DeleteLocalRef(display_name);
    if (failed) return MakeFuture(futures, handle);
  }

  // UserProfileChangeRequest.Builder.setPhotoUri().
  if (profile.photo_url) {
    jobject uri = CharsToJniUri(env, profile.photo_url);
    jobject builder = env->CallObjectMethod(
        user_profile_builder,
        userprofilebuilder::GetMethodId(userprofilebuilder::kSetPhotoUri), uri);
    bool failed = CheckAndCompleteFutureOnError(env, futures, handle);
    if (builder) env->DeleteLocalRef(builder);
    env->DeleteLocalRef(uri);
    if (failed) return MakeFuture(futures, handle);
  }

  jobject user_profile_request = nullptr;
  // UserProfileChangeRequest.Builder.build().
  user_profile_request = env->CallObjectMethod(
      user_profile_builder,
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
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle =
      futures->SafeAlloc<User*>(kUserFn_UpdatePhoneNumberCredential);
  if (!CredentialInternal::CompleteFutureIfInvalid(credential, futures,
                                                   handle)) {
    JNIEnv* env = GetJNIEnv();
    jobject platform_credential =
        CredentialInternal::GetPlatformCredential(credential)->object();
    if (env->IsInstanceOf(platform_credential, phonecredential::GetClass())) {
      jobject task = env->CallObjectMethod(
          platform_user_, user::GetMethodId(user::kUpdatePhoneNumberCredential),
          platform_credential);

      if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
        RegisterCallback(env, task, handle, futures, future_api_id(),
                         auth_internal_, ReadUserFromSignInResult);
      }
      if (task) env->DeleteLocalRef(task);
    } else {
      futures->Complete(handle, kAuthErrorInvalidCredential,
                        "Credential is not a phone credential.");
    }
  }
  return MakeFuture(futures, handle);
}

Future<void> UserInternal::Reload() {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_Reload);
  JNIEnv* env = GetJNIEnv();
  jobject task =
      env->CallObjectMethod(platform_user_, user::GetMethodId(user::kReload));
  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(env, task, handle, futures, future_api_id());
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

Future<void> UserInternal::Reauthenticate(const Credential& credential) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_Reauthenticate);
  if (!CredentialInternal::CompleteFutureIfInvalid(credential, futures,
                                                   handle)) {
    JNIEnv* env = GetJNIEnv();
    jobject task = env->CallObjectMethod(
        platform_user_, user::GetMethodId(user::kReauthenticate),
        CredentialInternal::GetPlatformCredential(credential)->object());

    if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
      RegisterCallback(env, task, handle, futures, future_api_id());
    }
    if (task) env->DeleteLocalRef(task);
  }
  return MakeFuture(futures, handle);
}

Future<SignInResult> UserInternal::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle =
      futures->SafeAlloc<SignInResult>(kUserFn_ReauthenticateAndRetrieveData);
  if (!CredentialInternal::CompleteFutureIfInvalid(credential, futures,
                                                   handle)) {
    JNIEnv* env = GetJNIEnv();
    jobject task = env->CallObjectMethod(
        platform_user_, user::GetMethodId(user::kReauthenticateAndRetrieveData),
        CredentialInternal::GetPlatformCredential(credential)->object());

    if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
      RegisterCallback(env, task, handle, futures, future_api_id(),
                       auth_internal_, ReadSignInResult);
    }
    if (task) env->DeleteLocalRef(task);
  }
  return MakeFuture(futures, handle);
}

Future<void> UserInternal::SendEmailVerification() {
  FIREBASE_ASSERT(is_valid());
  ReferenceCountedFutureImpl* futures = future_api();
  const auto handle = futures->SafeAlloc<void>(kUserFn_SendEmailVerification);
  JNIEnv* env = GetJNIEnv();
  jobject task = env->CallObjectMethod(
      platform_user_, user::GetMethodId(user::kSendEmailVerification));
  if (!CheckAndCompleteFutureOnError(env, futures, handle)) {
    RegisterCallback(env, task, handle, futures, future_api_id());
  }
  if (task) env->DeleteLocalRef(task);
  return MakeFuture(futures, handle);
}

void UserInternal::Invalidate() {
  if (is_valid()) {
    // TODO: Cancel all JNI callbacks.
    JNIEnv* env = GetJNIEnv();
    env->DeleteGlobalRef(platform_user_);
    platform_user_ = nullptr;
    auth_internal_->cleanup.UnregisterObject(this);
    auth_internal_ = nullptr;
  }
}

std::string UserInternal::GetUserInfoProperty(JNIEnv* env,
                                              jobject user_interface,
                                              userinfo::Method method_id,
                                              PropertyType type) {
  jobject property = user_interface
                         ? env->CallObjectMethod(
                               user_interface, userinfo::GetMethodId(method_id))
                         : nullptr;
  if (firebase::util::CheckAndClearJniExceptions(env) || !property) {
    return std::string();
  }
  switch (type) {
    case kPropertyTypeUri:
      return JniUriToString(env, property);
    case kPropertyTypeString:
      return JniStringToString(env, property);
  }
}

bool UserInternal::CacheUserMethodIds(JNIEnv* env, jobject activity) {
  return phonecredential::CacheMethodIds(env, activity) &&
         tokenresult::CacheMethodIds(env, activity) &&
         user::CacheMethodIds(env, activity) &&
         userinfo::CacheMethodIds(env, activity) &&
         metadata::CacheMethodIds(env, activity) &&
         userprofilebuilder::CacheMethodIds(env, activity);
}

void UserInternal::ReleaseUserClasses(JNIEnv* env) {
  phonecredential::ReleaseClass(env);
  tokenresult::ReleaseClass(env);
  user::ReleaseClass(env);
  userinfo::ReleaseClass(env);
  metadata::ReleaseClass(env);
  userprofilebuilder::ReleaseClass(env);
}

// TODO: when all platforms implement the internal interface, move this
// to user.cc.
User::User(AuthData* /*auth_internal*/) { internal_ = new UserInternal(); }

User::User(const User& user) { this->operator=(user); }

User::~User() {
  delete internal_;
  internal_ = nullptr;
}

User& User::operator=(const User& user) {
  if (internal_) {
    delete internal_;
    internal_ = nullptr;
  }
  if (user.internal_) {
    internal_ = new UserInternal(*user.internal_);
  }
  return *this;
}

std::string User::uid() const { return internal_->uid(); }

std::string User::email() const { return internal_->email(); }

std::string User::display_name() const { return internal_->display_name(); }

std::string User::phone_number() const { return internal_->phone_number(); }

std::string User::photo_url() const { return internal_->photo_url(); }

std::string User::provider_id() const { return internal_->provider_id(); }

Future<std::string> User::GetToken(bool force_refresh) {
  FIREBASE_ASSERT_RETURN(Future<std::string>(), is_valid());
  return internal_->GetToken(force_refresh);
}

const std::vector<UserInfoInterface*>& User::provider_data() const {
  static std::vector<UserInfoInterface*> empty_user_infos;
  FIREBASE_ASSERT_RETURN(empty_user_infos, is_valid());
  AuthData* auth_internal = internal_->auth_internal();
  ClearUserInfos(auth_internal);
  auto user_infos = &auth_internal->user_infos;
  auto internal_user_infos = internal_->provider_data();
  user_infos->resize(internal_user_infos.size());
  for (int i = 0; i < internal_user_infos.size(); ++i) {
    (*user_infos)[i] = internal_user_infos[i].release();
  }
  // Return a reference to our internally-backed values.
  return *user_infos;
}

Future<void> User::UpdateEmail(const char* email) {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->UpdateEmail(email);
}

Future<void> User::UpdatePassword(const char* password) {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->UpdatePassword(password);
}

Future<void> User::UpdateUserProfile(const UserProfile& profile) {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->UpdateUserProfile(profile);
}

Future<User*> User::LinkWithCredential(const Credential& credential) {
  FIREBASE_ASSERT_RETURN(Future<User*>(), is_valid());
  return internal_->LinkWithCredential(credential);
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential(
    const Credential& credential) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), is_valid());
  return internal_->LinkAndRetrieveDataWithCredential(credential);
}

Future<User*> User::Unlink(const char* provider) {
  FIREBASE_ASSERT_RETURN(Future<User*>(), is_valid());
  return internal_->Unlink(provider);
}

Future<User*> User::UpdatePhoneNumberCredential(const Credential& credential) {
  FIREBASE_ASSERT_RETURN(Future<User*>(), is_valid());
  return internal_->UpdatePhoneNumberCredential(credential);
}

Future<void> User::Reload() {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->Reload();
}

Future<void> User::Reauthenticate(const Credential& credential) {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->Reauthenticate(credential);
}

Future<SignInResult> User::ReauthenticateAndRetrieveData(
    const Credential& credential) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), is_valid());
  return internal_->ReauthenticateAndRetrieveData(credential);
}

Future<void> User::SendEmailVerification() {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->SendEmailVerification();
}

Future<void> User::Delete() {
  FIREBASE_ASSERT_RETURN(Future<void>(), is_valid());
  return internal_->Delete();
}

UserMetadata User::metadata() const { return internal_->metadata(); }

bool User::is_email_verified() const { return internal_->is_email_verified(); }

bool User::is_anonymous() const { return internal_->is_anonymous(); }

}  // namespace auth
}  // namespace firebase
