// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "auth/src/desktop/rpcs/error_codes.h"

#include <string>
#include <unordered_map>

namespace firebase {
namespace auth {

namespace {

const std::unordered_map<std::string, AuthError>& GetBackendErrorMap() {
  static const auto* const kMap = new std::unordered_map<std::string, AuthError>{
      {"INVALID_CUSTOM_TOKEN", kAuthErrorInvalidCustomToken},
      {"CREDENTIAL_MISMATCH", kAuthErrorCustomTokenMismatch},
      {"INVALID_IDP_RESPONSE", kAuthErrorInvalidCredential},
      {"INVALID_LOGIN_CREDENTIALS", kAuthErrorInvalidCredential},
      {"INVALID_REQUEST_URI", kAuthErrorInvalidCredential},
      {"USER_DISABLED", kAuthErrorUserDisabled},
      // kAuthErrorOperationNotAllowed is returned for two different backend
      // errors.
      {"OPERATION_NOT_ALLOWED", kAuthErrorOperationNotAllowed},
      {"PASSWORD_LOGIN_DISABLED", kAuthErrorOperationNotAllowed},
      {"EMAIL_EXISTS", kAuthErrorEmailAlreadyInUse},
      {"CREDENTIAL_TOO_OLD_LOGIN_AGAIN", kAuthErrorRequiresRecentLogin},
      {"FEDERATED_USER_ID_ALREADY_LINKED", kAuthErrorCredentialAlreadyInUse},
      {"INVALID_EMAIL", kAuthErrorInvalidEmail},
      {"INVALID_PASSWORD", kAuthErrorWrongPassword},
      // kAuthErrorUserNotFound is returned for two different backend errors.
      {"USER_NOT_FOUND", kAuthErrorUserNotFound},
      {"EMAIL_NOT_FOUND", kAuthErrorUserNotFound},
      {"INVALID_PROVIDER_ID : Provider Id is not supported.",
       kAuthErrorNoSuchProvider},
      {"INVALID_PROVIDER_ID", kAuthErrorInvalidProviderId},
      {"INVALID_ID_TOKEN", kAuthErrorInvalidUserToken},
      {"INVALID_REFRESH_TOKEN", kAuthErrorInvalidUserToken},
      {"MISSING_REFRESH_TOKEN", kAuthErrorInvalidUserToken},
      {"INVALID_GRANT_TYPE", kAuthErrorInvalidUserToken},
      {"TOKEN_EXPIRED", kAuthErrorUserTokenExpired},
      {"<<Network Error>>", kAuthErrorNetworkRequestFailed},
      {"EXPIRED_OOB_CODE", kAuthErrorExpiredActionCode},
      {"INVALID_OOB_CODE", kAuthErrorInvalidActionCode},
      {"INVALID_MESSAGE_PAYLOAD", kAuthErrorInvalidMessagePayload},
      {"INVALID_PHONE_NUMBER", kAuthErrorInvalidPhoneNumber},
      {"MISSING_PHONE_NUMBER", kAuthErrorMissingPhoneNumber},
      {"INVALID_RECIPIENT_EMAIL", kAuthErrorInvalidRecipientEmail},
      {"INVALID_SENDER", kAuthErrorInvalidSender},
      {"INVALID_CODE", kAuthErrorInvalidVerificationCode},
      {"INVALID_SESSION_INFO", kAuthErrorInvalidVerificationId},
      {"MISSING_CODE", kAuthErrorMissingVerificationCode},
      {"MISSING_SESSION_INFO", kAuthErrorMissingVerificationId},
      {"MISSING_EMAIL", kAuthErrorMissingEmail},
      {"MISSING_PASSWORD", kAuthErrorMissingPassword},
      {"QUOTA_EXCEEDED", kAuthErrorQuotaExceeded},
      {"SESSION_EXPIRED", kAuthErrorSessionExpired},
      {"INVALID_APP_CREDENTIAL", kAuthErrorAppNotAuthorized},
      {"MISSING_CLIENT_IDENTIFIER", kAuthErrorMissingClientIdentifier},
      {"MISSING_MFA_PENDING_CREDENTIAL", kAuthErrorMissingMultiFactorSession},
      {"MISSING_MFA_ENROLLMENT_ID", kAuthErrorMissingMultiFactorInfo},
      {"INVALID_MFA_PENDING_CREDENTIAL", kAuthErrorInvalidMultiFactorSession},
      {"MFA_ENROLLMENT_NOT_FOUND", kAuthErrorMultiFactorInfoNotFound},
      {"ADMIN_ONLY_OPERATION", kAuthErrorAdminRestrictedOperation},
      {"UNVERIFIED_EMAIL", kAuthErrorUnverifiedEmail},
      {"SECOND_FACTOR_EXISTS", kAuthErrorSecondFactorAlreadyEnrolled},
      {"SECOND_FACTOR_LIMIT_EXCEEDED",
       kAuthErrorMaximumSecondFactorCountExceeded},
      {"UNSUPPORTED_FIRST_FACTOR", kAuthErrorUnsupportedFirstFactor},
      {"EMAIL_CHANGE_NEEDS_VERIFICATION",
       kAuthErrorEmailChangeNeedsVerification},
      {"MISSING_OR_INVALID_NONCE", kAuthErrorMissingOrInvalidNonce},
      {"TOO_MANY_ATTEMPTS_TRY_LATER", kAuthErrorTooManyRequests},
      {"WEAK_PASSWORD", kAuthErrorWeakPassword},
  };
  return *kMap;
}

}  // namespace

AuthError GetAuthErrorCode(const std::string& error,
                           const std::string& reason) {
  const auto& error_map = GetBackendErrorMap();
  auto found = error_map.find(error);
  if (found == error_map.end()) {
    // Couldn't find it, check again with just the first token before space or
    // colon. Sometimes the backend will return the error code along with some
    // explanatory text (e.g. "WEAK_PASSWORD : ..." or "INVALID_IDP_RESPONSE: ...").
    const size_t delim = error.find_first_of(" :");
    if (delim != std::string::npos) {
      found = error_map.find(error.substr(0, delim));
    }
  }
  if (found != error_map.end()) {
    return found->second;
  }

  if (error.find("TOO_MANY_ATTEMPTS_TRY_LATER") != std::string::npos) {
    return kAuthErrorTooManyRequests;
  } else if (reason == "keyInvalid" ||
             error.find("API key not valid") != std::string::npos) {
    return kAuthErrorInvalidApiKey;
  } else if (reason == "ipRefererBlocked") {
    return kAuthErrorAppNotAuthorized;
  } else if (error.find("WEAK_PASSWORD") != std::string::npos) {
    return kAuthErrorWeakPassword;
  }

  // Never returned from the backend (SDK must check for these):
  // kAuthErrorUserMismatch
  // kAuthErrorProviderAlreadyLinked
  // kAuthErrorAccountExistsWithDifferentCredentials

  return kAuthErrorFailure;
}

const char* GetAuthErrorMessage(const AuthError error_code) {
  switch (error_code) {
    case kAuthErrorNone:
      return "";
    case kAuthErrorUnimplemented:
      return "Operation is not supported on non-mobile systems.";
    case kAuthErrorInvalidCustomToken:
      return "The custom token format is incorrect. Please check the "
             "documentation.";
    case kAuthErrorCustomTokenMismatch:
      return "The custom token corresponds to a different audience.";
    case kAuthErrorInvalidCredential:
      return "The supplied auth credential is malformed or has expired.";
    case kAuthErrorUserDisabled:
      return "The user account has been disabled by an administrator.";
    case kAuthErrorOperationNotAllowed:
      return "This operation is not allowed. You must enable this service in "
             "the console.";
    case kAuthErrorEmailAlreadyInUse:
      return "The email address is already in use by another account.";
    case kAuthErrorRequiresRecentLogin:
      return "This operation is sensitive and requires recent authentication. "
             "Log in again before retrying this request.";
    case kAuthErrorCredentialAlreadyInUse:
      return "This credential is already associated with a different user "
             "account.";
    case kAuthErrorInvalidEmail:
      return "The email address is badly formatted.";
    case kAuthErrorWrongPassword:
      return "The password is invalid or the user does not have a password.";
    case kAuthErrorUserNotFound:
      return "There is no user record corresponding to this identifier. The "
             "user may have been deleted.";
    case kAuthErrorNoSuchProvider:
      return "User was not linked to an account with the given provider.";
    case kAuthErrorInvalidUserToken:
      return "The user's credential is no longer valid. The user must sign in "
             "again.";
    case kAuthErrorUserTokenExpired:
      return "The user's credential has expired. The user must sign in again.";
    case kAuthErrorNetworkRequestFailed:
      return "A network error (such as timeout, interrupted connection or "
             "unreachable host) has occurred.";
    case kAuthErrorExpiredActionCode:
      return "The out of band code has expired.";
    case kAuthErrorInvalidActionCode:
      return "The out of band code is invalid. This can happen if the code is "
             "malformed, expired, or has already been used.";
    case kAuthErrorInvalidMessagePayload:
      return "The email template corresponding to this action contains invalid "
             "characters in its message. Please fix by going to the Auth email "
             "templates section in the Firebase Console.";
    case kAuthErrorInvalidPhoneNumber:
      return "The format of the phone number provided is incorrect. Please "
             "enter the phone number in a format that can be parsed into E.164 "
             "format. E.164 phone numbers are written in the format "
             "[+][country code][subscriber number including area code].";
    case kAuthErrorMissingPhoneNumber:
      return "To send verification codes, provide a phone number for the "
             "recipient.";
    case kAuthErrorInvalidRecipientEmail:
      return "The email corresponding to this action failed to send as the "
             "provided recipient email address is invalid.";
    case kAuthErrorInvalidSender:
      return "The email template corresponding to this action contains an "
             "invalid sender email or name. Please fix by going to the Auth "
             "email templates section in the Firebase Console.";
    case kAuthErrorInvalidVerificationCode:
      return "The sms verification code used to create the phone auth "
             "credential is invalid. Please resend the verification code sms "
             "and be sure use the verification code provided by the user.";
    case kAuthErrorInvalidVerificationId:
      return "The verification ID used to create the phone auth credential is "
             "invalid.";
    case kAuthErrorMissingVerificationCode:
      return "The Phone Auth Credential was created with an empty sms "
             "verification Code";
    case kAuthErrorMissingVerificationId:
      return "The Phone Auth Credential was created with an empty verification "
             "ID";
    case kAuthErrorMissingEmail:
      return "An email address must be provided.";
    case kAuthErrorMissingPassword:
      return "A password must be provided.";
    case kAuthErrorQuotaExceeded:
      return "The project's quota for this operation has been exceeded.";
    case kAuthErrorRetryPhoneAuth:
      return "One or more of the credentials passed to a method failed to "
             "identify or authenticate the user.";
    case kAuthErrorSessionExpired:
      return "The sms code has expired. Please re-send the verification code "
             "to try again.";
    case kAuthErrorAppNotVerified:
      return "The app could not be verified by Firebase during phone number "
             "authentication.";
    case kAuthErrorAppVerificationFailed:
      return "A general failure occurred during the app verification flow.";
    case kAuthErrorCaptchaCheckFailed:
      return "The reCAPTCHA token is not valid.";
    case kAuthErrorInvalidAppCredential:
      return "An invalid app credential was used in the request.";
    case kAuthErrorMissingAppCredential:
      return "The app credential is missing in the request.";
    case kAuthErrorInvalidClientId:
      return "The client ID used to invoke a web flow is invalid.";
    case kAuthErrorInvalidContinueUri:
      return "The domain specified in the continue URI is not valid.";
    case kAuthErrorMissingContinueUri:
      return "A continue URI must be provided in the request.";
    case kAuthErrorKeychainError:
      return "An error occurred while attempting to access the keychain.";
    case kAuthErrorMissingAppToken:
      return "The device token could not be obtained.";
    case kAuthErrorMissingIosBundleId:
      return "The iOS bundle ID is missing when an iOS App Store ID is "
             "provided.";
    case kAuthErrorNotificationNotForwarded:
      return "The app failed to forward remote notification to Firebase Auth.";
    case kAuthErrorUnauthorizedDomain:
      return "The domain specified in the continue URL is not allowlisted in "
             "the Firebase console.";
    case kAuthErrorWebContextAlreadyPresented:
      return "An attempt was made to present a new web context while one was "
             "already being presented.";
    case kAuthErrorWebContextCancelled:
      return "The URL presentation was cancelled prematurely by the user.";
    case kAuthErrorDynamicLinkNotActivated:
      return "Dynamic Links in the Firebase Console is not activated.";
    case kAuthErrorCancelled:
      return "The operation was cancelled.";
    case kAuthErrorAppNotAuthorized:
      return "This app is not authorized to use Firebase Authentication. "
             "Please verify that the correct configuration is set in the "
             "Firebase Console.";
    case kAuthErrorTooManyRequests:
      return "We have blocked all requests from this device due to unusual "
             "activity. Try again later.";
    case kAuthErrorInvalidApiKey:
      return "Your API key is invalid, please check you have copied it "
             "correctly.";
    case kAuthErrorWeakPassword:
      return "The given password is invalid.";
    case kAuthErrorUserMismatch:
      return "The supplied credentials do not correspond to the previously "
             "signed in user";
    case kAuthErrorAccountExistsWithDifferentCredentials:
      return "An account already exists with the same email address but "
             "different sign-in credentials. Sign in using a provider "
             "associated with this email address.";
    case kAuthErrorApiNotAvailable:
      return "The API that you are calling is not available on desktop.";
    case kAuthErrorProviderAlreadyLinked:
      return "User has already been linked to the given provider.";
    case kAuthErrorNoSignedInUser:
      return "Please sign in before trying to get a token.";
    case kAuthErrorInvalidProviderId:
      return "The requested provider ID is invalid. You must enable this "
             "provider in the console.";
    case kAuthErrorWebInternalError:
      return "An internal error occurred during a web operation.";
    case kAuthErrorWebStorageUnsupported:
      return "3rd party cookies or data are disabled, or there was a problem "
             "with the browser.";
    case kAuthErrorTenantIdMismatch:
      return "The provided tenant ID does not match the Auth instance's "
             "tenant ID.";
    case kAuthErrorUnsupportedTenantOperation:
      return "The requested operation is not supported in a multi-tenant "
             "context.";
    case kAuthErrorInvalidLinkDomain:
      return "The domain used for an out of band code flow is either not "
             "configured or is unauthorized for the current project.";
    case kAuthErrorRejectedCredential:
      return "The credential-related request data is invalid.";
    case kAuthErrorPhoneNumberNotFound:
      return "The phone number provided does not correspond to a phone second "
             "factor for the user.";
    case kAuthErrorInvalidTenantId:
      return "The tenant ID provided is invalid.";
    case kAuthErrorMissingClientIdentifier:
      return "This request is missing a reCAPTCHA token.";
    case kAuthErrorMissingMultiFactorSession:
      return "The request is missing proof of first factor successful sign-in.";
    case kAuthErrorMissingMultiFactorInfo:
      return "No second factor identifier is provided.";
    case kAuthErrorInvalidMultiFactorSession:
      return "The request does not contain a valid proof of first factor "
             "successful sign-in.";
    case kAuthErrorMultiFactorInfoNotFound:
      return "The user does not have a second factor matching the identifier "
             "provided.";
    case kAuthErrorAdminRestrictedOperation:
      return "This operation is restricted to administrators only.";
    case kAuthErrorUnverifiedEmail:
      return "This operation requires a verified email.";
    case kAuthErrorSecondFactorAlreadyEnrolled:
      return "The second factor is already enrolled on this account.";
    case kAuthErrorMaximumSecondFactorCountExceeded:
      return "The maximum allowed number of second factors on a user has been "
             "exceeded.";
    case kAuthErrorUnsupportedFirstFactor:
      return "Enrolling a second factor or signing in with a multi-factor "
             "account requires sign-in with a supported first factor.";
    case kAuthErrorEmailChangeNeedsVerification:
      return "Multi-factor users must always have a verified email.";
    case kAuthErrorInvalidEventHandler:
      return "The provided event handler is null or invalid.";
    case kAuthErrorFederatedProviderAlreadyInUse:
      return "Provider operation already in progress.";
    case kAuthErrorInvalidAuthenticatedUserData:
      return "One or more fields of the provided AuthenticatedUserData are "
             "invalid.";
    case kAuthErrorFederatedSignInUserInteractionFailure:
      return "An error occurred during the Federated Auth UI flow.";
    case kAuthErrorMissingOrInvalidNonce:
      return "The request is missing a valid nonce.";
    case kAuthErrorUserCancelled:
      return "The user did not authorize the application during sign-in.";
    case kAuthErrorUnsupportedPassthroughOperation:
      return "The requested operation is not supported in passthrough mode.";
    case kAuthErrorTokenRefreshUnavailable:
      return "Token refresh is unavailable because no refresh token is "
             "present.";
    case kAuthErrorFailure:
      return "An internal error has occurred.";
  }
  return "";
}

}  // namespace auth
}  // namespace firebase
