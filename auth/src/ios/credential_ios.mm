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
#include "auth/src/ios/common_ios.h"

#import <GameKit/GameKit.h>
#import <UIKit/UIKit.h>

#import "FIREmailAuthProvider.h"
#import "FIRFacebookAuthProvider.h"
#import "FIRFederatedAuthProvider.h"
#import "FIRGameCenterAuthProvider.h"
#import "FIRGitHubAuthProvider.h"
#import "FIRGoogleAuthProvider.h"
#import "FirebaseAuthInterop/FIRAuthInterop.h"
// This needs to be after the FIRAuthInterop import
#import "FirebaseAuth-Swift.h"

#if FIREBASE_PLATFORM_IOS
// PhoneAuth is not supported on non-iOS Apple platforms (eg: tvOS).
// We are using stub implementation for these platforms (just like on desktop).
#import "FIRPhoneAuthProvider.h"
#endif  // FIREBASE_PLATFORM_IOS

#import "FIRTwitterAuthProvider.h"

#if FIREBASE_PLATFORM_IOS
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
#endif  // FIREBASE_PLATFORM_IOS

namespace firebase {
namespace auth {

static const char* kMockVerificationId = "PhoneAuth not supported on this platform";

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
  if (this == &rhs) return *this;
  if (impl_ != rhs.impl_) {
    if (impl_ != nullptr) {
      delete static_cast<FIRAuthCredentialPointer*>(impl_);
      impl_ = nullptr;
    }

    if (rhs.impl_ != nullptr) {
      impl_ = new FIRAuthCredentialPointer(CredentialFromImpl(rhs.impl_));
    }
  }
  error_code_ = rhs.error_code_;
  error_message_ = rhs.error_message_;
  return *this;
}

bool Credential::operator==(const Credential& credential) const {
  if (impl_ == credential.impl_) {
    return error_code_ == credential.error_code_ &&
           error_message_ == credential.error_message_;
  }
  if (!impl_ || !credential.impl_) {
    return false;
  }
  FIRAuthCredential* lhs = CredentialFromImpl(impl_);
  FIRAuthCredential* rhs = CredentialFromImpl(credential.impl_);
  if (lhs == rhs) {
    return error_code_ == credential.error_code_ &&
           error_message_ == credential.error_message_;
  }
  if (!lhs || !rhs) {
    return false;
  }
  return [lhs isEqual:rhs] && error_code_ == credential.error_code_ &&
         error_message_ == credential.error_message_;
}

bool Credential::is_valid() const { return CredentialFromImpl(impl_) != NULL; }

std::string Credential::provider() const {
  return impl_ == nullptr ? std::string("")
                          : util::StringFromNSString(CredentialFromImpl(impl_).provider);
}

//
// PhoneAuthCredential methods
//
PhoneAuthCredential& PhoneAuthCredential::operator=(const PhoneAuthCredential& rhs) {
  if (this != &rhs) {
    Credential::operator=(rhs);
    sms_code_ = rhs.sms_code_;
  }
  return *this;
}

bool PhoneAuthCredential::operator==(const PhoneAuthCredential& credential) const {
  return Credential::operator==(credential) && sms_code_ == credential.sms_code_;
}

// Gets the automatically retrieved SMS verification code if applicable.
// This method is only supported on Android, so return the cached string.
std::string PhoneAuthCredential::sms_code() const { return sms_code_; }

//
// Provider methods
//
Credential InternalAuthResultProvider::GetCredential(FIRAuthCredential* _Nullable credential,
                                                     AuthError error_code,
                                                     const std::string& error_message) {
  if (credential != nil) {
    return Credential(new FIRAuthCredentialPointer(credential));
  }
  Credential cred(nullptr);
  cred.error_code_ = error_code;
  cred.error_message_ = error_message;
  return cred;
}

// static
Credential EmailAuthProvider::GetCredential(const char* email, const char* password) {
  FIREBASE_ASSERT_RETURN(Credential(), email && password);
  if (strlen(email) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorMissingEmail,
                                                     "An email address must be provided.");
  }
  if (strlen(password) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorMissingPassword,
                                                     "A password must be provided.");
  }
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = [FIREmailAuthProvider credentialWithEmail:util::CStringToNSString(email)
                                                  password:util::CStringToNSString(password)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential FacebookAuthProvider::GetCredential(const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), access_token);
  if (strlen(access_token) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorInvalidCredential,
                                                     "An access token must be provided.");
  }
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential =
        [FIRFacebookAuthProvider credentialWithAccessToken:util::CStringToNSString(access_token)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential GitHubAuthProvider::GetCredential(const char* token) {
  FIREBASE_ASSERT_RETURN(Credential(), token);
  if (strlen(token) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorInvalidCredential,
                                                     "A token must be provided.");
  }
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = [FIRGitHubAuthProvider credentialWithToken:util::CStringToNSString(token)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential GoogleAuthProvider::GetCredential(const char* id_token, const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), id_token || access_token);
  const bool has_id_token = id_token && strlen(id_token) > 0;
  const bool has_access_token = access_token && strlen(access_token) > 0;
  if (!has_id_token && !has_access_token) {
    return InternalAuthResultProvider::GetCredential(
        nil, kAuthErrorInvalidCredential, "Must supply either an ID token or an access token.");
  }
  NSString* ns_id_token = has_id_token ? util::CStringToNSString(id_token) : nil;
  NSString* ns_access_token = has_access_token ? util::CStringToNSString(access_token) : nil;
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = [FIRGoogleAuthProvider credentialWithIDToken:ns_id_token
                                                  accessToken:ns_access_token];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential PlayGamesAuthProvider::GetCredential(const char* server_auth_code) {
  (void)server_auth_code;
  // Play Games is not available on Apple platforms.
  LogWarning("Play Games is not available on Apple platforms.");
  return InternalAuthResultProvider::GetCredential(
      nil, kAuthErrorApiNotAvailable, "Play Games is not available on Apple platforms.");
}

// static
Credential TwitterAuthProvider::GetCredential(const char* token, const char* secret) {
  FIREBASE_ASSERT_RETURN(Credential(), token && secret);
  if (strlen(token) == 0 || strlen(secret) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorInvalidCredential,
                                                     "Both token and secret must be provided.");
  }
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = [FIRTwitterAuthProvider credentialWithToken:util::CStringToNSString(token)
                                                      secret:util::CStringToNSString(secret)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token,
                                        const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && (id_token || access_token));
  if (strlen(provider_id) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorInvalidProviderId,
                                                     "A provider ID must be provided.");
  }
  const bool has_id_token = id_token && strlen(id_token) > 0;
  const bool has_access_token = access_token && strlen(access_token) > 0;
  if (!has_id_token && !has_access_token) {
    return InternalAuthResultProvider::GetCredential(
        nil, kAuthErrorInvalidCredential, "Must supply either an ID token or an access token.");
  }
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = (FIRAuthCredential*)[FIROAuthProvider
        credentialWithProviderID:util::CStringToNSString(provider_id)
                         IDToken:(has_id_token ? util::CStringToNSString(id_token) : @"")
                     accessToken:(has_access_token ? util::CStringToNSString(access_token) : nil)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Credential OAuthProvider::GetCredential(const char* provider_id, const char* id_token,
                                        const char* raw_nonce, const char* access_token) {
  FIREBASE_ASSERT_RETURN(Credential(), provider_id && id_token && raw_nonce);
  if (strlen(provider_id) == 0) {
    return InternalAuthResultProvider::GetCredential(nil, kAuthErrorInvalidProviderId,
                                                     "A provider ID must be provided.");
  }
  if (strlen(id_token) == 0 || strlen(raw_nonce) == 0) {
    return InternalAuthResultProvider::GetCredential(
        nil, kAuthErrorInvalidCredential, "Both ID token and raw nonce must be provided.");
  }

  NSString* access_token_string = (access_token != nullptr && strlen(access_token) > 0)
                                      ? util::CStringToNSString(access_token)
                                      : nil;
  FIRAuthCredential* credential = nil;
  std::string error_message = "Invalid credential.";
  @try {
    credential = (FIRAuthCredential*)[FIROAuthProvider
        credentialWithProviderID:util::CStringToNSString(provider_id)
                         IDToken:util::CStringToNSString(id_token)
                        rawNonce:util::CStringToNSString(raw_nonce)
                     accessToken:access_token_string];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  return InternalAuthResultProvider::GetCredential(credential, kAuthErrorInvalidCredential,
                                                   error_message);
}

// static
Future<Credential> GameCenterAuthProvider::GetCredential() {
  auto future_api = GetCredentialFutureImpl();
  FIREBASE_ASSERT(future_api != nullptr);

  const auto handle = future_api->SafeAlloc<Credential>(kCredentialFn_GameCenterGetCredential);
  /**
   Linking GameKit.framework without using it on macOS results in App Store rejection.
   Thus we don't link GameKit.framework to our SDK directly. `gk_local_player_class` is used for
   checking whether the APP that consuming our SDK has linked GameKit.framework. If not, will
   complete with kAuthErrorInvalidCredential error.
   **/
  Class gk_local_player_class = NSClassFromString(@"GKLocalPlayer");
  GKLocalPlayer* _Nullable local_player =
      gk_local_player_class ? [gk_local_player_class localPlayer] : nil;

  // Early-out if GameKit is not linked
  if (!local_player) {
    future_api->Complete(handle, kAuthErrorInvalidCredential,
                         "GameCenter authentication is unavailable - missing GameKit capability.");
    return MakeFuture(future_api, handle);
  }

  [FIRGameCenterAuthProvider getCredentialWithCompletion:^(FIRAuthCredential* _Nullable credential,
                                                           NSError* _Nullable error) {
    ReferenceCountedFutureImpl* api = GetCredentialFutureImpl();
    if (!api) return;
    AuthError auth_error = AuthErrorFromNSError(error);
    std::string error_msg = ErrorMessageFromNSError(error);
    if (!credential && auth_error == kAuthErrorNone) {
      auth_error = kAuthErrorInvalidCredential;
      if (error_msg.empty()) {
        error_msg = "Failed to obtain GameCenter credential.";
      }
    }
    Credential result =
        InternalAuthResultProvider::GetCredential(credential, auth_error, error_msg);
    api->CompleteWithResult(handle, auth_error, error_msg.c_str(), result);
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
   Thus we don't link GameKit.framework to our SDK directly. `gk_local_player_class` is used for
   checking whether the APP that consuming our SDK has linked GameKit.framework. If not,
   early out.
   **/
  Class gk_local_player_class = NSClassFromString(@"GKLocalPlayer");
  if (!gk_local_player_class) {
    return false;
  }
  GKLocalPlayer* _Nullable localPlayer = [gk_local_player_class localPlayer];
  return localPlayer != nil && localPlayer.isAuthenticated;
}

// We skip the implementation of ForceResendingTokenData since it is not needed.
// The ForceResendingToken class for iOS is empty.
PhoneAuthProvider::ForceResendingToken::ForceResendingToken() : data_(nullptr) {}
PhoneAuthProvider::ForceResendingToken::~ForceResendingToken() {}
PhoneAuthProvider::ForceResendingToken::ForceResendingToken(const ForceResendingToken&)
    : data_(nullptr) {}
PhoneAuthProvider::ForceResendingToken& PhoneAuthProvider::ForceResendingToken::operator=(
    const ForceResendingToken&) {
  return *this;
}
bool PhoneAuthProvider::ForceResendingToken::operator==(const ForceResendingToken& rhs) const {
  return data_ == rhs.data_;
}
bool PhoneAuthProvider::ForceResendingToken::operator!=(const ForceResendingToken& rhs) const {
  return !(*this == rhs);
}

#if FIREBASE_PLATFORM_IOS
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
#else   // non-iOS Apple platforms (eg: tvOS)
// Stub for tvOS and other non-iOS apple platforms.
PhoneAuthProvider::Listener::Listener() : data_(nullptr) {}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
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
#else   // non-iOS Apple platforms (eg: tvOS)
PhoneAuthProvider::Listener::~Listener() {}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
// This implementation of PhoneAuthProviderData is specific to iOS.
struct PhoneAuthProviderData {
 public:
  explicit PhoneAuthProviderData(FIRPhoneAuthProvider* objc_provider)
      : objc_provider(objc_provider) {}

  // The wrapped provider in Objective-C.
  FIRPhoneAuthProvider* objc_provider;
};
#endif  // FIREBASE_PLATFORM_IOS

PhoneAuthProvider::PhoneAuthProvider() : data_(nullptr) {}

#if FIREBASE_PLATFORM_IOS
PhoneAuthProvider::~PhoneAuthProvider() { delete data_; }
#else   // non-iOS Apple platforms (eg: tvOS)
PhoneAuthProvider::~PhoneAuthProvider() {}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
void PhoneAuthProvider::VerifyPhoneNumber(const PhoneAuthOptions& options,
                                          PhoneAuthProvider::Listener* listener) {
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  if (!data_ || !data_->objc_provider) {
    listener->OnVerificationFailed("VerifyPhoneNumber: PhoneAuthProvider is not initialized.");
    return;
  }
  if (options.phone_number.empty()) {
    listener->OnVerificationFailed("VerifyPhoneNumber: unable to verify with empty phone number");
    return;
  }
  NSString* ns_phone_number = util::CStringToNSString(options.phone_number.c_str());
  if (!ns_phone_number) {
    listener->OnVerificationFailed(
        "VerifyPhoneNumber: unable to convert phone number to NSString.");
    return;
  }
  const PhoneAuthProvider::ForceResendingToken invalid_resending_token;
  PhoneListenerDataObjC* objc_listener_data = listener->data_->objc;

  @try {
    [data_->objc_provider
        verifyPhoneNumber:ns_phone_number
               UIDelegate:options.ui_parent
               completion:^(NSString* _Nullable verificationID, NSError* _Nullable error) {
                 MutexLock lock(objc_listener_data->listener_mutex);

                 // If the listener has been deleted before this callback, do nothing.
                 PhoneAuthProvider::Listener* active_listener =
                     objc_listener_data->active_listener;
                 if (active_listener == nullptr) return;

                 // Call OnVerificationFailed() or OnCodeSent() as appropriate.
                 if (error != nil || verificationID == nil) {
                   std::string error_msg = ErrorMessageFromNSError(error);
                   if (error_msg.empty()) {
                     error_msg = "Phone verification failed.";
                   }
                   active_listener->OnVerificationFailed(error_msg);
                 } else {
                   active_listener->OnCodeSent(util::StringFromNSString(verificationID),
                                               invalid_resending_token);
                 }
               }];
  } @catch (NSException* exception) {
    std::string error_msg = util::StringFromNSString(exception.reason);
    if (error_msg.empty()) {
      error_msg = "Phone verification failed.";
    }
    listener->OnVerificationFailed(error_msg);
  }
}
#else   // non-iOS Apple platforms (eg: tvOS)

void PhoneAuthProvider::VerifyPhoneNumber(const PhoneAuthOptions& options,
                                          PhoneAuthProvider::Listener* listener) {
  (void)options;
  FIREBASE_ASSERT_RETURN_VOID(listener != nullptr);
  listener->OnVerificationFailed(kMockVerificationId);
}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
PhoneAuthCredential PhoneAuthProvider::GetCredential(const char* verification_id,
                                                     const char* verification_code) {
  FIREBASE_ASSERT_RETURN(PhoneAuthCredential(), verification_id && verification_code);
  if (!data_ || !data_->objc_provider) {
    PhoneAuthCredential cred;
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = "PhoneAuthProvider is not initialized.";
    return cred;
  }
  if (strlen(verification_id) == 0) {
    PhoneAuthCredential cred;
    cred.error_code_ = kAuthErrorMissingVerificationId;
    cred.error_message_ = "A verification ID must be provided.";
    return cred;
  }
  if (strlen(verification_code) == 0) {
    PhoneAuthCredential cred;
    cred.error_code_ = kAuthErrorMissingVerificationCode;
    cred.error_message_ = "A verification code must be provided.";
    return cred;
  }
  FIRPhoneAuthCredential* credential = nil;
  std::string error_message = "Invalid phone credential.";
  @try {
    credential = [data_->objc_provider
        credentialWithVerificationID:util::CStringToNSString(verification_id)
                    verificationCode:util::CStringToNSString(verification_code)];
  } @catch (NSException* exception) {
    error_message = util::StringFromNSString(exception.reason);
  }
  PhoneAuthCredential cred(credential ? new FIRAuthCredentialPointer(credential) : nullptr);
  if (!credential) {
    cred.error_code_ = kAuthErrorInvalidCredential;
    cred.error_message_ = error_message;
  }
  return cred;
}

#else
// non-iOS Apple platforms (eg: tvOS)
PhoneAuthCredential PhoneAuthProvider::GetCredential(const char* verification_id,
                                                     const char* verification_code) {
  (void)verification_id;
  (void)verification_code;
  PhoneAuthCredential cred(nullptr);
  cred.error_code_ = kAuthErrorApiNotAvailable;
  cred.error_message_ = "Phone Auth is not supported on non-iOS Apple platforms (e.g. tvOS).";
  return cred;
}
#endif  // FIREBASE_PLATFORM_IOS

#if FIREBASE_PLATFORM_IOS
// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  static PhoneAuthProvider kInvalidPhoneAuthProvider;
  FIREBASE_ASSERT_RETURN(kInvalidPhoneAuthProvider,
                         auth != nullptr && auth->auth_data_ != nullptr);
  MutexLock lock(auth->auth_data_->future_impl.mutex());
  PhoneAuthProvider& provider = auth->auth_data_->phone_auth_provider;
  if (provider.data_ == nullptr) {
    FIRPhoneAuthProvider* objc_provider =
        [FIRPhoneAuthProvider providerWithAuth:AuthImpl(auth->auth_data_)];

    // Create the implementation class that holds the wrapped FIRPhoneAuthProvider.
    provider.data_ = new PhoneAuthProviderData(objc_provider);
  }
  return provider;
}
#else   // non-iOS Apple platforms (eg: tvOS)
// static
PhoneAuthProvider& PhoneAuthProvider::GetInstance(Auth* auth) {
  static PhoneAuthProvider kInvalidPhoneAuthProvider;
  FIREBASE_ASSERT_RETURN(kInvalidPhoneAuthProvider,
                         auth != nullptr && auth->auth_data_ != nullptr);
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

#ifdef INTERNAL_EXPERIMENTAL
void FederatedOAuthProvider::SetAuthHandler(AuthHandler* handler) { handler_ = handler; }
#endif  // INTERNAL_EXPERIMENTAL

Future<AuthResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  if (!auth_data) return Future<AuthResult>();
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kAuthFn_SignInWithProvider, AuthResult());
  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId, "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }
#if FIREBASE_PLATFORM_IOS
  FIROAuthProvider* ios_provider = nil;
  @try {
    ios_provider = (FIROAuthProvider*)[FIROAuthProvider
        providerWithProviderID:util::CStringToNSString(provider_data_.provider_id.c_str())
                          auth:AuthImpl(auth_data)];
  } @catch (NSException* exception) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     util::StringFromNSString(exception.reason).c_str());
    return MakeFuture(&futures, handle);
  }
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    FIRCPPAuthListenerHandle* listener_cpp_handle =
        reinterpret_cast<AuthDataIos*>(auth_data->auth_impl)->listener_handle.get();
    [AuthImpl(auth_data)
        signInWithProvider:ios_provider
                UIDelegate:nullptr
                completion:^(FIRAuthDataResult* _Nullable auth_result, NSError* _Nullable error) {
                  @synchronized(listener_cpp_handle) {
                    AuthData* data = listener_cpp_handle.authData;
                    if (!data) return;
                    AuthResultWithProviderCallback(auth_result, error, handle, data, ios_provider);
                  }
                }];
    return MakeFuture(&futures, handle);
  } else {
    Future<AuthResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                               AuthResult());
    return future;
  }

