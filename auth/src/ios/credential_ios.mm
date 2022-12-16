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
#include "app/src/util_ios.h"
#include "auth/src/credential_internal.h"
#include "auth/src/ios/common_ios.h"

#import <GameKit/GameKit.h>

#import "FIREmailAuthProvider.h"
#import "FIRFacebookAuthProvider.h"
#import "FIRFederatedAuthProvider.h"
#import "FIRGameCenterAuthProvider.h"
#import "FIRGitHubAuthProvider.h"
#import "FIRGoogleAuthProvider.h"
#import "FIROAuthCredential.h"
#import "FIROAuthProvider.h"

#if FIREBASE_PLATFORM_IOS
// PhoneAuth is not supported on non-iOS Apple platforms (eg: tvOS).
// We are using stub implementation for these platforms (just like on desktop).
#import "FIRPhoneAuthCredential.h"
#import "FIRPhoneAuthProvider.h"
#endif  // FIREBASE_PLATFORM_IOS

#import "FIRTwitterAuthProvider.h"

#if FIREBASE_PLATFORM_IOS
// This object is shared between the PhoneAuthProvider::Listener and the blocks in
// @ref VerifyPhoneNumber. It exists for as long as one of those two lives. We use Objective-C
// for reference counting.
@interface FIRCppPhoneListenerHandle : NSObject
- (instancetype)init NS_UNAVAILABLE;

// Initialize with a reference to the specified listener.
- (instancetype) initWithListener:(firebase::auth::PhoneAuthProvider::Listener*) initListener;

// Call the specified method with the current listener while holding the lock to access the
// listener. If a listener isn't set the callback will not be called.
- (void) usingListener:(void (^)(firebase::auth::PhoneAuthProvider::Listener* listener))callback;

// Clear the listener from this object.
- (void) clearListener;
@end

@implementation FIRCppPhoneListenerHandle {
  // Back-pointer to the PhoneAuthProvider::Listener, or nullptr if it has been deleted.
  // This lets the VerifyPhoneNumber block know when its Listener has been deleted so it should
  // become a no-op.
  firebase::auth::PhoneAuthProvider::Listener* listener;

  // The mutex protecting Listener callbacks and destruction. Callbacks and destruction are atomic.
  NSRecursiveLock* lock;
}

- (instancetype) initWithListener:(firebase::auth::PhoneAuthProvider::Listener*) initListener {
  self = [super init];
  if (self) {
    listener = initListener;
    lock = [[NSRecursiveLock alloc] init];
  }
  return self;
}

- (void) usingListener:(void (^)(firebase::auth::PhoneAuthProvider::Listener* listener))callback {
  [lock lock];
  if (listener) callback(listener);
  [lock unlock];
}

- (void) clearListener {
  [lock lock];
  listener = nullptr;
  [lock unlock];
}
@end
#endif  // FIREBASE_PLATFORM_IOS

