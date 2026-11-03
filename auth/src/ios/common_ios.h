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

// Wraps AuthData with an Obj-C object so that it's possible to remove
// the authData from any blocks that reference this handle.
@interface FIRCPPAuthListenerHandle : NSObject
@property(atomic, nullable) firebase::auth::AuthData *authData;
@end

namespace firebase {
namespace auth {

OBJ_C_PTR_WRAPPER(FIRAuth);
OBJ_C_PTR_WRAPPER(FIRAuthCredential);
OBJ_C_PTR_WRAPPER(FIRPhoneAuthCredential);
OBJ_C_PTR_WRAPPER(FIRUser);
OBJ_C_PTR_WRAPPER(FIRCPPAuthListenerHandle);

// Auth implementation on iOS.
struct AuthDataIos {
  FIRAuthPointer fir_auth;
  FIRCPPAuthListenerHandlePointer listener_handle;
};

// Invokes a private Credential constructor only accessible by friends of the
// Credential class.
//
// This is used to marshall and return Credential objects from the iOS SDK
// FIRAuthDataResult objects. That is, credentials that aren't created by our
// users' applications, but created to represent Credentials created internally
// by the iOS SDK.
class InternalAuthResultProvider {
 public:
  static Credential GetCredential(
      FIRAuthCredential *_Nullable credential,
      AuthError error_code = kAuthErrorInvalidCredential,
      const std::string &error_message = "Invalid credential.");
};

/// Convert from the platform-independent void* to the Obj-C FIRUser pointer.
static inline FIRUser *_Nullable UserImpl(AuthData *_Nonnull auth_data) {
  MutexLock lock(auth_data->future_impl.mutex());
  return FIRUserPointer::SafeGet(static_cast<FIRUserPointer *>(auth_data->user_impl));
}

/// Set or release the platform-dependent FIRUser object.
static inline void SetUserImpl(AuthData *_Nonnull auth_data, FIRUser *_Nullable user) {
  MutexLock lock(auth_data->future_impl.mutex());

  // Delete existing pointer to FIRUser.
  if (auth_data->user_impl != nullptr) {
    delete static_cast<FIRUserPointer *>(auth_data->user_impl);
    auth_data->user_impl = nullptr;
  }

  // Create new pointer to FIRUser.
  if (user != nullptr) {
    auth_data->user_impl = new FIRUserPointer(user);
  }
}

/// Convert from the platform-independent void* to the Obj-C FIRAuth pointer.
static inline FIRAuth *_Nullable AuthImpl(AuthData *_Nonnull auth_data) {
  return static_cast<AuthDataIos *>(auth_data->auth_impl)->fir_auth.get();
}

/// Convert from the void* credential implementation pointer into the Obj-C
/// FIRAuthCredential pointer.
static inline FIRAuthCredential *_Nullable CredentialFromImpl(void *_Nullable impl) {
  return impl ? static_cast<FIRAuthCredentialPointer *>(impl)->get() : nil;
}

/// Convert from the void* credential implementation pointer into the Obj-C
/// FIRPhoneAuthCredential pointer.
static inline FIRPhoneAuthCredential *_Nullable PhoneAuthCredentialFromImpl(void *_Nullable impl) {
  FIRAuthCredential *credential = CredentialFromImpl(impl);
  return [credential isKindOfClass:[FIRPhoneAuthCredential class]]
             ? static_cast<FIRPhoneAuthCredential *>(credential)
             : nil;
}

AuthError AuthErrorFromNSError(NSError *_Nullable error);

std::string ErrorMessageFromNSError(NSError *_Nullable error);

/// Common code for all API calls that return a AuthResult.
/// Initialize `auth_data->current_user` and complete the `future`.
void AuthResultCallback(FIRAuthDataResult *_Nullable fir_auth_result, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data);

/// Common code for all API calls that return a AuthResult where the iOS SDK
/// only returns a FIRUser.
/// Initialize `auth_data->current_user` and complete the `future`.
void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<AuthResult> handle, AuthData *_Nonnull auth_data);

/// Common code for all API calls that return a User.
/// Initialize `auth_data->current_user` and complete the `future`.
void AuthResultCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                        SafeFutureHandle<User> handle, AuthData *_Nonnull auth_data);

/// Common code for all FederatedOAuth API calls which return an AuthResult and
/// must hold a reference to a FIROAuthProvider so that the provider is not
/// deallocated by the Objective-C environment. Directly invokes
/// AuthResultCallback().
void AuthResultWithProviderCallback(FIRAuthDataResult *_Nullable auth_result,
                                    NSError *_Nullable error, SafeFutureHandle<AuthResult> handle,
                                    AuthData *_Nonnull auth_data,
                                    const FIROAuthProvider *_Nonnull ios_auth_provider);

/// Remap iOS SDK errors reported by the UIDelegate. While these errors seem
/// like user interaction errors, they are actually caused by bad provider ids.
NSError *_Nullable RemapBadProviderIDErrors(NSError *_Nullable error);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_IOS_COMMON_IOS_H_
