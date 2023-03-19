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

#include "app/src/time.h"
#include "auth/src/ios/common_ios.h"

#if FIREBASE_PLATFORM_IOS
#import "FIRPhoneAuthCredential.h"
#import "FIRUser.h"
#endif

namespace firebase {
namespace auth {

using util::StringFromNSString;
using util::StringFromNSUrl;

const char kInvalidCredentialMessage[] = "Credential is not a phone credential.";

///
/// User Class
/// Platform specific implementation
///
User::User(AuthData *auth_data, UserInternal *user_internal) {
  assert(auth_data);
  if (user_internal == nullptr) {
    // Create an invalid user internal.
    // This will return is_valid() false, and operations will fail.
    user_internal_ = new UserInternal(nullptr);
  } else {
    user_internal_ = user_internal;
  }
  auth_data_ = auth_data;
}

User::~User() {
  delete user_internal_;
  user_internal_ = nullptr;
  auth_data_ = nullptr;
}

User &User::operator=(const User &user) {
  assert(user_internal_);
  delete user_internal_;
  if (user.user_internal_ != nullptr) {
    user_internal_ = new UserInternal(user.user_internal_->user_);
  } else {
    user_internal_ = new UserInternal(nullptr);
  }
  auth_data_ = user.auth_data_;
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

Future<SignInResult> User::ReauthenticateAndRetrieveData_DEPRECATED(const Credential &credential) {
  assert(user_internal_);
  return user_internal_->ReauthenticateAndRetrieveData_DEPRECATED(auth_data_, credential);
}

Future<SignInResult> User::ReauthenticateAndRetrieveDataLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->ReauthenticateAndRetrieveDataLastResult_DEPRECATED();
}

Future<SignInResult> User::ReauthenticateWithProvider_DEPRECATED(
    FederatedAuthProvider *provider) const {
  assert(user_internal_);
  return user_internal_->ReauthenticateWithProvider_DEPRECATED(auth_data_, provider);
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

Future<User *> User::LinkWithCredential_DEPRECATED(const Credential &credential) {
  assert(user_internal_);
  return user_internal_->LinkWithCredential_DEPRECATED(auth_data_, credential);
}

Future<User *> User::LinkWithCredentialLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->LinkWithCredentialLastResult_DEPRECATED();
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential_DEPRECATED(
    const Credential &credential) {
  assert(user_internal_);
  return user_internal_->LinkAndRetrieveDataWithCredential_DEPRECATED(auth_data_, credential);
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED();
}

Future<SignInResult> User::LinkWithProvider_DEPRECATED(FederatedAuthProvider *provider) const {
  assert(user_internal_);
  return user_internal_->LinkWithProvider_DEPRECATED(auth_data_, provider);
}

Future<User *> User::Unlink_DEPRECATED(const char *provider) {
  assert(user_internal_);
  return user_internal_->Unlink_DEPRECATED(auth_data_, provider);
}

Future<User *> User::UnlinkLastResult_DEPRECATED() const {
  assert(user_internal_);
  return user_internal_->UnlinkLastResult_DEPRECATED();
}

Future<User *> User::UpdatePhoneNumberCredential_DEPRECATED(const Credential &credential) {
  assert(user_internal_);
  return user_internal_->UpdatePhoneNumberCredential_DEPRECATED(auth_data_, credential);
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
  return user_internal_->Delete(auth_data_);
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
/// IOSWrapperUserInfo Class
///
class IOSWrappedUserInfo : public UserInfoInterface {
 public:
  explicit IOSWrappedUserInfo(id<FIRUserInfo> user_info) : user_info_(user_info) {}

  virtual ~IOSWrappedUserInfo() { user_info_ = nil; }

  std::string uid() const override { return StringFromNSString(user_info_.uid); }

  std::string email() const override { return StringFromNSString(user_info_.email); }

  std::string display_name() const override { return StringFromNSString(user_info_.displayName); }

  std::string phone_number() const override { return StringFromNSString(user_info_.phoneNumber); }

  std::string photo_url() const override { return StringFromNSUrl(user_info_.photoURL); }

  std::string provider_id() const override { return StringFromNSString(user_info_.providerID); }

 private:
  id<FIRUserInfo> user_info_;
};

///
/// UserInternal Class
///
UserInternal::UserInternal(FIRUser *user) : user_(user), future_data_(kUserFnCount) {}

UserInternal::UserInternal(const UserInternal &user_internal)
    : user_(user_internal.user_), future_data_(kUserFnCount) {}

UserInternal::~UserInternal() {
  user_ = nil;
  {
    MutexLock user_info_lock(user_info_mutex_deprecated_);
    clear_user_infos();
  }
  // Make sure we don't have any pending futures in flight before we disappear.
  while (!future_data_.future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }
}

void UserInternal::set_native_user_object_deprecated(FIRUser *user) {
  MutexLock user_info_lock(user_mutex_);
  user_ = user;
}

bool UserInternal::is_valid() const { return user_ != nil; }

void UserInternal::clear_user_infos() {
  for (size_t i = 0; i < user_infos_.size(); ++i) {
    delete user_infos_[i];
    user_infos_[i] = nullptr;
  }
  user_infos_.clear();
}

Future<std::string> UserInternal::GetToken(bool force_refresh) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_GetToken, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_, "");
  }

