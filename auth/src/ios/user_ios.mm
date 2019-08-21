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

#import "FIRPhoneAuthCredential.h"

#include "app/src/time.h"
#include "auth/src/ios/common_ios.h"

namespace firebase {
namespace auth {

using util::StringFromNSString;
using util::StringFromNSUrl;

const char kInvalidCredentialMessage[] = "Credential is not a phone credential.";

class IOSWrappedUserInfo : public UserInfoInterface {
 public:
  explicit IOSWrappedUserInfo(id<FIRUserInfo> user_info) : user_info_(user_info) {}

  virtual ~IOSWrappedUserInfo() {
    user_info_ = nil;
  }

  virtual std::string uid() const {
    return StringFromNSString(user_info_.uid);
  }

  virtual std::string email() const {
    return StringFromNSString(user_info_.email);
  }

  virtual std::string display_name() const {
    return StringFromNSString(user_info_.displayName);
  }

  virtual std::string phone_number() const {
    return StringFromNSString(user_info_.phoneNumber);
  }

  virtual std::string photo_url() const {
    return StringFromNSUrl(user_info_.photoURL);
  }

  virtual std::string provider_id() const {
    return StringFromNSString(user_info_.providerID);
  }

 private:
  /// Pointer to the main class.
  /// Needed for context in implementation of virtuals.
  id<FIRUserInfo> user_info_;
};

User::~User() {
  // Make sure we don't have any pending futures in flight before we disappear.
  while (!auth_data_->future_impl.IsSafeToDelete()) {
    internal::Sleep(100);
  }
}

Future<std::string> User::GetToken(bool force_refresh) {
  if (!ValidUser(auth_data_)) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<std::string>(kUserFn_GetToken);
  [UserImpl(auth_data_)
      getIDTokenForcingRefresh:force_refresh
                    completion:^(NSString *_Nullable token, NSError *_Nullable error) {
                        futures.Complete(
                            handle, AuthErrorFromNSError(error),
                            [error.localizedDescription UTF8String], [token](std::string* data) {
                              data->assign(token == nullptr ? "" : [token UTF8String]);
                            });
                        }];
  return MakeFuture(&futures, handle);
}

const std::vector<UserInfoInterface*>& User::provider_data() const {
  ClearUserInfos(auth_data_);

  if (ValidUser(auth_data_)) {
    NSArray<id<FIRUserInfo>> *provider_data = UserImpl(auth_data_).providerData;

    // Wrap the FIRUserInfos in our IOSWrappedUserInfo class.
    auth_data_->user_infos.resize(provider_data.count);
    for (size_t i = 0; i < provider_data.count; ++i) {
      auth_data_->user_infos[i] = new IOSWrappedUserInfo(provider_data[i]);
    }
  }

  // Return a reference to our internally-backed values.
  return auth_data_->user_infos;
}

Future<void> User::UpdateEmail(const char *email) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdateEmail);
  [UserImpl(auth_data_) updateEmail:@(email)
                         completion:^(NSError *_Nullable error) {
                             futures.Complete(handle, AuthErrorFromNSError(error),
                                              [error.localizedDescription UTF8String]);
                           }];
  return MakeFuture(&futures, handle);
}

Future<void> User::UpdatePassword(const char *password) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdatePassword);
  [UserImpl(auth_data_)
      updatePassword:@(password)
          completion:^(NSError *_Nullable error) {
              futures.Complete(handle, AuthErrorFromNSError(error),
                               [error.localizedDescription UTF8String]);
            }];
  return MakeFuture(&futures, handle);
}

Future<void> User::UpdateUserProfile(const UserProfile& profile) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdateUserProfile);
  // Create and populate the change request.
  FIRUserProfileChangeRequest *request = [UserImpl(auth_data_) profileChangeRequest];
  if (profile.display_name != nullptr) {
    request.displayName = @(profile.display_name);
  }
  if (profile.photo_url != nullptr) {
    NSString *photo_url_string = @(profile.photo_url);
    request.photoURL = [NSURL URLWithString:[photo_url_string
                            stringByAddingPercentEscapesUsingEncoding:NSUTF8StringEncoding]];
  }

  // Execute the change request.
  [request commitChangesWithCompletion:^(NSError *_Nullable error) {
                             futures.Complete(handle, AuthErrorFromNSError(error),
                                              [error.localizedDescription UTF8String]);
                           }];
  return MakeFuture(&futures, handle);
}

Future<void> User::SendEmailVerification() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_SendEmailVerification);
  [UserImpl(auth_data_) sendEmailVerificationWithCompletion:^(NSError *_Nullable error) {
    futures.Complete(handle, AuthErrorFromNSError(error),
                     [error.localizedDescription UTF8String]);
  }];
  return MakeFuture(&futures, handle);
}

