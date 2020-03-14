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

#include "app/src/assert.h"
#include "app/src/build_type_generated.h"
#include "app/src/util_ios.h"
#include "auth/src/ios/common_ios.h"

#import <GameKit/GameKit.h>

#import "FIREmailAuthProvider.h"
#import "FIRFacebookAuthProvider.h"
#import "FIRFederatedAuthProvider.h"
#import "FIRGameCenterAuthProvider.h"
#import "FIRGitHubAuthProvider.h"
#import "FIRGoogleAuthProvider.h"
#import "FIROAuthProvider.h"
#import "FIRPhoneAuthProvider.h"
#import "FIRTwitterAuthProvider.h"

// This object is shared between the PhoneAuthProvider::Listener and the blocks in
// @ref VerifyPhoneNumber. It exists for as long as one of those two lives. We use Objective-C
// for reference counting.
@interface PhoneListenerDataObjC : NSObject {
@public
  // Back-pointer to the PhoneAuthProvider::Listener, or nullptr if it has been deleted.
  // This lets the VerifyPhoneNumber block know when its Listener has been deleted so it should
  // become a no-op.
  firebase::auth::PhoneAuthProvider::Listener* active_listener;

  // The mutex protecting Listener callbacks and destruction. Callbacks and destruction are atomic.
  firebase::Mutex listener_mutex;
}
@end
@implementation PhoneListenerDataObjC
@end