  FutureCallbackData<std::string> *callback_data = new FutureCallbackData<std::string>{
      &future_data_, future_data_.future_impl.SafeAlloc<std::string>(kUserFn_GetToken)};
  Future<std::string> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ getIDTokenForcingRefresh:force_refresh
                       completion:^(NSString *_Nullable token, NSError *_Nullable error) {
                         callback_data->future_data->future_impl.Complete(
                             callback_data->future_handle, AuthErrorFromNSError(error),
                             [error.localizedDescription UTF8String], [token](std::string *data) {
                               data->assign(token == nullptr ? "" : [token UTF8String]);
                             });
                         delete callback_data;
                       }];
  return future;
}

std::vector<UserInfoInterface> UserInternal::provider_data() const {
  std::vector<UserInfoInterface> local_user_infos;
  if (is_valid()) {
    NSArray<id<FIRUserInfo>> *provider_data = user_.providerData;
    local_user_infos.resize(provider_data.count);
    for (size_t i = 0; i < provider_data.count; ++i) {
      local_user_infos[i] = IOSWrappedUserInfo(provider_data[i]);
    }
  }
  return local_user_infos;
}

const std::vector<UserInfoInterface *> &UserInternal::provider_data_DEPRECATED() {
  MutexLock user_info_lock(user_info_mutex_deprecated_);
  clear_user_infos();
  if (is_valid()) {
    NSArray<id<FIRUserInfo>> *provider_data = user_.providerData;
    user_infos_.resize(provider_data.count);
    for (size_t i = 0; i < provider_data.count; ++i) {
      user_infos_[i] = new IOSWrappedUserInfo(provider_data[i]);
    }
  }

  // Return a reference to our internally-backed values.
  return user_infos_;
}

Future<void> UserInternal::UpdateEmail(const char *email) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_UpdateEmail, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdateEmail)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ updateEmail:@(email)
          completion:^(NSError *_Nullable error) {
            callback_data->future_data->future_impl.Complete(
                callback_data->future_handle, AuthErrorFromNSError(error),
                [error.localizedDescription UTF8String]);
            delete callback_data;
          }];
  return future;
}

Future<void> UserInternal::UpdateEmailLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdateEmail));
}

Future<void> UserInternal::UpdatePassword(const char *password) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_UpdatePassword, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdatePassword)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ updatePassword:@(password)
             completion:^(NSError *_Nullable error) {
               callback_data->future_data->future_impl.Complete(
                   callback_data->future_handle, AuthErrorFromNSError(error),
                   [error.localizedDescription UTF8String]);
               delete callback_data;
             }];
  return future;
}

Future<void> UserInternal::UpdatePasswordLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdatePassword));
}