namespace firebase {
namespace auth {

static const char* kMockVerificationId = "PhoneAuth not supported on this platform";

using util::StringFromNSString;

Credential CredentialInternal::Create(
    CreatePlatformCredentialCallback create_platform_credential) {
  AuthError error_code = kAuthErrorNone;
  std::string error_message;
  FIRAuthCredential* platform_credential;
  @try {
    platform_credential = create_platform_credential();
  } @catch (NSException* e) {
    platform_credential = nil;
    error_message = util::NSStringToString(e.reason);
  }
  if (!platform_credential) error_code = kAuthErrorInvalidCredential;
  return Credential(platform_credential ?
                    new FIRAuthCredentialPointer(platform_credential) : nullptr,
                    error_code, error_message);
}

Credential::~Credential() {
  delete CredentialInternal::GetPlatformCredential(*this);
  impl_ = nullptr;
}

// Default copy constructor.
Credential::Credential(const Credential& rhs) : impl_(nullptr) {
  operator=(rhs);
}

// Default assigment.
Credential& Credential::operator=(const Credential& rhs) {
  delete CredentialInternal::GetPlatformCredential(*this);
  impl_ = nullptr;
  auto* rhs_impl = CredentialInternal::GetPlatformCredential(rhs);
  if (rhs_impl) impl_ = new FIRAuthCredentialPointer(*rhs_impl);
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
  return *this;
}

bool Credential::is_valid() const {
  return CredentialInternal::GetPlatformCredential(*this) != nullptr;
}

std::string Credential::provider() const {
  return is_valid() ?
    util::StringFromNSString(CredentialInternal::GetPlatformCredential(*this)->ptr.provider) :
    std::string();
}

// static
Credential EmailAuthProvider::GetCredential(const char* email, const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  FIRAuthCredential* credential = [FIREmailAuthProvider credentialWithEmail:@(email)
                                                                   password:@(password)];
  return CredentialInternal::Create(^{
      return [FIREmailAuthProvider credentialWithEmail:@(email)
                                              password:@(password)];
    });
}

// static
Credential FacebookAuthProvider::GetCredential(const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), access_token);
  return CredentialInternal::Create(^{
      return [FIRFacebookAuthProvider credentialWithAccessToken:@(access_token)];
    });
}

