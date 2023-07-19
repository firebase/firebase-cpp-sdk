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

#ifndef FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_USER_H_
#define FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_USER_H_

#include <string>
#include <vector>

#include "firebase/auth/credential.h"
#include "firebase/auth/types.h"
#include "firebase/future.h"
#include "firebase/internal/common.h"
#include "firebase/variant.h"

namespace firebase {
namespace auth {

// Predeclarations.
class Auth;
struct AuthData;
struct AuthResult;
class FederatedAuthProvider;

/// @brief Interface implemented by each identity provider.
class UserInfoInterface {
 public:
  virtual ~UserInfoInterface();

  /// Gets the unique Firebase user ID for the user.
  ///
  /// @note The user's ID, unique to the Firebase project.
  /// Do NOT use this value to authenticate with your backend server, if you
  /// have one.
  /// @if cpp_examples
  /// Use User::GetToken() instead.
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// Use User.Token instead.
  /// @endif
  /// @xmlonly
  /// <csproperty name="UserId">
  /// Gets the unique Firebase user ID for the user.
  ///
  /// @note The user's ID, unique to the Firebase project.
  /// Do NOT use this value to authenticate with your backend server, if you
  /// have one. Use User.Token instead.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string uid() const;

  /// Gets email associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="Email">
  /// Gets email associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string email() const;

  /// Gets the display name associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="DisplayName">
  /// Gets the display name associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string display_name() const;

  /// Gets the photo url associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="PhotoUrl">
  /// Gets the photo url associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string photo_url() const;

  /// Gets the provider ID for the user (For example, "Facebook").
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="ProviderId">
  /// Gets the provider ID for the user (For example, \"Facebook\").
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string provider_id() const;

  /// Gets the phone number for the user, in E.164 format.
  virtual std::string phone_number() const;

 private:
  friend class User;
  std::string uid_;
  std::string email_;
  std::string display_name_;
  std::string photo_url_;
  std::string provider_id_;
  std::string phone_number_;
};

/// @brief Additional user data returned from an identity provider.
struct AdditionalUserInfo {
  /// The provider identifier.
  std::string provider_id;

  /// The name of the user.
  std::string user_name;

  /// Additional identity-provider specific information.
  /// Most likely a hierarchical key-value mapping, like a parsed JSON file.
  /// Note we use map instead of unordered_map to support older compilers.
  std::map<Variant, Variant> profile;

  /// On a nonce-based credential link failure where the user has already linked
  /// to the provider, the Firebase auth service may provide an updated
  /// Credential. If is_valid returns true on this credential, then it may be
  /// passed to a new firebase::auth::Auth::SignInWithCredential request to sign
  /// the user in with the provider.
  Credential updated_credential;
};

/// @brief Metadata corresponding to a Firebase user.
struct UserMetadata {
  UserMetadata() : last_sign_in_timestamp(0), creation_timestamp(0) {}

  /// The last sign in UTC timestamp in milliseconds.
  /// See https://en.wikipedia.org/wiki/Unix_time for details of UTC.
  uint64_t last_sign_in_timestamp;

  /// The Firebase user creation UTC timestamp in milliseconds.
  uint64_t creation_timestamp;
};

/// @deprecated This structure will be replaced with AuthResult.
///
/// @brief Result of operations that can affect authentication state.
struct SignInResult {
  SignInResult() : user(NULL) {}

  /// The currently signed-in User, or NULL if there isn't any (i.e. the
  /// user is signed out).
  User* user;

  /// Identity-provider specific information for the user, if the provider is
  /// one of Facebook, GitHub, Google, or Twitter.
  AdditionalUserInfo info;

  /// Metadata associated with the Firebase user.
  UserMetadata meta;
};

/// @brief Firebase user account object.
///
/// This class allows you to manipulate the profile of a user, link to and
/// unlink from authentication providers, and refresh authentication tokens.
class User : public UserInfoInterface {
 public:
  // Default constructor - creates an invalid user.
  User();

