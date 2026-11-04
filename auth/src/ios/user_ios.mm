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

bool User::is_valid() const { return ValidUser(auth_data_); }

static void NotifyIdTokenListenersIfNeeded(AuthData *data, bool force_refresh,
                                           NSError *error) {
  if (!force_refresh) return;
  if (error == nil) {
    bool should_notify = false;
    {
      MutexLock lock(data->future_impl.mutex());
      should_notify = data->ShouldTriggerIdTokenListenerCallback();
    }
    if (should_notify) NotifyIdTokenListeners(data);
  } else {
    MutexLock lock(data->future_impl.mutex());
    data->SetExpectIdTokenListenerCallback(false);
  }
}

static uint64_t DateToSec(NSDate *_Nullable date) {
  NSTimeInterval sec = date ? date.timeIntervalSince1970 : 0.0;
  return sec > 0.0 ? static_cast<uint64_t>(sec) : 0;
}

void GetToken(AuthData *auth_data, Promise<std::string> promise,
              bool force_refresh) {
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        if (force_refresh) {
          MutexLock lock(auth_data->future_impl.mutex());
          auth_data->SetExpectIdTokenListenerCallback(true);
        }
        [u getIDTokenForcingRefresh:force_refresh
                         completion:^(NSString *_Nullable token,
                                      NSError *_Nullable error) {
                           @synchronized(h) {
                             AuthData *data = h.authData;
                             if (!data) return;
                             NotifyIdTokenListenersIfNeeded(data, force_refresh,
                                                            error);
                             data->future_impl.Complete(
                                 promise.handle(), AuthErrorFromNSError(error),
                                 ErrorMessageFromNSError(error).c_str(),
                                 [token](std::string *result_data) {
                                   *result_data = StringFromNSString(token);
                                 });
                           }
                         }];
      });
}

void GetTokenInternal(AuthData *auth_data, Promise<std::string> promise,
                      bool force_refresh, int /*future_identifier*/) {
  GetToken(auth_data, promise, force_refresh);
}

void GetIdTokenResult(AuthData *auth_data, Promise<IdTokenResult> promise,
                      bool force_refresh) {
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        if (force_refresh) {
          MutexLock lock(auth_data->future_impl.mutex());
          auth_data->SetExpectIdTokenListenerCallback(true);
        }
        [u getIDTokenResultForcingRefresh:force_refresh
                               completion:^(FIRAuthTokenResult *_Nullable res,
                                            NSError *_Nullable error) {
                                 @synchronized(h) {
                                   AuthData *data = h.authData;
                                   if (!data) return;
                                   NotifyIdTokenListenersIfNeeded(
                                       data, force_refresh, error);
                                   data->future_impl.Complete(
                                       promise.handle(),
                                       AuthErrorFromNSError(error),
                                       ErrorMessageFromNSError(error).c_str(),
                                       [res](IdTokenResult *r) {
                                         if (!res) {
                                           *r = IdTokenResult();
                                           return;
                                         }
                                         r->token =
                                             StringFromNSString(res.token);
                                         r->auth_timestamp =
                                             DateToSec(res.authDate);
                                         r->issued_at_timestamp =
                                             DateToSec(res.issuedAtDate);
                                         r->expiration_timestamp =
                                             DateToSec(res.expirationDate);
                                         r->sign_in_provider =
                                             StringFromNSString(
                                                 res.signInProvider);
                                         r->sign_in_second_factor =
                                             StringFromNSString(
                                                 res.signInSecondFactor);
                                         r->claims.clear();
                                         if (res.claims != nil) {
                                           util::NSDictionaryToStdMap(
                                               res.claims, &r->claims);
                                         }
                                       });
                                 }
                               }];
      });
}

std::vector<UserInfoInterface> User::provider_data() const {
  std::vector<UserInfoInterface> provider_data;
  if (!auth_data_) return provider_data;
  MutexLock lock(auth_data_->future_impl.mutex());
  if (ValidUser(auth_data_)) {
    NSArray<id<FIRUserInfo>> *fir_provider_data =
        UserImpl(auth_data_).providerData;
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
  return provider_data;
}

void UpdatePassword(AuthData *auth_data, Promise<void> promise,
                    const char *password) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!password) {
    promise.Fail(kAuthErrorMissingPassword, "Password must be provided.");
    return;
  }
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u updatePassword:util::CStringToNSString(password)
               completion:^(NSError *error) {
                 @synchronized(h) {
                   if (!h.authData) return;
                   if (!error) UpdateCurrentUser(h.authData);
                   CompletePromiseWithNSError(h.authData, promise.handle(),
                                              error);
                 }
               }];
      });
}