Future<void> UserInternal::UpdateUserProfile(const User::UserProfile &profile) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_UpdateUserProfile, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_UpdateUserProfile)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  // Create and populate the change request.
  FIRUserProfileChangeRequest *request = [user_ profileChangeRequest];
  if (profile.display_name != nullptr) {
    request.displayName = @(profile.display_name);
  }
  if (profile.photo_url != nullptr) {
    NSString *photo_url_string = @(profile.photo_url);
    request.photoURL =
        [NSURL URLWithString:[photo_url_string
                                 stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
  }

  // Execute the change request.
  [request commitChangesWithCompletion:^(NSError *_Nullable error) {
    callback_data->future_data->future_impl.Complete(callback_data->future_handle,
                                                     AuthErrorFromNSError(error),
                                                     [error.localizedDescription UTF8String]);
    delete callback_data;
  }];

  return future;
}

Future<void> UserInternal::UpdateUserProfileLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_UpdateUserProfile));
}

Future<void> UserInternal::SendEmailVerification() {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_SendEmailVerification, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_SendEmailVerification)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ sendEmailVerificationWithCompletion:^(NSError *_Nullable error) {
    callback_data->future_data->future_impl.Complete(callback_data->future_handle,
                                                     AuthErrorFromNSError(error),
                                                     [error.localizedDescription UTF8String]);
    delete callback_data;
  }];

  return future;
}

Future<void> UserInternal::SendEmailVerificationLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_SendEmailVerification));
}

Future<User *> UserInternal::LinkWithCredential_DEPRECATED(AuthData *auth_data,
                                                           const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    SafeFutureHandle<User *> future_handle =
        future_data_.future_impl.SafeAlloc<User *>(kUserFn_LinkWithCredential_DEPRECATED);
    Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage, future_handle,
                   &future_data_, (User *)nullptr);
    return future;
  }

  FutureCallbackData<User *> *callback_data = new FutureCallbackData<User *>{
      &future_data_,
      future_data_.future_impl.SafeAlloc<User *>(kUserFn_LinkWithCredential_DEPRECATED)};
  Future<User *> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ linkWithCredential:CredentialFromImpl(credential.impl_)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   SignInCallback(auth_result.user, error, callback_data->future_handle,
                                  callback_data->future_data->future_impl, auth_data);
                   delete callback_data;
                 }];
  return future;
}

Future<User *> UserInternal::LinkWithCredentialLastResult_DEPRECATED() const {
  return static_cast<const Future<User *> &>(
      future_data_.future_impl.LastResult(kUserFn_LinkWithCredential_DEPRECATED));
}

Future<SignInResult> UserInternal::LinkAndRetrieveDataWithCredential_DEPRECATED(
    AuthData *auth_data, const Credential &credential) {
  // MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    printf("DEDB LinkAndRetrieveDataWithCredential_DEPRECATED user is not valid\n");
    SafeFutureHandle<SignInResult> future_handle = future_data_.future_impl.SafeAlloc<SignInResult>(
        kUserFn_LinkAndRetrieveDataWithCredential_DEPRECATED);
    Future<SignInResult> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage, future_handle,
                   &future_data_, SignInResult());
    return future;
  }

  FutureCallbackData<SignInResult> *callback_data = new FutureCallbackData<SignInResult>{
      &future_data_, future_data_.future_impl.SafeAlloc<SignInResult>(
                         kUserFn_LinkAndRetrieveDataWithCredential_DEPRECATED)};
  Future<SignInResult> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ linkWithCredential:CredentialFromImpl(credential.impl_)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   NSLog(@"DEDB Completion auth_result: %p error: %p\n", auth_result, error);
                   SignInResultCallback(auth_result, error, callback_data->future_handle,
                                        callback_data->future_data->future_impl, auth_data);
                   NSLog(@"DEDB Completion SignInResultCallback returned\n");
                   // delete callback_data;
                 }];
  return future;
}

Future<SignInResult> UserInternal::LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED() const {
  return static_cast<const Future<SignInResult> &>(
      future_data_.future_impl.LastResult(kUserFn_LinkAndRetrieveDataWithCredential_DEPRECATED));
}

Future<SignInResult> UserInternal::LinkWithProvider_DEPRECATED(AuthData *auth_data,
                                                               FederatedAuthProvider *provider) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Link(auth_data, this);
}

