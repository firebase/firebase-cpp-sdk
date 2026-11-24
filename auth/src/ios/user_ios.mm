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

#include <cstdint>
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

  virtual ~IOSWrappedUserInfo() { user_info_ = nil; }

  virtual std::string uid() const { return StringFromNSString(user_info_.uid); }

  virtual std::string email() const { return StringFromNSString(user_info_.email); }

  virtual std::string display_name() const { return StringFromNSString(user_info_.displayName); }

  virtual std::string phone_number() const { return StringFromNSString(user_info_.phoneNumber); }

  virtual std::string photo_url() const { return StringFromNSUrl(user_info_.photoURL); }

  virtual std::string provider_id() const { return StringFromNSString(user_info_.providerID); }

 private:
  /// Pointer to the main class.
  /// Needed for context in implementation of virtuals.
  id<FIRUserInfo> user_info_;
};

bool User::is_valid() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get());
}

Future<std::string> User::GetToken(bool force_refresh) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<std::string>(kUserFn_GetToken);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      getIDTokenForcingRefresh:force_refresh
                    completion:^(NSString *_Nullable token, NSError *_Nullable error) {
                      auto locked = weak_auth.lock();
                      if (!locked) return;
                      locked->future_impl.Complete(
                          handle, AuthErrorFromNSError(error),
                          [error.localizedDescription UTF8String],
                          [token](std::string *result) {
                            result->assign(token == nullptr ? "" : [token UTF8String]);
                          });
                    }];
  return MakeFuture(&futures, handle);
}

Future<IdTokenResult> User::GetIdTokenResult(bool force_refresh) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<IdTokenResult>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<IdTokenResult>(kUserFn_GetIdTokenResult);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      getIDTokenResultForcingRefresh:force_refresh
                          completion:^(FIRAuthTokenResult *_Nullable token_result,
                                       NSError *_Nullable error) {
                            auto locked = weak_auth.lock();
                            if (!locked) return;
                            locked->future_impl.Complete(
                                handle, AuthErrorFromNSError(error),
                                [error.localizedDescription UTF8String],
                                [token_result](IdTokenResult *result) {
                                  if (token_result != nullptr) {
                                    result->token = StringFromNSString(token_result.token);
                                    result->auth_timestamp =
                                        token_result.authDate
                                            ? static_cast<uint64_t>(
                                                  token_result.authDate.timeIntervalSince1970)
                                            : 0;
                                    result->issued_at_timestamp =
                                        token_result.issuedAtDate
                                            ? static_cast<uint64_t>(
                                                  token_result.issuedAtDate.timeIntervalSince1970)
                                            : 0;
                                    result->expiration_timestamp =
                                        token_result.expirationDate
                                            ? static_cast<uint64_t>(
                                                  token_result.expirationDate.timeIntervalSince1970)
                                            : 0;
                                    result->sign_in_provider =
                                        StringFromNSString(token_result.signInProvider);
                                    result->sign_in_second_factor =
                                        StringFromNSString(token_result.signInSecondFactor);
                                    result->claims.clear();
                                    if (token_result.claims != nil) {
                                      util::NSDictionaryToStdMap(token_result.claims,
                                                                 &result->claims);
                                    }
                                  } else {
                                    *result = IdTokenResult();
                                  }
                                });
                          }];
  return MakeFuture(&futures, handle);
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  auto data = auth_data_.lock();
  if (ValidUser(data.get())) {
    NSArray<id<FIRUserInfo>> *fir_provider_data = UserImpl(data.get()).providerData;
    if (fir_provider_data != nullptr) {
      provider_data.reserve(fir_provider_data.count);
      for (size_t i = 0; i < fir_provider_data.count; ++i) {
        IOSWrappedUserInfo ios_user_info(fir_provider_data[i]);
        UserInfoInterface user_info;
        user_info.uid_ = ios_user_info.uid();
        user_info.email_ = ios_user_info.email();
        user_info.display_name_ = ios_user_info.display_name();
        user_info.photo_url_ = ios_user_info.photo_url();
        user_info.provider_id_ = ios_user_info.provider_id();
        user_info.phone_number_ = ios_user_info.phone_number();
        provider_data.push_back(user_info);
      }
    }
  }

  // Return a reference to our internally-backed values.
  return provider_data;
}

Future<void> User::UpdatePassword(const char *password) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdatePassword);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      updatePassword:@(password)
          completion:^(NSError *_Nullable error) {
            auto locked = weak_auth.lock();
            if (!locked) return;
            locked->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                         [error.localizedDescription UTF8String]);
          }];
  return MakeFuture(&futures, handle);
}

Future<void> User::UpdateUserProfile(const UserProfile &profile) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_UpdateUserProfile);
  // Create and populate the change request.
  FIRUserProfileChangeRequest *request = [UserImpl(data.get()) profileChangeRequest];
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
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [request commitChangesWithCompletion:^(NSError *_Nullable error) {
    auto locked = weak_auth.lock();
    if (!locked) return;
    locked->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                 [error.localizedDescription UTF8String]);
  }];
  return MakeFuture(&futures, handle);
}

Future<void> User::SendEmailVerification() {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_SendEmailVerification);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get()) sendEmailVerificationWithCompletion:^(NSError *_Nullable error) {
    auto locked = weak_auth.lock();
    if (!locked) return;
    locked->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                 [error.localizedDescription UTF8String]);
  }];
  return MakeFuture(&futures, handle);
}