  /// Parameters to the UpdateUserProfile() function.
  ///
  /// For fields you don't want to update, pass NULL.
  /// For fields you want to reset, pass "".
  struct UserProfile {
    /// Construct a UserProfile with no display name or photo URL.
    UserProfile() : display_name(NULL), photo_url(NULL) {}

    /// User display name.
    const char* display_name;

    /// User photo URI.
    const char* photo_url;
  };

  /// Copy constructor.
  User(const User&);

  /// Assignment operator.
  User& operator=(const User&);

  /// Equality operator.
  bool operator==(const User&) const;

  /// Inequality operator.
  bool operator!=(const User&) const;

  ~User();

  /// Returns whether this User object represents a valid user. Could be false
  /// on Users contained with AuthResult structures from failed Auth
  /// operations.
  bool is_valid() const;

  /// The Java Web Token (JWT) that can be used to identify the user to
  /// the backend.
  ///
  /// If a current ID token is still believed to be valid (i.e. it has not yet
  /// expired), that token will be returned immediately.
  /// A developer may set the optional force_refresh flag to get a new ID token,
  /// whether or not the existing token has expired. For example, a developer
  /// may use this when they have discovered that the token is invalid for some
  /// other reason.
  Future<std::string> GetToken(bool force_refresh);

#if defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)
  /// A "thread safer" version of GetToken.
  /// If called by two threads simultaneously, GetToken can return the same
  /// pending Future twice. This creates problems if both threads try to set
  /// the OnCompletion callback, unaware that there's another copy.
  /// GetTokenThreadSafe returns a proxy to the Future if it's still pending,
  /// allowing each proxy to have their own callback.
  Future<std::string> GetTokenThreadSafe(bool force_refresh);
#endif  // defined(INTERNAL_EXPERIMENTAL) || defined(SWIG)

  /// Get results of the most recent call to GetToken.
  Future<std::string> GetTokenLastResult() const;

  /// Gets the third party profile data associated with this user returned by
  /// the authentication server, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="ProviderData">
  /// Gets the third party profile data associated with this user returned by
  /// the authentication server, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  std::vector<UserInfoInterface> provider_data() const;

  /// @deprecated This is a deprecated method. Please use provider_data()
  /// instead.
  ///
  /// Gets the third party profile data associated with this user returned by
  /// the authentication server, if any.
  FIREBASE_DEPRECATED const std::vector<UserInfoInterface*>&
  provider_data_DEPRECATED() const;

  /// Sets the email address for the user.
  ///
  /// May fail if there is already an email/password-based account for the same
  /// email address.
  Future<void> UpdateEmail(const char* email);

  /// Get results of the most recent call to UpdateEmail.
  Future<void> UpdateEmailLastResult() const;

  /// Attempts to change the password for the current user.
  ///
  /// For an account linked to an Identity Provider (IDP) with no password,
  /// this will result in the account becoming an email/password-based account
  /// while maintaining the IDP link. May fail if the password is invalid,
  /// if there is a conflicting email/password-based account, or if the token
  /// has expired.
  /// To retrieve fresh tokens,
  /// @if cpp_examples
  /// call Reauthenticate.
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// call ReauthenticateAsync.
  /// @endif
  /// </SWIG>
  Future<void> UpdatePassword(const char* password);

  /// Get results of the most recent call to UpdatePassword.
  Future<void> UpdatePasswordLastResult() const;

  /// Convenience function for ReauthenticateAndRetrieveData that discards
  /// the returned AdditionalUserInfo data.
  Future<void> Reauthenticate(const Credential& credential);

  /// Get results of the most recent call to Reauthenticate.
  Future<void> ReauthenticateLastResult() const;

