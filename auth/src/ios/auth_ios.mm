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

#import "FIRAdditionalUserInfo.h"
#import "FIRAuthDataResult.h"
#import "FIRAuthErrors.h"
#import "FIROptions.h"
#import "FIROAuthCredential.h"

#include "app/src/app_ios.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "auth/src/ios/common_ios.h"

// Wraps AuthData with an Obj-C object so that it's possible to remove
// the authData from any blocks that reference this handle.
@interface FIRCPPAuthListenerHandle : NSObject
@property(atomic, nullable) firebase::auth::AuthData *authData;
@end

@implementation FIRCPPAuthListenerHandle
@end

namespace firebase {
namespace auth {

static const struct {
  int ios_error;
  AuthError cpp_error;
} kIosToCppErrorMap[] = {
    {FIRAuthErrorCodeAccountExistsWithDifferentCredential,
     kAuthErrorAccountExistsWithDifferentCredentials},
    {FIRAuthErrorCodeAppNotAuthorized, kAuthErrorAppNotAuthorized},
    {FIRAuthErrorCodeAppNotVerified, kAuthErrorAppNotVerified},
    {FIRAuthErrorCodeAppVerificationUserInteractionFailure, kAuthErrorAppVerificationFailed},
    {FIRAuthErrorCodeCaptchaCheckFailed, kAuthErrorCaptchaCheckFailed},
    {FIRAuthErrorCodeCredentialAlreadyInUse, kAuthErrorCredentialAlreadyInUse},
    {FIRAuthErrorCodeCustomTokenMismatch, kAuthErrorCustomTokenMismatch},
    {FIRAuthErrorCodeEmailAlreadyInUse, kAuthErrorEmailAlreadyInUse},
    {FIRAuthErrorCodeExpiredActionCode, kAuthErrorExpiredActionCode},
    {FIRAuthErrorCodeInternalError, kAuthErrorFailure},
    {FIRAuthErrorCodeInvalidActionCode, kAuthErrorInvalidActionCode},
    {FIRAuthErrorCodeInvalidAPIKey, kAuthErrorInvalidApiKey},
    {FIRAuthErrorCodeInvalidAppCredential, kAuthErrorInvalidAppCredential},
    {FIRAuthErrorCodeInvalidClientID, kAuthErrorInvalidClientId},
    {FIRAuthErrorCodeInvalidContinueURI, kAuthErrorInvalidContinueUri},
    {FIRAuthErrorCodeInvalidCredential, kAuthErrorInvalidCredential},
    {FIRAuthErrorCodeInvalidCustomToken, kAuthErrorInvalidCustomToken},
    {FIRAuthErrorCodeInvalidEmail, kAuthErrorInvalidEmail},
    {FIRAuthErrorCodeInvalidMessagePayload, kAuthErrorInvalidMessagePayload},
    {FIRAuthErrorCodeInvalidPhoneNumber, kAuthErrorInvalidPhoneNumber},
    {FIRAuthErrorCodeInvalidRecipientEmail, kAuthErrorInvalidRecipientEmail},
    {FIRAuthErrorCodeInvalidSender, kAuthErrorInvalidSender},
    {FIRAuthErrorCodeInvalidUserToken, kAuthErrorInvalidUserToken},
    {FIRAuthErrorCodeInvalidVerificationCode, kAuthErrorInvalidVerificationCode},
    {FIRAuthErrorCodeInvalidVerificationID, kAuthErrorInvalidVerificationId},
    {FIRAuthErrorCodeKeychainError, kAuthErrorKeychainError},
    {FIRAuthErrorCodeMissingAppCredential, kAuthErrorMissingAppCredential},
    {FIRAuthErrorCodeMissingAppToken, kAuthErrorMissingAppToken},
    {FIRAuthErrorCodeMissingContinueURI, kAuthErrorMissingContinueUri},
    {FIRAuthErrorCodeMissingEmail, kAuthErrorMissingEmail},
    {FIRAuthErrorCodeMissingIosBundleID, kAuthErrorMissingIosBundleId},
    {FIRAuthErrorCodeMissingPhoneNumber, kAuthErrorMissingPhoneNumber},
    {FIRAuthErrorCodeMissingVerificationCode, kAuthErrorMissingVerificationCode},
    {FIRAuthErrorCodeMissingVerificationID, kAuthErrorMissingVerificationId},
    {FIRAuthErrorCodeNetworkError, kAuthErrorNetworkRequestFailed},
    {FIRAuthErrorCodeNoSuchProvider, kAuthErrorNoSuchProvider},
    {FIRAuthErrorCodeNotificationNotForwarded, kAuthErrorNotificationNotForwarded},
    {FIRAuthErrorCodeOperationNotAllowed, kAuthErrorOperationNotAllowed},
    {FIRAuthErrorCodeProviderAlreadyLinked, kAuthErrorProviderAlreadyLinked},
    {FIRAuthErrorCodeQuotaExceeded, kAuthErrorQuotaExceeded},
    {FIRAuthErrorCodeRequiresRecentLogin, kAuthErrorRequiresRecentLogin},
    {FIRAuthErrorCodeSessionExpired, kAuthErrorSessionExpired},
    {FIRAuthErrorCodeTooManyRequests, kAuthErrorTooManyRequests},
    {FIRAuthErrorCodeUnauthorizedDomain, kAuthErrorUnauthorizedDomain},
    {FIRAuthErrorCodeUserDisabled, kAuthErrorUserDisabled},
    {FIRAuthErrorCodeUserMismatch, kAuthErrorUserMismatch},
    {FIRAuthErrorCodeUserNotFound, kAuthErrorUserNotFound},
    {FIRAuthErrorCodeUserTokenExpired, kAuthErrorUserTokenExpired},
    {FIRAuthErrorCodeWeakPassword, kAuthErrorWeakPassword},
    {FIRAuthErrorCodeWebContextAlreadyPresented, kAuthErrorWebContextAlreadyPresented},
    {FIRAuthErrorCodeWebContextCancelled, kAuthErrorWebContextCancelled},
    {FIRAuthErrorCodeWrongPassword, kAuthErrorWrongPassword},
    {FIRAuthErrorCodeInvalidProviderID, kAuthErrorInvalidProviderId},
    {FIRAuthErrorCodeWebSignInUserInteractionFailure,
     kAuthErrorFederatedSignInUserInteractionFailure},
    // TODO(b/137141080): Handle New Thick Client Error
    // Uncomment once support for MISSING_CLIENT_IDENTIFIER is added to the iOS SDK.
    // {FIRAuthErrorCodeMissingClientIdentifier, kAuthErrorMissingClientIdentifier},
    // TODO(b/139495259): Handle MFA errors
    // Uncomment once support for MFA errors is added to the iOS SDK.
    // {FIRAuthErrorCodeMissingMultiFactorSession, kAuthErrorMissingMultiFactorSession},
    // {FIRAuthErrorCodeMissingMultiFactorInfo, kAuthErrorMissingMultiFactorInfo},
    // {FIRAuthErrorCodeInvalidMultiFactorSession, kAuthErrorInvalidMultiFactorSession},
    // {FIRAuthErrorCodeMultiFactorInfoNotFound, kAuthErrorMultiFactorInfoNotFound},
    // {FIRAuthErrorCodeAdminRestrictedOperation, kAuthErrorAdminRestrictedOperation},
    // {FIRAuthErrorCodeUnverifiedEmail, kAuthErrorUnverifiedEmail},
    // {FIRAuthErrorCodeSecondFactorAlreadyEnrolled, kAuthErrorSecondFactorAlreadyEnrolled},
    // {FIRAuthErrorCodeMaximumSecondFactorCountExceeded,
    // kAuthErrorMaximumSecondFactorCountExceeded}, {FIRAuthErrorCodeUnsupportedFirstFactor,
    // kAuthErrorUnsupportedFirstFactor}, {FIRAuthErrorCodeEmailChangeNeedsVerification,
    // kAuthErrorEmailChangeNeedsVerification},
};

// A provider that wraps a Credential returned from the Firebase iOS SDK which may occur when a
// nonce based link failure occurs due to a pre-existing account already linked with a Provider.
// This credential may be used to attempt to sign into Firebase using that account.
class ServiceUpdatedCredentialProvider {
 public:
  // Construct a Credential given a preexisting FIRAuthCredential wrapped by a
  // FIRAuthCredentialPointer.
  static Credential GetCredential(FIRAuthCredentialPointer* impl) {
    return Credential(impl);
  }
};

template<typename T>
struct ListenerHandleHolder {
  explicit ListenerHandleHolder(T handle) : handle(handle) {}
  T handle;
};

// Platform-specific method to create the wrapped Auth class.
void *CreatePlatformAuth(App *app) {
  // Grab the auth for our app.
  FIRAuth *auth = [FIRAuth authWithApp:app->GetPlatformApp()];
  FIREBASE_ASSERT(auth != nullptr);

  // Create a FIRAuth* that uses Objective-C's automatic reference counting.
  AuthDataIos *auth_impl = new AuthDataIos();
  auth_impl->fir_auth = auth;
  return auth_impl;
}

// Grab the user value from the iOS API and remember it locally.
void UpdateCurrentUser(AuthData* auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  FIRUser *user = [AuthImpl(auth_data) currentUser];
  SetUserImpl(auth_data, user);
}

// Platform-specific method to initialize AuthData.
void Auth::InitPlatformAuth(AuthData *auth_data) {
  FIRCPPAuthListenerHandle* listener_cpp_handle =
      [[FIRCPPAuthListenerHandle alloc] init];
  listener_cpp_handle.authData = auth_data;
  reinterpret_cast<AuthDataIos*>(auth_data->auth_impl)->listener_handle = listener_cpp_handle;
  // Register a listening block that will notify all the C++ listeners on
  // auth state change.
  FIRAuthStateDidChangeListenerHandle listener_handle =
      [AuthImpl(auth_data)
          addAuthStateDidChangeListener:^(FIRAuth * _Nonnull __strong,
                                          FIRUser * _Nullable __strong) {
            @synchronized (listener_cpp_handle) {
              AuthData* data = listener_cpp_handle.authData;
              if (data) {
                UpdateCurrentUser(data);
                NotifyAuthStateListeners(data);
              }
            }
          }];
  FIRIDTokenDidChangeListenerHandle id_token_listener_handle =
      [AuthImpl(auth_data)
          addIDTokenDidChangeListener:^(FIRAuth * _Nonnull __strong,
                                        FIRUser * _Nullable __strong) {
            @synchronized (listener_cpp_handle) {
              AuthData* data = listener_cpp_handle.authData;
              if (data) {
                UpdateCurrentUser(data);
                NotifyIdTokenListeners(data);
              }
            }
          }];

  auth_data->listener_impl =
      new ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle>(listener_handle);
  auth_data->id_token_listener_impl =
      new ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle>(id_token_listener_handle);

  // Ensure initial user value is correct.
  // It's possible for the user to be signed-in at creation, if the user signed-in during a
  // previous run, for example.
  UpdateCurrentUser(auth_data);
}

// Platform-specific method to destroy the wrapped Auth class.
void Auth::DestroyPlatformAuth(AuthData *auth_data) {
  // Remove references from listener blocks.
  AuthDataIos* auth_data_ios = reinterpret_cast<AuthDataIos *>(auth_data->auth_impl);
  FIRCPPAuthListenerHandle *listener_cpp_handle = auth_data_ios->listener_handle.get();
  @synchronized (listener_cpp_handle) {
    listener_cpp_handle.authData = nullptr;
  }

  // Unregister the listeners.
  ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle>* handle_holder =
      reinterpret_cast<
          ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle>*>(auth_data->listener_impl);
  [AuthImpl(auth_data) removeAuthStateDidChangeListener:handle_holder->handle];
  delete handle_holder;
  auth_data->listener_impl = nullptr;
  ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle>* id_token_listener_handle_holder =
      reinterpret_cast<
          ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle>*>(
              auth_data->id_token_listener_impl);
  [AuthImpl(auth_data) removeIDTokenDidChangeListener:id_token_listener_handle_holder->handle];
  delete id_token_listener_handle_holder;
  auth_data->id_token_listener_impl = nullptr;

  SetUserImpl(auth_data, nullptr);

  // Release the FIRAuth* that we allocated in CreatePlatformAuth().
  delete auth_data_ios;
  auth_data->auth_impl = nullptr;
}

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(const char *email) {
  // Create data structure to hold asynchronous results.
  FetchProvidersResult initial_data;

  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<FetchProvidersResult>(kAuthFn_FetchProvidersForEmail, initial_data);

  [AuthImpl(auth_data_)
      fetchSignInMethodsForEmail:@(email)
                      completion:^(NSArray<NSString *> *_Nullable providers,
                                   NSError *_Nullable error) {
      futures.Complete<FetchProvidersResult>(
          handle, AuthErrorFromNSError(error),
          [error.localizedDescription UTF8String],
          [providers](FetchProvidersResult* data) {
            // Copy data to our result format.
            data->providers.resize(providers.count);
            for (size_t i = 0; i < providers.count; ++i) {
              data->providers[i] = util::StringFromNSString(providers[i]);
            }
          });
    }];

  return MakeFuture(&futures, handle);
}

// It's safe to return a direct pointer to `current_user` because that class
// holds nothing but a pointer to AuthData, which never changes.
// All User functions that require synchronization go through AuthData's mutex.
User *Auth::current_user() {
  if (!auth_data_) return nullptr;
  MutexLock lock(auth_data_->future_impl.mutex());

  // auth_data_->current_user should be available after Auth is created because
  // [AuthImpl(auth_data) currentUser] is called during Auth::InitPlatformAuth()
  // and it would block until persistence is loaded.
  User *user = auth_data_->user_impl == nullptr ? nullptr : &auth_data_->current_user;
  return user;
}

static User* AssignUser(FIRUser *_Nullable user, AuthData *auth_data) {
  // Update our pointer to the iOS user that we're wrapping.
  MutexLock lock(auth_data->future_impl.mutex());
  if (user) {
    SetUserImpl(auth_data, user);
  }

  // If the returned `user` is non-null then the current user is active.
  return auth_data->user_impl == nullptr ? nullptr : &auth_data->current_user;
}

AuthError AuthErrorFromNSError(NSError *_Nullable error) {
  if (!error || error.code == 0) {
    return kAuthErrorNone;
  }
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kIosToCppErrorMap); i++) {
    if (error.code == kIosToCppErrorMap[i].ios_error) return kIosToCppErrorMap[i].cpp_error;
  }
  return kAuthErrorUnimplemented;
}

void SignInCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                    SafeFutureHandle<User*> handle, AuthData *auth_data) {
  User* result = AssignUser(user, auth_data);

  // Finish off the asynchronous call so that the caller can read it.
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  futures.CompleteWithResult(handle, AuthErrorFromNSError(error),
                             util::NSStringToString(error.localizedDescription).c_str(),
                             result);
}

void SignInResultWithProviderCallback(
    FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
    SafeFutureHandle<SignInResult> handle, AuthData *_Nonnull auth_data,
    const FIROAuthProvider *_Nonnull ios_auth_provider /*unused */) {
  // ios_auth_provider exists as a parameter to hold a reference to the FIROAuthProvider preventing
  // its release by the Objective-C runtime during the asynchronous SignIn operation.
  error = RemapBadProviderIDErrors(error);
  SignInResultCallback(auth_result, error, handle, auth_data);
}

void SignInResultCallback(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
                          SafeFutureHandle<SignInResult> handle, AuthData *auth_data) {
  User* user = AssignUser(auth_result.user, auth_data);

  SignInResult result;
  result.user = user;

  result.info.provider_id = util::StringFromNSString(auth_result.additionalUserInfo.providerID);
  // username and profile are nullable, and so need to be checked for.
  result.info.user_name = util::StringFromNSString(auth_result.additionalUserInfo.username);
  if (auth_result.additionalUserInfo.profile) {
    util::NSDictionaryToStdMap(auth_result.additionalUserInfo.profile, &result.info.profile);
  }

  if (error.userInfo != nullptr) {
    if (error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey] != nullptr) {
      result.info.updated_credential = ServiceUpdatedCredentialProvider::GetCredential(
          new FIRAuthCredentialPointer(error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey]));
     }
  }

  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  futures.CompleteWithResult(handle, AuthErrorFromNSError(error),
                             util::NSStringToString(error.localizedDescription).c_str(), result);
}