// static
Credential GitHubAuthProvider::GetCredential(const char* token) {
  FIREBASE_ASSERT_RETURN(Credential(), token);
  FIRAuthCredential* credential = [FIRGitHubAuthProvider credentialWithToken:@(token)];
  return CredentialInternal::Create(^{
      return [FIRGitHubAuthProvider credentialWithToken:@(token)];
    });
}

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token, const char* access_token) {
  return CredentialInternal::Create(^{
        return [FIRGoogleAuthProvider credentialWithIDToken:(id_token ? @(id_token) : nil)
                                                accessToken:(access_token ? @(access_token) : nil)];
    });
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
  FIRAuthCredential* credential = [FIRTwitterAuthProvider credentialWithToken:@(token)
                                                                       secret:@(secret)];
  return CredentialInternal::Create(^{
      return [FIRTwitterAuthProvider credentialWithToken:@(token) secret:@(secret)];
    });
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && access_token);
  return CredentialInternal::Create(^{
      FIRAuthCredential* credential = [FIROAuthProvider credentialWithProviderID:@(provider_id)
                                                                         IDToken:@(id_token)
                                                                     accessToken:@(access_token)];
      return credential;
    });
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token,
                                        const char* raw_nonce, const char* access_token) {
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
  /**
   Linking GameKit.framework without using it on macOS results in App Store rejection.
   Thus we don't link GameKit.framework to our SDK directly. `optionalLocalPlayer` is used for
   checking whether the APP that consuming our SDK has linked GameKit.framework. If not, will
   complete with kAuthErrorInvalidCredential error.
   **/
  GKLocalPlayer* _Nullable optionalLocalPlayer = [[NSClassFromString(@"GKLocalPlayer") alloc] init];

  // Early-out if GameKit is not linked
  if (!optionalLocalPlayer) {
    future_api->Complete(handle, kAuthErrorInvalidCredential,
                         "GameCenter authentication is unavailable - missing GameKit capability.");
    return MakeFuture(future_api, handle);
  }

  [FIRGameCenterAuthProvider getCredentialWithCompletion:^(FIRAuthCredential* _Nullable credential,
                                                           NSError* _Nullable error) {
    future_api->CompleteWithResult(
        handle, AuthErrorFromNSError(error),
        util::NSStringToString(error.localizedDescription).c_str(),
        CredentialInternal::Create(^{ return credential; }));
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
  /**
   Linking GameKit.framework without using it on macOS results in App Store rejection.
   Thus we don't link GameKit.framework to our SDK directly. `optionalLocalPlayer` is used for
   checking whether the APP that consuming our SDK has linked GameKit.framework. If not,
   early out.
   **/
  GKLocalPlayer* _Nullable optionalLocalPlayer = [[NSClassFromString(@"GKLocalPlayer") alloc] init];
  // If the GameKit Framework isn't linked - early out.
  if (!optionalLocalPlayer) {
    return false;
  }
  __weak GKLocalPlayer* localPlayer = [[optionalLocalPlayer class] localPlayer];
  return localPlayer.isAuthenticated;
}

// We skip the implementation of ForceResendingTokenInternal since it is not needed.
// The ForceResendingToken class for iOS is empty.
PhoneAuthProvider::ForceResendingToken::ForceResendingToken() : internal_(nullptr) {}
PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() {}
PhoneAuthProvider::ForceResendingToken::ForceResendingToken(const ForceResendingToken&)
    : internal_(nullptr) {}
PhoneAuthProvider::ForceResendingToken& PhoneAuthProvider::ForceResendingToken::operator=(
    const ForceResendingToken&) {
  return *this;
}
bool PhoneAuthProvider::ForceResendingToken::operator==(const ForceResendingToken&) const {
  return true;
}
bool PhoneAuthProvider::ForceResendingToken::operator!=(const ForceResendingToken&) const {
  return false;
}
bool PhoneAuthProvider::ForceResendingToken::is_valid() const { return false; }

#if FIREBASE_PLATFORM_IOS
// This implementation of PhoneListenerInternal is specific to iOS.
OBJ_C_PTR_WRAPPER_NAMED(PhoneListenerInternal, FIRCppPhoneListenerHandle);

PhoneAuthProvider::Listener::Listener() {
  internal_ = new PhoneListenerInternal([[FIRCppPhoneListenerHandle alloc] initWithListener:this]);
}

PhoneAuthProvider::Listener::~Listener() {
  [internal_->get() clearListener];
    delete internal_;
    internal_ = nullptr;
}

bool PhoneAuthProvider::Listener::is_valid() const {
  return internal_->get() != nullptr;
}
#else   // non-iOS Apple platforms (eg: tvOS)
// Stub for tvOS and other non-iOS apple platforms.
PhoneAuthProvider::Listener::Listener() : data_(nullptr) {}
PhoneAuthProvider::Listener::~Listener() {}

bool PhoneAuthProvider::Listener::is_valid() const { return false; }
#endif  // FIREBASE_PLATFORM_IOS




#if FIREBASE_PLATFORM_IOS
void PhoneAuthProvider::VerifyPhoneNumber(const char* phone_number,
                                          uint32_t /*auto_verify_time_out_ms*/,
                                          const ForceResendingToken* /*force_resending_token*/,
                                          Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  const PhoneAuthProvider::ForceResendingToken invalid_resending_token;
  PhoneListenerInternal* listener_internal = listener->internal_->get();

  [internal_->get() verifyPhoneNumber:@(phone_number)
                           UIDelegate:nil
                           completion:^(NSString *_Nullable verificationID,
                                        NSError *_Nullable error) {
      [listener_internal usingListener:^(Listener* callback_listener) {
          // Call OnVerificationFailed() or OnCodeSent() as appropriate.
          if (verificationID == nullptr) {
            callback_listener->OnVerificationFailed(
                util::StringFromNSString(error.localizedDescription));
          } else {
            callback_listener->OnCodeSent(util::StringFromNSString(verificationID),
                                          invalid_resending_token);
          }
        }];
    }];

  // Only call the callback when protected by the mutex.
  [listener_internal usingListener:^(Listener* callback_listener) {
      callback_listener->OnCodeAutoRetrievalTimeOut(std::string());
    }];
}
#else   // non-iOS Apple platforms (eg: tvOS)
void PhoneAuthProvider::VerifyPhoneNumber(const char* /*phone_number*/,
                                          uint32_t /*auto_verify_time_out_ms*/,
                                          const ForceResendingToken* force_resending_token,
                                          Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);

  // Mock the tokens by sending a new one whenever it's unspecified.
  ForceResendingToken token;
  if (force_resending_token != nullptr) {
    token = *force_resending_token;
  }

  listener->OnCodeAutoRetrievalTimeOut(kMockVerificationId);
  listener->OnCodeSent(kMockVerificationId, token);
}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
Credential PhoneAuthProvider::GetCredential(const char* verification_id,
                                            const char* verification_code) {
  FIREBASE_ASSERT_RETURN(Credential(), verification_id && verification_code);
  FIRPhoneAuthProvider* provider = internal_->get();
  return CredentialInternal::Create(^{
      FIRAuthCredential* credential = [provider credentialWithVerificationID:@(verification_id)
                                                            verificationCode:@(verification_code)];
      return credential;
    });
}
#else   // non-iOS Apple platforms (eg: tvOS)
Credential PhoneAuthProvider::GetCredential(const char* verification_id,
                                            const char* verification_code) {
  FIREBASE_ASSERT_MESSAGE_RETURN(
      Credential(nullptr), false,
      "Phone Auth is not supported on non iOS Apple platforms (eg:tvOS).");

  return Credential(nullptr);
}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  static PhoneAuthProvider invalid_provider;
  FIREBASE_ASSERT_RETURN(invalid_provider, auth);
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (provider.internal_ == nullptr) {
    // Create the implementation class that holds the wrapped FIRPhoneAuthProvider.
     provider.internal_ = new PhoneAuthProviderInternal(
      [FIRPhoneAuthProvider providerWithAuth:AuthImpl(auth->auth_data_)]);
  }
  return provider;
}
#else   // non-iOS Apple platforms (eg: tvOS)
// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  return auth->auth_data_->phone_auth_provider;
}
#endif  // FIREBASE_PLATFORM_IOS