#else   // non-iOS Apple platforms (eg: tvOS)
  Future<AuthResult> future = MakeFuture(&futures, handle);
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "OAuth provider sign-in is not supported on non-iOS Apple platforms.");
  return future;
#endif  // FIREBASE_PLATFORM_IOS
}

Future<AuthResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  auto handle = futures.SafeAlloc<AuthResult>(kUserFn_LinkWithProvider, AuthResult());
  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId, "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }
#if FIREBASE_PLATFORM_IOS
  FIROAuthProvider* ios_provider = nil;
  @try {
    ios_provider = (FIROAuthProvider*)[FIROAuthProvider
        providerWithProviderID:util::CStringToNSString(provider_data_.provider_id.c_str())
                          auth:AuthImpl(auth_data)];
  } @catch (NSException* exception) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     util::StringFromNSString(exception.reason).c_str());
    return MakeFuture(&futures, handle);
  }
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    FIRCPPAuthListenerHandle* listener_cpp_handle =
        reinterpret_cast<AuthDataIos*>(auth_data->auth_impl)->listener_handle.get();
    [UserImpl(auth_data)
        linkWithProvider:ios_provider
              UIDelegate:nullptr
              completion:^(FIRAuthDataResult* _Nullable auth_result, NSError* _Nullable error) {
                @synchronized(listener_cpp_handle) {
                  AuthData* data = listener_cpp_handle.authData;
                  if (!data) return;
                  AuthResultWithProviderCallback(auth_result, error, handle, data, ios_provider);
                }
              }];
    return MakeFuture(&futures, handle);
  } else {
    Future<AuthResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                               AuthResult());
    return future;
  }

