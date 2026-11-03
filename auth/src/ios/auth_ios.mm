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

#import <UIKit/UIKit.h>

#import "FIRAuthErrors.h"
#import "FIROptions.h"
#import "FirebaseAuthInterop/FIRAuthInterop.h"
// This needs to be after the FIRAuthInterop import
#import "FirebaseAuth-Swift.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "app/src/app_ios.h"
#include "app/src/assert.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_ios.h"
#include "auth/src/ios/common_ios.h"

@implementation FIRCPPAuthListenerHandle
@end

namespace firebase {
namespace auth {

const char *const kEmulatorLocalHost = "localhost";
const char *const kEmulatorPort = "9099";

static const struct {
  NSInteger ios_error;
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
    {FIRAuthErrorCodeMissingClientIdentifier, kAuthErrorMissingClientIdentifier},
    {FIRAuthErrorCodeMissingMultiFactorSession, kAuthErrorMissingMultiFactorSession},
    {FIRAuthErrorCodeMissingMultiFactorInfo, kAuthErrorMissingMultiFactorInfo},
    {FIRAuthErrorCodeInvalidMultiFactorSession, kAuthErrorInvalidMultiFactorSession},
    {FIRAuthErrorCodeMultiFactorInfoNotFound, kAuthErrorMultiFactorInfoNotFound},
    {FIRAuthErrorCodeAdminRestrictedOperation, kAuthErrorAdminRestrictedOperation},
    {FIRAuthErrorCodeUnverifiedEmail, kAuthErrorUnverifiedEmail},
    {FIRAuthErrorCodeSecondFactorAlreadyEnrolled, kAuthErrorSecondFactorAlreadyEnrolled},
    {FIRAuthErrorCodeMaximumSecondFactorCountExceeded,
     kAuthErrorMaximumSecondFactorCountExceeded},
    {FIRAuthErrorCodeUnsupportedFirstFactor, kAuthErrorUnsupportedFirstFactor},
    {FIRAuthErrorCodeEmailChangeNeedsVerification, kAuthErrorEmailChangeNeedsVerification},
};

// A provider that wraps a Credential returned from the Firebase iOS SDK which may occur when a
// nonce based link failure occurs due to a pre-existing account already linked with a Provider.
// This credential may be used to attempt to sign into Firebase using that account.
class ServiceUpdatedCredentialProvider {
 public:
  // Construct a Credential given a preexisting FIRAuthCredential wrapped by a
  // FIRAuthCredentialPointer.
  static Credential GetCredential(FIRAuthCredentialPointer *impl) { return Credential(impl); }
};

template <typename T>
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
void UpdateCurrentUser(AuthData *auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  FIRUser *user = [AuthImpl(auth_data) currentUser];
  SetUserImpl(auth_data, user);
}

void SetEmulatorIos(AuthData *auth_data, const char *host, uint32_t port) {
  if (!auth_data || !host || strlen(host) == 0 || port == 0 || port > 65535) {
    return;
  }
  NSString *ns_host = util::CStringToNSString(host);
  if (!ns_host) return;
  NSInteger ns_port = static_cast<NSInteger>(port);
  [AuthImpl(auth_data) useEmulatorWithHost:ns_host port:ns_port];
}

void CheckEmulator(AuthData *auth_data) {
  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_AUTH_EMULATOR") == nullptr) {
    LogInfo("Using Auth Prod for testing.");
    return;
  }
  LogInfo("Using Auth Emulator.");
  // Use AUTH_EMULATOR_PORT if it is set to non empty string,
  // otherwise use the default port.
  uint32_t port = static_cast<uint32_t>(std::strtoul(kEmulatorPort, nullptr, 10));
  const char *env_port = std::getenv("AUTH_EMULATOR_PORT");
  if (env_port != nullptr && env_port[0] != '\0') {
    char *endptr = nullptr;
    unsigned long parsed = std::strtoul(env_port, &endptr, 10);
    if (endptr != env_port && *endptr == '\0' && parsed > 0 && parsed <= 65535) {
      port = static_cast<uint32_t>(parsed);
    }
  }