Future<User *> Auth::SignInWithCustomToken(const char *token) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User *>(kAuthFn_SignInWithCustomToken, nullptr);

  [AuthImpl(auth_data_)
      signInWithCustomToken:@(token)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   SignInCallback(auth_result.user, error, handle, auth_data_);
                 }];

  return MakeFuture(&futures, handle);
}

Future<User *> Auth::SignInWithCredential(const Credential &credential) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User *>(kAuthFn_SignInWithCredential, nullptr);

  [AuthImpl(auth_data_)
      signInWithCredential:CredentialFromImpl(credential.impl_)
                completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                  SignInCallback(auth_result.user, error, handle, auth_data_);
                }];

  return MakeFuture(&futures, handle);
}

Future<SignInResult> Auth::SignInAndRetrieveDataWithCredential(
    const Credential& credential) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<SignInResult>(kAuthFn_SignInAndRetrieveDataWithCredential, SignInResult());

  [AuthImpl(auth_data_) signInWithCredential:CredentialFromImpl(credential.impl_)
                                  completion:^(FIRAuthDataResult *_Nullable auth_result,
                                               NSError *_Nullable error) {
      SignInResultCallback(auth_result, error, handle, auth_data_);
    }];

  return MakeFuture(&futures, handle);
}

Future<SignInResult> Auth::SignInWithProvider(FederatedAuthProvider* provider) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), provider);
  return provider->SignIn(auth_data_);
}

