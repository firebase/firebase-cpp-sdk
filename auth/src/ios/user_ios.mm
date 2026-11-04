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
#include <cstring>
#include "app/src/time.h"
#include "auth/src/ios/common_ios.h"

namespace firebase {
namespace auth {

using util::StringFromNSString;
using util::StringFromNSUrl;

User::User(const User &user) { auth_data_ = user.auth_data_; }

User &User::operator=(const User &user) {
  auth_data_ = user.auth_data_;
  return *this;
}

User::~User() { auth_data_ = nullptr; }

bool User::is_valid() const { return ValidUser(auth_data_); }

void GetToken(AuthData* auth_data, Promise<std::string> promise, bool force_refresh) {
  if (!ValidUser(auth_data)) {
    return Future<std::string>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (force_refresh) {
    MutexLock lock(auth_data->future_impl.mutex());
    auth_data->SetExpectIdTokenListenerCallback(true);
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      getIDTokenForcingRefresh:force_refresh
                    completion:^(NSString *_Nullable token, NSError *_Nullable error) {
                      @synchronized(listener_cpp_handle) {
                        AuthData *data = listener_cpp_handle.authData;
                        if (!data) return;
                        data->future_impl.Complete(
                            promise.handle(), AuthErrorFromNSError(error),
                            ErrorMessageFromNSError(error).c_str(),
                            [token](std::string *result_data) {
                              *result_data = StringFromNSString(token);
                            });
                        if (force_refresh) {
                          if (error == nil) {
                            bool should_notify = false;
                            {
                              MutexLock lock(data->future_impl.mutex());
                              should_notify = data->ShouldTriggerIdTokenListenerCallback();
                            }
                            if (should_notify) {
                              NotifyIdTokenListeners(data);
                            }
                          } else {
                            MutexLock lock(data->future_impl.mutex());
                            data->SetExpectIdTokenListenerCallback(false);
                          }
                        }
                      }
                    }];
}

void GetIdTokenResult(AuthData* auth_data, Promise<IdTokenResult> promise, bool force_refresh) {
  if (!ValidUser(auth_data)) {
    return Future<IdTokenResult>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (force_refresh) {
    MutexLock lock(auth_data->future_impl.mutex());
    auth_data->SetExpectIdTokenListenerCallback(true);
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      getIDTokenResultForcingRefresh:force_refresh
                          completion:^(FIRAuthTokenResult *_Nullable token_result,
                                       NSError *_Nullable error) {
                            @synchronized(listener_cpp_handle) {
                              AuthData *data = listener_cpp_handle.authData;
                              if (!data) return;
                              data->future_impl.Complete(
                                  promise.handle(), AuthErrorFromNSError(error),
                                  ErrorMessageFromNSError(error).c_str(),
                                  [token_result](IdTokenResult *result_data) {
                                    if (token_result != nullptr) {
                                      result_data->token =
                                          StringFromNSString(token_result.token);
                                      NSTimeInterval auth_sec =
                                          token_result.authDate
                                              ? token_result.authDate.timeIntervalSince1970
                                              : 0.0;
                                      result_data->auth_timestamp =
                                          auth_sec > 0.0 ? static_cast<uint64_t>(auth_sec) : 0;
                                      NSTimeInterval issued_sec =
                                          token_result.issuedAtDate
                                              ? token_result.issuedAtDate.timeIntervalSince1970
                                              : 0.0;
                                      result_data->issued_at_timestamp =
                                          issued_sec > 0.0 ? static_cast<uint64_t>(issued_sec) : 0;
                                      NSTimeInterval exp_sec =
                                          token_result.expirationDate
                                              ? token_result.expirationDate.timeIntervalSince1970
                                              : 0.0;
                                      result_data->expiration_timestamp =
                                          exp_sec > 0.0 ? static_cast<uint64_t>(exp_sec) : 0;
                                      result_data->sign_in_provider =
                                          StringFromNSString(token_result.signInProvider);
                                      result_data->sign_in_second_factor =
                                          StringFromNSString(token_result.signInSecondFactor);
                                      result_data->claims.clear();
                                      if (token_result.claims != nil) {
                                        util::NSDictionaryToStdMap(token_result.claims,
                                                                   &result_data->claims);
                                      }
                                    } else {
                                      *result_data = IdTokenResult();
                                    }
                                  });
                              if (force_refresh) {
                                if (error == nil) {
                                  bool should_notify = false;
                                  {
                                    MutexLock lock(data->future_impl.mutex());
                                    should_notify = data->ShouldTriggerIdTokenListenerCallback();
                                  }
                                  if (should_notify) {
                                    NotifyIdTokenListeners(data);
                                  }
                                } else {
                                  MutexLock lock(data->future_impl.mutex());
                                  data->SetExpectIdTokenListenerCallback(false);
                                }
                              }
                            }
                          }];
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  MutexLock lock(auth_data_->future_impl.mutex());
  if (ValidUser(auth_data_)) {
    NSArray<id<FIRUserInfo>> *fir_provider_data = UserImpl(auth_data_).providerData;
    if (fir_provider_data != nullptr) {
      provider_data.reserve(fir_provider_data.count);
      for (size_t i = 0; i < fir_provider_data.count; ++i) {
        id<FIRUserInfo> fir_user_info = fir_provider_data[i];
        if (!fir_user_info) continue;
        UserInfoInterface user_info;
        user_info.uid_ = StringFromNSString(fir_user_info.uid);
        user_info.email_ = StringFromNSString(fir_user_info.email);
        user_info.display_name_ = StringFromNSString(fir_user_info.displayName);
        user_info.photo_url_ = StringFromNSUrl(fir_user_info.photoURL);
        user_info.provider_id_ = StringFromNSString(fir_user_info.providerID);
        user_info.phone_number_ = StringFromNSString(fir_user_info.phoneNumber);
        provider_data.push_back(user_info);
      }
    }
  }

  // Return a copy of the provider data values.
  return provider_data;
}

void UpdatePassword(AuthData* auth_data, Promise<void> promise, const char *password) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (!password) {
    futures.Complete(promise.handle(), kAuthErrorMissingPassword, "Password must be provided.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      updatePassword:util::CStringToNSString(password)
          completion:^(NSError *_Nullable error) {
            @synchronized(listener_cpp_handle) {
              AuthData *data = listener_cpp_handle.authData;
              if (!data) return;
              if (!error) {
                UpdateCurrentUser(data);
              }
              data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                         ErrorMessageFromNSError(error).c_str());
            }
          }];
}

void UpdateUserProfile(AuthData* auth_data, Promise<void> promise, const UserProfile &profile) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  // Create and populate the change request.
  FIRUserProfileChangeRequest *request = [UserImpl(auth_data) profileChangeRequest];
  if (profile.display_name != nullptr) {
    request.displayName = util::CStringToNSString(profile.display_name);
  }
  if (profile.photo_url != nullptr) {
    if (strlen(profile.photo_url) == 0) {
      request.photoURL = [NSURL URLWithString:@""];
    } else {
      NSString *photo_url_string = util::CStringToNSString(profile.photo_url);
      NSURL *photo_url = photo_url_string ? [NSURL URLWithString:photo_url_string] : nil;
      if (!photo_url && photo_url_string) {
        NSString *encoded = [photo_url_string
            stringByAddingPercentEncodingWithAllowedCharacters:[NSCharacterSet
                                                                   URLQueryAllowedCharacterSet]];
        photo_url = encoded ? [NSURL URLWithString:encoded] : nil;
      }
      request.photoURL = photo_url;
    }
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  // Execute the change request.
  [request commitChangesWithCompletion:^(NSError *_Nullable error) {
    @synchronized(listener_cpp_handle) {
      AuthData *data = listener_cpp_handle.authData;
      if (!data) return;
      if (!error) {
        UpdateCurrentUser(data);
      }
      data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                 ErrorMessageFromNSError(error).c_str());
    }
  }];
}

void SendEmailVerification(AuthData* auth_data, Promise<void> promise) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data) sendEmailVerificationWithCompletion:^(NSError *_Nullable error) {
    @synchronized(listener_cpp_handle) {
      AuthData *data = listener_cpp_handle.authData;
      if (!data) return;
      data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                 ErrorMessageFromNSError(error).c_str());
    }
  }];
}

