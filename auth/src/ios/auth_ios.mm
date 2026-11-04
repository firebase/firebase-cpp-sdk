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
    {FIRAuthErrorCodeAppVerificationUserInteractionFailure,
     kAuthErrorAppVerificationFailed},
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
    {FIRAuthErrorCodeInvalidVerificationCode,
     kAuthErrorInvalidVerificationCode},
    {FIRAuthErrorCodeInvalidVerificationID, kAuthErrorInvalidVerificationId},
    {FIRAuthErrorCodeKeychainError, kAuthErrorKeychainError},
    {FIRAuthErrorCodeMissingAppCredential, kAuthErrorMissingAppCredential},
    {FIRAuthErrorCodeMissingAppToken, kAuthErrorMissingAppToken},
    {FIRAuthErrorCodeMissingContinueURI, kAuthErrorMissingContinueUri},
    {FIRAuthErrorCodeMissingEmail, kAuthErrorMissingEmail},
    {FIRAuthErrorCodeMissingIosBundleID, kAuthErrorMissingIosBundleId},
    {FIRAuthErrorCodeMissingPhoneNumber, kAuthErrorMissingPhoneNumber},
    {FIRAuthErrorCodeMissingVerificationCode,
     kAuthErrorMissingVerificationCode},
    {FIRAuthErrorCodeMissingVerificationID, kAuthErrorMissingVerificationId},
    {FIRAuthErrorCodeNetworkError, kAuthErrorNetworkRequestFailed},
    {FIRAuthErrorCodeNoSuchProvider, kAuthErrorNoSuchProvider},
    {FIRAuthErrorCodeNotificationNotForwarded,
     kAuthErrorNotificationNotForwarded},
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
    {FIRAuthErrorCodeWebContextAlreadyPresented,
     kAuthErrorWebContextAlreadyPresented},
    {FIRAuthErrorCodeWebContextCancelled, kAuthErrorWebContextCancelled},
    {FIRAuthErrorCodeWrongPassword, kAuthErrorWrongPassword},
    {FIRAuthErrorCodeInvalidProviderID, kAuthErrorInvalidProviderId},
    {FIRAuthErrorCodeWebSignInUserInteractionFailure,
     kAuthErrorFederatedSignInUserInteractionFailure},
    {FIRAuthErrorCodeMissingClientIdentifier,
     kAuthErrorMissingClientIdentifier},
    {FIRAuthErrorCodeMissingMultiFactorSession,
     kAuthErrorMissingMultiFactorSession},
    {FIRAuthErrorCodeMissingMultiFactorInfo, kAuthErrorMissingMultiFactorInfo},
    {FIRAuthErrorCodeInvalidMultiFactorSession,
     kAuthErrorInvalidMultiFactorSession},
    {FIRAuthErrorCodeMultiFactorInfoNotFound,
     kAuthErrorMultiFactorInfoNotFound},
    {FIRAuthErrorCodeAdminRestrictedOperation,
     kAuthErrorAdminRestrictedOperation},
    {FIRAuthErrorCodeUnverifiedEmail, kAuthErrorUnverifiedEmail},
    {FIRAuthErrorCodeSecondFactorAlreadyEnrolled,
     kAuthErrorSecondFactorAlreadyEnrolled},
    {FIRAuthErrorCodeMaximumSecondFactorCountExceeded,
     kAuthErrorMaximumSecondFactorCountExceeded},
    {FIRAuthErrorCodeUnsupportedFirstFactor, kAuthErrorUnsupportedFirstFactor},
    {FIRAuthErrorCodeEmailChangeNeedsVerification,
     kAuthErrorEmailChangeNeedsVerification},
};

class ServiceUpdatedCredentialProvider {
 public:
  static Credential GetCredential(FIRAuthCredentialPointer *impl) {
    return Credential(impl);
  }
};

template <typename T>
struct ListenerHandleHolder {
  explicit ListenerHandleHolder(T h) : handle(h) {}
  T handle;
};