Future<User *> Auth::SignInAnonymously() {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = auth_data_->future_impl.SafeAlloc<User *>(kAuthFn_SignInAnonymously, nullptr);

  [AuthImpl(auth_data_) signInAnonymouslyWithCompletion:^(FIRAuthDataResult *_Nullable auth_result,
                                                          NSError *_Nullable error) {
    SignInCallback(auth_result.user, error, handle, auth_data_);
  }];

  return MakeFuture(&futures, handle);
}

Future<User *> Auth::SignInWithEmailAndPassword(const char *email, const char *password) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User *>(kAuthFn_SignInWithEmailAndPassword, nullptr);
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
  } else if (!password || strlen(password) == 0) {
    futures.Complete(handle, kAuthErrorMissingPassword, "Empty password is not allowed.");
  } else {
    [AuthImpl(auth_data_)
        signInWithEmail:@(email)
               password:@(password)
             completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
               SignInCallback(auth_result.user, error, handle, auth_data_);
             }];
  }
  return MakeFuture(&futures, handle);
}

Future<User *> Auth::CreateUserWithEmailAndPassword(const char *email, const char *password) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<User *>(kAuthFn_CreateUserWithEmailAndPassword, nullptr);
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
  } else if (!password || strlen(password) == 0) {
    futures.Complete(handle, kAuthErrorMissingPassword, "Empty password is not allowed.");
  } else {
    [AuthImpl(auth_data_)
        createUserWithEmail:@(email)
                   password:@(password)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   SignInCallback(auth_result.user, error, handle, auth_data_);
                 }];
  }
  return MakeFuture(&futures, handle);
}