  SetEmulatorIos(auth_data, kEmulatorLocalHost, port);
}

// Platform-specific method to initialize AuthData.
void Auth::InitPlatformAuth(AuthData *auth_data) {
  // Ensure initial user value and emulator configuration are set before
  // registering listeners.
  UpdateCurrentUser(auth_data);
  CheckEmulator(auth_data);

  FIRCPPAuthListenerHandle *listener_cpp_handle = [[FIRCPPAuthListenerHandle alloc] init];
  listener_cpp_handle.authData = auth_data;
  reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle = listener_cpp_handle;
  // Register a listening block that will notify all the C++ listeners on
  // auth state change.
  FIRAuthStateDidChangeListenerHandle listener_handle = [AuthImpl(auth_data)
      addAuthStateDidChangeListener:^(FIRAuth *_Nonnull __strong, FIRUser *_Nullable __strong) {
        @synchronized(listener_cpp_handle) {
          AuthData *data = listener_cpp_handle.authData;
          if (data) {
            UpdateCurrentUser(data);
            NotifyAuthStateListeners(data);
          }
        }
      }];
  FIRIDTokenDidChangeListenerHandle id_token_listener_handle = [AuthImpl(auth_data)
      addIDTokenDidChangeListener:^(FIRAuth *_Nonnull __strong, FIRUser *_Nullable __strong) {
        @synchronized(listener_cpp_handle) {
          AuthData *data = listener_cpp_handle.authData;
          if (data) {
            UpdateCurrentUser(data);
            {
              MutexLock lock(data->future_impl.mutex());
              data->SetExpectIdTokenListenerCallback(false);
            }
            NotifyIdTokenListeners(data);
          }
        }
      }];

  auth_data->listener_impl =
      new ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle>(listener_handle);
  auth_data->id_token_listener_impl =
      new ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle>(id_token_listener_handle);
}

// Platform-specific method to destroy the wrapped Auth class.
void Auth::DestroyPlatformAuth(AuthData *auth_data) {
  // Remove references from listener blocks.
  AuthDataIos *auth_data_ios = reinterpret_cast<AuthDataIos *>(auth_data->auth_impl);
  FIRCPPAuthListenerHandle *listener_cpp_handle = auth_data_ios->listener_handle.get();
  @synchronized(listener_cpp_handle) {
    listener_cpp_handle.authData = nullptr;
  }

  // Unregister the listeners.
  ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle> *handle_holder =
      reinterpret_cast<ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle> *>(
          auth_data->listener_impl);
  [AuthImpl(auth_data) removeAuthStateDidChangeListener:handle_holder->handle];
  delete handle_holder;
  auth_data->listener_impl = nullptr;
  ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle> *id_token_listener_handle_holder =
      reinterpret_cast<ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle> *>(
          auth_data->id_token_listener_impl);
  [AuthImpl(auth_data) removeIDTokenDidChangeListener:id_token_listener_handle_holder->handle];
  delete id_token_listener_handle_holder;
  auth_data->id_token_listener_impl = nullptr;

  SetUserImpl(auth_data, nullptr);

  // Release the FIRAuth* that we allocated in CreatePlatformAuth().
  delete auth_data_ios;
  auth_data->auth_impl = nullptr;
}

void LogHeartbeat(Auth *auth) { (void)auth; }

Future<Auth::FetchProvidersResult> Auth::FetchProvidersForEmail(const char *email) {
  if (!auth_data_) return Future<FetchProvidersResult>();
  // Create data structure to hold asynchronous results.
  FetchProvidersResult initial_data;

  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<FetchProvidersResult>(kAuthFn_FetchProvidersForEmail, initial_data);
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
    return MakeFuture(&futures, handle);
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_)
      fetchSignInMethodsForEmail:@(email)
                      completion:^(NSArray<NSString *> *_Nullable providers,
                                   NSError *_Nullable error) {
                        @synchronized(listener_cpp_handle) {
                          AuthData *data = listener_cpp_handle.authData;
                          if (!data) return;
                          data->future_impl.Complete<FetchProvidersResult>(
                              handle, AuthErrorFromNSError(error),
                              ErrorMessageFromNSError(error).c_str(),
                              [providers](FetchProvidersResult *result_data) {
                                result_data->providers.clear();
                                if (providers != nil) {
                                  result_data->providers.resize(providers.count);
                                  for (size_t i = 0; i < providers.count; ++i) {
                                    result_data->providers[i] =
                                        util::StringFromNSString(providers[i]);
                                  }
                                }
                              });
                        }
                      }];

  return MakeFuture(&futures, handle);
}

