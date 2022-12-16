/*
 * Copyright 2022 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_ANDROID_USER_ANDROID_H_
#define FIREBASE_AUTH_SRC_ANDROID_USER_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "auth/src/data.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

// clang-format off
#define USER_INFO_METHODS(X)                                                   \
    X(GetUid, "getUid", "()Ljava/lang/String;"),                               \
    X(GetProviderId, "getProviderId", "()Ljava/lang/String;"),                 \
    X(GetDisplayName, "getDisplayName", "()Ljava/lang/String;"),               \
    X(GetPhoneNumber, "getPhoneNumber", "()Ljava/lang/String;"),               \
    X(GetPhotoUrl, "getPhotoUrl", "()Landroid/net/Uri;"),                      \
    X(GetEmail, "getEmail", "()Ljava/lang/String;"),                           \
    X(IsEmailVerified, "isEmailVerified", "()Z")
// clang-format on
METHOD_LOOKUP_DECLARATION(userinfo, USER_INFO_METHODS)

// Platform specific user implementation.
class UserInternal : public UserInfoInterface {
 private:
  // Property to query from an object that implements the UserInfo interface.
  enum PropertyType {
    kPropertyTypeString,  // Read string property.
    kPropertyTypeUri      // Read a URI property and convert to a string.
  };

 public:
  // Construct an invalid user object.
  UserInternal();

  // Construct the object from the specified internal auth object and platform
  // user, the platform user reference is destroyed by this method.
  UserInternal(AuthData* auth_internal, jobject platform_user);

  // Construct a copy.
  UserInternal(const UserInternal& user);

  ~UserInternal() override;

  // Copy the object.
  UserInternal& operator=(const UserInternal& user);

  // Assign a new platform user object to this instance from the specified local
  // reference to a platform user owned by the specified auth object.
  // This method deletes the local reference to the specified platform_user.
  // Returns true if the platform user differs to the object associated with
  // this instance, false otherwise.
  void SetPlatformUser(AuthData* auth_internal, jobject platform_user);

  // Get the user ID.
  std::string uid() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_, userinfo::kGetUid,
                               kPropertyTypeString);
  }

  // Get the user's email.
  std::string email() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_, userinfo::kGetEmail,
                               kPropertyTypeString);
  }

  // Get the user's display name.
  std::string display_name() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_,
                               userinfo::kGetDisplayName, kPropertyTypeString);
  }

  // Get the user's photo URL.
  std::string photo_url() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_,
                               userinfo::kGetPhotoUrl, kPropertyTypeUri);
  }

  // Get the auth provider's ID.
  std::string provider_id() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_,
                               userinfo::kGetProviderId, kPropertyTypeString);
  }

  // Get the user's phone number.
  std::string phone_number() const override {
    return GetUserInfoProperty(GetJNIEnv(), platform_user_,
                               userinfo::kGetPhoneNumber, kPropertyTypeString);
  }

  // Get the provider data.
  std::vector<UniquePtr<UserInfoInterface>> provider_data() const;

  // Get metadata.
  UserMetadata metadata() const;

  // Whether this user's email address has been verified.
  bool is_email_verified() const;

  // Whether this user is anonymous.
  bool is_anonymous() const;

  // Get the auth token for this user.
  Future<std::string> GetToken(bool force_refresh);

  // Update this user's email address.
  Future<void> UpdateEmail(const char* email);

  // Update this user's password.
  Future<void> UpdatePassword(const char* password);

  // Update user profile data.
  Future<void> UpdateUserProfile(const User::UserProfile& profile);

  // Link the user with the specified credential.
  Future<User*> LinkWithCredential(const Credential& credential);

  // Link the user with the specified credential and retrieve user metadata.
  Future<SignInResult> LinkAndRetrieveDataWithCredential(
      const Credential& credential);

  // Unlink the user from the specified provider.
  Future<User*> Unlink(const char* provider);

  // Update the user with the specified phone number credential.
  Future<User*> UpdatePhoneNumberCredential(const Credential& credential);

  // Reload the user.
  Future<void> Reload();

  // Reauthenticate this user.
  Future<void> Reauthenticate(const Credential& credential);

  // Reauthenticate this user and retrieve user metadata.
  Future<SignInResult> ReauthenticateAndRetrieveData(
      const Credential& credential);

  // Send verification email for this user.
  Future<void> SendEmailVerification();

  // Delete this user.
  Future<void> Delete();

  // Determine whether this object references a valid FirebaseUser or UserInfo
  // interface.
  bool is_valid() const {
    return auth_internal_ != nullptr && platform_user_ != nullptr;
  }

  // Get the future API for this user.
  ReferenceCountedFutureImpl* future_api() {
    return is_valid() ? &auth_internal_->future_impl : nullptr;
  }

  // TODO: Remove this when user no longer depends upon AuthData.
  AuthData* auth_internal() const { return auth_internal_; }

  // Cache the method ids so we don't have to look up JNI functions by name.
  static bool CacheUserMethodIds(JNIEnv* env, jobject activity);
  // Release user classes cached by CacheUserMethodIds().
  static void ReleaseUserClasses(JNIEnv* env);

 private:
  // Get the JNI environment.
  JNIEnv* GetJNIEnv() const {
    return auth_internal_ ? auth_internal_->app->GetJNIEnv() : nullptr;
  }

  // Invalidate this object.
  void Invalidate();

  // Unique identifier of the future API for this user.
  const char* future_api_id() const {
    return is_valid() ? auth_internal_->future_api_id.c_str() : "";
  }

  // Sets if the Id Token Listener is expecting a callback.
  // Used to workaround an issue where the Id Token is not reset with a new one,
  // and thus not triggered correctly.
  void SetExpectIdTokenListenerCallback(bool expect);

  // Returns if the Id Token Listener is expecting a callback, and clears the
  // flag.
  bool ShouldTriggerIdTokenListenerCallback();

  // Get a property from an object that implements the UserInfo interface.
  static std::string GetUserInfoProperty(JNIEnv* env, jobject user_interface,
                                         userinfo::Method method_id,
                                         PropertyType type);

 private:
  // Reasons why we can't remove the link to AuthData for now:
  // * User::provider_data() stores the returned vector in the auth object.
  //   This could move into the User object instead.
  // * NotifyIdTokenListeners() is called when the token changes for some reason
  //   rather than just relying upon the platform token listener.
  // * Token listener notifications are disabled when the token isn't being
  //   refreshed.
  // * JNI callback registration currently occurs via the auth object.
  // * The future interface backing async operations are stored in the auth
  //   object.
  AuthData* auth_internal_;
  jobject platform_user_;
  // TODO(smiles): Determine whether this is still required as b/91267104
  // was not reproducible by the auth team.
  // Tracks if the Id Token listener is expecting a callback to occur.
  bool expect_id_token_listener_callback_;
  // Guards expect_id_token_listener_callback_.
  Mutex expect_id_token_mutex_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_ANDROID_USER_ANDROID_H_