  /// Reauthenticate using a credential.
  ///
  /// @if cpp_examples
  /// Some APIs (for example, UpdatePassword, Delete) require that
  /// the token used to invoke them be from a recent login attempt.
  /// This API takes an existing credential for the user and retrieves fresh
  /// tokens, ensuring that the operation can proceed. Developers can call
  /// this method prior to calling UpdatePassword() to ensure success.
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// Some APIs (for example, UpdatePasswordAsync, DeleteAsync)
  /// require that the token used to invoke them be from a recent login attempt.
  /// This API takes an existing credential for the user and retrieves fresh
  /// tokens, ensuring that the operation can proceed. Developers can call
  /// this method prior to calling UpdatePasswordAsync() to ensure success.
  /// @endif
  /// </SWIG>
  ///
  /// Data from the Identity Provider used to sign-in is returned in the
  /// AdditionalUserInfo inside the returned AuthResult.
  ///
  /// Returns an error if the existing credential is not for this user
  /// or if sign-in with that credential failed.
  ///
  /// @note: The current user may be signed out if this operation fails on
  /// Android and desktop platforms.
  Future<AuthResult> ReauthenticateAndRetrieveData(
      const Credential& credential);

  /// Get results of the most recent call to ReauthenticateAndRetrieveData.
  Future<AuthResult> ReauthenticateAndRetrieveDataLastResult() const;

  /// @deprecated This is a deprecated method. Please use
  /// ReauthenticateAndRetrieveData(const Credential&) instead.
  ///
  /// Reauthenticate using a credential.
  ///
  /// Data from the Identity Provider used to sign-in is returned in the
  /// AdditionalUserInfo inside the returned SignInResult.
  ///
  /// Returns an error if the existing credential is not for this user
  /// or if sign-in with that credential failed.
  ///
  /// @note: The current user may be signed out if this operation fails on
  /// Android and desktop platforms.
  FIREBASE_DEPRECATED Future<SignInResult>
  ReauthenticateAndRetrieveData_DEPRECATED(const Credential& credential);

  /// @deprecated
  ///
  /// Get results of the most recent call to
  /// ReauthenticateAndRetrieveData_DEPRECATED.
  FIREBASE_DEPRECATED Future<SignInResult>
  ReauthenticateAndRetrieveDataLastResult_DEPRECATED() const;

  /// @brief Re-authenticates the user with a federated auth provider.
  ///
  /// @param[in] provider Contains information on the auth provider to
  /// authenticate with.
  /// @return A Future<SignInResult> with the result of the re-authentication
  /// request.
  /// @note: This operation is supported only on iOS, tvOS and Android
  /// platforms. On other platforms this method will return a Future with a
  /// preset error code: kAuthErrorUnimplemented.
  Future<AuthResult> ReauthenticateWithProvider(
      FederatedAuthProvider* provider) const;

  /// @deprecated This is a deprecated method. Please use
  /// ReauthenticateWithProvider(FederatedAuthProvider*) instead.
  ///
  /// @brief Re-authenticates the user with a federated auth provider.
  ///
  /// @param[in] provider Contains information on the auth provider to
  /// authenticate with.
  /// @return A Future<SignInResult> with the result of the re-authentication
  /// request.
  /// @note: This operation is supported only on iOS, tvOS and Android
  /// platforms. On other platforms this method will return a Future with a
  /// preset error code: kAuthErrorUnimplemented.
  FIREBASE_DEPRECATED Future<SignInResult>
  ReauthenticateWithProvider_DEPRECATED(FederatedAuthProvider* provider) const;

  /// Initiates email verification for the user.
  Future<void> SendEmailVerification();

  /// Get results of the most recent call to SendEmailVerification.
  Future<void> SendEmailVerificationLastResult() const;

  /// Updates a subset of user profile information.
  Future<void> UpdateUserProfile(const UserProfile& profile);

  /// Get results of the most recent call to UpdateUserProfile.
  Future<void> UpdateUserProfileLastResult() const;

  /// Links the user with the given 3rd party credentials.
  ///
  /// For example, a Facebook login access token, a Twitter token/token-secret
  /// pair.
  ///
  /// Status will be an error if the token is invalid, expired, or otherwise
  /// not accepted by the server as well as if the given 3rd party
  /// user id is already linked with another user account or if the current user
  /// is already linked with another id from the same provider.
  ///
  /// Data from the Identity Provider used to sign-in is returned in the
  /// AdditionalUserInfo inside AuthResult.
  Future<AuthResult> LinkWithCredential(const Credential& credential);

  /// Get results of the most recent call to LinkWithCredential.
  Future<AuthResult> LinkWithCredentialLastResult() const;

