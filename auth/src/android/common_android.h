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

#ifndef FIREBASE_AUTH_SRC_ANDROID_COMMON_ANDROID_H_
#define FIREBASE_AUTH_SRC_ANDROID_COMMON_ANDROID_H_

#include <cstring>

#include "app/src/app_android.h"
#include "app/src/embedded_file.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "auth/src/common.h"
#include "auth/src/include/firebase/auth/credential.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

#define AUTH_RESULT_METHODS(X)                                       \
  X(GetUser, "getUser", "()Lcom/google/firebase/auth/FirebaseUser;") \
  X(GetAdditionalUserInfo, "getAdditionalUserInfo",                  \
    "()Lcom/google/firebase/auth/AdditionalUserInfo;")               \
  X(GetCredential, "getCredential",                                  \
    "()Lcom/google/firebase/auth/AuthCredential;")
JNI_METHOD_LOOKUP_DECLARATION(authresult, AUTH_RESULT_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/AuthResult")

#define ADDITIONAL_USER_INFO_METHODS(X)                     \
  X(GetProviderId, "getProviderId", "()Ljava/lang/String;") \
  X(GetProfile, "getProfile", "()Ljava/util/Map;")          \
  X(GetUsername, "getUsername", "()Ljava/lang/String;")     \
  X(IsNewUser, "isNewUser", "()Z")
JNI_METHOD_LOOKUP_DECLARATION(additional_user_info,
                              ADDITIONAL_USER_INFO_METHODS, FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/AdditionalUserInfo")

#define PHONE_CREDENTIAL_METHODS(X) \
  X(GetSmsCode, "getSmsCode", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(phonecredential, PHONE_CREDENTIAL_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/auth/PhoneAuthCredential")

class InternalAuthResultProvider {
 public:
  static Credential GetCredential(jobject credential);
  static Credential GetCredential(void* impl, AuthError error_code,
                                  const std::string& error_message) {
    Credential c(impl);
    c.error_code_ = error_code;
    c.error_message_ = error_message;
    return c;
  }
};

template <typename T>
struct FutureCallbackData {
  typedef void ReadFutureResultFn(jobject result, FutureCallbackData* d,
                                  bool success, void* void_data);

  FutureCallbackData(const SafeFutureHandle<T>& handle, AuthData* auth_data,
                     ReadFutureResultFn* future_data_read_fn)
      : handle(handle),
        auth_data(auth_data),
        future_data_read_fn(future_data_read_fn) {}

  SafeFutureHandle<T> handle;
  AuthData* auth_data;
  ReadFutureResultFn* future_data_read_fn;
};

void ReadAuthResult(jobject result, FutureCallbackData<AuthResult>* d,
                    bool success, void* void_data);

void ReadUserFromAuthResult(jobject result, FutureCallbackData<User>* d,
                            bool success, void* void_data);

inline void ReadAdditionalUserInfo(JNIEnv* env, jobject j_additional_user_info,
                                   AdditionalUserInfo* info) {
  if (j_additional_user_info == nullptr) {
    *info = AdditionalUserInfo();
    return;
  }
  const jobject j_provider_id = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetProviderId));
  (void)jni::CheckAndClearException(env);
  const jobject j_profile = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetProfile));
  (void)jni::CheckAndClearException(env);
  const jobject j_user_name = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetUsername));
  (void)jni::CheckAndClearException(env);
  const jboolean j_is_new_user = env->CallBooleanMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kIsNewUser));
  (void)jni::CheckAndClearException(env);

  info->provider_id = ::firebase::jni::JStringToString(env, j_provider_id);
  info->user_name = ::firebase::jni::JStringToString(env, j_user_name);
  info->is_new_user = (j_is_new_user != JNI_FALSE);
  if (j_profile) {
    firebase::Variant v = firebase::util::JavaObjectToVariant(env, j_profile);
    if (v.is_map()) info->profile = v.map();
    env->DeleteLocalRef(j_profile);
  }
}

void RecordPretriggeredIdTokenCallback(AuthData* auth_data);
bool ConsumePretriggeredIdTokenCallback(AuthData* auth_data);
void ClearPretriggeredIdTokenCallbacks(AuthData* auth_data);

inline JNIEnv* Env(AuthData* auth_data) { return auth_data->app->GetJNIEnv(); }
void SetImplFromLocalRef(JNIEnv* env, jobject j_local, void** impl);

inline jobject AuthImpl(AuthData* auth_data) {
  return static_cast<jobject>(auth_data->auth_impl);
}
inline jobject UserImpl(AuthData* auth_data) {
  return static_cast<jobject>(auth_data->user_impl);
}
inline void* ImplFromUser(jobject user) { return static_cast<void*>(user); }
inline jobject UserFromImpl(void* impl) { return static_cast<jobject>(impl); }
inline jobject CredentialFromImpl(void* impl) {
  return static_cast<jobject>(impl);
}

inline jobject AcquireUserLocalRef(JNIEnv* env, AuthData* auth_data) {
  if (!env || !auth_data) return nullptr;
  MutexLock lock(auth_data->future_impl.mutex());
  jobject user_impl = UserImpl(auth_data);
  return user_impl != nullptr ? env->NewLocalRef(user_impl) : nullptr;
}

inline void UpdateUserOnSuccess(jobject /*result*/, FutureCallbackData<void>* d,
                                bool success, void* /*void_data*/) {
  if (success) UpdateCurrentUser(d->auth_data);
}