void *CreatePlatformAuth(App *app) {
  FIRAuth *auth = [FIRAuth authWithApp:app->GetPlatformApp()];
  FIREBASE_ASSERT(auth != nullptr);
  AuthDataIos *auth_impl = new AuthDataIos();
  auth_impl->fir_auth = auth;
  return auth_impl;
}

void UpdateCurrentUser(AuthData *auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  SetUserImpl(auth_data, [AuthImpl(auth_data) currentUser]);
}

void SetEmulatorIos(AuthData *auth_data, const char *host, uint32_t port) {
  if (!auth_data || !host || strlen(host) == 0 || port == 0 || port > 65535) {
    return;
  }
  if (NSString *ns_host = util::CStringToNSString(host)) {
    [AuthImpl(auth_data) useEmulatorWithHost:ns_host
                                        port:static_cast<NSInteger>(port)];
  }
}

void CheckEmulator(AuthData *auth_data) {
  if (std::getenv("USE_AUTH_EMULATOR") == nullptr) {
    LogInfo("Using Auth Prod for testing.");
    return;
  }
  LogInfo("Using Auth Emulator.");
  uint32_t port =
      static_cast<uint32_t>(std::strtoul(kEmulatorPort, nullptr, 10));
  const char *env_port = std::getenv("AUTH_EMULATOR_PORT");
  if (env_port && env_port[0] != '\0') {
    char *endptr = nullptr;
    unsigned long parsed = std::strtoul(env_port, &endptr, 10);
    if (endptr != env_port && *endptr == '\0' && parsed > 0 &&
        parsed <= 65535) {
      port = static_cast<uint32_t>(parsed);
    }
  }
  SetEmulatorIos(auth_data, kEmulatorLocalHost, port);
}

void Auth::InitPlatformAuth(AuthData *auth_data) {
  CheckEmulator(auth_data);
  UpdateCurrentUser(auth_data);

  FIRCPPAuthListenerHandle *listener_cpp_handle =
      [[FIRCPPAuthListenerHandle alloc] init];
  listener_cpp_handle.authData = auth_data;
  reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)->listener_handle =
      listener_cpp_handle;

  FIRAuthStateDidChangeListenerHandle listener_handle = [AuthImpl(auth_data)
      addAuthStateDidChangeListener:^(FIRAuth *_Nonnull __strong,
                                      FIRUser *_Nullable __strong) {
        @synchronized(listener_cpp_handle) {
          if (AuthData *data = listener_cpp_handle.authData) {
            UpdateCurrentUser(data);
            NotifyAuthStateListeners(data);
          }
        }
      }];
  FIRIDTokenDidChangeListenerHandle id_token_listener_handle =
      [AuthImpl(auth_data)
          addIDTokenDidChangeListener:^(FIRAuth *_Nonnull __strong,
                                        FIRUser *_Nullable __strong) {
            @synchronized(listener_cpp_handle) {
              if (AuthData *data = listener_cpp_handle.authData) {
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
      new ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle>(
          listener_handle);
  auth_data->id_token_listener_impl =
      new ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle>(
          id_token_listener_handle);
}

void Auth::DestroyPlatformAuth(AuthData *auth_data) {
  AuthDataIos *auth_data_ios =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl);
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      auth_data_ios->listener_handle.get();
  @synchronized(listener_cpp_handle) {
    listener_cpp_handle.authData = nullptr;
  }

  auto *handle_holder = reinterpret_cast<
      ListenerHandleHolder<FIRAuthStateDidChangeListenerHandle> *>(
      auth_data->listener_impl);
  [AuthImpl(auth_data) removeAuthStateDidChangeListener:handle_holder->handle];
  delete handle_holder;
  auth_data->listener_impl = nullptr;

  auto *id_token_holder = reinterpret_cast<
      ListenerHandleHolder<FIRIDTokenDidChangeListenerHandle> *>(
      auth_data->id_token_listener_impl);
  [AuthImpl(auth_data) removeIDTokenDidChangeListener:id_token_holder->handle];
  delete id_token_holder;
  auth_data->id_token_listener_impl = nullptr;

  SetUserImpl(auth_data, nullptr);
  delete auth_data_ios;
  auth_data->auth_impl = nullptr;
}

void LogHeartbeat(Auth * /*auth*/) {}

void FetchProvidersForEmail(AuthData *auth_data,
                            Promise<Auth::FetchProvidersResult> promise,
                            const char *email) {
  if (!email || strlen(email) == 0) {
    promise.Fail(kAuthErrorMissingEmail, "Empty email is not allowed.");
    return;
  }
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a fetchSignInMethodsForEmail:@(email)
                           completion:^(NSArray<NSString *> *_Nullable providers,
                                        NSError *_Nullable error) {
                             @synchronized(h) {
                               AuthData *data = h.authData;
                               if (!data) return;
                               data->future_impl.Complete<FetchProvidersResult>(
                                   promise.handle(),
                                   AuthErrorFromNSError(error),
                                   ErrorMessageFromNSError(error).c_str(),
                                   [providers](FetchProvidersResult *res) {
                                     res->providers.clear();
                                     if (providers != nil) {
                                       res->providers.resize(providers.count);
                                       for (size_t i = 0; i < providers.count;
                                            ++i) {
                                         res->providers[i] =
                                             util::StringFromNSString(
                                                 providers[i]);
                                       }
                                     }
                                   });
                             }
                           }];
      });
}