namespace firebase {
namespace auth {

using util::StringFromNSString;

Credential::~Credential() {
  delete static_cast<FIRAuthCredentialPointer*>(impl_);
  impl_ = NULL;
}

// Default copy constructor.
Credential::Credential(const Credential& rhs) : impl_(NULL) {
  if (rhs.impl_ != NULL) {
    impl_ = new FIRAuthCredentialPointer(CredentialFromImpl(rhs.impl_));
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
}

// Default assigment.
Credential& Credential::operator=(const Credential& rhs) {
  if (rhs.impl_ != NULL) {
    delete static_cast<FIRAuthCredentialPointer*>(impl_);
    impl_ = new FIRAuthCredentialPointer(CredentialFromImpl(rhs.impl_));
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
  return *this;
}

bool Credential::is_valid() const { return CredentialFromImpl(impl_) != NULL; }

std::string Credential::provider() const {
  return impl_ == nullptr ? std::string("") :
    util::StringFromNSString(CredentialFromImpl(impl_).provider);
}

// static
Credential EmailAuthProvider::GetCredential(const char* email, const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  FIRAuthCredential *credential = [FIREmailAuthProvider credentialWithEmail:@(email)
                                                                   password:@(password)];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential FacebookAuthProvider::GetCredential(const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), access_token);
  FIRAuthCredential *credential =
      [FIRFacebookAuthProvider credentialWithAccessToken:@(access_token)];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential GitHubAuthProvider::GetCredential(const char* token) {
  FIREBASE_ASSERT_RETURN(Credential(), token);
  FIRAuthCredential *credential = [FIRGitHubAuthProvider credentialWithToken:@(token)];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token, const char* access_token) {
  NSString* ns_id_token = id_token ? @(id_token) : nil;
  NSString* ns_access_token = access_token ? @(access_token) : nil;
  FIRAuthCredential* credential =
      [FIRGoogleAuthProvider credentialWithIDToken:ns_id_token accessToken:ns_access_token];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  // Play Games is not available on iOS.
  const BOOL is_play_games_available_on_ios = NO;
  FIREBASE_ASSERT_RETURN(Credential(), is_play_games_available_on_ios);
  return Credential();  // placate -Wreturn-type
}

// static
Credential TwitterAuthProvider::GetCredential(const char* token, const char* secret) {
  FIREBASE_ASSERT_RETURN(Credential(), token && secret);
  FIRAuthCredential *credential = [FIRTwitterAuthProvider credentialWithToken:@(token)
                                                                       secret:@(secret)];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential OAuthProvider::GetCredential(
    const char* provider_id, const char* id_token, const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && access_token);
  FIRAuthCredential* credential =
      (FIRAuthCredential*)[FIROAuthProvider credentialWithProviderID:@(provider_id)
                                                             IDToken:@(id_token)
                                                         accessToken:@(access_token)];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id,
                                        const char* id_token,
                                        const char* raw_nonce,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && raw_nonce);

  NSString* access_token_string =
      (access_token != nullptr) ? util::CStringToNSString(access_token) : nullptr;
  FIRAuthCredential* credential =
      (FIRAuthCredential*)[FIROAuthProvider credentialWithProviderID:@(provider_id)
                                                             IDToken:@(id_token)
                                                            rawNonce:@(raw_nonce)
                                                         accessToken:access_token_string];
  return Credential(new FIRAuthCredentialPointer(credential));
}

// static
Future<Credential> GameCenterAuthProvider::GetCredential() {
  auto future_api = GetCredentialFutureImpl();
  FIREBASE_ASSERT(future_api != nullptr);

  const auto handle = future_api->SafeAlloc<Credential>(kCredentialFn_GameCenterGetCredential);

  // Early-out if GameKit is not linked
  if ([GKLocalPlayer class] == nullptr) {
    future_api->Complete(handle, kAuthErrorInvalidCredential,
                       "GameCenter authentication is unavailable - missing GameKit capability.");
    return MakeFuture(future_api, handle);
  }

  [FIRGameCenterAuthProvider getCredentialWithCompletion:^(FIRAuthCredential* _Nullable credential,
                                                           NSError* _Nullable error) {
    Credential result(new FIRAuthCredentialPointer(credential));
    future_api->CompleteWithResult(handle, AuthErrorFromNSError(error),
                                  util::NSStringToString(error.localizedDescription).c_str(),
                                  result);
  }];

  return MakeFuture(future_api, handle);
}

  // static
Future<Credential> GameCenterAuthProvider::GetCredentialLastResult() {
  auto future_api = GetCredentialFutureImpl();
  FIREBASE_ASSERT(future_api != nullptr);

  auto last_result = future_api->LastResult(kCredentialFn_GameCenterGetCredential);
  return static_cast<const Future<Credential>&>(last_result);
}


// static
bool GameCenterAuthProvider::IsPlayerAuthenticated() {
  // If the GameKit Framework isn't linked - early out.
  if ([GKLocalPlayer class] == nullptr) {
    return false;
  }
  GKLocalPlayer *localPlayer = [GKLocalPlayer localPlayer];
  return localPlayer.isAuthenticated;
}

// We skip the implementation of ForceResendingTokenData since it is not needed.
// The ForceResendingToken class for iOS is empty.
PhoneAuthProvider::ForceResendingToken::ForceResendingToken()
    : data_(nullptr) {}
PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() {}
PhoneAuthProvider::ForceResendingToken::ForceResendingToken(
    const ForceResendingToken&) : data_(nullptr) {}
PhoneAuthProvider::ForceResendingToken& PhoneAuthProvider::ForceResendingToken::
operator=(const ForceResendingToken&) { return *this; }
bool PhoneAuthProvider::ForceResendingToken::operator==(
    const ForceResendingToken&) const { return true; }
bool PhoneAuthProvider::ForceResendingToken::operator!=(
    const ForceResendingToken&) const { return false; }

// This implementation of PhoneListenerData is specific to iOS.
struct PhoneListenerData {
  // Hold the reference-counted ObjC structure. This structure is shared by the blocks in
  // @ref VerifyPhoneNumber
  PhoneListenerDataObjC* objc;
};

PhoneAuthProvider::Listener::Listener() : data_(new PhoneListenerData) {
  data_->objc = [[PhoneListenerDataObjC alloc] init];
  data_->objc->active_listener = this;
}

PhoneAuthProvider::Listener::~Listener() {
  // Wait while the Listener is being used (in the callbacks in VerifyPhoneNumber).
  // Then reset the active_listener so that callbacks become no-ops.
  {
    MutexLock lock(data_->objc->listener_mutex);
    data_->objc->active_listener = nullptr;
  }
  data_->objc = nil;
  delete data_;
}

// This implementation of PhoneAuthProviderData is specific to iOS.
struct PhoneAuthProviderData {
 public:
  explicit PhoneAuthProviderData(FIRPhoneAuthProvider* objc_provider)
      : objc_provider(objc_provider) {}

  // The wrapped provider in Objective-C.
  FIRPhoneAuthProvider* objc_provider;
};

PhoneAuthProvider::PhoneAuthProvider() : data_(nullptr) {}
PhoneAuthProvider::~PhoneAuthProvider() { delete data_; }

void PhoneAuthProvider::VerifyPhoneNumber(
    const char* phone_number, uint32_t /*auto_verify_time_out_ms*/,
    const ForceResendingToken* /*force_resending_token*/, Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  const PhoneAuthProvider::ForceResendingToken invalid_resending_token;
  PhoneListenerDataObjC* objc_listener_data = listener->data_->objc;

  [data_->objc_provider verifyPhoneNumber:@(phone_number)
                               UIDelegate:nil
                               completion:^(NSString *_Nullable verificationID,
                                            NSError *_Nullable error) {
                          MutexLock lock(objc_listener_data->listener_mutex);

                          // If the listener has been deleted before this callback, do nothing.
                          if (objc_listener_data->active_listener == nullptr) return;

                          // Call OnVerificationFailed() or OnCodeSent() as appropriate.
                          if (verificationID == nullptr) {
                            listener->OnVerificationFailed(
                                util::StringFromNSString(error.localizedDescription));
                          } else {
                            listener->OnCodeSent(util::StringFromNSString(verificationID),
                                                 invalid_resending_token);
                          }
                        }];

  // Only call the callback when protected by the mutex.
  {
    MutexLock lock(objc_listener_data->listener_mutex);
    if (objc_listener_data->active_listener != nullptr) {
      listener->OnCodeAutoRetrievalTimeOut(std::string());
    }
  }
}

Credential PhoneAuthProvider::GetCredential(const char* verification_id,
                                            const char* verification_code) {
  FIREBASE_ASSERT_RETURN(Credential(), verification_id && verification_code);
  FIRPhoneAuthCredential *credential =
      [data_->objc_provider credentialWithVerificationID:@(verification_id)
                                        verificationCode:@(verification_code)];
  return Credential(new FIRAuthCredentialPointer((FIRAuthCredential*)credential));
}

// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (provider.data_ == nullptr) {
    FIRPhoneAuthProvider* objc_provider =
        [FIRPhoneAuthProvider providerWithAuth:AuthImpl(auth->auth_data_)];

    // Create the implementation class that holds the wrapped FIRPhoneAuthProvider.
    provider.data_ = new PhoneAuthProviderData(objc_provider);
  }
  return provider;
}

// FederatedAuthHandlers
FederatedOAuthProvider::FederatedOAuthProvider() { }

FederatedOAuthProvider::FederatedOAuthProvider(const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

FederatedOAuthProvider::~FederatedOAuthProvider() { }

void FederatedOAuthProvider::SetProviderData(const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

// Callback to funnel a result from a GetCredential request to linkWithCredential request. This
// fulfills User::LinkWithProvider functionality on iOS, which currently isn't accessible via their
// current API.
void LinkWithProviderGetCredentialCallback(FIRAuthCredential* _Nullable credential,
                                           NSError* _Nullable error,
                                           SafeFutureHandle<SignInResult> handle,
                                           AuthData* auth_data,
                                           const FIROAuthProvider* ios_auth_provider) {
  if (error && error.code != 0) {
    ReferenceCountedFutureImpl& futures = auth_data->future_impl;
    error = RemapBadProviderIDErrors(error);
    futures.CompleteWithResult(handle, AuthErrorFromNSError(error),
                               util::NSStringToString(error.localizedDescription).c_str(),
                               SignInResult());
  } else {
    [UserImpl(auth_data)
        linkWithCredential:credential
                completion:^(FIRAuthDataResult* _Nullable auth_result, NSError* _Nullable error) {
                  SignInResultWithProviderCallback(auth_result, error, handle, auth_data,
                                                   ios_auth_provider);
                }];
  }
}

// Callback to funnel a result from a GetCredential request to reauthetnicateWithCredential request.
// This fulfills User::ReauthenticateWithProvider functionality on iOS, which currently isn't
// accessible via their current API.
void ReauthenticateWithProviderGetCredentialCallback(FIRAuthCredential* _Nullable credential,
                                                     NSError* _Nullable error,
                                                     SafeFutureHandle<SignInResult> handle,
                                                     AuthData* auth_data,
                                                     const FIROAuthProvider* ios_auth_provider) {
  if (error && error.code != 0) {
    ReferenceCountedFutureImpl& futures = auth_data->future_impl;
    error = RemapBadProviderIDErrors(error);
    futures.CompleteWithResult(handle, AuthErrorFromNSError(error),
                               util::NSStringToString(error.localizedDescription).c_str(),
                               SignInResult());
  } else {
    [UserImpl(auth_data)
        reauthenticateWithCredential:credential
                          completion:^(FIRAuthDataResult* _Nullable auth_result,
                                       NSError* _Nullable error) {
                            SignInResultWithProviderCallback(auth_result, error, handle, auth_data,
                                                             ios_auth_provider);
                          }];
  }
}

Future<SignInResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  assert(auth_data);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  const auto handle =
      futures.SafeAlloc<SignInResult>(kAuthFn_SignInWithProvider, SignInResult());
  FIROAuthProvider* ios_provider = (FIROAuthProvider*)[FIROAuthProvider
      providerWithProviderID:@(provider_data_.provider_id.c_str())
                        auth:AuthImpl(auth_data)];
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    [AuthImpl(auth_data)
        signInWithProvider:ios_provider
                UIDelegate:nullptr
                completion:^(FIRAuthDataResult* _Nullable auth_result, NSError* _Nullable error) {
                  SignInResultWithProviderCallback(auth_result, error, handle, auth_data,
                                                   ios_provider);
                }];
    return MakeFuture(&futures, handle);
  } else {
    Future<SignInResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                           SignInResult());
    return future;
  }
}