void Auth::SignOut() {
  // TODO(jsanmiya): Verify with iOS team why this returns an error.
  NSError *_Nullable error;
  [AuthImpl(auth_data_) signOut:&error];
  SetUserImpl(auth_data_, NULL);
}

Future<void> Auth::SendPasswordResetEmail(const char *email) {
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kAuthFn_SendPasswordResetEmail);

  [AuthImpl(auth_data_) sendPasswordResetWithEmail:@(email)
                                        completion:^(NSError *_Nullable error) {
                                            futures.Complete(
                                               handle, AuthErrorFromNSError(error),
                                               [error.localizedDescription UTF8String]);
                                          }];

  return MakeFuture(&futures, handle);
}

// Remap iOS SDK errors reported by the UIDelegate. While these errors seem like
// user interaction errors, they are actually caused by bad provider ids.
NSError* RemapBadProviderIDErrors(NSError* _Nonnull error) {
  if (error.code == FIRAuthErrorCodeWebSignInUserInteractionFailure &&
      [error.domain isEqualToString:@"FIRAuthErrorDomain"]) {
    return [[NSError alloc] initWithDomain:error.domain
                                      code:FIRAuthErrorCodeInvalidProviderID
                                  userInfo:error.userInfo];
  }
  return error;
}

// Not implemented for iOS.
void EnableTokenAutoRefresh(AuthData *auth_data) {}
void DisableTokenAutoRefresh(AuthData *auth_data) {}
void InitializeTokenRefresher(AuthData *auth_data) {}
void DestroyTokenRefresher(AuthData *auth_data) {}

}  // namespace auth
}  // namespace firebase
