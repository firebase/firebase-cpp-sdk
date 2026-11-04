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
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
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
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kEmailCodes[] = {
    {"ERROR_INVALID_SENDER", kAuthErrorInvalidSender},
    {"ERROR_INVALID_RECIPIENT_EMAIL", kAuthErrorInvalidRecipientEmail},
    {"ERROR_INVALID_MESSAGE_PAYLOAD", kAuthErrorInvalidMessagePayload},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kWeakPasswordCodes[] = {
    {"ERROR_WEAK_PASSWORD", kAuthErrorWeakPassword},
    {nullptr, kAuthErrorNone},
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
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kUserCodes[] = {
    {"ERROR_USER_DISABLED", kAuthErrorUserDisabled},
    {"ERROR_USER_NOT_FOUND", kAuthErrorUserNotFound},
    {"ERROR_INVALID_USER_TOKEN", kAuthErrorInvalidUserToken},
    {"ERROR_USER_TOKEN_EXPIRED", kAuthErrorUserTokenExpired},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kRecentLoginCodes[] = {
    {"ERROR_REQUIRES_RECENT_LOGIN", kAuthErrorRequiresRecentLogin},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kUserCollisionCodes[] = {
    {"ERROR_ACCOUNT_EXISTS_WITH_DIFFERENT_CREDENTIAL",
     kAuthErrorAccountExistsWithDifferentCredentials},
    {"ERROR_CREDENTIAL_ALREADY_IN_USE", kAuthErrorCredentialAlreadyInUse},
    {"ERROR_EMAIL_ALREADY_IN_USE", kAuthErrorEmailAlreadyInUse},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kWebCodes[] = {
    {"ERROR_WEB_CONTEXT_ALREADY_PRESENTED",
     kAuthErrorWebContextAlreadyPresented},
    {"ERROR_WEB_CONTEXT_CANCELED", kAuthErrorWebContextCancelled},
    {"ERROR_WEB_INTERNAL_ERROR", kAuthErrorWebInternalError},
    {"ERROR_WEB_STORAGE_UNSUPPORTED", kAuthErrorWebStorageUnsupported},
    {nullptr, kAuthErrorNone},
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
    {"ERROR_USER_CANCELLED", kAuthErrorUserCancelled},
    {"ERROR_UNSUPPORTED_PASSTHROUGH_OPERATION",
     kAuthErrorUnsupportedPassthroughOperation},
    {"ERROR_TOKEN_REFRESH_UNAVAILABLE", kAuthErrorTokenRefreshUnavailable},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kTooManyRequestsCodes[] = {
    {"The sms quota for this project has been exceeded.",
     kAuthErrorQuotaExceeded},
    {nullptr, kAuthErrorNone},
};
static const ErrorCodeMapping kFirebaseCodes[] = {
    {"An internal error has occurred.", kAuthErrorFailure},
    {"Please sign in before trying", kAuthErrorNoSignedInUser},
    {"User was not linked", kAuthErrorNoSuchProvider},
    {"User has already been linked", kAuthErrorProviderAlreadyLinked},
    {nullptr, kAuthErrorNone},
};

JNI_METHOD_LOOKUP_DECLARATION(
    api_not_available_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/FirebaseApiNotAvailableException")
JNI_METHOD_LOOKUP_DECLARATION(
    action_code_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthActionCodeException")
JNI_METHOD_LOOKUP_DECLARATION(
    email_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthEmailException")

#define WEAK_PASSWORD_METHODS(X) \
  X(GetReason, "getReason", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(
    weak_password_exception, WEAK_PASSWORD_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthWeakPasswordException")

JNI_METHOD_LOOKUP_DECLARATION(
    invalid_credentials_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthInvalidCredentialsException")
JNI_METHOD_LOOKUP_DECLARATION(
    invalid_user_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthInvalidUserException")
JNI_METHOD_LOOKUP_DECLARATION(
    recent_login_required_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthRecentLoginRequiredException")

#define USER_COLLISION_METHODS(X)                  \
  X(GetUpdatedCredential, "getUpdatedCredential",  \
    "()Lcom/google/firebase/auth/AuthCredential;", \
    jni::kMethodTypeInstance, jni::kMethodOptional)
JNI_METHOD_LOOKUP_DECLARATION(
    user_collision_exception, USER_COLLISION_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthUserCollisionException")

JNI_METHOD_LOOKUP_DECLARATION(
    android_web_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/auth/FirebaseAuthWebException")

#define AUTH_EXCEPTION_METHODS(X) \
  X(GetErrorCode, "getErrorCode", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(firebase_auth_exception, AUTH_EXCEPTION_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/FirebaseAuthException")

JNI_METHOD_LOOKUP_DECLARATION(
    firebase_network_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/FirebaseNetworkException")
JNI_METHOD_LOOKUP_DECLARATION(
    too_many_requests_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE,
    "com/google/firebase/FirebaseTooManyRequestsException")
JNI_METHOD_LOOKUP_DECLARATION(firebase_exception, METHOD_LOOKUP_NONE,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/FirebaseException")

bool CacheCommonMethodIds(JNIEnv* env, jobject activity) {
  android_web_exception::CacheMethodIds(env, activity);
  (void)::firebase::jni::CheckAndClearException(env);
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
  if (android_web_exception::GetClass() != nullptr) {
    android_web_exception::ReleaseClass(env);
  }
}

namespace {
Mutex g_pretriggered_id_token_mutex;
std::map<AuthData*, int> g_pretriggered_id_token_counts;
}  // namespace

void RecordPretriggeredIdTokenCallback(AuthData* auth_data) {
  if (!auth_data) return;
  MutexLock lock(g_pretriggered_id_token_mutex);
  ++g_pretriggered_id_token_counts[auth_data];
}

bool ConsumePretriggeredIdTokenCallback(AuthData* auth_data) {
  if (!auth_data) return false;
  MutexLock lock(g_pretriggered_id_token_mutex);
  auto it = g_pretriggered_id_token_counts.find(auth_data);
  if (it != g_pretriggered_id_token_counts.end() && it->second > 0) {
    if (--it->second == 0) g_pretriggered_id_token_counts.erase(it);
    return true;
  }
  return false;
}

void ClearPretriggeredIdTokenCallbacks(AuthData* auth_data) {
  if (!auth_data) return;
  MutexLock lock(g_pretriggered_id_token_mutex);
  g_pretriggered_id_token_counts.erase(auth_data);
}

static std::string GetFirebaseAuthExceptionErrorCode(JNIEnv* env,
                                                     jobject exception) {
  const jobject j_error_code = env->CallObjectMethod(
      exception, firebase_auth_exception::GetMethodId(
                     firebase_auth_exception::kGetErrorCode));
  (void)jni::CheckAndClearException(env);
  return ::firebase::jni::JStringToString(env, j_error_code);
}

AuthError ErrorCodeFromException(JNIEnv* env, jobject exception) {
  if (!exception) return kAuthErrorNone;

  std::string error =
      env->IsInstanceOf(exception, firebase_auth_exception::GetClass())
          ? GetFirebaseAuthExceptionErrorCode(env, exception)
          : ::firebase::jni::ExtractExceptionMessage(
                env, static_cast<jthrowable>(exception));

  const struct ClassMap {
    jclass exception_class;
    const ErrorCodeMapping* error_map;
    AuthError catch_all;
    bool prefix_match;
  } exception_map[] = {
      {api_not_available_exception::GetClass(), nullptr,
       kAuthErrorApiNotAvailable, false},
      {action_code_exception::GetClass(), kActionCodes, kAuthErrorFailure,
       false},
      {email_exception::GetClass(), kEmailCodes, kAuthErrorFailure, false},
      {weak_password_exception::GetClass(), kWeakPasswordCodes,
       kAuthErrorWeakPassword, false},
      {invalid_credentials_exception::GetClass(), kCredentialCodes,
       kAuthErrorInvalidCredential, false},
      {invalid_user_exception::GetClass(), kUserCodes, kAuthErrorFailure, false},
      {recent_login_required_exception::GetClass(), kRecentLoginCodes,
       kAuthErrorRequiresRecentLogin, false},
      {user_collision_exception::GetClass(), kUserCollisionCodes,
       kAuthErrorFailure, false},
      {android_web_exception::GetClass(), kWebCodes, kAuthErrorWebInternalError,
       false},
      {firebase_auth_exception::GetClass(), kFirebaseAuthCodes,
       kAuthErrorFailure, false},
      {firebase_network_exception::GetClass(), nullptr,
       kAuthErrorNetworkRequestFailed, false},
      {too_many_requests_exception::GetClass(), kTooManyRequestsCodes,
       kAuthErrorTooManyRequests, true},
      {firebase_exception::GetClass(), kFirebaseCodes, kAuthErrorFailure, true},
  };

  for (const auto& entry : exception_map) {
    if (!entry.exception_class ||
        !env->IsInstanceOf(exception, entry.exception_class)) {
      continue;
    }
    for (const ErrorCodeMapping* m = entry.error_map; m && m->error_str; ++m) {
      const bool matched = entry.prefix_match
                               ? (error.compare(0, strlen(m->error_str),
                                                m->error_str) == 0)
                               : (error == m->error_str);
      if (!matched) continue;
      if (m->result_error == kAuthErrorFailure) {
        if (error.find("EMAIL_NOT_FOUND") != std::string::npos ||
            error.find("USER_NOT_FOUND") != std::string::npos)
          return kAuthErrorUserNotFound;
        if (error.find("INVALID_PASSWORD") != std::string::npos ||
            error.find("INVALID_LOGIN_CREDENTIALS") != std::string::npos)
          return kAuthErrorWrongPassword;
        if (error.find("USER_DISABLED") != std::string::npos)
          return kAuthErrorUserDisabled;
        if (error.find("EMAIL_EXISTS") != std::string::npos)
          return kAuthErrorEmailAlreadyInUse;
        if (error.find("OPERATION_NOT_ALLOWED") != std::string::npos)
          return kAuthErrorOperationNotAllowed;
        if (error.find("TOO_MANY_ATTEMPTS_TRY_LATER") != std::string::npos)
          return kAuthErrorTooManyRequests;
        if (error.find("INVALID_IDP_RESPONSE") != std::string::npos ||
            error.find("INVALID_CREDENTIAL") != std::string::npos)
          return kAuthErrorInvalidCredential;
      }
      return m->result_error;
    }
    return entry.catch_all;
  }
  return kAuthErrorFailure;
}

AuthError CheckAndClearJniAuthExceptions(JNIEnv* env,
                                         std::string* error_message) {
  jobject exception = env->ExceptionOccurred();
  if (!exception) return kAuthErrorNone;
  env->ExceptionClear();
  AuthError error_code = ErrorCodeFromException(env, exception);
  if (error_message) {
    *error_message = ::firebase::jni::ExtractExceptionMessage(
        env, static_cast<jthrowable>(exception));
  }
  env->DeleteLocalRef(exception);
  return error_code;
}

AuthError MapFutureCallbackResultToAuthError(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
    bool* success) {
  *success = (result_code == util::kFutureResultSuccess);
  if (result_code == util::kFutureResultSuccess) return kAuthErrorNone;
  if (result_code == util::kFutureResultFailure)
    return ErrorCodeFromException(env, result);
  if (result_code == util::kFutureResultCancelled) return kAuthErrorCancelled;
  return kAuthErrorFailure;
}

void SetImplFromLocalRef(JNIEnv* env, jobject j_local, void** impl) {
  if (*impl != nullptr) {
    env->DeleteGlobalRef(static_cast<jobject>(*impl));
    *impl = nullptr;
  }
  if (j_local != nullptr) {
    *impl = static_cast<void*>(env->NewGlobalRef(j_local));
    env->DeleteLocalRef(j_local);
  }
}

void ReadAuthResult(jobject result, FutureCallbackData<AuthResult>* d,
                    bool success, void* void_data) {
  JNIEnv* env = Env(d->auth_data);
  AuthResult* auth_result = static_cast<AuthResult*>(void_data);

  if (success && result != nullptr) {
    const jobject j_user = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetUser));
    (void)jni::CheckAndClearException(env);
    {
      MutexLock lock(d->auth_data->future_impl.mutex());
      SetImplFromLocalRef(env, j_user, &d->auth_data->user_impl);
      auth_result->user = d->auth_data->auth->current_user();
    }

    const jobject j_additional_user_info = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetAdditionalUserInfo));
    (void)jni::CheckAndClearException(env);
    ReadAdditionalUserInfo(env, j_additional_user_info,
                           &auth_result->additional_user_info);
    if (j_additional_user_info) env->DeleteLocalRef(j_additional_user_info);

    const jobject j_credential = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetCredential));
    (void)jni::CheckAndClearException(env);
    if (j_credential != nullptr) {
      auth_result->credential =
          InternalAuthResultProvider::GetCredential(j_credential);
    }
  } else if (!success && result != nullptr &&
             user_collision_exception::GetClass() != nullptr &&
             env->IsInstanceOf(result, user_collision_exception::GetClass())) {
    jmethodID get_updated_cred = user_collision_exception::GetMethodId(
        user_collision_exception::kGetUpdatedCredential);
    if (get_updated_cred != nullptr) {
      jobject j_updated_cred = env->CallObjectMethod(result, get_updated_cred);
      (void)jni::CheckAndClearException(env);
      if (j_updated_cred != nullptr) {
        auth_result->additional_user_info.updated_credential =
            InternalAuthResultProvider::GetCredential(j_updated_cred);
      }
    }
  }
}

void ReadUserFromAuthResult(jobject result, FutureCallbackData<User>* d,
                            bool success, void* void_data) {
  JNIEnv* env = Env(d->auth_data);
  User* user_ptr = static_cast<User*>(void_data);
  if (success && result != nullptr) {
    const jobject j_user = env->CallObjectMethod(
        result, authresult::GetMethodId(authresult::kGetUser));
    (void)jni::CheckAndClearException(env);
    MutexLock lock(d->auth_data->future_impl.mutex());
    SetImplFromLocalRef(env, j_user, &d->auth_data->user_impl);
    *user_ptr = d->auth_data->auth->current_user();
  } else {
    *user_ptr = User();
  }
}

}  // namespace auth
}  // namespace firebase