bool CacheUserMethodIds(JNIEnv* env, jobject activity);
void ReleaseUserClasses(JNIEnv* env);
bool CacheCredentialMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<::firebase::internal::EmbeddedFile>& embedded_files);
void ReleaseCredentialClasses(JNIEnv* env);
bool CacheCommonMethodIds(JNIEnv* env, jobject activity);
void ReleaseCommonClasses(JNIEnv* env);

AuthError ErrorCodeFromException(JNIEnv* env, jobject exception);
AuthError CheckAndClearJniAuthExceptions(JNIEnv* env,
                                         std::string* error_message);
AuthError MapFutureCallbackResultToAuthError(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
    bool* success);

template <typename T>
void CompleteAndroidFutureCallback(JNIEnv* env, jobject result,
                                   firebase::util::FutureResult result_code,
                                   const char* status_message,
                                   void* callback_data, bool remap_provider) {
  auto* data = static_cast<FutureCallbackData<T>*>(callback_data);
  bool success = false;
  AuthError error =
      MapFutureCallbackResultToAuthError(env, result, result_code, &success);
  if (remap_provider && error == kAuthErrorOperationNotAllowed &&
      (status_message == nullptr ||
       strstr(status_message, "is disabled for this Firebase project") ==
           nullptr)) {
    error = kAuthErrorInvalidProviderId;
  }
  data->auth_data->future_impl.Complete(
      data->handle, error, status_message,
      [result, success, data](void* user_data) {
        if (data->future_data_read_fn != nullptr) {
          data->future_data_read_fn(result, data, success, user_data);
        }
      });
  delete data;
}

template <typename T>
void FutureCallback(JNIEnv* env, jobject result,
                    firebase::util::FutureResult result_code,
                    const char* status_message, void* callback_data) {
  CompleteAndroidFutureCallback<T>(env, result, result_code, status_message,
                                   callback_data, false);
}

template <typename T>
void FederatedAuthProviderFutureCallback(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  CompleteAndroidFutureCallback<T>(env, result, result_code, status_message,
                                   callback_data, true);
}

template <typename T>
void RegisterCallback(
    jobject pending_result, SafeFutureHandle<T> handle, AuthData* auth_data,
    typename FutureCallbackData<T>::ReadFutureResultFn read_result_fn) {
  firebase::util::RegisterCallbackOnTask(
      Env(auth_data), pending_result, FutureCallback<T>,
      new FutureCallbackData<T>(handle, auth_data, read_result_fn),
      auth_data->future_api_id.c_str());
}

template <typename T>
void RegisterFederatedAuthProviderCallback(
    jobject pending_result, SafeFutureHandle<T> handle, AuthData* auth_data,
    typename FutureCallbackData<T>::ReadFutureResultFn read_result_fn) {
  firebase::util::RegisterCallbackOnTask(
      Env(auth_data), pending_result, FederatedAuthProviderFutureCallback<T>,
      new FutureCallbackData<T>(handle, auth_data, read_result_fn),
      auth_data->future_api_id.c_str());
}

template <typename T>
bool CheckAndCompleteFutureOnError(JNIEnv* env,
                                   ReferenceCountedFutureImpl* futures,
                                   const SafeFutureHandle<T>& handle) {
  std::string error_message;
  AuthError error_code = CheckAndClearJniAuthExceptions(env, &error_message);
  if (error_code != kAuthErrorNone) {
    futures->Complete(handle, error_code, error_message.c_str());
    return true;
  }
  return false;
}

template <typename T, typename Func>
void RunAndroidTaskWithPromise(
    AuthData* auth_data, Promise<T> promise,
    typename FutureCallbackData<T>::ReadFutureResultFn* read_fn,
    Func&& make_task) {
  JNIEnv* env = Env(auth_data);
  jobject pending_result = make_task(env);
  if (!CheckAndCompleteFutureOnError(env, &auth_data->future_impl,
                                     promise.handle())) {
    if (pending_result) {
      RegisterCallback(pending_result, promise.handle(), auth_data, read_fn);
      env->DeleteLocalRef(pending_result);
    } else {
      promise.Fail(kAuthErrorFailure, "User is no longer valid.");
    }
  } else if (pending_result) {
    env->DeleteLocalRef(pending_result);
  }
}

template <typename T, typename Func>
void RunAndroidUserTaskWithPromise(
    AuthData* auth_data, Promise<T> promise,
    typename FutureCallbackData<T>::ReadFutureResultFn* read_fn,
    Func&& make_task) {
  if (!ValidUser(auth_data)) {
    promise.Fail(kAuthErrorFailure, "Invalid user");
    return;
  }
  RunAndroidTaskWithPromise(auth_data, promise, read_fn,
                            [&](JNIEnv* env) -> jobject {
                              jobject user_local =
                                  AcquireUserLocalRef(env, auth_data);
                              if (!user_local) return nullptr;
                              jobject task = make_task(env, user_local);
                              env->DeleteLocalRef(user_local);
                              return task;
                            });
}

template <typename T>
inline bool ValidateCredentialForPromise(Promise<T>& promise,
                                         const Credential& credential) {
  AuthError err = internal::CredentialAccess::GetErrorCode(credential);
  if (err != kAuthErrorNone) {
    promise.Fail(err,
                 internal::CredentialAccess::GetErrorMessage(credential).c_str());
    return false;
  }
  if (!credential.is_valid()) {
    promise.Fail(kAuthErrorInvalidCredential, "Invalid credential.");
    return false;
  }
  return true;
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_ANDROID_COMMON_ANDROID_H_