  /// @deprecated This is a deprecated method. Please use
  /// LinkWithCredential(const Credential&) instead.
  ///
  /// Convenience function for ReauthenticateAndRetrieveData that discards
  /// the returned AdditionalUserInfo in SignInResult.
  FIREBASE_DEPRECATED Future<User*> LinkWithCredential_DEPRECATED(
      const Credential& credential);

  /// @deprecated
  ///
  /// Get results of the most recent call to LinkWithCredential_DEPRECATED.
  FIREBASE_DEPRECATED Future<User*> LinkWithCredentialLastResult_DEPRECATED()
      const;

  /// @deprecated This is a deprecated method. Please use
  /// LinkWithCredential(const Credential&) instead.
  ///
  /// Links the user with the given 3rd party credentials.
  ///
  /// For example, a Facebook login access token, a Twitter token/token-secret
  /// pair.
  /// Status will be an error if the token is invalid, expired, or otherwise
  /// not accepted by the server as well as if the given 3rd party
  /// user id is already linked with another user account or if the current user
  /// is already linked with another id from the same provider.
  ///
  /// Data from the Identity Provider used to sign-in is returned in the
  /// AdditionalUserInfo inside SignInResult.
  FIREBASE_DEPRECATED Future<SignInResult> LinkAndRetrieveDataWithCredential(
      const Credential& credential);

  /// @deprecated
  ///
  /// Get results of the most recent call to
  /// LinkAndRetrieveDataWithCredential.
  FIREBASE_DEPRECATED Future<SignInResult>
  LinkAndRetrieveDataWithCredentialLastResult() const;

  ///
  /// @param[in] provider Contains information on the auth provider to link
  /// with.
  /// @return A Future<AuthResult> with the user data result of the link
  /// request.
  ///
  /// @note: This operation is supported only on iOS, tvOS and Android
  /// platforms. On other platforms this method will return a Future with a
  /// preset error code: kAuthErrorUnimplemented.
  Future<AuthResult> LinkWithProvider(FederatedAuthProvider* provider) const;

  /// @deprecated This is a deprecated method. Please use
  /// LinkWithProvider(FederatedAuthProvider*) instead.
  ///
  /// Links this user with a federated auth provider.
  ///
  /// @param[in] provider Contains information on the auth provider to link
  /// with.
  /// @return A Future<SignInResult> with the user data result of the link
  /// request.
  ///
  /// @note: This operation is supported only on iOS, tvOS and Android
  /// platforms. On other platforms this method will return a Future with a
  /// preset error code: kAuthErrorUnimplemented.
  FIREBASE_DEPRECATED Future<SignInResult> LinkWithProvider_DEPRECATED(
      FederatedAuthProvider* provider) const;

  /// Unlinks the current user from the provider specified.
  /// Status will be an error if the user is not linked to the given provider.
  Future<AuthResult> Unlink(const char* provider);

  /// Get results of the most recent call to Unlink.
  Future<AuthResult> UnlinkLastResult() const;

  /// @deprecated This is a deprecated method. Please use Unlink(const
  /// char*) instead.
  ///
  /// Unlinks the current user from the provider specified.
  /// Status will be an error if the user is not linked to the given provider.
  FIREBASE_DEPRECATED Future<User*> Unlink_DEPRECATED(const char* provider);

  /// @deprecated
  ///
  /// Get results of the most recent call to Unlink_DEPRECATED.
  FIREBASE_DEPRECATED Future<User*> UnlinkLastResult_DEPRECATED() const;

  /// Updates the currently linked phone number on the user.
  /// This is useful when a user wants to change their phone number. It is a
  /// shortcut to calling Unlink(phone_credential.provider().c_str())
  /// and then LinkWithCredential(phone_credential). `credential`
  /// must have been created with PhoneAuthProvider.
  Future<User> UpdatePhoneNumberCredential(
      const PhoneAuthCredential& credential);

  /// Get results of the most recent call to
  /// UpdatePhoneNumberCredential.
  Future<User> UpdatePhoneNumberCredentialLastResult() const;