Future<void> User::SendEmailVerificationBeforeUpdatingEmail(const char *email) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_SendEmailVerificationBeforeUpdatingEmail);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      sendEmailVerificationBeforeUpdatingEmail:@(email)
                                    completion:^(NSError *_Nullable error) {
                                      auto locked = weak_auth.lock();
                                      if (!locked) return;
                                      locked->future_impl.Complete(
                                          handle, AuthErrorFromNSError(error),
                                          [error.localizedDescription UTF8String]);
                                    }];
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::LinkWithCredential(const Credential &credential) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kUserFn_LinkWithCredential, AuthResult());
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      linkWithCredential:CredentialFromImpl(credential.impl_)
              completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                auto locked = weak_auth.lock();
                if (!locked) return;
                AuthResultCallback(auth_result, error, handle, locked.get());
              }];
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::LinkWithProvider(FederatedAuthProvider *provider) const {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<AuthResult>();
  }
  return provider->Link(data.get());
}

Future<AuthResult> User::Unlink(const char *provider) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kUserFn_Unlink);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get()) unlinkFromProvider:@(provider)
                                completion:^(FIRUser *_Nullable user, NSError *_Nullable error) {
                                  auto locked = weak_auth.lock();
                                  if (!locked) return;
                                  AuthResultCallback(user, error, handle, locked.get());
                                }];
  return MakeFuture(&futures, handle);
}

Future<User> User::UpdatePhoneNumberCredential(const PhoneAuthCredential &credential) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<User>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<User>(kUserFn_UpdatePhoneNumberCredential);

#if FIREBASE_PLATFORM_IOS
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }
  FIRPhoneAuthCredential *objc_credential = PhoneAuthCredentialFromImpl(credential.impl_);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      updatePhoneNumberCredential:objc_credential
                       completion:^(NSError *_Nullable error) {
                         auto locked = weak_auth.lock();
                         if (!locked) return;
                         locked->future_impl.Complete(
                             handle, AuthErrorFromNSError(error),
                             [error.localizedDescription UTF8String]);
                       }];
#else   // non iOS Apple platforms (eg: tvOS).
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "Phone Auth is not supported on non-iOS apple platforms.");
#endif  // FIREBASE_PLATFORM_IOS

  return MakeFuture(&futures, handle);
}

Future<void> User::Reload() {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reload);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get()) reloadWithCompletion:^(NSError *_Nullable error) {
    auto locked = weak_auth.lock();
    if (!locked) return;
    locked->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                 [error.localizedDescription UTF8String]);
  }];
  return MakeFuture(&futures, handle);
}

Future<void> User::Reauthenticate(const Credential &credential) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Reauthenticate);
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          auto locked = weak_auth.lock();
                          if (!locked) return;
                          locked->future_impl.Complete(
                              handle, AuthErrorFromNSError(error),
                              [error.localizedDescription UTF8String]);
                        }];
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::ReauthenticateAndRetrieveData(const Credential &credential) {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(
      kUserFn_ReauthenticateAndRetrieveData, AuthResult());
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get())
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          auto locked = weak_auth.lock();
                          if (!locked) return;
                          AuthResultCallback(auth_result, error, handle, locked.get());
                        }];
  return MakeFuture(&futures, handle);
}

Future<AuthResult> User::ReauthenticateWithProvider(FederatedAuthProvider *provider) const {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<AuthResult>();
  }
  return provider->Reauthenticate(data.get());
}

Future<void> User::Delete() {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &futures = data->future_impl;
  const auto handle = futures.SafeAlloc<void>(kUserFn_Delete);
  std::weak_ptr<AuthData> weak_auth = auth_data_;
  [UserImpl(data.get()) deleteWithCompletion:^(NSError *_Nullable error) {
    auto locked = weak_auth.lock();
    if (!locked) return;
    if (!error) {
      UpdateCurrentUser(locked.get());
      locked->future_impl.Complete(handle, kAuthErrorNone, "");
    } else {
      locked->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                   [error.localizedDescription UTF8String]);
    }
  }];
  return MakeFuture(&futures, handle);
}

UserMetadata User::metadata() const {
  auto data = auth_data_.lock();
  if (!ValidUser(data.get())) return UserMetadata();

  FIRUserMetadata *meta = UserImpl(data.get()).metadata;
  if (!meta) return UserMetadata();

  UserMetadata result;
  // * 1000 because timeIntervalSince1970 is in seconds, and we need ms.
  // TODO(b/69176745): Use a util for this so we don't forget about this ms conversion elsewhere.
  result.last_sign_in_timestamp =
      static_cast<uint64_t>([meta.lastSignInDate timeIntervalSince1970] * 1000.0);
  result.creation_timestamp =
      static_cast<uint64_t>([meta.creationDate timeIntervalSince1970] * 1000.0);
  return result;
}

bool User::is_email_verified() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? UserImpl(data.get()).emailVerified : false;
}

bool User::is_anonymous() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? UserImpl(data.get()).anonymous : false;
}

std::string User::uid() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSString(UserImpl(data.get()).uid) : "";
}

std::string User::email() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSString(UserImpl(data.get()).email) : "";
}

std::string User::display_name() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSString(UserImpl(data.get()).displayName) : "";
}

std::string User::phone_number() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSString(UserImpl(data.get()).phoneNumber) : "";
}

std::string User::photo_url() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSUrl(UserImpl(data.get()).photoURL) : "";
}

std::string User::provider_id() const {
  auto data = auth_data_.lock();
  return ValidUser(data.get()) ? StringFromNSString(UserImpl(data.get()).providerID) : "";
}

}  // namespace auth
}  // namespace firebase