static User *AssignUser(FIRUser *_Nullable user, AuthData *auth_data) {
  if (!user) return nullptr;
  MutexLock lock(auth_data->future_impl.mutex());
  SetUserImpl(auth_data, user);
  return auth_data->user_impl ? &auth_data->current_user : nullptr;
}

std::string Auth::language_code() const {
  NSString *lang = [AuthImpl(auth_data_) languageCode];
  return lang ? util::NSStringToString(lang) : std::string();
}

void Auth::set_language_code(const char *language_code) {
  AuthImpl(auth_data_).languageCode =
      (language_code && strlen(language_code) > 0)
          ? util::CStringToNSString(language_code)
          : nil;
}

void Auth::UseAppLanguage() { [AuthImpl(auth_data_) useAppLanguage]; }

AuthError AuthErrorFromNSError(NSError *_Nullable error) {
  if (!error) return kAuthErrorNone;
  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(kIosToCppErrorMap); i++) {
    if (error.code == kIosToCppErrorMap[i].ios_error)
      return kIosToCppErrorMap[i].cpp_error;
  }
  return kAuthErrorFailure;
}

std::string ErrorMessageFromNSError(NSError *_Nullable error) {
  if (!error) return std::string();
  std::string error_string =
      util::StringFromNSString(error.localizedDescription);
  if (error.userInfo && error.userInfo[NSLocalizedFailureReasonErrorKey]) {
    error_string +=
        ": " + util::StringFromNSString(
                   error.userInfo[NSLocalizedFailureReasonErrorKey]);
  }
  return error_string;
}

static void PopulateUpdatedCredentialFromError(NSError *_Nullable error,
                                               AuthResult *result) {
  if (error && error.userInfo &&
      error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey]) {
    result->additional_user_info.updated_credential =
        ServiceUpdatedCredentialProvider::GetCredential(
            new FIRAuthCredentialPointer(
                error.userInfo[FIRAuthErrorUserInfoUpdatedCredentialKey]));
  }
}