User Auth::current_user() {
  if (!auth_data_) {
    // Return an invalid user.
    return User(nullptr);
  }
  MutexLock lock(auth_data_->future_impl.mutex());

  if (auth_data_->user_impl == nullptr) {
    // Return an invalid user.
    return User(nullptr);
  }
  return auth_data_->current_user;
}

static User *AssignUser(FIRUser *_Nullable user, AuthData *auth_data) {
  if (!user) {
    return nullptr;
  }
  // Update our pointer to the iOS user that we're wrapping.
  MutexLock lock(auth_data->future_impl.mutex());
  SetUserImpl(auth_data, user);

  // If the returned `user` is non-null then the current user is active.
  return auth_data->user_impl == nullptr ? nullptr : &auth_data->current_user;
}

std::string Auth::language_code() const {
  if (!auth_data_) return "";
  NSString *language_code = [AuthImpl(auth_data_) languageCode];
  if (language_code == nil) {
    return std::string();
  } else {
    return util::NSStringToString(language_code);
  }
}

void Auth::set_language_code(const char *language_code) {
  if (!auth_data_) return;
  if (language_code != nullptr && strlen(language_code) > 0) {
    AuthImpl(auth_data_).languageCode = util::CStringToNSString(language_code);
  } else {
    AuthImpl(auth_data_).languageCode = nil;
  }
}

void Auth::UseAppLanguage() {
  if (!auth_data_) return;
  [AuthImpl(auth_data_) useAppLanguage];
}

AuthError AuthErrorFromNSError(NSError *_Nullable error) {
  if (error == nil) {
    return kAuthErrorNone;
  }
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kIosToCppErrorMap); i++) {
    if (error.code == kIosToCppErrorMap[i].ios_error) return kIosToCppErrorMap[i].cpp_error;
  }
  return kAuthErrorFailure;
}

std::string ErrorMessageFromNSError(NSError *_Nullable error) {
  if (error == nil) {
    return std::string();
  }
  std::string error_string = util::StringFromNSString(error.localizedDescription);
  if (error.userInfo != nil && error.userInfo[NSLocalizedFailureReasonErrorKey] != nil) {
    error_string += ": ";
    error_string += util::StringFromNSString(error.userInfo[NSLocalizedFailureReasonErrorKey]);
  }
  return error_string;
}

void AuthResultCallback(FIRAuthDataResult *_Nullable fir_auth_result, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data) {
  User *current_user =
      (error == nil && fir_auth_result != nil) ? AssignUser(fir_auth_result.user, auth_data)
                                               : nullptr;

  AuthResult result;
  if (current_user != nullptr) {
    result.user = *current_user;
    if (fir_auth_result.additionalUserInfo != nil) {
      result.additional_user_info.provider_id =
          util::StringFromNSString(fir_auth_result.additionalUserInfo.providerID);
      result.additional_user_info.user_name =
          util::StringFromNSString(fir_auth_result.additionalUserInfo.username);
      result.additional_user_info.is_new_user = fir_auth_result.additionalUserInfo.newUser;
      if (fir_auth_result.additionalUserInfo.profile != nil) {
        util::NSDictionaryToStdMap(fir_auth_result.additionalUserInfo.profile,
                                   &result.additional_user_info.profile);
      }
    }
    if (fir_auth_result.credential != nil) {
      result.credential = InternalAuthResultProvider::GetCredential(fir_auth_result.credential);
    }
  }

  // If there is an error, and it has an updated credential, pass that along via the result.
  if (error != nil && error.userInfo != nil) {
    if (error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey] != nil) {
      result.additional_user_info.updated_credential =
          ServiceUpdatedCredentialProvider::GetCredential(new FIRAuthCredentialPointer(
              error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey]));
    }
  }

  std::string error_string = ErrorMessageFromNSError(error);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  futures.CompleteWithResult(handle, AuthErrorFromNSError(error), error_string.c_str(), result);
}

