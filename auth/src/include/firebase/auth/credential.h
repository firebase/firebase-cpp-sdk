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

#ifndef FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_CREDENTIAL_H_
#define FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_CREDENTIAL_H_

#include <stdint.h>

#include <string>

#include "firebase/auth/types.h"
#include "firebase/internal/common.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
extern "C" {
#include <objc/objc.h>
}  // extern "C"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {

// Predeclarations.
class App;

/// @cond FIREBASE_APP_INTERNAL
template <typename T>
class Future;
/// @endcond

namespace auth {

#if FIREBASE_PLATFORM_ANDROID
/// An Android Activity from Java.
typedef jobject UIParent;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
/// A pointer to a UIView.
typedef id UIParent;
#else
/// A void pointer for stub classes.
typedef void* UIParent;
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

// Predeclarations.
class Auth;
class User;

// Opaque internal types.
struct AuthData;
class ForceResendingTokenData;
struct PhoneAuthOptions;
struct PhoneAuthProviderData;
struct PhoneListenerData;

/// @brief Authentication credentials for an authentication provider.
///
/// An authentication provider is a service that allows you to authenticate
/// a user. Firebase provides email/password authentication, but there are also
/// external authentication providers such as Facebook.
class Credential {
#ifndef SWIG
  /// @cond FIREBASE_APP_INTERNAL
  friend class EmailAuthProvider;
  friend class FacebookAuthProvider;
  friend class GameCenterAuthProvider;
  friend class GitHubAuthProvider;
  friend class GoogleAuthProvider;
  friend class JniAuthPhoneListener;
  friend class MicrosoftAuthProvider;
  friend class OAuthProvider;
  friend class PhoneAuthProvider;
  friend class PlayGamesAuthProvider;
  friend class TwitterAuthProvider;
  friend class YahooAuthProvider;
  friend class ServiceUpdatedCredentialProvider;
  friend class PhoneAuthCredential;
  friend class InternalAuthResultProvider;
  /// @endcond
#endif  // !SWIG

 private:
  /// Should only be created by `Provider` classes.
  ///
  /// @see EmailAuthProvider::GetCredential
  /// @see FacebookAuthProvider::GetCredential
  /// @see GoogleAuthProvider::GetCredential
  explicit Credential(void* impl) : impl_(impl), error_code_(kAuthErrorNone) {}

 public:
  Credential() : impl_(nullptr), error_code_(kAuthErrorNone) {}
  // Note: Credential is a value type returned by value across the public API;
  // ~Credential() is intentionally non-virtual to avoid adding a vptr and
  // breaking ABI layout compatibility.
  ~Credential();

  /// Copy constructor.
  Credential(const Credential& rhs);

  /// Copy a Credential.
  Credential& operator=(const Credential& rhs);

#if !defined(SWIG)
  /// Move constructor.
  Credential(Credential&& rhs) noexcept;

  /// Move assignment operator.
  Credential& operator=(Credential&& rhs) noexcept;

  /// Equality operator.
  bool operator==(const Credential& rhs) const;

  /// Inequality operator.
  bool operator!=(const Credential& rhs) const { return !(*this == rhs); }
#endif  // !defined(SWIG)

  /// Gets the name of the Identification Provider (IDP) for the credential.
  ///
  /// <SWIG>
  /// @xmlonly
  /// <csproperty name="Provider">
  /// Gets the name of the Identification Provider (IDP) for the credential.
  /// </csproperty>
  /// @endxmlonly
  /// </SWIG>
  std::string provider() const;

  /// Get whether this credential is valid. A credential can be
  /// invalid in an error condition, e.g. empty username/password.
  ///
  /// @returns True if the credential is valid, false otherwise.
  bool is_valid() const;

 protected:
  /// @cond FIREBASE_APP_INTERNAL
  friend class Auth;
  friend class User;

  /// Platform-specific implementation.
  /// For example, FIRAuthCredential* on iOS.
  void* impl_;

  // If not kAuthErrorNone, then use this error code and string to override
  // whatever error we would normally return when trying to sign-in with this
  // credential.
  AuthError error_code_;
  std::string error_message_;
  /// @endcond
};

/// Wraps phone number and verification information for authentication purposes.
class PhoneAuthCredential : public Credential {
 public:
  /// Default constructor.
  PhoneAuthCredential();

  /// Copy constructor.
  PhoneAuthCredential(const PhoneAuthCredential& rhs);

  /// Copy a Credential.
  PhoneAuthCredential& operator=(const PhoneAuthCredential& rhs);

#if !defined(SWIG)
  /// Move constructor.
  PhoneAuthCredential(PhoneAuthCredential&& rhs) noexcept;

