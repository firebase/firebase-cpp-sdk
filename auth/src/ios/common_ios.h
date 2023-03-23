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

#import "FIRAuth.h"
#import "FIRAuthCredential.h"
#import "FIROAuthProvider.h"
#import "FIRUser.h"
#import "FIRUserInfo.h"
#import "FIRUserMetadata.h"

#include <string>
#include <vector>

#include "app/src/log.h"
#include "app/src/util_ios.h"
#include "auth/src/common.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/user.h"

@class FIRCPPAuthListenerHandle;

namespace firebase {
namespace auth {

// No static_assert available without C++11, unfortunately.
#define AUTH_STATIC_ASSERT(x) typedef char AUTH_STATIC_ASSERT_FAILED[(x) ? 1 : -1]

OBJ_C_PTR_WRAPPER(FIRAuth);
OBJ_C_PTR_WRAPPER(FIRAuthCredential);
OBJ_C_PTR_WRAPPER(FIRUser);
OBJ_C_PTR_WRAPPER(FIRCPPAuthListenerHandle);

// Auth implementation on iOS.
struct AuthDataIos {
  FIRAuthPointer fir_auth;
  FIRCPPAuthListenerHandlePointer listener_handle;
};

// Struct to contain the data required to complete
// futures asynchronously on iOS.
template <class T>
struct FutureCallbackData {
  FutureData *future_data;
  SafeFutureHandle<T> future_handle;
};

// Contains the interface between the public API and the underlying
// Obj-C SDK FirebaseUser implemention.
class UserInternal {
 public:
  // Constructor
  explicit UserInternal(FIRUser *user);

  // Copy constructor.
  UserInternal(const UserInternal &user_internal);

  ~UserInternal();

  // @deprecated
  //
  // Provides a mechanism for the deprecated auth-contained user object to
  // update its underlying FIRUser data.
  void set_native_user_object_deprecated(FIRUser *user);

  bool is_valid() const;

  Future<std::string> GetToken(bool force_refresh);
  Future<std::string> GetTokenLastResult() const;

  Future<void> UpdateEmail(const char *email);
  Future<void> UpdateEmailLastResult() const;

  std::vector<UserInfoInterface> provider_data() const;
  const std::vector<UserInfoInterface *> &provider_data_DEPRECATED();

  Future<void> UpdatePassword(const char *password);
  Future<void> UpdatePasswordLastResult() const;

  Future<void> UpdateUserProfile(const User::UserProfile &profile);
  Future<void> UpdateUserProfileLastResult() const;

  Future<void> SendEmailVerification();
  Future<void> SendEmailVerificationLastResult() const;

  Future<User *> LinkWithCredential_DEPRECATED(AuthData *auth_data, const Credential &credential);
  Future<User *> LinkWithCredentialLastResult_DEPRECATED() const;

  Future<SignInResult> LinkAndRetrieveDataWithCredential_DEPRECATED(AuthData *auth_data_,
                                                                    const Credential &credential);
  Future<SignInResult> LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED() const;

  Future<SignInResult> LinkWithProvider_DEPRECATED(AuthData *auth_data,
                                                   FederatedAuthProvider *provider);
  Future<SignInResult> LinkWithProviderLastResult_DEPRECATED() const;

  Future<User *> Unlink_DEPRECATED(AuthData *auth_data, const char *provider);
  Future<User *> UnlinkLastResult_DEPRECATED() const;

  Future<User *> UpdatePhoneNumberCredential_DEPRECATED(AuthData *auth_data,
                                                        const Credential &credential);
  Future<User *> UpdatePhoneNumberCredentialLastResult_DEPRECATED() const;

  Future<void> Reload();
  Future<void> ReloadLastResult() const;

  Future<void> Reauthenticate(const Credential &credential);
  Future<void> ReauthenticateLastResult() const;

  Future<SignInResult> ReauthenticateAndRetrieveData_DEPRECATED(AuthData *auth_data,
                                                                const Credential &credential);
  Future<SignInResult> ReauthenticateAndRetrieveDataLastResult_DEPRECATED() const;

