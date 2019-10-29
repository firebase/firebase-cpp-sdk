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

#include "auth/src/android/common_android.h"

#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"

namespace firebase {
namespace auth {

struct ErrorCodeMapping {
  const char* error_str;
  AuthError result_error;
};

static const ErrorCodeMapping kActionCodes[] = {
    {"ERROR_EXPIRED_ACTION_CODE", kAuthErrorExpiredActionCode},
    {"ERROR_INVALID_ACTION_CODE", kAuthErrorInvalidActionCode},
    {nullptr},
};
static const ErrorCodeMapping kEmailCodes[] = {
    {"ERROR_INVALID_SENDER", kAuthErrorInvalidSender},
    {"ERROR_INVALID_RECIPIENT_EMAIL", kAuthErrorInvalidRecipientEmail},
    {"ERROR_INVALID_MESSAGE_PAYLOAD", kAuthErrorInvalidMessagePayload},
    {nullptr},
};
static const ErrorCodeMapping kWeakPasswordCodes[] = {
    {"ERROR_WEAK_PASSWORD", kAuthErrorWeakPassword},
    {nullptr},
};
static const ErrorCodeMapping kCredentialCodes[] = {
    {"ERROR_INVALID_CUSTOM_TOKEN", kAuthErrorInvalidCustomToken},
    {"ERROR_CUSTOM_TOKEN_MISMATCH", kAuthErrorCustomTokenMismatch},
    {"ERROR_INVALID_CREDENTIAL", kAuthErrorInvalidCredential},
    {"ERROR_INVALID_EMAIL", kAuthErrorInvalidEmail},
    {"ERROR_WRONG_PASSWORD", kAuthErrorWrongPassword},
    {"ERROR_USER_MISMATCH", kAuthErrorUserMismatch},
    {"ERROR_INVALID_PHONE_NUMBER", kAuthErrorInvalidPhoneNumber},
    {"ERROR_INVALID_VERIFICATION_CODE", kAuthErrorInvalidVerificationCode},
    {"ERROR_INVALID_VERIFICATION_ID", kAuthErrorInvalidVerificationId},
    {"ERROR_MISSING_EMAIL", kAuthErrorMissingEmail},
    {"ERROR_MISSING_PHONE_NUMBER", kAuthErrorMissingPhoneNumber},
    {"ERROR_MISSING_PASSWORD", kAuthErrorMissingPassword},
    {"ERROR_MISSING_VERIFICATION_CODE", kAuthErrorMissingVerificationCode},
    {"ERROR_MISSING_VERIFICATION_ID", kAuthErrorMissingVerificationId},
    {"ERROR_RETRY_PHONE_AUTH", kAuthErrorRetryPhoneAuth},
    {"ERROR_SESSION_EXPIRED", kAuthErrorSessionExpired},
    {"ERROR_REJECTED_CREDENTIAL", kAuthErrorRejectedCredential},
    {"ERROR_PHONE_NUMBER_NOT_FOUND", kAuthErrorPhoneNumberNotFound},
    {"ERROR_MISSING_MULTI_FACTOR_SESSION", kAuthErrorMissingMultiFactorSession},
    {"ERROR_MISSING_MULTI_FACTOR_INFO", kAuthErrorMissingMultiFactorInfo},
    {"ERROR_INVALID_MULTI_FACTOR_SESSION", kAuthErrorInvalidMultiFactorSession},
    {"ERROR_MULTI_FACTOR_INFO_NOT_FOUND", kAuthErrorMultiFactorInfoNotFound},
    {"ERROR_MISSING_OR_INVALID_NONCE", kAuthErrorMissingOrInvalidNonce},
    {nullptr},
};
static const ErrorCodeMapping kUserCodes[] = {
    {"ERROR_USER_DISABLED", kAuthErrorUserDisabled},
    {"ERROR_USER_NOT_FOUND", kAuthErrorUserNotFound},
    {"ERROR_INVALID_USER_TOKEN", kAuthErrorInvalidUserToken},
    {"ERROR_USER_TOKEN_EXPIRED", kAuthErrorUserTokenExpired},
    {nullptr},
};
static const ErrorCodeMapping kRecentLoginCodes[] = {
    {"ERROR_REQUIRES_RECENT_LOGIN", kAuthErrorRequiresRecentLogin},
    {nullptr},
};
static const ErrorCodeMapping kUserCollisionCodes[] = {
    {"ERROR_ACCOUNT_EXISTS_WITH_DIFFERENT_CREDENTIAL",
     kAuthErrorAccountExistsWithDifferentCredentials},
    {"ERROR_CREDENTIAL_ALREADY_IN_USE", kAuthErrorCredentialAlreadyInUse},
    {"ERROR_EMAIL_ALREADY_IN_USE", kAuthErrorEmailAlreadyInUse},
    {nullptr},
};
static const ErrorCodeMapping kWebCodes[] = {
    {"ERROR_WEB_CONTEXT_ALREADY_PRESENTED",
     kAuthErrorWebContextAlreadyPresented},
    {"ERROR_WEB_CONTEXT_CANCELED", kAuthErrorWebContextCancelled},
    {"ERROR_WEB_INTERNAL_ERROR", kAuthErrorWebInternalError},
    {"ERROR_WEB_STORAGE_UNSUPPORTED", kAuthErrorWebStorateUnsupported},
    {nullptr},
};
static const ErrorCodeMapping kFirebaseAuthCodes[] = {
    {"ERROR_APP_NOT_AUTHORIZED", kAuthErrorAppNotAuthorized},
    {"ERROR_OPERATION_NOT_ALLOWED", kAuthErrorOperationNotAllowed},
    {"ERROR_MISSING_CONTINUE_URI", kAuthErrorMissingContinueUri},
    {"ERROR_DYNAMIC_LINK_NOT_ACTIVATED", kAuthErrorDynamicLinkNotActivated},
    {"ERROR_INVALID_PROVIDER_ID", kAuthErrorInvalidProviderId},
    {"ERROR_UNSUPPORTED_TENANT_OPERATION",
     kAuthErrorUnsupportedTenantOperation},
    {"ERROR_INVALID_TENANT_ID", kAuthErrorInvalidTenantId},
    {"ERROR_INVALID_DYNAMIC_LINK_DOMAIN", kAuthErrorInvalidLinkDomain},
    {"ERROR_TENANT_ID_MISMATCH", kAuthErrorTenantIdMismatch},
    {"ERROR_MISSING_CLIENT_IDENTIFIER", kAuthErrorMissingClientIdentifier},
    {"ERROR_ADMIN_RESTRICTED_OPERATION", kAuthErrorAdminRestrictedOperation},
    {"ERROR_UNVERIFIED_EMAIL", kAuthErrorUnverifiedEmail},
    {"ERROR_SECOND_FACTOR_ALREADY_ENROLLED",
     kAuthErrorSecondFactorAlreadyEnrolled},
    {"ERROR_MAXIMUM_SECOND_FACTOR_COUNT_EXCEEDED",
     kAuthErrorMaximumSecondFactorCountExceeded},
    {"ERROR_UNSUPPORTED_FIRST_FACTOR", kAuthErrorUnsupportedFirstFactor},
    {"ERROR_EMAIL_CHANGE_NEEDS_VERIFICATION",
     kAuthErrorEmailChangeNeedsVerification},
    {"ERROR_USER_CANCELLED", kAuthErrorMissingOrInvalidNonce},
    {nullptr},
};

// NOTE: THIS IS WHERE IT GETS REALLY DODGY
// The rest of these have to match the exception message.
// However, only enough of the string needs to be present to be unique relative
// to other errors as we only test if the error starts with the strings below.

// "ERROR_NETWORK_REQUEST_FAILED"
// This error string is not checked as we have a catch all for the entire
// FirebaseNetworkException.

// TODO(b/69859374): Add new error codes for v16.
// New error codes:
// ERROR_WEB_NETWORK_REQUEST_FAILED <- maps to a FirebaseNetworkException
// ERROR_INVALID_CERT_HASH
// Also, include mapping for kWebCodes when receiving a FirebaseAuthWebException

// These have no error codes to test, so we have to rely on the message.
static const ErrorCodeMapping kTooManyRequestsCodes[] = {
    // ERROR_QUOTA_EXCEEDED
    {"The sms quota for this project has been exceeded.",
     kAuthErrorQuotaExceeded},
    {nullptr},
};
// These have no error codes to test, so we have to rely on the message.
static const ErrorCodeMapping kFirebaseCodes[] = {
    // ERROR_INTERNAL_ERROR
    {"An internal error has occurred.", kAuthErrorFailure},
    // ERROR_NO_SIGNED_IN_USER
    {"Please sign in before trying", kAuthErrorNoSignedInUser},
    // ERROR_NO_SUCH_PROVIDER
    {"User was not linked", kAuthErrorNoSuchProvider},
    // ERROR_PROVIDER_ALREADY_LINKED
    {"User has already been linked", kAuthErrorProviderAlreadyLinked},
    {nullptr},
};

METHOD_LOOKUP_DEFINITION(authresult,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/AuthResult",
                         AUTH_RESULT_METHODS)

METHOD_LOOKUP_DEFINITION(additional_user_info,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/AdditionalUserInfo",
                         ADDITIONAL_USER_INFO_METHODS)

METHOD_LOOKUP_DECLARATION(api_not_available_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(api_not_available_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseApiNotAvailableException",
                         METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(action_code_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(
    action_code_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthActionCodeException",
    METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(email_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(email_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseAuthEmailException",
                         METHOD_LOOKUP_NONE)

// clang-format off
#define WEAK_PASSWORD_METHODS(X)                                              \
  X(GetReason, "getReason", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(weak_password_exception, WEAK_PASSWORD_METHODS)
METHOD_LOOKUP_DEFINITION(
    weak_password_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthWeakPasswordException",
    WEAK_PASSWORD_METHODS)

METHOD_LOOKUP_DECLARATION(invalid_credentials_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(
    invalid_credentials_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthInvalidCredentialsException",
    METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(invalid_user_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(
    invalid_user_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthInvalidUserException",
    METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(recent_login_required_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(
    recent_login_required_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthRecentLoginRequiredException",
    METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(user_collision_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(
    user_collision_exception,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/auth/FirebaseAuthUserCollisionException",
    METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(android_web_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(android_web_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseAuthWebException",
                         METHOD_LOOKUP_NONE)

// clang-format off
#define AUTH_EXCEPTION_METHODS(X)                                              \
  X(GetErrorCode, "getErrorCode", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(firebase_auth_exception, AUTH_EXCEPTION_METHODS)
METHOD_LOOKUP_DEFINITION(firebase_auth_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/auth/FirebaseAuthException",
                         AUTH_EXCEPTION_METHODS)

METHOD_LOOKUP_DECLARATION(firebase_network_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(firebase_network_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseNetworkException",
                         METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(too_many_requests_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(too_many_requests_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseTooManyRequestsException",
                         METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DECLARATION(firebase_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(firebase_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/FirebaseException",
                         METHOD_LOOKUP_NONE)

bool CacheCommonMethodIds(JNIEnv* env, jobject activity) {
  // FirebaseAuthWebException may not exist. Check whether the class exists
  // first before trying to cache its methods.
  android_web_exception::CacheClass(env, activity, util::kClassOptional);
  if (android_web_exception::GetClass() != nullptr) {
    android_web_exception::CacheMethodIds(env, activity);
  }
  return authresult::CacheMethodIds(env, activity) &&
         additional_user_info::CacheMethodIds(env, activity) &&
         api_not_available_exception::CacheMethodIds(env, activity) &&
         action_code_exception::CacheMethodIds(env, activity) &&
         email_exception::CacheMethodIds(env, activity) &&
         weak_password_exception::CacheMethodIds(env, activity) &&
         invalid_credentials_exception::CacheMethodIds(env, activity) &&
         invalid_user_exception::CacheMethodIds(env, activity) &&
         recent_login_required_exception::CacheMethodIds(env, activity) &&
         user_collision_exception::CacheMethodIds(env, activity) &&
         firebase_auth_exception::CacheMethodIds(env, activity) &&
         firebase_network_exception::CacheMethodIds(env, activity) &&
         too_many_requests_exception::CacheMethodIds(env, activity) &&
         firebase_exception::CacheMethodIds(env, activity);
}

void ReleaseCommonClasses(JNIEnv* env) {
  authresult::ReleaseClass(env);
  additional_user_info::ReleaseClass(env);
  api_not_available_exception::ReleaseClass(env);
  action_code_exception::ReleaseClass(env);
  email_exception::ReleaseClass(env);
  weak_password_exception::ReleaseClass(env);
  invalid_credentials_exception::ReleaseClass(env);
  invalid_user_exception::ReleaseClass(env);
  recent_login_required_exception::ReleaseClass(env);
  user_collision_exception::ReleaseClass(env);
  firebase_auth_exception::ReleaseClass(env);
  firebase_network_exception::ReleaseClass(env);
  too_many_requests_exception::ReleaseClass(env);
  firebase_exception::ReleaseClass(env);
  // Class may not exist - not yet released on Android
  if (android_web_exception::GetClass() != nullptr) {
    android_web_exception::ReleaseClass(env);
  }
}

static std::string GetFirebaseAuthExceptionErrorCode(JNIEnv* env,
                                                     jobject exception) {
  const jobject j_error_code = env->CallObjectMethod(
      exception, firebase_auth_exception::GetMethodId(
                     firebase_auth_exception::kGetErrorCode));
  util::CheckAndClearJniExceptions(env);
  return util::JniStringToString(env, j_error_code);
}

static bool StringStartsWith(const std::string& str, const std::string& start) {
  return str.compare(0, start.length(), start) == 0;
}

AuthError ErrorCodeFromException(JNIEnv* env, jobject exception) {
  if (!exception) return kAuthErrorNone;

  // The exceptions and error mappings are all listed here:
  // go/firebase-exceptions
  //
  // Note: The order of the following if conditions are based on class
  // hierarchy. We have to check for subclasses before the superclasses.
  // The exception types and their inheritance are listed in the comment above
  // each block.

  std::string error;
  if (env->IsInstanceOf(exception, firebase_auth_exception::GetClass())) {
    error = GetFirebaseAuthExceptionErrorCode(env, exception);
  } else {
    error = ::firebase::util::GetMessageFromException(env, exception);
  }

  // This cannot be static because the ::GetClass() call isn't guaranteed to
  // persist as the same pointer.
  const struct ClassMap {
    jclass exception_class;
    const ErrorCodeMapping* error_map;
    AuthError catch_all;
  } exception_map[] = {
      // FirebaseApiNotAvailableException derives from FirebaseException,
      // so error codes are not available. This exception is currently mapped to
      // a single error in c++, so it's a catch all. The error code string would
      // be: "ERROR_API_NOT_AVAILABLE"
      {api_not_available_exception::GetClass(), nullptr,
       kAuthErrorApiNotAvailable},

      // FirebaseAuthActionCodeException
      {action_code_exception::GetClass(), kActionCodes,
       kAuthErrorUnimplemented},

      // FirebaseAuthEmailException
      {email_exception::GetClass(), kEmailCodes, kAuthErrorUnimplemented},

      // FirebaseAuthWeakPasswordException
      {weak_password_exception::GetClass(), kWeakPasswordCodes,
       kAuthErrorUnimplemented},

      // FirebaseAuthInvalidCredentialsException
      {invalid_credentials_exception::GetClass(), kCredentialCodes,
       kAuthErrorUnimplemented},

      // FirebaseAuthInvalidUserException
      {invalid_user_exception::GetClass(), kUserCodes, kAuthErrorUnimplemented},

      // FirebaseAuthRecentLoginRequiredException
      {recent_login_required_exception::GetClass(), kRecentLoginCodes,
       kAuthErrorUnimplemented},

      // FirebaseAuthUserCollisionException
      {user_collision_exception::GetClass(), kUserCollisionCodes,
       kAuthErrorUnimplemented},

      // FirebaseAuthWebException
      {android_web_exception::GetClass(), kWebCodes, kAuthErrorUnimplemented},

      // FirebaseAuthException
      {firebase_auth_exception::GetClass(), kFirebaseAuthCodes,
       kAuthErrorUnimplemented},

      // FirebaseNetworkException
      {firebase_network_exception::GetClass(), nullptr,
       kAuthErrorNetworkRequestFailed},

      // FirebaseTooManyRequestsException
      {too_many_requests_exception::GetClass(), kTooManyRequestsCodes,
       kAuthErrorTooManyRequests},

      // FirebaseException
      {firebase_exception::GetClass(), kFirebaseCodes, kAuthErrorUnimplemented},
  };

  for (size_t i = 0; i < FIREBASE_ARRAYSIZE(exception_map); i++) {
    if (exception_map[i].exception_class &&
        env->IsInstanceOf(exception, exception_map[i].exception_class)) {
      const ErrorCodeMapping* error_map = exception_map[i].error_map;
      while (error_map != nullptr && error_map->error_str != nullptr) {
        if (StringStartsWith(error, error_map->error_str)) {
          // When signing in with an invalid email, the error is a generic
          // message with a long json blob, with the error message contained
          // within, so we need to check for that case.
          if (error_map->result_error == kAuthErrorFailure &&
              error.find("EMAIL_NOT_FOUND") != -1) {
            return kAuthErrorUserNotFound;
          }
          return error_map->result_error;
        }
        error_map++;
      }
      return exception_map[i].catch_all;
    }
  }
  return kAuthErrorUnimplemented;
}

AuthError CheckAndClearJniAuthExceptions(JNIEnv* env,
                                         std::string* error_message) {
  jobject exception = env->ExceptionOccurred();
  if (exception != nullptr) {
    env->ExceptionClear();
    AuthError error_code = ErrorCodeFromException(env, exception);
    *error_message = ::firebase::util::GetMessageFromException(env, exception);
    env->DeleteLocalRef(exception);
    return error_code;
  }
  return kAuthErrorNone;
}

// Checks for Future success and/or Android based Exceptions, and maps them
// to corresonding AuthError codes.
AuthError MapFutureCallbackResultToAuthError(JNIEnv* env, jobject result,
                                             util::FutureResult result_code,
                                             bool* success) {
  *success = false;
  switch (result_code) {
    case util::kFutureResultSuccess:
      *success = true;
      return kAuthErrorNone;
    case util::kFutureResultFailure:
      return ErrorCodeFromException(env, result);
    case util::kFutureResultCancelled:
      return kAuthErrorCancelled;
    default:
      return kAuthErrorFailure;
  }
}

// Convert j_user (a local reference to FirebaseUser) into a global reference,
// delete the local reference, and update the user_impl pointer in auth_data
// to refer to it, deleted the existing auth_data->user_impl reference if it
// already exists.
void SetImplFromLocalRef(JNIEnv* env, jobject j_local, void** impl) {
  // Delete existing global reference before overwriting it.
  if (*impl != nullptr) {
    env->DeleteGlobalRef(static_cast<jobject>(*impl));
    *impl = nullptr;
  }

  // Create new global reference, so it's valid indefinitely.
  if (j_local != nullptr) {
    jobject j_global = env->NewGlobalRef(j_local);
    env->DeleteLocalRef(j_local);
    *impl = static_cast<void*>(j_global);
  }
}

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadSignInResult(jobject result, FutureCallbackData<SignInResult>* d,
                      bool success, void* void_data) {
  JNIEnv* env = Env(d->auth_data);

  // Update the currently signed-in user on success.
  // Note: `result` is only valid when success is true.
  if (success && result != nullptr) {
    // `result` is of type AuthResult.
    const jobject j_user = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetUser));
    util::CheckAndClearJniExceptions(env);

    // Update our pointer to the Android FirebaseUser that we're wrapping.
    // Note: Cannot call UpdateCurrentUser(d->auth_data) because the Java
    //       Auth class has not been updated at this point.
    SetImplFromLocalRef(env, j_user, &d->auth_data->user_impl);

    // Grab the additional user info too.
    // Additional user info is not guaranteed to exist, so could be nullptr.
    const jobject j_additional_user_info = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetAdditionalUserInfo));
    util::CheckAndClearJniExceptions(env);

    // If additional user info exists, assume that the returned data is of
    // type SignInResult (as opposed to just User*).
    SignInResult* sign_in_result = static_cast<SignInResult*>(void_data);

    // Return a pointer to the user and gather the additional data.
    sign_in_result->user = d->auth_data->auth->current_user();
    ReadAdditionalUserInfo(env, j_additional_user_info, &sign_in_result->info);
    env->DeleteLocalRef(j_additional_user_info);
  }
}

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadUserFromSignInResult(jobject result, FutureCallbackData<User*>* d,
                              bool success, void* void_data) {
  JNIEnv* env = Env(d->auth_data);

  // Update the currently signed-in user on success.
  // Note: `result` is only valid when success is true.
  if (success && result != nullptr) {
    // `result` is of type AuthResult.
    const jobject j_user = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetUser));
    util::CheckAndClearJniExceptions(env);

    // Update our pointer to the Android FirebaseUser that we're wrapping.
    // Note: Cannot call UpdateCurrentUser(d->auth_data) because the Java
    //       Auth class has not been updated at this point.
    SetImplFromLocalRef(env, j_user, &d->auth_data->user_impl);
  }

  // Return a pointer to the current user, if the current user is valid.
  User** user_ptr = static_cast<User**>(void_data);
  *user_ptr = d->auth_data->auth->current_user();
}

}  // namespace auth
}  // namespace firebase