void AuthResultCallback(FIRAuthDataResult *_Nullable fir_auth_result,
                        NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle,
                        AuthData *_Nonnull auth_data) {
  User *current_user = (error == nil && fir_auth_result != nil)
                           ? AssignUser(fir_auth_result.user, auth_data)
                           : nullptr;
  AuthResult result;
  if (current_user != nullptr) {
    result.user = *current_user;
    if (fir_auth_result.additionalUserInfo != nil) {
      result.additional_user_info.provider_id = util::StringFromNSString(
          fir_auth_result.additionalUserInfo.providerID);
      result.additional_user_info.user_name =
          util::StringFromNSString(fir_auth_result.additionalUserInfo.username);
      result.additional_user_info.is_new_user =
          fir_auth_result.additionalUserInfo.newUser;
      if (fir_auth_result.additionalUserInfo.profile != nil) {
        util::NSDictionaryToStdMap(fir_auth_result.additionalUserInfo.profile,
                                   &result.additional_user_info.profile);
      }
    }
    if (fir_auth_result.credential != nil) {
      result.credential = InternalAuthResultProvider::GetCredential(
          fir_auth_result.credential);
    }
  }
  PopulateUpdatedCredentialFromError(error, &result);
  auth_data->future_impl.CompleteWithResult(
      handle, AuthErrorFromNSError(error),
      ErrorMessageFromNSError(error).c_str(), result);
}

void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle,
                        AuthData *_Nonnull auth_data) {
  User *current_user =
      (error == nil && user != nil) ? AssignUser(user, auth_data) : nullptr;
  AuthResult auth_result;
  if (current_user != nullptr) auth_result.user = *current_user;
  PopulateUpdatedCredentialFromError(error, &auth_result);
  auth_data->future_impl.CompleteWithResult(
      handle, AuthErrorFromNSError(error),
      ErrorMessageFromNSError(error).c_str(), auth_result);
}

void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<User> handle,
                        AuthData *_Nonnull auth_data) {
  User *current_user =
      (error == nil && user != nil) ? AssignUser(user, auth_data) : nullptr;
  User user_result = current_user ? *current_user : User();
  auth_data->future_impl.CompleteWithResult(
      handle, AuthErrorFromNSError(error),
      ErrorMessageFromNSError(error).c_str(), user_result);
}

void AuthResultWithProviderCallback(
    FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
    SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data,
    const FIROAuthProvider *_Nullable /*ios_auth_provider*/) {
  AuthResultCallback(auth_result, RemapBadProviderIDErrors(error), handle,
                     auth_data);
}

void SignInWithCustomToken(AuthData *auth_data, Promise<AuthResult> promise,
                           const char *token) {
  if (!token || strlen(token) == 0) {
    promise.Fail(kAuthErrorInvalidCustomToken,
                 "Empty custom token is not allowed.");
    return;
  }
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a signInWithCustomToken:@(token)
                      completion:^(FIRAuthDataResult *r, NSError *e) {
                        @synchronized(h) {
                          if (h.authData)
                            AuthResultCallback(r, e, promise.handle(),
                                               h.authData);
                        }
                      }];
      });
}

void SignInWithCredential(AuthData *auth_data, Promise<User> promise,
                          const Credential &credential) {
  if (!ValidateIosCredential(credential, &promise)) return;
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a signInWithCredential:CredentialFromImpl(credential.impl_)
                     completion:^(FIRAuthDataResult *r, NSError *e) {
                       @synchronized(h) {
                         if (h.authData)
                           AuthResultCallback(r ? r.user : nil, e,
                                              promise.handle(), h.authData);
                       }
                     }];
      });
}

void SignInAndRetrieveDataWithCredential(AuthData *auth_data,
                                         Promise<AuthResult> promise,
                                         const Credential &credential) {
  if (!ValidateIosCredential(credential, &promise)) return;
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a signInWithCredential:CredentialFromImpl(credential.impl_)
                     completion:^(FIRAuthDataResult *r, NSError *e) {
                       @synchronized(h) {
                         if (h.authData)
                           AuthResultCallback(r, e, promise.handle(),
                                              h.authData);
                       }
                     }];
      });
}