void UpdateUserProfile(AuthData *auth_data, Promise<void> promise,
                       const UserProfile &profile) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  FIRUserProfileChangeRequest *request =
      [UserImpl(auth_data) profileChangeRequest];
  if (profile.display_name != nullptr) {
    request.displayName = util::CStringToNSString(profile.display_name);
  }
  if (profile.photo_url != nullptr) {
    if (strlen(profile.photo_url) == 0) {
      request.photoURL = [NSURL URLWithString:@""];
    } else {
      NSString *photo_url_string = util::CStringToNSString(profile.photo_url);
      NSURL *photo_url =
          photo_url_string ? [NSURL URLWithString:photo_url_string] : nil;
      if (!photo_url && photo_url_string) {
        NSString *encoded = [photo_url_string
            stringByAddingPercentEncodingWithAllowedCharacters:
                [NSCharacterSet URLQueryAllowedCharacterSet]];
        photo_url = encoded ? [NSURL URLWithString:encoded] : nil;
      }
      request.photoURL = photo_url;
    }
  }

  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser * /*u*/, FIRCPPAuthListenerHandle *h) {
        [request commitChangesWithCompletion:^(NSError *error) {
          @synchronized(h) {
            if (!h.authData) return;
            if (!error) UpdateCurrentUser(h.authData);
            CompletePromiseWithNSError(h.authData, promise.handle(), error);
          }
        }];
      });
}

void SendEmailVerification(AuthData *auth_data, Promise<void> promise) {
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u sendEmailVerificationWithCompletion:^(NSError *_Nullable error) {
          @synchronized(h) {
            if (h.authData)
              CompletePromiseWithNSError(h.authData, promise.handle(), error);
          }
        }];
      });
}

void SendEmailVerificationBeforeUpdatingEmail(AuthData *auth_data,
                                              Promise<void> promise,
                                              const char *email) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!email || strlen(email) == 0) {
    promise.Fail(kAuthErrorMissingEmail, "Email must be provided.");
    return;
  }
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u sendEmailVerificationBeforeUpdatingEmail:util::CStringToNSString(
                                                        email)
                                         completion:^(NSError *_Nullable err) {
                                           @synchronized(h) {
                                             if (h.authData)
                                               CompletePromiseWithNSError(
                                                   h.authData, promise.handle(),
                                                   err);
                                           }
                                         }];
      });
}

void LinkWithCredential(AuthData *auth_data, Promise<AuthResult> promise,
                        const Credential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateIosCredential(credential, &promise)) return;
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u linkWithCredential:CredentialFromImpl(credential.impl_)
                   completion:^(FIRAuthDataResult *_Nullable auth_result,
                                NSError *_Nullable error) {
                     @synchronized(h) {
                       if (h.authData)
                         AuthResultCallback(auth_result, error,
                                            promise.handle(), h.authData);
                     }
                   }];
      });
}

Future<AuthResult> LinkWithProvider(AuthData *auth_data,
                                    FederatedAuthProvider *provider) {
  if (!auth_data) return Future<AuthResult>();
  if (!ValidUser(auth_data)) {
    Promise<AuthResult> promise(&auth_data->future_impl,
                                kUserFn_LinkWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Link(auth_data);
}

void Unlink(AuthData *auth_data, Promise<AuthResult> promise,
            const char *provider) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!provider || strlen(provider) == 0) {
    promise.Fail(kAuthErrorInvalidProviderId, "Provider ID must be provided.");
    return;
  }
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u unlinkFromProvider:util::CStringToNSString(provider)
                   completion:^(FIRUser *_Nullable user,
                                NSError *_Nullable error) {
                     @synchronized(h) {
                       if (h.authData)
                         AuthResultCallback(user, error, promise.handle(),
                                            h.authData);
                     }
                   }];
      });
}

void UpdatePhoneNumberCredential(AuthData *auth_data, Promise<User> promise,
                                 const PhoneAuthCredential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
#if FIREBASE_PLATFORM_IOS
  if (!ValidateIosCredential(credential, &promise)) return;
  FIRPhoneAuthCredential *objc_credential =
      PhoneAuthCredentialFromImpl(credential.impl_);
  if (!objc_credential) {
    promise.Fail(kAuthErrorInvalidCredential,
                 "Credential is not a phone credential.");
    return;
  }
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u updatePhoneNumberCredential:objc_credential
                            completion:^(NSError *error) {
                              @synchronized(h) {
                                if (!h.authData) return;
                                if (!error) UpdateCurrentUser(h.authData);
                                AuthResultCallback(UserImpl(h.authData), error,
                                                   promise.handle(),
                                                   h.authData);
                              }
                            }];
      });
