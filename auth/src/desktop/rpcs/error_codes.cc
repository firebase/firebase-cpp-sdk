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

#include <algorithm>
#include <iterator>

namespace firebase {
namespace auth {

namespace {

struct BackendErrorToCode {
  const char* const backend_error;
  const AuthError error_code;
};
static const BackendErrorToCode kBackendErrorsToErrorCodeMap[] = {
    {"INVALID_CUSTOM_TOKEN", kAuthErrorInvalidCustomToken},
    {"CREDENTIAL_MISMATCH", kAuthErrorCustomTokenMismatch},
    {"INVALID_IDP_RESPONSE", kAuthErrorInvalidCredential},
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
    {"EMAIL_CHANGE_NEEDS_VERIFICATION", kAuthErrorEmailChangeNeedsVerification},
};

static const struct ErrorCodeToDescription {
  const AuthError error_code;
  const char* const description;
} kErrorCodesToDescriptionMap[] = {
    {kAuthErrorInvalidCustomToken,
     "The custom token format is incorrect. Please check the documentation."},
    {kAuthErrorCustomTokenMismatch,
     "The custom token corresponds to a different audience."},
    {kAuthErrorInvalidCredential,
     "The supplied auth credential is malformed or has expired."},
    {kAuthErrorUserDisabled,
     "The user account has been disabled by an administrator."},
    {kAuthErrorOperationNotAllowed,
     "This operation is not allowed. You must enable this service in the "
     "console."},
    {kAuthErrorEmailAlreadyInUse,
     "The email address is already in use by another account."},
    {kAuthErrorRequiresRecentLogin,
     "This operation is sensitive and requires recent authentication. Log in "
     "again before retrying this request."},
    {kAuthErrorCredentialAlreadyInUse,
     "This credential is already associated with a different user account."},
    {kAuthErrorInvalidEmail, "The email address is badly formatted."},
    {kAuthErrorWrongPassword,
     "The password is invalid or the user does not have a password."},
    {kAuthErrorUserNotFound,
     "There is no user record corresponding to this identifier. The user may "
     "have been deleted."},
    {kAuthErrorNoSuchProvider,
     "User was not linked to an account with the given provider."},
    {kAuthErrorInvalidUserToken,
     "The user's credential is no longer valid. The user must sign in again."},
    {kAuthErrorUserTokenExpired,
     "The user's credential is no longer valid. The user must sign in again."},
    {kAuthErrorNetworkRequestFailed,
     "A network error (such as timeout, interrupted connection or unreachable "
     "host) has occurred."},
    {kAuthErrorExpiredActionCode, "The out of band code has expired."},
    {kAuthErrorInvalidActionCode,
     "The out of band code is invalid. This can happen if the code is "
     "malformed, expired, or has already been used."},
    {kAuthErrorInvalidMessagePayload,
     "The email template corresponding to this action contains invalid "
     "characters in its message. Please fix by going to the Auth email "
     "templates section in the Firebase Console."},
    {kAuthErrorInvalidPhoneNumber,
     "The format of the phone number provided is incorrect. Please enter the "
     "phone number in a format that can be parsed into E.164 format. E.164 "
     "phone numbers are written in the format [+][country code][subscriber "
     "number including area code]."},
    {kAuthErrorMissingPhoneNumber,
     "To send verification codes, provide a phone number for the recipient."},
    {kAuthErrorInvalidRecipientEmail,
     "The email corresponding to this action failed to send as the provided "
     "recipient email address is invalid."},
    {kAuthErrorInvalidSender,
     "The email template corresponding to this action contains an invalid "
     "sender email or name. Please fix by going to the Auth email templates "
     "section in the Firebase Console."},
    {kAuthErrorInvalidVerificationCode,
     "The sms verification code used to create the phone auth credential is "
     "invalid. Please resend the verification code sms and be sure use the "
     "verification code provided by the user."},
    {kAuthErrorInvalidVerificationId,
     "The verification ID used to create the phone auth credential is "
     "invalid."},
    {kAuthErrorMissingVerificationCode,
     "The Phone Auth Credential was created with an empty sms verification "
     "Code"},
    {kAuthErrorMissingVerificationId,
     "The Phone Auth Credential was created with an empty verification ID"},
    {kAuthErrorMissingEmail, "An email address must be provided."},
    {kAuthErrorMissingPassword, "A password must be provided."},
    {kAuthErrorQuotaExceeded,
     "The project's quota for this operation has been exceeded."},
    {kAuthErrorSessionExpired,
     "The sms code has expired. Please re-send the verification code to try "
     "again."},
    {kAuthErrorAppNotAuthorized,
     "This app is not authorized to use Firebase Authentication. Please verify "
     "that the correct package name and SHA-1 are configured in the Firebase "
     "Console."},
    {kAuthErrorTooManyRequests,
     "We have blocked all requests from this device due to unusual activity. "
     "Try again later."},
    {kAuthErrorInvalidApiKey,
     "Your API key is invalid, please check you have copied it correctly."},
    {kAuthErrorWeakPassword, "The given password is invalid."},
    {kAuthErrorUserMismatch,
     "The supplied credentials do not correspond to the previously signed in "
     "user"},
    {kAuthErrorAccountExistsWithDifferentCredentials,
     "An account already exists with the same email address but different "
     "sign-in credentials. Sign in using a provider associated with this email "
     "address."},
    {kAuthErrorApiNotAvailable,
     "The API that you are calling is not available on desktop."},
    {kAuthErrorProviderAlreadyLinked,
     "User has already been linked to the given provider."},
    {kAuthErrorNoSignedInUser, "Please sign in before trying to get a token."},
    {kAuthErrorInvalidProviderId,
     "The requested provider ID is invalid. You must enable this provider in "
     "the console."},
    {kAuthErrorFailure, "An internal error has occurred."},
    {kAuthErrorMissingClientIdentifier,
     "This request is missing a reCAPTCHA token."},
    {kAuthErrorMissingMultiFactorSession,
     "The request is missing proof of first factor successful sign-in."},
    {kAuthErrorMissingMultiFactorInfo,
     "No second factor identifier is provided."},
    {kAuthErrorInvalidMultiFactorSession,
     "The request does not contain a valid proof of first factor successful "
     "sign-in."},
    {kAuthErrorMultiFactorInfoNotFound,
     "The user does not have a second factor matching the identifier "
     "provided."},
    {kAuthErrorAdminRestrictedOperation,
     "This operation is restricted to administrators only."},
    {kAuthErrorUnverifiedEmail, "This operation requires a verified email."},
    {kAuthErrorSecondFactorAlreadyEnrolled,
     "The second factor is already enrolled on this account."},
    {kAuthErrorMaximumSecondFactorCountExceeded,
     "The maximum allowed number of second factors on a user has been "
     "exceeded."},
    {kAuthErrorUnsupportedFirstFactor,
     "Enrolling a second factor or signing in with a multi-factor account "
     "requires sign-in with a supported first factor."},
    {kAuthErrorEmailChangeNeedsVerification,
     "Multi-factor users must always have a verified email."}};

}  // namespace

AuthError GetAuthErrorCode(const std::string& error,
                           const std::string& reason) {
  const auto find_error = [](const std::string& error_to_find) {
    return std::find_if(std::begin(kBackendErrorsToErrorCodeMap),
                        std::end(kBackendErrorsToErrorCodeMap),
                        [&error_to_find](const BackendErrorToCode& rhs) {
                          return error_to_find == rhs.backend_error;
                        });
  };
  auto found = find_error(error);
  if (found == std::end(kBackendErrorsToErrorCodeMap)) {
    // Couldn't find it, check again with just the first word.
    // Sometimes the backend will return the error code along with
    // some explanatory text (see kAuthErrorNoSuchProvider above).
    size_t space = error.find(" ");
    if (space != std::string::npos) {
      std::string first_word = error.substr(0, space);
      found = find_error(first_word);
    }
  }
  if (found != std::end(kBackendErrorsToErrorCodeMap)) {
    return found->error_code;
  }

  // Error is either "TOO_MANY_ATTEMPTS_TRY_LATER" or
  // "TOO_MANY_ATTEMPTS_TRY_LATER : <some additional info>"
  if (error.find("TOO_MANY_ATTEMPTS_TRY_LATER") != std::string::npos) {
    return kAuthErrorTooManyRequests;
  } else if (error == "Bad Request" && reason == "keyInvalid") {
    return kAuthErrorInvalidApiKey;
  } else if (reason == "ipRefererBlocked") {
    return kAuthErrorAppNotAuthorized;
  } else if (error.find("WEAK_PASSWORD") != std::string::npos) {
    // Error is either "WEAK_PASSWORD" or "WEAK_PASSWORD : <some reason>"
    return kAuthErrorWeakPassword;
  }

  // TODO(varconst): handle INVALID_REQUEST_URI

  // Never returned from the backend (SDK must check for these):
  // kAuthErrorUserMismatch
  // kAuthErrorProviderAlreadyLinked
  // kAuthErrorAccountExistsWithDifferentCredentials

  return kAuthErrorFailure;
}

const char* GetAuthErrorMessage(const AuthError error_code) {
  const auto found =
      std::find_if(std::begin(kErrorCodesToDescriptionMap),
                   std::end(kErrorCodesToDescriptionMap),
                   [error_code](const ErrorCodeToDescription& rhs) {
                     return error_code == rhs.error_code;
                   });
  if (found != std::end(kErrorCodesToDescriptionMap)) {
    return found->description;
  }
  return "";
}

}  // namespace auth
}  // namespace firebase