Future<AuthResult> SignInWithProvider(AuthData *auth_data,
                                      FederatedAuthProvider *provider) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), provider);
  return provider->SignIn(auth_data);
}

void SignInAnonymously(AuthData *auth_data, Promise<AuthResult> promise) {
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a signInAnonymouslyWithCompletion:^(FIRAuthDataResult *r, NSError *e) {
          @synchronized(h) {
            if (h.authData)
              AuthResultCallback(r, e, promise.handle(), h.authData);
          }
        }];
      });
}

static bool ValidateIosEmailAndPassword(const char *email, const char *password,
                                        Promise<AuthResult> *promise) {
  if (!email || strlen(email) == 0) {
    promise->Fail(kAuthErrorMissingEmail, "Empty email is not allowed.");
    return false;
  }
  if (!password || strlen(password) == 0) {
    promise->Fail(kAuthErrorMissingPassword, "Empty password is not allowed.");
    return false;
  }
  return true;
}

void SignInWithEmailAndPassword(AuthData *auth_data,
                                Promise<AuthResult> promise, const char *email,
                                const char *password) {
  if (!ValidateIosEmailAndPassword(email, password, &promise)) return;
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a signInWithEmail:@(email)
                  password:@(password)
                completion:^(FIRAuthDataResult *r, NSError *e) {
                  @synchronized(h) {
                    if (h.authData)
                      AuthResultCallback(r, e, promise.handle(), h.authData);
                  }
                }];
      });
}

void CreateUserWithEmailAndPassword(AuthData *auth_data,
                                    Promise<AuthResult> promise,
                                    const char *email, const char *password) {
  if (!ValidateIosEmailAndPassword(email, password, &promise)) return;
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a createUserWithEmail:@(email)
                      password:@(password)
                    completion:^(FIRAuthDataResult *r, NSError *e) {
                      @synchronized(h) {
                        if (h.authData)
                          AuthResultCallback(r, e, promise.handle(),
                                             h.authData);
                      }
                    }];
      });
}

void SendPasswordResetEmail(AuthData *auth_data, Promise<void> promise,
                            const char *email) {
  if (!email || strlen(email) == 0) {
    promise.Fail(kAuthErrorMissingEmail, "Empty email is not allowed.");
    return;
  }
  RunAppleAuthTaskWithPromise(
      auth_data, promise, [=](FIRAuth *a, FIRCPPAuthListenerHandle *h) {
        [a sendPasswordResetWithEmail:@(email)
                           completion:^(NSError *e) {
                             @synchronized(h) {
                               if (h.authData)
                                 CompletePromiseWithNSError(
                                     h.authData, promise.handle(), e);
                             }
                           }];
      });
}

void Auth::UseEmulator(std::string host, uint32_t port) {
  SetEmulatorIos(auth_data_, host.c_str(), port);
}

NSError *_Nullable RemapBadProviderIDErrors(NSError *_Nullable error) {
  if (!error) return nil;
  if (error.code == FIRAuthErrorCodeWebSignInUserInteractionFailure &&
      [error.domain isEqualToString:@"FIRAuthErrorDomain"]) {
    return [[NSError alloc] initWithDomain:error.domain
                                      code:FIRAuthErrorCodeInvalidProviderID
                                  userInfo:error.userInfo];
  }
  return error;
}

void EnableTokenAutoRefresh(AuthData * /*auth_data*/) {}
void DisableTokenAutoRefresh(AuthData * /*auth_data*/) {}

void SignOut(AuthData *auth_data) {
  if (!auth_data) return;
  NSError *_Nullable error = nil;
  if ([AuthImpl(auth_data) signOut:&error]) {
    MutexLock lock(auth_data->future_impl.mutex());
    SetUserImpl(auth_data, nullptr);
  } else if (error != nil) {
    LogError("Failed to sign out: %s", ErrorMessageFromNSError(error).c_str());
  }
}

}  // namespace auth
}  // namespace firebase