void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data) {
  User *current_user = (error == nil && user != nil) ? AssignUser(user, auth_data) : nullptr;
  AuthResult auth_result;
  if (current_user != nullptr) auth_result.user = *current_user;

  // If there is an error, and it has an updated credential, pass that along via the result.
  if (error != nil && error.userInfo != nil) {
    if (error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey] != nil) {
      auth_result.additional_user_info.updated_credential =
          ServiceUpdatedCredentialProvider::GetCredential(new FIRAuthCredentialPointer(
              error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey]));
    }
  }

  std::string error_string = ErrorMessageFromNSError(error);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  futures.CompleteWithResult(handle, AuthErrorFromNSError(error), error_string.c_str(),
                             auth_result);
}

void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<User> handle, AuthData *_Nonnull auth_data) {
  User *current_user = (error == nil && user != nil) ? AssignUser(user, auth_data) : nullptr;
  User user_result;
  if (current_user != nullptr) user_result = *current_user;

  std::string error_string = ErrorMessageFromNSError(error);
  ReferenceCountedFutureImpl &futures = auth_data->future_impl;
  futures.CompleteWithResult(handle, AuthErrorFromNSError(error), error_string.c_str(),
                             user_result);
}

void AuthResultWithProviderCallback(
    FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
    SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data,
    const FIROAuthProvider *_Nonnull ios_auth_provider /*unused */) {
  // ios_auth_provider exists as a parameter to hold a reference to the FIROAuthProvider preventing
  // its release by the Objective-C runtime during the asynchronous operations.
  error = RemapBadProviderIDErrors(error);
  AuthResultCallback(auth_result, error, handle, auth_data);
}

Future<AuthResult> Auth::SignInWithCustomToken(const char *token) {
  if (!auth_data_) return Future<AuthResult>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<AuthResult>(kAuthFn_SignInWithCustomToken, AuthResult());
  if (!token || strlen(token) == 0) {
    futures.Complete(handle, kAuthErrorInvalidCustomToken, "Empty custom token is not allowed.");
    return MakeFuture(&futures, handle);
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_)
      signInWithCustomToken:@(token)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   @synchronized(listener_cpp_handle) {
                     AuthData *data = listener_cpp_handle.authData;
                     if (!data) return;
                     AuthResultCallback(auth_result, error, handle, data);
                   }
                 }];

  return MakeFuture(&futures, handle);
}

Future<User> Auth::SignInWithCredential(const Credential &credential) {
  if (!auth_data_) return Future<User>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<User>(kAuthFn_SignInWithCredential, User());
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_, credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  }
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_)
      signInWithCredential:CredentialFromImpl(credential.impl_)
                completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                  @synchronized(listener_cpp_handle) {
                    AuthData *data = listener_cpp_handle.authData;
                    if (!data) return;
                    AuthResultCallback(auth_result.user, error, handle, data);
                  }
                }];

  return MakeFuture(&futures, handle);
}

Future<AuthResult> Auth::SignInAndRetrieveDataWithCredential(const Credential &credential) {
  if (!auth_data_) return Future<AuthResult>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kAuthFn_SignInAndRetrieveDataWithCredential, AuthResult());
  if (credential.error_code_ != kAuthErrorNone) {
    futures.Complete(handle, credential.error_code_, credential.error_message_.c_str());
    return MakeFuture(&futures, handle);
  }
  if (!credential.is_valid()) {
    futures.Complete(handle, kAuthErrorInvalidCredential, "Invalid credential is not allowed.");
    return MakeFuture(&futures, handle);
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_)
      signInWithCredential:CredentialFromImpl(credential.impl_)
                completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                  @synchronized(listener_cpp_handle) {
                    AuthData *data = listener_cpp_handle.authData;
                    if (!data) return;
                    AuthResultCallback(auth_result, error, handle, data);
                  }
                }];

  return MakeFuture(&futures, handle);
}

Future<AuthResult> Auth::SignInWithProvider(FederatedAuthProvider *provider) {
  if (!auth_data_) return Future<AuthResult>();
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->SignIn(auth_data_);
}

Future<AuthResult> Auth::SignInAnonymously() {
  if (!auth_data_) return Future<AuthResult>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      auth_data_->future_impl.SafeAlloc<AuthResult>(kAuthFn_SignInAnonymously, AuthResult());

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_) signInAnonymouslyWithCompletion:^(FIRAuthDataResult *_Nullable auth_result,
                                                          NSError *_Nullable error) {
    @synchronized(listener_cpp_handle) {
      AuthData *data = listener_cpp_handle.authData;
      if (!data) return;
      AuthResultCallback(auth_result, error, handle, data);
    }
  }];

  return MakeFuture(&futures, handle);
}