#else   // non-iOS Apple platforms (eg: tvOS)
  Future<AuthResult> future = MakeFuture(&futures, handle);
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "OAuth provider linking is not supported on non-iOS Apple platforms.");
  return future;
#endif  // FIREBASE_PLATFORM_IOS
}

Future<AuthResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  if (!ValidUser(auth_data)) return Future<AuthResult>();
  ReferenceCountedFutureImpl& futures = auth_data->future_impl;
  auto handle = futures.SafeAlloc<AuthResult>(kUserFn_ReauthenticateWithProvider, AuthResult());
  if (provider_data_.provider_id.empty()) {
    futures.Complete(handle, kAuthErrorInvalidProviderId, "Invalid provider ID.");
    return MakeFuture(&futures, handle);
  }
#if FIREBASE_PLATFORM_IOS
  FIROAuthProvider* ios_provider = nil;
  @try {
    ios_provider = (FIROAuthProvider*)[FIROAuthProvider
        providerWithProviderID:util::CStringToNSString(provider_data_.provider_id.c_str())
                          auth:AuthImpl(auth_data)];
  } @catch (NSException* exception) {
    futures.Complete(handle, kAuthErrorInvalidProviderId,
                     util::StringFromNSString(exception.reason).c_str());
    return MakeFuture(&futures, handle);
  }
  if (ios_provider != nullptr) {
    ios_provider.customParameters = util::StringMapToNSDictionary(provider_data_.custom_parameters);
    ios_provider.scopes = util::StringVectorToNSMutableArray(provider_data_.scopes);
    FIRCPPAuthListenerHandle* listener_cpp_handle =
        reinterpret_cast<AuthDataIos*>(auth_data->auth_impl)->listener_handle.get();
    [UserImpl(auth_data)
        reauthenticateWithProvider:ios_provider
                        UIDelegate:nullptr
                        completion:^(FIRAuthDataResult* _Nullable auth_result,
                                     NSError* _Nullable error) {
                          @synchronized(listener_cpp_handle) {
                            AuthData* data = listener_cpp_handle.authData;
                            if (!data) return;
                            AuthResultWithProviderCallback(auth_result, error, handle, data,
                                                           ios_provider);
                          }
                        }];
    return MakeFuture(&futures, handle);
  } else {
    Future<AuthResult> future = MakeFuture(&futures, handle);
    futures.CompleteWithResult(handle, kAuthErrorFailure, "Internal error constructing provider.",
                               AuthResult());
    return future;
  }

#else   // non-iOS Apple platforms (eg: tvOS)
  Future<AuthResult> future = MakeFuture(&futures, handle);
  futures.Complete(handle, kAuthErrorApiNotAvailable,
                   "OAuth reauthentication is not supported on non-iOS Apple platforms.");
  return future;
#endif  // FIREBASE_PLATFORM_IOS
}

}  // namespace auth
}  // namespace firebase