  /// @deprecated This is a deprecated method. Please use
  /// UpdatePhoneNumberCredential(const PhoneAuthCredential&) instead.
  ///
  /// Updates the currently linked phone number on the user.
  /// This is useful when a user wants to change their phone number. It is a
  /// shortcut to calling Unlink_DEPRECATED(phone_credential.provider().c_str())
  /// and then LinkWithCredential_DEPRECATED(phone_credential). `credential`
  /// must have been created with PhoneAuthProvider.
  FIREBASE_DEPRECATED Future<User*> UpdatePhoneNumberCredential_DEPRECATED(
      const Credential& credential);

  /// @deprecated
  ////
  /// Get results of the most recent call to
  /// UpdatePhoneNumberCredential_DEPRECATED.
  FIREBASE_DEPRECATED Future<User*>
  UpdatePhoneNumberCredentialLastResult_DEPRECATED() const;

  /// Refreshes the data for this user.
  ///
  /// For example, the attached providers, email address, display name, etc.
  Future<void> Reload();

  /// Get results of the most recent call to Reload.
  Future<void> ReloadLastResult() const;

  /// Deletes the user account.
  Future<void> Delete();

  /// Get results of the most recent call to Delete.
  Future<void> DeleteLastResult() const;

  /// Gets the metadata for this user account.
  UserMetadata metadata() const;

  /// Returns true if the email address associated with this user has been
  /// verified.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="IsEmailVerified">
  /// True if the email address associated with this user has been verified.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  bool is_email_verified() const;

  /// Returns true if user signed in anonymously.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="IsAnonymous">
  /// True if user signed in anonymously.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  bool is_anonymous() const;

  /// Gets the unique Firebase user ID for the user.
  ///
  /// @note The user's ID, unique to the Firebase project.
  /// Do NOT use this value to authenticate with your backend server, if you
  /// have one.
  /// @if cpp_examples
  /// Use User::GetToken() instead.
  /// @endif
  /// <SWIG>
  /// @if swig_examples
  /// Use User.Token instead.
  /// @endif
  /// @xmlonly
  /// <csproperty name="UserId">
  /// Gets the unique Firebase user ID for the user.
  ///
  /// @note The user's ID, unique to the Firebase project.
  /// Do NOT use this value to authenticate with your backend server, if you
  /// have one. Use User.Token instead.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string uid() const;

  /// Gets email associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="Email">
  /// Gets email associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string email() const;

  /// Gets the display name associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="DisplayName">
  /// Gets the display name associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string display_name() const;

  /// Gets the photo url associated with the user, if any.
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="PhotoUrl">
  /// Gets the photo url associated with the user, if any.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string photo_url() const;

  /// Gets the provider ID for the user (For example, "Facebook").
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="ProviderId">
  /// Gets the provider ID for the user (For example, \"Facebook\").
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  virtual std::string provider_id() const;

  /// Gets the phone number for the user, in E.164 format.
  virtual std::string phone_number() const;

 private:
  friend struct AuthData;
  // Only exists in AuthData. Access via Auth::CurrentUser().
  explicit User(AuthData* auth_data) : auth_data_(auth_data) {}

#if defined(INTERNAL_EXPERIMENTAL)
  // Doxygen should not make docs for this function.
  /// @cond FIREBASE_APP_INTERNAL
  friend class IdTokenRefreshThread;
  friend class IdTokenRefreshListener;
  friend class Auth;
  Future<std::string> GetTokenInternal(const bool force_refresh,
                                       const int future_identifier);
  /// @endcond
#endif  // defined(INTERNAL_EXPERIMENTAL)

  // Use the pimpl mechanism to hide data details in the cpp files.
  AuthData* auth_data_;
};

/// @brief The result of operations that can affect authentication state.
struct AuthResult {
  /// Identity-provider specific information for the user, if the provider is
  /// one of Facebook, GitHub, Google, or Twitter.
  AdditionalUserInfo additional_user_info;

  /// A Credential instance for the recently signed-in user. This is not
  /// supported on desktop platforms.
  Credential credential;

  /// The currently signed-in User, or an invalid User if there isn't
  /// one (i.e. if the user is signed-out then is_valid() will return false).
  User user;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_USER_H_
