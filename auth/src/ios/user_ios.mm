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
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
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
                        data->future_impl.Complete(
                            promise.handle(), AuthErrorFromNSError(error),
                            ErrorMessageFromNSError(error).c_str(),
                            [token](std::string *result_data) {
                              *result_data = StringFromNSString(token);
                            });
                      }
                    }];
}

void GetTokenInternal(AuthData* auth_data, Promise<std::string> promise,
                      bool force_refresh, int /*future_identifier*/) {
  GetToken(auth_data, promise, force_refresh);
}

void GetIdTokenResult(AuthData* auth_data, Promise<IdTokenResult> promise, bool force_refresh) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
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
                            }
                          }];
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  if (!auth_data_) return provider_data;
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
  if (!password) { promise.Fail(kAuthErrorMissingPassword, "Password must be provided."); return; }
  RunAppleUserTaskWithPromise(auth_data, promise, [=](FIRUser* u, FIRCPPAuthListenerHandle* h) {
    [u updatePassword:util::CStringToNSString(password) completion:^(NSError *error) {
      @synchronized(h) {
        if (!h.authData) return;
        if (!error) UpdateCurrentUser(h.authData);
        CompletePromiseWithNSError(h.authData, promise.handle(), error);
      }
    }];
  });
}

void UpdateUserProfile(AuthData* auth_data, Promise<void> promise, const UserProfile &profile) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
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
  RunAppleUserTaskWithPromise(auth_data, promise, [=](FIRUser* u, FIRCPPAuthListenerHandle* h) {
    NSString *language_code = nil;
    { MutexLock lock(static_cast<AuthImpl *>(auth_data->auth_impl)->provider_mutex);
      if (!static_cast<AuthImpl *>(auth_data->auth_impl)->language_code.empty())
        language_code = util::CStringToNSString(static_cast<AuthImpl *>(auth_data->auth_impl)->language_code.c_str());
    }
    FIRActionCodeSettings *actionCodeSettings = [[FIRActionCodeSettings alloc] init];
    actionCodeSettings.URL = [NSURL URLWithString:@"https://example.com/verify?"];
    [u sendEmailVerificationBeforeUpdatingEmail:nil actionCodeSettings:actionCodeSettings completion:^(NSError *error) {
      @synchronized(h) {
        if (!h.authData) return;
        CompletePromiseWithNSError(h.authData, promise.handle(), error);
      }
    }];
  });
}

void SendEmailVerificationBeforeUpdatingEmail(AuthData* auth_data, Promise<void> promise, const char *email) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  if (!email || strlen(email) == 0) {
    futures.Complete(promise.handle(), kAuthErrorMissingEmail, "Email must be provided.");
    return;
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
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
    return;
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return;
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
  if (!auth_data) {
    return Future<AuthResult>();
  }
  if (!ValidUser(auth_data)) {
    Promise<AuthResult> promise(&auth_data->future_impl, kUserFn_LinkWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data);
}

void Unlink(AuthData* auth_data, Promise<AuthResult> promise, const char *provider) {
  if (!provider) { promise.Fail(kAuthErrorNoSuchProvider, "Invalid provider"); return; }
  RunAppleUserTaskWithPromise(auth_data, promise, [=](FIRUser* u, FIRCPPAuthListenerHandle* h) {
    [u unlinkFromProvider:util::CStringToNSString(provider) completion:^(FIRAuthDataResult* r, NSError* e) {
      @synchronized(h) {
        if (!h.authData) return;
        if (e && e.code == FIRAuthErrorCodeNoSuchProvider) { CompletePromiseWithNSError(h.authData, promise.handle(), e, kAuthErrorNoSuchProvider); return; }
        AuthResultWithProviderCallback(r, e, promise.handle(), h.authData, nil);
      }
    }];
  });
}

void UpdatePhoneNumberCredential(AuthData* auth_data, Promise<User> promise, const PhoneAuthCredential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;

#if FIREBASE_PLATFORM_IOS
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
    return;
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return;
  }
  FIRPhoneAuthCredential *objc_credential = PhoneAuthCredentialFromImpl(credential.impl_);
  if (!objc_credential) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Credential is not a phone credential.");
    return;
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
  RunAppleUserTaskWithPromise(auth_data, promise, [=](FIRUser* u, FIRCPPAuthListenerHandle* h) {
    [u reloadWithCompletion:^(NSError *error) {
      @synchronized(h) {
        if (!h.authData) return;
        if (!error) UpdateCurrentUser(h.authData);
        CompletePromiseWithNSError(h.authData, promise.handle(), error);
      }
    }];
  });
}

void Reauthenticate(AuthData* auth_data, Promise<void> promise, const Credential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
    return;
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return;
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
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(promise.handle(), credential.error_code_, credential.error_message_.c_str());
    return;
  }
  if (!credential.is_valid()) {
    futures.Complete(promise.handle(), kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return;
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
  if (!auth_data) {
    return Future<AuthResult>();
  }
  if (!ValidUser(auth_data)) {
    Promise<AuthResult> promise(&auth_data->future_impl, kUserFn_ReauthenticateWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data);
}

void Delete(AuthData* auth_data, Promise<void> promise) {
  RunAppleUserTaskWithPromise(auth_data, promise, [=](FIRUser* u, FIRCPPAuthListenerHandle* h) {
    [u deleteWithCompletion:^(NSError *error) {
      @synchronized(h) {
        if (!h.authData) return;
        if (!error) SignOut(h.authData);
        CompletePromiseWithNSError(h.authData, promise.handle(), error);
      }
    }];
  });
}