// FederatedAuthHandlers
FederatedOAuthProvider::FederatedOAuthProvider() {}

FederatedOAuthProvider::FederatedOAuthProvider(const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

FederatedOAuthProvider::~FederatedOAuthProvider() {}

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
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle = futures.SafeAlloc<SignInResult>(kAuthFn_SignInWithProvider, SignInResult());
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
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  auto handle = futures.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider, SignInResult());
#if FIREBASE_PLATFORM_IOS
  FIROAuthProvider* ios_provider = (FIROAuthProvider*)[FIROAuthProvider
      providerWithProviderID:@(provider_data_.provider_id.c_str())
                        auth:AuthImpl(auth_data)];
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    // TODO(b/138788092) invoke FIRUser linkWithProvider instead, once that method is added to the
    // iOS SDK.
    [ios_provider getCredentialWithUIDelegate:nullptr
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

#else   // non-iOS Apple platforms (eg: tvOS)
  Future<SignInResult> future = MakeFuture(&futures, handle);
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "OAuth provider linking is not supported on non-iOS Apple platforms.");
#endif  // FIREBASE_PLATFORM_IOS
}

Future<SignInResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  assert(auth_data);
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  auto handle = futures.SafeAlloc<SignInResult>(kUserFn_LinkWithProvider, SignInResult());
#if FIREBASE_PLATFORM_IOS
  FIROAuthProvider* ios_provider = (FIROAuthProvider*)[FIROAuthProvider
      providerWithProviderID:@(provider_data_.provider_id.c_str())
                        auth:AuthImpl(auth_data)];
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    // TODO(b/138788092) invoke FIRUser:reuthenticateWithProvider instead, once that method is added
    // to the iOS SDK.
    [ios_provider getCredentialWithUIDelegate:nullptr
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

#else   // non-iOS Apple platforms (eg: tvOS)
  Future<SignInResult> future = MakeFuture(&futures, handle);
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "OAuth reauthentication is not supported on non-iOS Apple platforms.");
#endif  // FIREBASE_PLATFORM_IOS
}

}  // namespace auth
}  // namespace firebase