  Future<SignInResult> ReauthenticateWithProvider_DEPRECATED(AuthData *auth_data,
                                                             FederatedAuthProvider *provider);
  Future<SignInResult> ReauthenticateWithProviderLastResult_DEPRECATED() const;

  Future<void> Delete(AuthData *auth_data);
  Future<void> DeleteLastResult() const;

  UserMetadata metadata() const;
  bool is_email_verified() const;
  bool is_anonymous() const;
  std::string uid() const;
  std::string email() const;
  std::string display_name() const;
  std::string phone_number() const;
  std::string photo_url() const;
  std::string provider_id() const;

 private:
  friend class firebase::auth::FederatedOAuthProvider;
  friend class firebase::auth::User;

  void clear_user_infos();

  // Obj-c Implementation of a User object.
  FIRUser *user_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Used to support older method invocation of provider_data_DEPRECATED().
  std::vector<UserInfoInterface *> user_infos_;

  // Guard the creation and deletion of the vector of UserInfoInterface*
  // allocations in provider_data_DEPRECATED().
  Mutex user_info_mutex_deprecated_;

  // Guard against changes to the user_ object.
  Mutex user_mutex_;
};

/// @deprecated
///
/// Replace the platform-dependent FIRUser object.
/// Note: this function is only used to support DEPRECATED methods which return User*. This
/// functionality should be removed when those deprecated methods are removed.
static inline void SetUserImpl(AuthData *_Nonnull auth_data, FIRUser *_Nullable ios_user) {
  auth_data->deprecated_fields.user_internal_deprecated->set_native_user_object_deprecated(
      ios_user);
}

/// Convert from the platform-independent void* to the Obj-C FIRAuth pointer.
static inline FIRAuth *_Nonnull AuthImpl(AuthData *_Nonnull auth_data) {
  return static_cast<AuthDataIos *>(auth_data->auth_impl)->fir_auth.get();
}

/// Convert from the void* credential implementation pointer into the Obj-C
/// FIRAuthCredential pointer.
static inline FIRAuthCredential *_Nonnull CredentialFromImpl(void *_Nonnull impl) {
  return static_cast<FIRAuthCredentialPointer *>(impl)->get();
}

AuthError AuthErrorFromNSError(NSError *_Nullable error);

/// Common code for all API calls that return a User*.
/// Initialize `auth_data->current_user` and complete the `future`.
void SignInCallback(FIRUser *_Nullable user, NSError *_Nullable error,
                    SafeFutureHandle<User *> handle, ReferenceCountedFutureImpl &future_impl,
                    AuthData *_Nonnull auth_data);

/// Common code for all API calls that return a SignInResult.
/// Initialize `auth_data->current_user` and complete the `future`.
void SignInResultCallback(FIRAuthDataResult *_Nullable auth_result, NSError *_Nullable error,
                          SafeFutureHandle<SignInResult> handle,
                          ReferenceCountedFutureImpl &future_impl, AuthData *_Nonnull auth_data);

/// Common code for all FederatedOAuth API calls which return a SignInResult and
/// must hold a reference to a FIROAuthProvider so that the provider is not
/// deallocated by the Objective-C environment. Directly invokes
/// SignInResultCallback().
void SignInResultWithProviderCallback(FIRAuthDataResult *_Nullable auth_result,
                                      NSError *_Nullable error,
                                      SafeFutureHandle<SignInResult> handle,
                                      ReferenceCountedFutureImpl &future_impl,
                                      AuthData *_Nonnull auth_data,
                                      const FIROAuthProvider *_Nonnull ios_auth_provider);

/// Remap iOS SDK errors reported by the UIDelegate. While these errors seem
/// like user interaction errors, they are actually caused by bad provider ids.
NSError *RemapBadProviderIDErrors(NSError *_Nonnull error);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_IOS_COMMON_IOS_H_