  /// Move assignment operator.
  PhoneAuthCredential& operator=(PhoneAuthCredential&& rhs) noexcept;

  using Credential::operator==;
  using Credential::operator!=;

  /// Equality operator.
  bool operator==(const PhoneAuthCredential& credential) const;

  /// Inequality operator.
  bool operator!=(const PhoneAuthCredential& credential) const {
    return !(*this == credential);
  }
#endif  // !defined(SWIG)

  /// Gets the automatically retrieved SMS verification code if applicable.
  /// This method is only supported on Android.
  std::string sms_code() const;

 private:
#ifndef SWIG
  friend class PhoneAuthProvider;
#if FIREBASE_PLATFORM_ANDROID
  friend class JniAuthPhoneListener;
#endif  // FIREBASE_PLATFORM_ANDROID
#endif  // SWIG

  /// Should only be created by the `PhoneAuthProvider` class.
  explicit PhoneAuthCredential(void* impl);

  std::string sms_code_;
};

/// @brief Use email and password to authenticate.
///
/// Allows developers to use the email and password credentials as they could
/// other auth providers.  For example, this can be used to change passwords,
/// log in, etc.
class EmailAuthProvider {
 public:
  /// Generate a credential from the given email and password.
  ///
  /// @param[in] email E-mail to generate the credential from.
  /// @param[in] password Password to use for the new credential.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* email, const char* password);

#if !defined(SWIG)
  /// Generate a credential from the given email and password.
  static Credential GetCredential(const std::string& email,
                                  const std::string& password) {
    return GetCredential(email.c_str(), password.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  EmailAuthProvider() {}
};

/// @brief Use an access token provided by Facebook to authenticate.
class FacebookAuthProvider {
 public:
  /// Generate a credential from the given Facebook token.
  ///
  /// @param[in] access_token Facebook token to generate the credential from.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* access_token);

#if !defined(SWIG)
  /// Generate a credential from the given Facebook token.
  static Credential GetCredential(const std::string& access_token) {
    return GetCredential(access_token.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  FacebookAuthProvider() {}
};

/// @brief GameCenter (Apple) auth provider.
class GameCenterAuthProvider {
 public:
  /// Generate a credential from GameCenter for the current user.
  ///
  /// @return a Future that will be fulfilled with the resulting credential.
  static Future<Credential> GetCredential();

  /// Get the result of the most recent GetCredential() call.
  ///
  /// @return an object which can be used to retrieve the Credential.
  static Future<Credential> GetCredentialLastResult();

  /// Tests to see if the current user is signed in to GameCenter.
  ///
  /// @return true if the user is signed in, false otherwise.
  static bool IsPlayerAuthenticated();

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  GameCenterAuthProvider() {}
};

/// @brief Use an access token provided by GitHub to authenticate.
class GitHubAuthProvider {
 public:
  /// Generate a credential from the given GitHub token.
  ///
  /// @param[in] token The GitHub OAuth access token.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* token);

#if !defined(SWIG)
  /// Generate a credential from the given GitHub token.
  static Credential GetCredential(const std::string& token) {
    return GetCredential(token.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  GitHubAuthProvider() {}
};

/// @brief Use an ID token and access token provided by Google to authenticate.
class GoogleAuthProvider {
 public:
  /// Generate a credential from the given Google ID token and/or access token.
  ///
  /// @param[in] id_token Google Sign-In ID token.
  /// @param[in] access_token Google Sign-In access token.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* id_token,
                                  const char* access_token);

#if !defined(SWIG)
  /// Generate a credential from the given Google ID token and/or access token.
  /// If `id_token` or `access_token` is empty, `nullptr` is passed for that
  /// token to the underlying API.
  static Credential GetCredential(const std::string& id_token,
                                  const std::string& access_token) {
    return GetCredential(id_token.empty() ? nullptr : id_token.c_str(),
                         access_token.empty() ? nullptr : access_token.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  GoogleAuthProvider() {}
};

/// @brief Use an access token provided by Microsoft to authenticate.
class MicrosoftAuthProvider {
 public:
  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  MicrosoftAuthProvider() {}
};

/// @brief OAuth2.0+UserInfo auth provider (OIDC compliant and non-compliant).
class OAuthProvider {
 public:
  /// Generate a credential for an OAuth2 provider.
  ///
  /// @param[in] provider_id Name of the OAuth2 provider (e.g., "apple.com").
  /// @param[in] id_token The authentication token (OIDC only).
  /// @param[in] access_token The access token associated with the Auth
  /// credential to be created, if available.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* provider_id, const char* id_token,
                                  const char* access_token);

  /// Generate a credential for an OAuth2 provider.
  ///
  /// @param[in] provider_id Name of the OAuth2 provider.
  /// @param[in] id_token The authentication token (OIDC only).
  /// @param[in] raw_nonce The raw nonce associated with the Auth credential
  /// being created.
  /// @param[in] access_token The access token associated with the Auth
  /// credential to be created, if available. This value may be null.
  ///
  /// @returns New Credential.
  static Credential GetCredential(const char* provider_id, const char* id_token,
                                  const char* raw_nonce,
                                  const char* access_token);

#if !defined(SWIG)
  /// Generate a credential for an OAuth2 provider.
  /// If `id_token` or `access_token` is empty, `nullptr` is passed for that
  /// token to the underlying API.
  static Credential GetCredential(const std::string& provider_id,
                                  const std::string& id_token,
                                  const std::string& access_token) {
    return GetCredential(provider_id.c_str(),
                         id_token.empty() ? nullptr : id_token.c_str(),
                         access_token.empty() ? nullptr : access_token.c_str());
  }

  /// Generate a credential for an OAuth2 provider with a nonce.
  /// If `access_token` is empty, `nullptr` is passed to the underlying API.
  static Credential GetCredential(const std::string& provider_id,
                                  const std::string& id_token,
                                  const std::string& raw_nonce,
                                  const std::string& access_token) {
    return GetCredential(
        provider_id.c_str(), id_token.c_str(), raw_nonce.c_str(),
        access_token.empty() ? nullptr : access_token.c_str());
  }
#endif  // !defined(SWIG)

 private:
  OAuthProvider() {}
};

/// @brief Use phone number text messages to authenticate.
///
/// Allows developers to use the phone number and SMS verification codes
/// to authenticate a user on a mobile device.
///
/// This class is not supported on tvOS and Desktop platforms.
///
/// The verification flow results in a Credential that can be used to,
/// * Sign in to an existing phone number account/sign up with a new
///   phone number
/// * Link a phone number to a current user. This provider will be added to
///   the user.
/// * Update a phone number on an existing user.
/// * Re-authenticate an existing user. This may be needed when a sensitive
///   operation requires the user to be recently logged in.
///
/// Possible verification flows:
/// (1) User manually enters verification code.
/// @if cpp_examples
///     - App calls @ref VerifyPhoneNumber.
///     - Web verification page is displayed to user where they may need to
///       solve a CAPTCHA. [iOS only].
///     - Auth server sends the verification code via SMS to the provided
///       phone number. App receives verification id via Listener::OnCodeSent().
///     - User receives SMS and enters verification code in app's GUI.
///     - App uses user's verification code to call
///       @ref PhoneAuthProvider::GetCredential.
/// @endif
/// <SWIG>
/// @if swig_examples
///     - App calls @ref VerifyPhoneNumber.
///     - Web verification page is displayed to user where they may need to
///       solve a CAPTCHA. [iOS only].
///     - Auth server sends the verification code via SMS to the provided
///       phone number. App receives verification id via @ref CodeSent.
///     - User receives SMS and enters verification code in app's GUI.
///     - App uses user's verification code to call
///       @ref PhoneAuthProvider::GetCredential.
/// @endif
/// </SWIG>
///
/// (2) SMS is automatically retrieved (Android only).
///     - App calls @ref VerifyPhoneNumber with
///       `PhoneAuthOptions::timeout_milliseconds` > 0.
///     - Auth server sends the verification code via SMS to the provided
///       phone number.
///     - SMS arrives and is automatically retrieved by the operating system.
///       Credential is automatically created and passed to the app via
///       @if cpp_examples
///       Listener::OnVerificationCompleted().
///       @endif
///       <SWIG>
///       @if swig_examples
///       @ref VerificationCompleted.
///       @endif
///       </SWIG>
///
/// (3) Phone number is instantly verified (Android only).
///     - App calls @ref VerifyPhoneNumber.
///     - The operating system validates the phone number without having to
///       send an SMS. Credential is automatically created and passed to
///       the app via
///       @if cpp_examples
///       Listener::OnVerificationCompleted().
///       @endif
///       <SWIG>
///       @if swig_examples
///       @ref VerificationCompleted.
///       @endif
///       </SWIG>
class PhoneAuthProvider {
 public:
  /// @brief Token to maintain current phone number verification session.
  /// Acquired via @ref Listener::OnCodeSent. Used in @ref VerifyPhoneNumber.
  class ForceResendingToken {
   public:
    /// This token will be invalid until it is assigned a value sent via
    /// @ref Listener::OnCodeSent. It can still be passed into
    /// @ref VerifyPhoneNumber, but it will be ignored.
    ForceResendingToken();

    /// Make `this` token refer to the same phone session as `rhs`.
    ForceResendingToken(const ForceResendingToken& rhs);

    /// Releases internal resources when destructing.
    ~ForceResendingToken();

    /// Make `this` token refer to the same phone session as `rhs`.
    ForceResendingToken& operator=(const ForceResendingToken& rhs);

    /// Return true if `rhs` refers to the same phone number session as `this`.
    bool operator==(const ForceResendingToken& rhs) const;

    /// Return true if `rhs` refers to a different phone number session as
    /// `this`.
    bool operator!=(const ForceResendingToken& rhs) const;

    /// Returns true if this token is associated with an active verification
    /// session.
    bool is_valid() const { return data_ != nullptr; }

   private:
    friend class JniAuthPhoneListener;
    friend class PhoneAuthProvider;
    ForceResendingTokenData* data_;
  };

  /// @brief Receive callbacks from @ref VerifyPhoneNumber events.
  ///
  /// Please see @ref PhoneAuthProvider for a sample implementation.
  class Listener {
   public:
    Listener();
    virtual ~Listener();

#if !defined(SWIG)
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
#endif  // !defined(SWIG)

    /// @brief Phone number auto-verification succeeded.
    ///
    /// Called when,
    ///  - auto-sms-retrieval has succeeded--flow (2) in @ref PhoneAuthProvider
    ///  - instant validation has succeeded--flow (3) in @ref PhoneAuthProvider
    ///
    /// @note This callback is never called on iOS, since iOS does not have
    ///    auto-validation.
    ///
    /// @param[in] credential The completed credential from the phone number
    ///    verification flow.
    virtual void OnVerificationCompleted(PhoneAuthCredential credential) = 0;

    /// @brief Phone number verification failed with an error.
    ///
    /// Called when an error occurred doing phone number authentication.
    /// For example,
    ///  - quota exceeded
    ///  - unknown phone number format
    ///
    /// @param[in] error A description of the failure.
    virtual void OnVerificationFailed(const std::string& error) = 0;

    /// @brief SMS message with verification code sent to phone number.
    ///
    /// Called immediately after Auth server sends a verification SMS.
    /// Once receiving this, you can allow users to manually input the
    /// verification code (even if you're also performing auto-verification).
    /// For user manual input case, get the SMS verification code from the user
    /// and then call @ref GetCredential with the user's code.
    ///
    /// @param[in] verification_id Pass to @ref GetCredential along with the
    ///   user-input verification code to complete the phone number verification
    ///   flow.
    /// @param[in] force_resending_token If the user requests that another SMS
    ///    message be sent, use this when you recall @ref VerifyPhoneNumber.
    virtual void OnCodeSent(const std::string& verification_id,
                            const ForceResendingToken& force_resending_token);

    /// @brief The timeout specified in @ref VerifyPhoneNumber has expired.
    ///
    /// Called once `timeout_milliseconds` has passed.
    /// If using auto SMS retrieval, you can choose to block the UI (do not
    /// allow manual input of the verification code) until timeout is hit.
    ///
    /// @note This callback is called immediately on iOS, since iOS does not
    ///    have auto-validation.
    ///
    /// @param[in] verification_id Identify the transaction that has timed out.
    virtual void OnCodeAutoRetrievalTimeOut(const std::string& verification_id);

   private:
    friend class PhoneAuthProvider;

    /// Back-pointer to the data of the PhoneAuthProvider that
    /// @ref VerifyPhoneNumber was called with. Used internally.
    PhoneListenerData* data_;
  };

  /// Start the phone number authentication operation.
  ///
  /// @param[in] options The PhoneAuthOptions struct with a verification
  /// configuration.
  /// @param[in,out] listener Class that receives notification whenever an SMS
  ///    verification event occurs.
  void VerifyPhoneNumber(const PhoneAuthOptions& options,
                         PhoneAuthProvider::Listener* listener);

  /// Generate a credential for the given verification ID and verification code.
  ///
  /// @param[in] verification_id The id returned when sending the verification
  ///    code. Sent to the caller via @ref Listener::OnCodeSent.
  /// @param[in] verification_code The verification code supplied by the user,
  ///    most likely by a GUI where the user manually enters the code
  ///    received in the SMS sent by @ref VerifyPhoneNumber.
  ///
  /// @returns New Credential.
  PhoneAuthCredential GetCredential(const char* verification_id,
                                    const char* verification_code);

#if !defined(SWIG)
  /// Generate a credential for the given verification ID and verification code.
  PhoneAuthCredential GetCredential(const std::string& verification_id,
                                    const std::string& verification_code) {
    return GetCredential(verification_id.c_str(), verification_code.c_str());
  }
#endif  // !defined(SWIG)

  /// Return the PhoneAuthProvider for the specified `auth`.
  ///
  /// @param[in] auth The Auth session for which we want to get a
  ///    PhoneAuthProvider.
  static PhoneAuthProvider& GetInstance(Auth* auth);

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  friend struct AuthData;
  friend class JniAuthPhoneListener;

  // Use @ref GetInstance to access the PhoneAuthProvider.
  PhoneAuthProvider();

  // The PhoneAuthProvider is owned by the Auth class.
  ~PhoneAuthProvider();

#if !defined(SWIG)
  PhoneAuthProvider(const PhoneAuthProvider&) = delete;
  PhoneAuthProvider& operator=(const PhoneAuthProvider&) = delete;
#endif  // !defined(SWIG)

  PhoneAuthProviderData* data_;
};

/// Options object for configuring phone validation flows in @ref
/// PhoneAuthProvider.
struct PhoneAuthOptions {
  /// Constructor.
  PhoneAuthOptions();

  /// @brief Sets the @ref PhoneAuthProvider::ForceResendingToken to force
  /// another verification SMS to be sent before the auto-retrieval timeout.
  ///
  /// If nullptr, assume this is a new phone number to verify. If non-nullptr,
  /// bypass the verification session deduping and force resending a new SMS.
  /// This token is received in @ref PhoneAuthProvider::Listener::OnCodeSent.
  /// This should only be used when the user presses a Resend SMS button.
  PhoneAuthProvider::ForceResendingToken* force_resending_token;

  /// The phone number for sign-in, sign-up, or second factor enrollment.
  std::string phone_number;

  /// The maximum amount of time you’re willing to wait for SMS auto-retrieval
  /// to be completed by the SDK.
  ///
  /// This value is supported on Android devices only.
  ///
  /// The minimum timeout is 30 seconds, and the maximum timeout is 2 minutes.
  /// If you specified a positive value less than 30 seconds, the SDK will
  /// default to 30 seconds. Specifying a timeout that is greater than 120
  /// seconds will result in an IllegalArgumentException being thrown.
  ///
  /// Use 0 to disable SMS-auto-retrieval. This will also cause
  /// @ref PhoneAuthProvider::Listener::OnCodeAutoRetrievalTimeOut to be called
  /// immediately.
  uint32_t timeout_milliseconds;

  /// Sets the context to which the callbacks are scoped, and with which app
  /// verification will be completed.
  ///
  /// On Android, the context should be a jobject referencing an Android
  /// Activity. On Apple platforms, this should be a pointer to UIView.
  /// For any other platforms, the context is ignored.
  ///
  /// If ui_parent isn’t defined (ie: nullptr or nil) then the FirebaseApp’s
  /// default Activity or UIView will be used.
  UIParent ui_parent;
};

/// @brief Use a server auth code provided by Google Play Games to authenticate.
class PlayGamesAuthProvider {
 public:
  /// Generate a credential from the given Server Auth Code.
  ///
  /// @param[in] server_auth_code Play Games Sign in Server Auth Code.
  ///
  /// @return New Credential.
  static Credential GetCredential(const char* server_auth_code);

#if !defined(SWIG)
  /// Generate a credential from the given Server Auth Code.
  static Credential GetCredential(const std::string& server_auth_code) {
    return GetCredential(server_auth_code.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  PlayGamesAuthProvider() {}
};

/// @brief Use a token and secret provided by Twitter to authenticate.
class TwitterAuthProvider {
 public:
  /// Generate a credential from the given Twitter token and secret.
  ///
  /// @param[in] token The Twitter OAuth token.
  /// @param[in] secret The Twitter OAuth secret.
  ///
  /// @return New Credential.
  static Credential GetCredential(const char* token, const char* secret);

#if !defined(SWIG)
  /// Generate a credential from the given Twitter token and secret.
  static Credential GetCredential(const std::string& token,
                                  const std::string& secret) {
    return GetCredential(token.c_str(), secret.c_str());
  }
#endif  // !defined(SWIG)

  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  TwitterAuthProvider() {}
};

/// @brief Use an access token provided by Yahoo to authenticate.
class YahooAuthProvider {
 public:
  /// The string used to identify this provider.
  static const char* const kProviderId;

 private:
  YahooAuthProvider() {}
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_INCLUDE_FIREBASE_AUTH_CREDENTIAL_H_