Future<User *> UserInternal::Unlink_DEPRECATED(AuthData *auth_data, const char *provider) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    SafeFutureHandle<User *> future_handle =
        future_data_.future_impl.SafeAlloc<User *>(kUserFn_Unlink_DEPRECATED);
    Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage, future_handle,
                   &future_data_, (User *)nullptr);
    return future;
  }

  FutureCallbackData<User *> *callback_data = new FutureCallbackData<User *>{
      &future_data_, future_data_.future_impl.SafeAlloc<User *>(kUserFn_Unlink_DEPRECATED)};
  Future<User *> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ unlinkFromProvider:@(provider)
                 completion:^(FIRUser *_Nullable user, NSError *_Nullable error) {
                   SignInCallback(user, error, callback_data->future_handle,
                                  callback_data->future_data->future_impl, auth_data);
                   delete callback_data;
                 }];
  return future;
}

Future<User *> UserInternal::UnlinkLastResult_DEPRECATED() const {
  return static_cast<const Future<User *> &>(
      future_data_.future_impl.LastResult(kUserFn_Unlink_DEPRECATED));
}

Future<User *> UserInternal::UpdatePhoneNumberCredential_DEPRECATED(AuthData *auth_data,
                                                                    const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
#if FIREBASE_PLATFORM_IOS
  if (!is_valid()) {
    SafeFutureHandle<User *> future_handle =
        future_data_.future_impl.SafeAlloc<User *>(kUserFn_UpdatePhoneNumberCredential_DEPRECATED);
    Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage, future_handle,
                   &future_data_, (User *)nullptr);
    return future;
  }

  FutureCallbackData<User *> *callback_data = new FutureCallbackData<User *>{
      &future_data_,
      future_data_.future_impl.SafeAlloc<User *>(kUserFn_UpdatePhoneNumberCredential_DEPRECATED)};
  Future<User *> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  FIRAuthCredential *objc_credential = CredentialFromImpl(credential.impl_);
  if ([objc_credential isKindOfClass:[FIRPhoneAuthCredential class]]) {
    [user_ updatePhoneNumberCredential:(FIRPhoneAuthCredential *)objc_credential
                            completion:^(NSError *_Nullable error) {
                              SignInCallback(user_, error, callback_data->future_handle,
                                             callback_data->future_data->future_impl, auth_data);
                              delete callback_data;
                            }];
  } else {
    CompleteFuture(kAuthErrorInvalidCredential, kInvalidCredentialMessage,
                   callback_data->future_handle, &future_data_, (User *)nullptr);
  }
  return future;

#else   // non iOS Apple platforms (eg: tvOS).
  SafeFutureHandle<User *> future_handle =
      future_data_.future_impl.SafeAlloc<User *>(kUserFn_UpdatePhoneNumberCredential_DEPRECATED);
  Future<User *> future = MakeFuture(&future_data_.future_impl, future_handle);
  CompleteFuture(kAuthErrorApiNotAvailable, kPhoneAuthNotSupportedErrorMessage, future_handle,
                 &future_data_, (User *)nullptr);
  return future;
#endif  // FIREBASE_PLATFORM_IOS
}

Future<void> UserInternal::Reload() {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_Reload, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_Reload)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ reloadWithCompletion:^(NSError *_Nullable error) {
    callback_data->future_data->future_impl.Complete(callback_data->future_handle,
                                                     AuthErrorFromNSError(error),
                                                     [error.localizedDescription UTF8String]);
    delete callback_data;
  }];
  return future;
}

Future<void> UserInternal::ReloadLastResult() const {
  return static_cast<const Future<void> &>(future_data_.future_impl.LastResult(kUserFn_Reload));
}

Future<void> UserInternal::Reauthenticate(const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_Reauthenticate, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_Reauthenticate)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                           completion:^(FIRAuthDataResult *_Nullable auth_result,
                                        NSError *_Nullable error) {
                             callback_data->future_data->future_impl.Complete(
                                 callback_data->future_handle, AuthErrorFromNSError(error),
                                 [error.localizedDescription UTF8String]);
                             delete callback_data;
                           }];
  return future;
}

Future<void> UserInternal::ReauthenticateLastResult() const {
  return static_cast<const Future<void> &>(
      future_data_.future_impl.LastResult(kUserFn_Reauthenticate));
}

