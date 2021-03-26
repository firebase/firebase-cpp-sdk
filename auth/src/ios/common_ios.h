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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_IOS_COMMON_IOS_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_IOS_COMMON_IOS_H_

#import <Foundation/Foundation.h>

#import "FIRAuth.h"
#import "FIRAuthCredential.h"
#import "FIROAuthProvider.h"
#import "FIRUser.h"
#import "FIRUserInfo.h"
#import "FIRUserMetadata.h"

#include "app/src/log.h"
#include "app/src/util_ios.h"
#include "auth/src/common.h"
#include "auth/src/include/firebase/auth/user.h"

@class FIRCPPAuthListenerHandle;

namespace firebase {
namespace auth {

// No static_assert available without C++11, unfortunately.
#define AUTH_STATIC_ASSERT(x) \
  typedef char AUTH_STATIC_ASSERT_FAILED[(x) ? 1 : -1]

OBJ_C_PTR_WRAPPER(FIRAuth);
OBJ_C_PTR_WRAPPER(FIRAuthCredential);
OBJ_C_PTR_WRAPPER(FIRUser);
OBJ_C_PTR_WRAPPER(FIRCPPAuthListenerHandle);

// Auth implementation on iOS.
struct AuthDataIos {
  FIRAuthPointer fir_auth;
  FIRCPPAuthListenerHandlePointer listener_handle;
};

/// Convert from the platform-independent void* to the Obj-C FIRUser pointer.
static inline FIRUser *_Nullable UserImpl(AuthData *_Nonnull auth_data) {
  return FIRUserPointer::SafeGet(
    static_cast<FIRUserPointer *>(auth_data->user_impl));
}

/// Release the platform-dependent FIRUser object.
static inline void SetUserImpl(AuthData *_Nonnull auth_data,
                               FIRUser *_Nullable user) {
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
static inline FIRAuth *_Nonnull AuthImpl(AuthData *_Nonnull auth_data) {
  return static_cast<AuthDataIos *>(auth_data->auth_impl)->fir_auth.get();
}

/// Convert from the void* credential implementation pointer into the Obj-C
/// FIRAuthCredential pointer.
static inline FIRAuthCredential *_Nonnull CredentialFromImpl(
    void *_Nonnull impl) {
  return static_cast<FIRAuthCredentialPointer *>(impl)->get();
}

AuthError AuthErrorFromNSError(NSError *_Nullable error);

/// Common code for all API calls that return a User*.
/// Initialize `auth_data->current_user` and complete the `future`.
void SignInCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                    SafeFutureHandle<User*> handle,
                    AuthData *_Nonnull auth_data);

/// Common code for all API calls that return a SignInResult.
/// Initialize `auth_data->current_user` and complete the `future`.
void SignInResultCallback(FIRAuthDataResult *_Nullable auth_result,
                          NSError *_Nullable error,
                          SafeFutureHandle<SignInResult> handle,
                          AuthData *_Nonnull auth_data);

/// Common code for all FederatedOAuth API calls which return a SignInResult and
/// must hold a reference to a FIROAuthProvider so that the provider is not
/// deallocated by the Objective-C environment. Directly invokes
/// SignInResultCallback().
void SignInResultWithProviderCallback(
    FIRAuthDataResult* _Nullable auth_result, NSError* _Nullable error,
    SafeFutureHandle<SignInResult> handle, AuthData *_Nonnull auth_data,
    const FIROAuthProvider *_Nonnull ios_auth_provider);

/// Remap iOS SDK errors reported by the UIDelegate. While these errors seem
/// like user interaction errors, they are actually caused by bad provider ids.
NSError* RemapBadProviderIDErrors(NSError* _Nonnull error);


}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_IOS_COMMON_IOS_H_
