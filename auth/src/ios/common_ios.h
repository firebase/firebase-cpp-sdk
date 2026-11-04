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

#ifndef FIREBASE_AUTH_SRC_IOS_COMMON_IOS_H_
#define FIREBASE_AUTH_SRC_IOS_COMMON_IOS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "FIRAuth.h"
#import "FIRUser.h"
#import "FirebaseAuthInterop/FIRAuthInterop.h"
// This needs to be after the FIRAuthInterop import
#import "FirebaseAuth-Swift.h"

#include "app/src/log.h"
#include "app/src/util_ios.h"
#include "auth/src/common.h"
#include "auth/src/include/firebase/auth/user.h"

@interface FIRCPPAuthListenerHandle : NSObject
@property(nonatomic, nullable) firebase::auth::AuthData *authData;
@end

namespace firebase {
namespace auth {

OBJ_C_PTR_WRAPPER(FIRAuth);
OBJ_C_PTR_WRAPPER(FIRAuthCredential);
OBJ_C_PTR_WRAPPER(FIRPhoneAuthCredential);
OBJ_C_PTR_WRAPPER(FIRUser);
OBJ_C_PTR_WRAPPER(FIRCPPAuthListenerHandle);

struct AuthDataIos {
  FIRAuthPointer fir_auth;
  FIRCPPAuthListenerHandlePointer listener_handle;
};

class InternalAuthResultProvider {
 public:
  static Credential GetCredential(FIRAuthCredential *_Nullable credential);
  static Credential GetCredential(FIRAuthCredential *_Nullable credential,
                                  AuthError error_code,
                                  const std::string &error_message);
};

static inline FIRUser *_Nullable UserImpl(AuthData *_Nonnull auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  __strong FIRUser *user = FIRUserPointer::SafeGet(
      static_cast<FIRUserPointer *>(auth_data->user_impl));
  return user;
}

static inline void SetUserImpl(AuthData *_Nonnull auth_data,
                               FIRUser *_Nullable user) {
  MutexLock lock(auth_data->future_impl.mutex());
  if (auth_data->user_impl != nullptr) {
    delete static_cast<FIRUserPointer *>(auth_data->user_impl);
    auth_data->user_impl = nullptr;
  }
  if (user != nullptr) {
    auth_data->user_impl = new FIRUserPointer(user);
  }
}

static inline FIRAuth *_Nullable AuthImpl(AuthData *_Nonnull auth_data) {
  return static_cast<AuthDataIos *>(auth_data->auth_impl)->fir_auth.get();
}

static inline FIRAuthCredential *_Nullable CredentialFromImpl(
    void *_Nullable impl) {
  return impl ? static_cast<FIRAuthCredentialPointer *>(impl)->get() : nil;
}

static inline FIRPhoneAuthCredential *_Nullable PhoneAuthCredentialFromImpl(
    void *_Nullable impl) {
  FIRAuthCredential *credential = CredentialFromImpl(impl);
  return [credential isKindOfClass:[FIRPhoneAuthCredential class]]
             ? static_cast<FIRPhoneAuthCredential *>(credential)
             : nil;
}

AuthError AuthErrorFromNSError(NSError *_Nullable error);
std::string ErrorMessageFromNSError(NSError *_Nullable error);

void AuthResultCallback(FIRAuthDataResult *_Nullable fir_auth_result,
                        NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle,
                        AuthData *_Nonnull auth_data);
void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle,
                        AuthData *_Nonnull auth_data);
void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<User> handle,
                        AuthData *_Nonnull auth_data);
void AuthResultWithProviderCallback(
    FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
    SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data,
    const FIROAuthProvider *_Nullable ios_auth_provider);

NSError *_Nullable RemapBadProviderIDErrors(NSError *_Nullable error);

template <typename T>
void CompletePromiseWithNSError(AuthData *auth_data,
                                SafeFutureHandle<T> handle,
                                NSError *_Nullable error,
                                AuthError default_error = kAuthErrorFailure) {
  if (error) {
    AuthError err = AuthErrorFromNSError(error);
    if (err == kAuthErrorFailure && default_error != kAuthErrorFailure) {
      err = default_error;
    }
    auth_data->future_impl.Complete(handle, err,
                                    ErrorMessageFromNSError(error).c_str());
  } else {
    auth_data->future_impl.Complete(handle, kAuthErrorNone, "");
  }
}

template <typename T>
bool ValidateIosCredential(const Credential &credential, Promise<T> *promise) {
  if (credential.error_code_ != kAuthErrorNone) {
    promise->Fail(credential.error_code_, credential.error_message_.c_str());
    return false;
  }
  if (!credential.is_valid()) {
    promise->Fail(kAuthErrorInvalidCredential,
                  "Invalid credential is not allowed.");
    return false;
  }
  return true;
}

template <typename T, typename Func>
void RunAppleUserTaskWithPromise(AuthData *auth_data, Promise<T> promise,
                                 Func func) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)
          ->listener_handle.get();
  func(UserImpl(auth_data), listener_cpp_handle);
}

template <typename T, typename Func>
void RunAppleAuthTaskWithPromise(AuthData *auth_data, Promise<T> /*promise*/,
                                 Func func) {
  FIRCPPAuthListenerHandle *listener_cpp_handle =
      reinterpret_cast<AuthDataIos *>(auth_data->auth_impl)
          ->listener_handle.get();
  func(AuthImpl(auth_data), listener_cpp_handle);
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_IOS_COMMON_IOS_H_