Future<SignInResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  assert(auth_data);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  auto handle =
      futures.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider, SignInResult());
  FIROAuthProvider* ios_provider = (FIROAuthProvider*)[FIROAuthProvider
      providerWithProviderID:@(provider_data_.provider_id.c_str())
                        auth:AuthImpl(auth_data)];
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    // TODO(b/138788092) invoke FIRUser linkWithProvider instead, once that method is added to the
    // iOS SDK.
    [ios_provider
        getCredentialWithUIDelegate:nullptr
                         completion:^(FIRAuthCredential* _Nullable credential,
                                      NSError* _Nullable error) {
                           LinkWithProviderGetCredentialCallback(
                               credential, error, handle, auth_data, ios_provider);
                         }];
    return MakeFuture(&futures, handle);
  } else {
    Future<SignInResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                               SignInResult());
    return future;
  }
}

Future<SignInResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  assert(auth_data);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  auto handle =
      futures.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider, SignInResult());
  FIROAuthProvider* ios_provider = (FIROAuthProvider*)[FIROAuthProvider
      providerWithProviderID:@(provider_data_.provider_id.c_str())
                        auth:AuthImpl(auth_data)];
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    // TODO(b/138788092) invoke FIRUser:reuthenticateWithProvider instead, once that method is added
    // to the iOS SDK.
    [ios_provider
        getCredentialWithUIDelegate:nullptr
                         completion:^(FIRAuthCredential* _Nullable credential,
                                      NSError* _Nullable error) {
                           ReauthenticateWithProviderGetCredentialCallback(
                               credential, error, handle, auth_data, ios_provider);
                         }];
    return MakeFuture(&futures, handle);
  } else {
    Future<SignInResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                               SignInResult());
    return future;
  }
}

}  // namespace auth
}  // namespace firebase