Future<User*> User::LinkWithCredential(const Credential &credential) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kUserFn_LinkWithCredential);
  [UserImpl(auth_data_)
      linkWithCredential:CredentialFromImpl(credential.impl_)
              completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                SignInCallback(auth_result.user, error, handle, auth_data_);
              }];
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::LinkAndRetrieveDataWithCredential(const Credential &credential) {
  if (!ValidUser(auth_data_)) {
    return Future<SignInResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = auth_data_->future_impl.SafeAlloc<SignInResult>(
    kUserFn_LinkAndRetrieveDataWithCredential, SignInResult());
  [UserImpl(auth_data_)
      linkWithCredential:CredentialFromImpl(credential.impl_)
              completion:^(FIRAuthDataResult *_Nullable auth_result,
                           NSError *_Nullable error) {
      SignInResultCallback(auth_result, error, handle, auth_data_);
    }];
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::LinkWithProvider(FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Link(auth_data_);
}

Future<User*> User::Unlink(const char *provider) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kUserFn_Unlink);
  [UserImpl(auth_data_)
      unlinkFromProvider:@(provider)
              completion:^(FIRUser *_Nullable user, NSError *_Nullable error) {
                  SignInCallback(user, error, handle, auth_data_);
                }];
  return MakeFuture(&futures, handle);
}

Future<User*> User::UpdatePhoneNumberCredential(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<User*>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User*>(kUserFn_UpdatePhoneNumberCredential);
  FIRAuthCredential *objc_credential = CredentialFromImpl(credential.impl_);
  if ([objc_credential isKindOfClass:[FIRPhoneAuthCredential class]]) {
    [UserImpl(auth_data_)
         updatePhoneNumberCredential:(FIRPhoneAuthCredential*)objc_credential
                          completion:^(NSError *_Nullable error) {
                              futures.Complete(handle, AuthErrorFromNSError(error),
                                               [error.localizedDescription UTF8String]);
                            }];
  } else {
    futures.Complete(handle, kAuthErrorInvalidCredential, kInvalidCredentialMessage);
  }
  return MakeFuture(&futures, handle);
}

Future<void> User::Reload() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reload);

  [UserImpl(auth_data_) reloadWithCompletion:^(NSError *_Nullable error) {
    futures.Complete(handle, AuthErrorFromNSError(error),
                     [error.localizedDescription UTF8String]);
  }];
  return MakeFuture(&futures, handle);
}

Future<void> User::Reauthenticate(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reauthenticate);

  [UserImpl(auth_data_)
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          futures.Complete(handle, AuthErrorFromNSError(error),
                                           [error.localizedDescription UTF8String]);
                        }];
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::ReauthenticateAndRetrieveData(const Credential& credential) {
  if (!ValidUser(auth_data_)) {
    return Future<SignInResult>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = auth_data_->future_impl.SafeAlloc<SignInResult>(
      kUserFn_ReauthenticateAndRetrieveData, SignInResult());

  [UserImpl(auth_data_)
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
      SignInResultCallback(auth_result, error, handle, auth_data_);
    }];
  return MakeFuture(&futures, handle);
}

Future<SignInResult> User::ReauthenticateWithProvider(FederatedAuthProvider* provider) const {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->Reauthenticate(auth_data_);
}

Future<void> User::Delete() {
  if (!ValidUser(auth_data_)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl& futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Delete);

  [UserImpl(auth_data_) deleteWithCompletion:^(NSError *_Nullable error) {
    if (!error) {
      UpdateCurrentUser(auth_data_);
      futures.Complete(handle, kAuthErrorNone, "");
    } else {
      futures.Complete(handle, AuthErrorFromNSError(error),
                       [error.localizedDescription UTF8String]);
    }
  }];
  return MakeFuture(&futures, handle);
}

UserMetadata User::metadata() const {
  if (!ValidUser(auth_data_)) return UserMetadata();

  FIRUserMetadata *meta = UserImpl(auth_data_).metadata;
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

bool User::is_email_verified() const {
  return ValidUser(auth_data_) ? UserImpl(auth_data_).emailVerified : false;
}

bool User::is_anonymous() const {
  return ValidUser(auth_data_) ? UserImpl(auth_data_).anonymous : false;
}

std::string User::uid() const {
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).uid) : "";
}

std::string User::email() const {
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).email) : "";
}

std::string User::display_name() const {
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).displayName) : "";
}

std::string User::phone_number() const {
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).phoneNumber) : "";
}

std::string User::photo_url() const {
  return ValidUser(auth_data_) ? StringFromNSUrl(UserImpl(auth_data_).photoURL) : "";
}

std::string User::provider_id() const {
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).providerID) : "";
}

}  // namespace auth
}  // namespace firebase