#else
  promise.Fail(kAuthErrorApiNotAvailable,
               "Phone Auth is not supported on non-iOS apple platforms.");
#endif
}

void Reload(AuthData *auth_data, Promise<void> promise) {
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u reloadWithCompletion:^(NSError *error) {
          @synchronized(h) {
            if (!h.authData) return;
            if (!error) UpdateCurrentUser(h.authData);
            CompletePromiseWithNSError(h.authData, promise.handle(), error);
          }
        }];
      });
}

void Reauthenticate(AuthData *auth_data, Promise<void> promise,
                    const Credential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateIosCredential(credential, &promise)) return;
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                             completion:^(FIRAuthDataResult *res,
                                          NSError *err) {
                               @synchronized(h) {
                                 if (!h.authData) return;
                                 if (!err) {
                                   if (res && res.user)
                                     SetUserImpl(h.authData, res.user);
                                   else
                                     UpdateCurrentUser(h.authData);
                                 }
                                 CompletePromiseWithNSError(
                                     h.authData, promise.handle(), err);
                               }
                             }];
      });
}

void ReauthenticateAndRetrieveData(AuthData *auth_data,
                                   Promise<AuthResult> promise,
                                   const Credential &credential) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  if (!ValidateIosCredential(credential, &promise)) return;
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u reauthenticateWithCredential:CredentialFromImpl(credential.impl_)
                             completion:^(FIRAuthDataResult *res,
                                          NSError *err) {
                               @synchronized(h) {
                                 if (h.authData)
                                   AuthResultCallback(res, err,
                                                      promise.handle(),
                                                      h.authData);
                               }
                             }];
      });
}

Future<AuthResult> ReauthenticateWithProvider(AuthData *auth_data,
                                              FederatedAuthProvider *provider) {
  if (!auth_data) return Future<AuthResult>();
  if (!ValidUser(auth_data)) {
    Promise<AuthResult> promise(&auth_data->future_impl,
                                kUserFn_ReauthenticateWithProvider);
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return promise.future();
  }
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->Reauthenticate(auth_data);
}

void Delete(AuthData *auth_data, Promise<void> promise) {
  RunAppleUserTaskWithPromise(
      auth_data, promise, [=](FIRUser *u, FIRCPPAuthListenerHandle *h) {
        [u deleteWithCompletion:^(NSError *error) {
          @synchronized(h) {
            if (!h.authData) return;
            if (!error) UpdateCurrentUser(h.authData);
            CompletePromiseWithNSError(h.authData, promise.handle(), error);
          }
        }];
      });
}

UserMetadata User::metadata() const {
  if (!auth_data_) return UserMetadata();
  MutexLock lock(auth_data_->future_impl.mutex());
  if (!ValidUser(auth_data_)) return UserMetadata();
  FIRUserMetadata *meta = UserImpl(auth_data_).metadata;
  if (!meta) return UserMetadata();

  UserMetadata data;
  NSTimeInterval last_sec =
      meta.lastSignInDate ? [meta.lastSignInDate timeIntervalSince1970] : 0.0;
  data.last_sign_in_timestamp =
      last_sec > 0.0 ? static_cast<uint64_t>(last_sec * 1000.0) : 0;
  NSTimeInterval create_sec =
      meta.creationDate ? [meta.creationDate timeIntervalSince1970] : 0.0;
  data.creation_timestamp =
      create_sec > 0.0 ? static_cast<uint64_t>(create_sec * 1000.0) : 0;
  return data;
}

template <typename R, typename Fn>
static R GetUserIosProp(AuthData *auth_data, R default_val, Fn fn) {
  if (!auth_data) return default_val;
  MutexLock lock(auth_data->future_impl.mutex());
  return ValidUser(auth_data) ? fn(UserImpl(auth_data)) : default_val;
}

bool User::is_email_verified() const {
  return GetUserIosProp(auth_data_, false,
                        [](FIRUser *u) { return u.emailVerified; });
}
bool User::is_anonymous() const {
  return GetUserIosProp(auth_data_, false,
                        [](FIRUser *u) { return u.anonymous; });
}
std::string User::uid() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSString(u.uid);
  });
}
std::string User::email() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSString(u.email);
  });
}
std::string User::display_name() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSString(u.displayName);
  });
}
std::string User::phone_number() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSString(u.phoneNumber);
  });
}
std::string User::photo_url() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSUrl(u.photoURL);
  });
}
std::string User::provider_id() const {
  return GetUserIosProp(auth_data_, std::string(), [](FIRUser *u) {
    return StringFromNSString(u.providerID);
  });
}

}  // namespace auth
}  // namespace firebase