void SendEmailVerificationBeforeUpdatingEmail(AuthData* auth_data, Promise<void> promise, const char *email) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (!email || strlen(email) == 0) {
    futures.Complete(promise.handle(), kAuthErrorMissingEmail, "Email must be provided.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      sendEmailVerificationBeforeUpdatingEmail:util::CStringToNSString(email)
                                    completion:^(NSError *_Nullable error) {
                                      @synchronized(listener_cpp_handle) {
                                        AuthData *data = listener_cpp_handle.authData;
                                        if (!data) return;
                                        data->future_impl.Complete(
                                            promise.handle(), AuthErrorFromNSError(error),
                                            ErrorMessageFromNSError(error).c_str());
                                      }
                                    }];
}

void LinkWithCredential(AuthData* auth_data, Promise<AuthResult> promise, const Credential &credential) {
  if (!ValidUser(auth_data)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      linkWithCredential:CredentialFromImpl(credential.impl_)
              completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                @synchronized(listener_cpp_handle) {
                  AuthData *data = listener_cpp_handle.authData;
                  if (!data) return;
                  AuthResultCallback(auth_result, error, promise.handle(), data);
                }
              }];
}

Future<AuthResult> LinkWithProvider(AuthData* auth_data, FederatedAuthProvider *provider) {
  if (!ValidUser(auth_data)) {
    return Future<AuthResult>();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data);
}

