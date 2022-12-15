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

#include "app/src/embedded_file.h"
#include "app/src/util_android.h"
#include "auth/src/common.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

// clang-format off
#define AUTH_RESULT_METHODS(X)                                                 \
  X(GetUser, "getUser", "()Lcom/google/firebase/auth/FirebaseUser;",           \
    util::kMethodTypeInstance),                                                \
  X(GetAdditionalUserInfo, "getAdditionalUserInfo",                            \
    "()Lcom/google/firebase/auth/AdditionalUserInfo;",                         \
    util::kMethodTypeInstance)
// clang-format on
METHOD_LOOKUP_DECLARATION(authresult, AUTH_RESULT_METHODS)

// clang-format off
#define ADDITIONAL_USER_INFO_METHODS(X)                                        \
  X(GetProviderId, "getProviderId", "()Ljava/lang/String;"),                   \
  X(GetProfile, "getProfile", "()Ljava/util/Map;"),                            \
  X(GetUsername, "getUsername", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(additional_user_info, ADDITIONAL_USER_INFO_METHODS)

// For each asynchronous call, a pointer to one of these structs is passed
// into Java. When the call completes, Java returns the pointer via a callback
// function. In this way, we match the context of the callback with the call.
template <typename T, typename CONTEXT_T>
struct FutureCallbackData {
  // During the callback, read `result` data from Java into the returned
  // C++ data in `d->future_data->Data()`.
  typedef void ReadFutureResultFn(jobject task_result,
                                  FutureCallbackData* callback_data,
                                  bool success, T* result_data);

  FutureCallbackData(const SafeFutureHandle<T>& handle,
                     ReferenceCountedFutureImpl* future_api,
                     ReadFutureResultFn* future_data_read_fn,
                     CONTEXT_T* context)
      : handle(handle),
        future_api(future_api),
        future_data_read_fn(future_data_read_fn),
        context(context) {}

  SafeFutureHandle<T> handle;
  ReferenceCountedFutureImpl* future_api;
  ReadFutureResultFn* future_data_read_fn;
  CONTEXT_T* context;
};

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadSignInResult(jobject task_result,
                      FutureCallbackData<SignInResult, AuthData>* callback_data,
                      bool success, SignInResult* sign_in_result);

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadUserFromSignInResult(
    jobject task_result, FutureCallbackData<User*, AuthData>* callback_data,
    bool success, User** user_ptr);

// Return the JNI environment.
inline JNIEnv* Env(AuthData* auth_data) { return auth_data->app->GetJNIEnv(); }

// Convert j_local (a local reference) into a global reference, delete the local
// reference, and set the impl pointer to the new global reference.
// Delete the existing impl pointer global reference, if it already exists.
void SetImplFromLocalRef(JNIEnv* env, jobject j_local, void** impl);

// Return the Java FirebaseAuth class from our platform-independent
// representation.
inline jobject AuthImpl(AuthData* auth_data) {
  return static_cast<jobject>(auth_data->auth_impl);
}

// Cache the method ids so we don't have to look up JNI functions by name.
bool CacheCredentialMethodIds(
    JNIEnv* env, jobject activity,
    const std::vector<internal::EmbeddedFile>& embedded_files);
// Release credential classes cached by CacheCredentialMethodIds().
void ReleaseCredentialClasses(JNIEnv* env);

// Cache the method ids so we don't have to look up JNI functions by name.
bool CacheCommonMethodIds(JNIEnv* env, jobject activity);
// Release common classes cached by CacheCommonMethodIds().
void ReleaseCommonClasses(JNIEnv* env);

// Examines an exception object to determine the error code.
AuthError ErrorCodeFromException(JNIEnv* env, jobject exception);

// Checks for pending jni exceptions, captures the error and message
// if there is one, and clears the exception state.
// Returns kAuthErrorNone if there was no exception.
// Returns kAuthErrorUnimplemented if the exception did not match a known auth
// Exception.
// Otherwise, the error code is returned and error_message out is written with
// the exception message.
AuthError CheckAndClearJniAuthExceptions(JNIEnv* env,
                                         std::string* error_message);

// Checks for Future success / failure or Android based exceptions, and maps
// them to corresponding AuthError codes.
AuthError MapFutureCallbackResultToAuthError(JNIEnv* env, jobject result,
                                             util::FutureResult result_code,
                                             bool* success);

// The function called by the Java thread when a result completes.
template <typename T, typename CONTEXT_T>
static void FutureCallback(JNIEnv* env, jobject result,
                           util::FutureResult result_code, int status,
                           const char* status_message, void* callback_data) {
  FutureCallbackData<T, CONTEXT_T>* data =
      static_cast<FutureCallbackData<T, CONTEXT_T>*>(callback_data);
  bool success = false;
  const AuthError error =
      MapFutureCallbackResultToAuthError(env, result, result_code, &success);
  // Finish off the asynchronous call so that the caller can read it.
  data->auth_data->future_impl.Complete(
      data->handle, error, status_message,
      [result, success, data](void* result_data) {
        if (data->future_data_read_fn != nullptr) {
          data->future_data_read_fn(result, data, success,
                                    static_cast<T*>(result_data));
        }
      });

  // Remove the callback structure that was allocated when the callback was
  // created in SetupFuture().
  delete data;
  data = nullptr;
}

// The function called by the Java thread when a result completes.
template <typename T>
void FederatedAuthProviderFutureCallback(JNIEnv* env, jobject result,
                                         util::FutureResult result_code,
                                         const char* status_message,
                                         void* callback_data) {
  FutureCallbackData<T>* data =
      static_cast<FutureCallbackData<T>*>(callback_data);
  bool success = false;
  AuthError error =
      MapFutureCallbackResultToAuthError(env, result, result_code, &success);
  // The Android SDK Web Activity returns Operation Not Allowed when the
  // provider id is invalid or a federated auth operation is requested of a
  // disabled provider.
  if (error == kAuthErrorOperationNotAllowed) {
    error = kAuthErrorInvalidProviderId;
  }
  // Finish off the asynchronous call so that the caller can read it.
  data->auth_data->future_impl.Complete(
      data->handle, error, status_message,
      [result, success, data](void* user_data) {
        if (data->future_data_read_fn != nullptr) {
          data->future_data_read_fn(result, data, success, user_data);
        }
      });

  // Remove the callback structure that was allocated when the callback was
  // created in SetupFuture().
  delete data;
  data = nullptr;
}

// Ensure `FutureCallback` gets called when `task` completes.
// Inside `FutureCallback`, we call `read_result_fn` to grab the Future result
// data from Java, and then complete the Future for `handle`.
template <typename T, typename CONTEXT_T>
static void RegisterCallback(
    JNIEnv* env, jobject task, SafeFutureHandle<T> handle,
    ReferenceCountedFutureImpl* future_api, const char* future_api_id,
    CONTEXT_T* context,
    typename FutureCallbackData<T, CONTEXT_T>::ReadFutureResultFn
        read_result_fn) {
  // The FutureCallbackData structure is deleted in FutureCallback().
  util::RegisterCallbackOnTask(env, task, FutureCallback<T, CONTEXT_T>,
                               new FutureCallbackData<T, CONTEXT_T>(
                                   handle, future_api, read_result_fn, context),
                               future_api_id);
}

// Register a callback that doesn't set a result.
static void RegisterCallback(JNIEnv* env, jobject task,
                             SafeFutureHandle<void> handle,
                             ReferenceCountedFutureImpl* future_api,
                             const char* future_api_id) {
  // The FutureCallbackData structure is deleted in FutureCallback().
  util::RegisterCallbackOnTask(
      env, task, FutureCallback<void, void*>,
      new FutureCallbackData<void, void*>(handle, future_api, nullptr, nullptr),
      future_api_id);
}

// AuthData specialized variant of RegisterCallback().
template <typename T>
static void RegisterCallback(
    jobject task, SafeFutureHandle<T> handle, AuthData* auth_data,
    typename FutureCallbackData<T, AuthData>::ReadFutureResultFn
        read_result_fn) {
  RegisterCallback(Env(auth_data), task, handle, &auth_data->future_impl,
                   auth_data->future_api_id.c_str(), auth_data, read_result_fn);
}

// Akin to RegisterCallback above, but has a special callback handler
// to detect specific error codes associated with the phone's
// Web Activity implementation. This lets us map SDK-specific error idioms
// to consistent error codes for both iOS and Android without interfering
// with the existing API behavior for other sign in events.
template <typename T>
void RegisterFederatedAuthProviderCallback(
    jobject pending_result, SafeFutureHandle<T> handle, AuthData* auth_data,
    typename FutureCallbackData<T>::ReadFutureResultFn read_result_fn) {
  // The FutureCallbackData structure is deleted in
  // FederatedAuthProviderFutureCallback().
  util::RegisterCallbackOnTask(
      Env(auth_data), pending_result, FederatedAuthProviderFutureCallback<T>,
      new FutureCallbackData<T>(handle, auth_data, read_result_fn),
      auth_data->future_api_id.c_str());
}

// Checks if there was an error, and if so, completes the given future with the
// proper error message. Returns true if there was an error and the future was
// completed.
template <typename T>
static bool CheckAndCompleteFutureOnError(JNIEnv* env,
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

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_ANDROID_COMMON_ANDROID_H_