Future<SignInResult> UserInternal::ReauthenticateAndRetrieveData_DEPRECATED(
    AuthData *auth_data, const Credential &credential) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    SafeFutureHandle<SignInResult> future_handle = future_data_.future_impl.SafeAlloc<SignInResult>(
        kUserFn_ReauthenticateAndRetrieveData_DEPRECATED);
    Future<SignInResult> future = MakeFuture(&future_data_.future_impl, future_handle);
    CompleteFuture(kAuthErrorInvalidUser, kUserNotInitializedErrorMessage, future_handle,
                   &future_data_, SignInResult());
    return future;
  }

  FutureCallbackData<SignInResult> *callback_data = new FutureCallbackData<SignInResult>{
      &future_data_, future_data_.future_impl.SafeAlloc<SignInResult>(
                         kUserFn_ReauthenticateAndRetrieveData_DEPRECATED)};
  Future<SignInResult> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          SignInResultCallback(auth_result, error, callback_data->future_handle,
                                               callback_data->future_data->future_impl, auth_data);
                          delete callback_data;
                        }];
  return future;
}

Future<SignInResult> UserInternal::ReauthenticateAndRetrieveDataLastResult_DEPRECATED() const {
  return static_cast<const Future<SignInResult> &>(
      future_data_.future_impl.LastResult(kUserFn_ReauthenticateAndRetrieveData_DEPRECATED));
}

Future<SignInResult> UserInternal::ReauthenticateWithProvider_DEPRECATED(
    AuthData *auth_data, FederatedAuthProvider *provider) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Reauthenticate(auth_data, this);
}

Future<void> UserInternal::Delete(AuthData *auth_data) {
  MutexLock user_info_lock(user_mutex_);
  if (!is_valid()) {
    return CreateAndCompleteFuture(kUserFn_Delete, kAuthErrorInvalidUser,
                                   kUserNotInitializedErrorMessage, &future_data_);
  }

  FutureCallbackData<void> *callback_data = new FutureCallbackData<void>{
      &future_data_, future_data_.future_impl.SafeAlloc<void>(kUserFn_Delete)};
  Future<void> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  [user_ deleteWithCompletion:^(NSError *_Nullable error) {
    if (!error) {
      callback_data->future_data->future_impl.Complete(callback_data->future_handle, kAuthErrorNone,
                                                       "");
    } else {
      callback_data->future_data->future_impl.Complete(callback_data->future_handle,
                                                       AuthErrorFromNSError(error),
                                                       [error.localizedDescription UTF8String]);
    }
    delete callback_data;
  }];
  return future;
}

Future<void> UserInternal::DeleteLastResult() const {
  return static_cast<const Future<void> &>(future_data_.future_impl.LastResult(kUserFn_Delete));
}

UserMetadata UserInternal::metadata() const {
  if (!is_valid()) return UserMetadata();

  FIRUserMetadata *meta = user_.metadata;
  if (!meta) return UserMetadata();

  UserMetadata data;
  // * 1000 because timeIntervalSince1970 is in seconds, and we need ms.
  // TODO(b/69176745): Use a util for this so we don't forget about this ms conversion elsewhere.
  data.last_sign_in_timestamp =
      static_cast<uint64_t>([meta.lastSignInDate timeIntervalSince1970] * 1000.0);
  data.creation_timestamp =
      static_cast<uint64_t>([meta.creationDate timeIntervalSince1970] * 1000.0);
  return data;
}

bool UserInternal::is_email_verified() const { return is_valid() ? user_.emailVerified : false; }

bool UserInternal::is_anonymous() const { return is_valid() ? user_.anonymous : false; }

std::string UserInternal::uid() const { return is_valid() ? StringFromNSString(user_.uid) : ""; }

std::string UserInternal::email() const {
  return is_valid() ? StringFromNSString(user_.email) : "";
}

std::string UserInternal::display_name() const {
  return is_valid() ? StringFromNSString(user_.displayName) : "";
}

std::string UserInternal::phone_number() const {
  return is_valid() ? StringFromNSString(user_.phoneNumber) : "";
}

std::string UserInternal::photo_url() const {
  return is_valid() ? StringFromNSUrl(user_.photoURL) : "";
}

std::string UserInternal::provider_id() const {
  return is_valid() ? StringFromNSString(user_.providerID) : "";
}

}  // namespace auth
}  // namespace firebase