void Unlink(AuthData* auth_data, Promise<AuthResult> promise, const char *provider) {
  if (!ValidUser(auth_data)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (!provider || strlen(provider) == 0) {
    futures.Complete(promise.handle(), kAuthErrorInvalidProviderId, "Provider ID must be provided.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      unlinkFromProvider:util::CStringToNSString(provider)
              completion:^(FIRUser *_Nullable user, NSError *_Nullable error) {
                @synchronized(listener_cpp_handle) {
                  AuthData *data = listener_cpp_handle.authData;
                  if (!data) return;
                  AuthResultCallback(user, error, promise.handle(), data);
                }
              }];
}

void UpdatePhoneNumberCredential(AuthData* auth_data, Promise<User> promise, const PhoneAuthCredential &credential) {
  if (!ValidUser(auth_data)) {
    return Future<User>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;

#if FIREBASE_PLATFORM_IOS
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
  }
  FIRPhoneAuthCredential *objc_credential = PhoneAuthCredentialFromImpl(credential.impl_);
  if (!objc_credential) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Credential is not a phone credential.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      updatePhoneNumberCredential:objc_credential
                       completion:^(NSError *_Nullable error) {
                         @synchronized(listener_cpp_handle) {
                           AuthData *data = listener_cpp_handle.authData;
                           if (!data) return;
                           if (!error) {
                             UpdateCurrentUser(data);
                           }
                           AuthResultCallback(UserImpl(data), error, promise.handle(), data);
                         }
                       }];
#else   // non iOS Apple platforms (eg: tvOS).
  futures.Complete(promise.handle(), kAuthErrorApiNotAvailable,
                   "Phone Auth is not supported on non-iOS apple platforms.");
#endif  // FIREBASE_PLATFORM_IOS
}

void Reload(AuthData* auth_data, Promise<void> promise) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data) reloadWithCompletion:^(NSError *_Nullable error) {
    @synchronized(listener_cpp_handle) {
      AuthData *data = listener_cpp_handle.authData;
      if (!data) return;
      if (!error) {
        UpdateCurrentUser(data);
      }
      data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                 ErrorMessageFromNSError(error).c_str());
    }
  }];
}