Future<AuthResult> Auth::SignInWithEmailAndPassword(const char *email, const char *password) {
  if (!auth_data_) return Future<AuthResult>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kAuthFn_SignInWithEmailAndPassword, AuthResult());
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
  } else if (!password || strlen(password) == 0) {
    futures.Complete(handle, kAuthErrorMissingPassword, "Empty password is not allowed.");
  } else {
    FIRCPPAuthListenerHandle *listener_cpp_handle =
        reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
    [AuthImpl(auth_data_)
        signInWithEmail:@(email)
               password:@(password)
             completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
               @synchronized(listener_cpp_handle) {
                 AuthData *data = listener_cpp_handle.authData;
                 if (!data) return;
                 AuthResultCallback(auth_result, error, handle, data);
               }
             }];
  }
  return MakeFuture(&futures, handle);
}

Future<AuthResult> Auth::CreateUserWithEmailAndPassword(const char *email, const char *password) {
  if (!auth_data_) return Future<AuthResult>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle =
      futures.SafeAlloc<AuthResult>(kAuthFn_CreateUserWithEmailAndPassword, AuthResult());
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
  } else if (!password || strlen(password) == 0) {
    futures.Complete(handle, kAuthErrorMissingPassword, "Empty password is not allowed.");
  } else {
    FIRCPPAuthListenerHandle *listener_cpp_handle =
        reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
    [AuthImpl(auth_data_)
        createUserWithEmail:@(email)
                   password:@(password)
                 completion:^(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error) {
                   @synchronized(listener_cpp_handle) {
                     AuthData *data = listener_cpp_handle.authData;
                     if (!data) return;
                     AuthResultCallback(auth_result, error, handle, data);
                   }
                 }];
  }
  return MakeFuture(&futures, handle);
}

void Auth::SignOut() {
  if (!auth_data_) return;
  NSError *_Nullable error = nil;
  if ([AuthImpl(auth_data_) signOut:&error]) {
    SetUserImpl(auth_data_, nullptr);
  } else if (error != nil) {
    LogError("Failed to sign out: %s", ErrorMessageFromNSError(error).c_str());
  }
}

Future<void> Auth::SendPasswordResetEmail(const char *email) {
  if (!auth_data_) return Future<void>();
  ReferenceCountedFutureImpl &futures = auth_data_->future_impl;
  const auto handle = futures.SafeAlloc<void>(kAuthFn_SendPasswordResetEmail);
  if (!email || strlen(email) == 0) {
    futures.Complete(handle, kAuthErrorMissingEmail, "Empty email is not allowed.");
    return MakeFuture(&futures, handle);
  }

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data_->auth_impl)->listener_handle.get();
  [AuthImpl(auth_data_)
      sendPasswordResetWithEmail:@(email)
                      completion:^(NSError *_Nullable error) {
                        @synchronized(listener_cpp_handle) {
                          AuthData *data = listener_cpp_handle.authData;
                          if (!data) return;
                          data->future_impl.Complete(handle, AuthErrorFromNSError(error),
                                                     ErrorMessageFromNSError(error).c_str());
                        }
                      }];

  return MakeFuture(&futures, handle);
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  if (!auth_data_) return;
  SetEmulatorIos(auth_data_, host.c_str(), port);
}

// Remap iOS SDK errors reported by the UIDelegate. While these errors seem like
// user interaction errors, they are actually caused by bad provider ids.
NSError *_Nullable RemapBadProviderIDErrors(NSError *_Nullable error) {
  if (error == nil) {
    return nil;
  }
  if (error.code == FIRAuthErrorCodeWebSignInUserInteractionFailure &&
      [error.domain isEqualToString:@"FIRAuthErrorDomain"]) {
    return [[NSError alloc] initWithDomain:error.domain
                                      code:FIRAuthErrorCodeInvalidProviderID
                                  userInfo:error.userInfo];
  }
  return error;
}

// Not implemented for iOS.
void EnableTokenAutoRefresh(AuthData *auth_data) { (void)auth_data; }
void DisableTokenAutoRefresh(AuthData *auth_data) { (void)auth_data; }

}  // namespace auth
}  // namespace firebase