void Reauthenticate(AuthData* auth_data, Promise<void> promise, const Credential &credential) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          @synchronized(listener_cpp_handle) {
                            AuthData *data = listener_cpp_handle.authData;
                            if (!data) return;
                            if (!error) {
                              if (auth_result != nil && auth_result.user != nil) {
                                SetUserImpl(data, auth_result.user);
                              } else {
                                UpdateCurrentUser(data);
                              }
                            }
                            data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                                       ErrorMessageFromNSError(error).c_str());
                          }
                        }];
}

void ReauthenticateAndRetrieveData(AuthData* auth_data, Promise<AuthResult> promise, const Credential &credential) {
  if (!ValidUser(auth_data)) {
    return Future<AuthResult>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  const auto promise.handle() = auth_data->future_impl.SafeAlloc<AuthResult>(
      kUserFn_ReauthenticateAndRetrieveData, AuthResult());
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data)
      reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                        completion:^(FIRAuthDataResult *_Nullable auth_result,
                                     NSError *_Nullable error) {
                          @synchronized(listener_cpp_handle) {
                            AuthData *data = listener_cpp_handle.authData;
                            if (!data) return;
                            AuthResultCallback(auth_result, error, promise.handle(), data);
                          }
                        }];
}

Future<AuthResult> ReauthenticateWithProvider(AuthData* auth_data, FederatedAuthProvider *provider) {
  if (!ValidUser(auth_data)) {
    return Future<AuthResult>();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data);
}

void Delete(AuthData* auth_data, Promise<void> promise) {
  if (!ValidUser(auth_data)) {
    return Future<void>();
  }
  ReferenceCountedFutureImpl &auth_data->future_impl = auth_data->future_impl;
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle.get();
  [UserImpl(auth_data) deleteWithCompletion:^(NSError *_Nullable error) {
    @synchronized(listener_cpp_handle) {
      AuthData *data = listener_cpp_handle.authData;
      if (!data) return;
      if (!error) {
        UpdateCurrentUser(data);
        data->future_impl.Complete(promise.handle(), kAuthErrorNone, "");
      } else {
        data->future_impl.Complete(promise.handle(), AuthErrorFromNSError(error),
                                   ErrorMessageFromNSError(error).c_str());
      }
    }
  }];
}

UserMetadata User::metadata() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  if (!ValidUser(auth_data_)) return UserMetadata();

  FIRUserMetadata *meta = UserImpl(auth_data_).metadata;
  if (!meta) return UserMetadata();

  UserMetadata data;
  // * 1000 because timeIntervalSince1970 is in seconds, and we need ms.
  NSTimeInterval last_sign_in_sec =
      meta.lastSignInDate ? [meta.lastSignInDate timeIntervalSince1970] : 0.0;
  data.last_sign_in_timestamp =
      last_sign_in_sec > 0.0 ? static_cast<uint64_t>(last_sign_in_sec * 1000.0) : 0;
  NSTimeInterval creation_sec = meta.creationDate ? [meta.creationDate timeIntervalSince1970] : 0.0;
  data.creation_timestamp = creation_sec > 0.0 ? static_cast<uint64_t>(creation_sec * 1000.0) : 0;
  return data;
}

bool User::is_email_verified() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? UserImpl(auth_data_).emailVerified : false;
}

bool User::is_anonymous() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? UserImpl(auth_data_).anonymous : false;
}

std::string User::uid() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).uid) : "";
}

std::string User::email() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).email) : "";
}

std::string User::display_name() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).displayName) : "";
}

std::string User::phone_number() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).phoneNumber) : "";
}

std::string User::photo_url() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSUrl(UserImpl(auth_data_).photoURL) : "";
}

std::string User::provider_id() const {
  MutexLock lock(auth_data_->future_impl.mutex());
  return ValidUser(auth_data_) ? StringFromNSString(UserImpl(auth_data_).providerID) : "";
}

}  // namespace auth
}  // namespace firebase
