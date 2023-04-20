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
template <typename T>
struct FutureCallbackData {
  // During the callback, read `result` data from Java into the returned
  // C++ data in `d->future_data->Data()`.
  typedef void ReadFutureResultFn(jobject result,
                                  FutureCallbackData* future_callback_Data,
                                  bool success, void* void_data);

  FutureCallbackData(AuthData* auth_data, const SafeFutureHandle<T>& handle,
                     ReferenceCountedFutureImpl* future_impl,
                     ReadFutureResultFn* future_data_read_fn)
      : auth_data(auth_data),
        handle(handle),
        future_impl(future_impl),
        future_data_read_fn(future_data_read_fn) {}
  AuthData* auth_data;
  SafeFutureHandle<T> handle;
  ReferenceCountedFutureImpl* future_impl;
  ReadFutureResultFn* future_data_read_fn;
};

// Contains the interface between the public API and the underlying
// Android Java SDK FirebaseUser implemention.
class UserInternal {
 public:
  // Constructor
  explicit UserInternal(AuthData* auth_data, jobject android_user);

  // Copy constructor.
  UserInternal(const UserInternal& user_internal);

  ~UserInternal();

  // @deprecated
  //
  // Provides a mechanism for the deprecated auth-contained user object to
  // update its underlying Android Java SDK FirebaseUser object. Assumes that
  // a global ref has already been set on android_user. Releases the global
  // ref on any previously held user.
  void set_native_user_object_deprecated(jobject android_user);

  bool is_valid() const;

  Future<std::string> GetToken(bool force_refresh);
  Future<std::string> GetTokenLastResult() const;

  Future<void> UpdateEmail(const char* email);
  Future<void> UpdateEmailLastResult() const;

  std::vector<UserInfoInterface> provider_data() const;
  const std::vector<UserInfoInterface*>& provider_data_DEPRECATED();

  Future<void> UpdatePassword(const char* password);
  Future<void> UpdatePasswordLastResult() const;

  Future<void> UpdateUserProfile(const User::UserProfile& profile);
  Future<void> UpdateUserProfileLastResult() const;

  Future<void> SendEmailVerification();
  Future<void> SendEmailVerificationLastResult() const;

  Future<User*> LinkWithCredential_DEPRECATED(const Credential& credential);
  Future<User*> LinkWithCredentialLastResult_DEPRECATED() const;

  Future<SignInResult> LinkAndRetrieveDataWithCredential_DEPRECATED(
      const Credential& credential);
  Future<SignInResult> LinkAndRetrieveDataWithCredentialLastResult_DEPRECATED()
      const;

  Future<SignInResult> LinkWithProvider_DEPRECATED(
      FederatedAuthProvider* provider);
  Future<SignInResult> LinkWithProviderLastResult_DEPRECATED() const;

  Future<User*> Unlink_DEPRECATED(const char* provider);
  Future<User*> UnlinkLastResult_DEPRECATED() const;

  Future<User*> UpdatePhoneNumberCredential_DEPRECATED(
      const Credential& credential);
  Future<User*> UpdatePhoneNumberCredentialLastResult_DEPRECATED() const;

  Future<void> Reload();
  Future<void> ReloadLastResult() const;

  Future<void> Reauthenticate(const Credential& credential);
  Future<void> ReauthenticateLastResult() const;

  Future<SignInResult> ReauthenticateAndRetrieveData_DEPRECATED(
      const Credential& credential);
  Future<SignInResult> ReauthenticateAndRetrieveDataLastResult_DEPRECATED()
      const;

  Future<SignInResult> ReauthenticateWithProvider_DEPRECATED(
      FederatedAuthProvider* provider);
  Future<SignInResult> ReauthenticateWithProviderLastResult_DEPRECATED() const;

  Future<void> Delete();
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

  // Pointer to the originating auth context.
  AuthData* auth_data_;

  // Android Java SDK Implementation of a FirebaseUser object.
  jobject user_;

  // Future data used to synchronize asynchronous calls.
  FutureData future_data_;

  // Used to support older method invocation of provider_data_DEPRECATED().
  std::vector<UserInfoInterface*> user_infos_;

  // Guard against changes to the user_ object.
  Mutex user_mutex_;

  // Used to identify on-going futures in case we need to cancel them
  // upon UserInternal destruction.
  std::string future_api_id_;
};

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadSignInResult(jobject result, FutureCallbackData<SignInResult>* d,
                      bool success, void* void_data);

// The `ReadFutureResultFn` for `SignIn` APIs.
// Reads the `AuthResult` in `result` and initialize the `User*` in `void_data`.
void ReadUserFromSignInResult(jobject result, FutureCallbackData<User*>* d,
                              bool success, void* void_data);

inline void ReadAdditionalUserInfo(JNIEnv* env, jobject j_additional_user_info,
                                   AdditionalUserInfo* info) {
  if (j_additional_user_info == nullptr) {
    *info = AdditionalUserInfo();
    return;
  }

  // Get references to Java data members of AdditionalUserInfo object.
  const jobject j_provider_id = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetProviderId));
  util::CheckAndClearJniExceptions(env);
  const jobject j_profile = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetProfile));
  util::CheckAndClearJniExceptions(env);
  const jobject j_user_name = env->CallObjectMethod(
      j_additional_user_info,
      additional_user_info::GetMethodId(additional_user_info::kGetUsername));
  util::CheckAndClearJniExceptions(env);

  // Convert Java references to C++ types.
  info->provider_id = util::JniStringToString(env, j_provider_id);
  info->user_name = util::JniStringToString(env, j_user_name);
  if (j_profile) util::JavaMapToVariantMap(env, &info->profile, j_profile);

  // Release local references. Note that JniStringToString releases for us.
  env->DeleteLocalRef(j_profile);
}

// Return the JNI environment.
inline JNIEnv* Env(AuthData* auth_data) { return auth_data->app->GetJNIEnv(); }

// Convert j_local (a local reference) into a global reference, delete the local
// reference, and set the impl pointer to the new global reference.
// Delete the existing impl pointer global reference, if it already exists.
void SetImplFromLocalRef(JNIEnv* env, jobject j_local, void** impl);

// Synchronize the current user.
void UpdateCurrentUser(JNIEnv* env, AuthData* auth_data);

/// @deprecated
///
/// Replace the platform-dependent FirebaseUser Android SDK object.
/// Note: this function is only used to support DEPRECATED methods which return
/// User*. This functionality should be removed when those deprecated methods
/// are removed.
inline void SetUserImpl(AuthData* _Nonnull auth_data, jobject j_user) {
  assert(auth_data->deprecated_fields.user_internal_deprecated);
  auth_data->deprecated_fields.user_internal_deprecated
      ->set_native_user_object_deprecated(j_user);
}

// Return the Java FirebaseAuth class from our platform-independent
// representation.
inline jobject AuthImpl(AuthData* auth_data) {
  return static_cast<jobject>(auth_data->auth_impl);
}

// Return a platform-independent representation of Java's FirebaseUser class.
inline void* ImplFromUser(jobject user) { return static_cast<void*>(user); }

// Return the Java FirebaseUser class from our platform-independent
// representation.
inline jobject UserFromImpl(void* impl) { return static_cast<jobject>(impl); }

// Return the Java Credential class from our platform-independent
// representation.
inline jobject CredentialFromImpl(void* impl) {
  return static_cast<jobject>(impl);
}

// Cache the method ids so we don't have to look up JNI functions by name.
bool CacheUserMethodIds(JNIEnv* env, jobject activity);
// Release user classes cached by CacheUserMethodIds().
void ReleaseUserClasses(JNIEnv* env);

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
template <typename T>
void FutureCallback(JNIEnv* env, jobject result, util::FutureResult result_code,
                    const char* status_message, void* callback_data) {
  FutureCallbackData<T>* future_callback_data =
      static_cast<FutureCallbackData<T>*>(callback_data);
  bool success = false;
  const AuthError error =
      MapFutureCallbackResultToAuthError(env, result, result_code, &success);
  // Finish off the asynchronous call so that the caller can read it.
  future_callback_data->future_impl->Complete(
      future_callback_data->handle, error, status_message,
      [result, success, future_callback_data](void* user_data) {
        if (future_callback_data->future_data_read_fn != nullptr) {
          future_callback_data->future_data_read_fn(
              result, future_callback_data, success, user_data);
        }
      });

  // Remove the callback structure that was allocated when the callback was
  // created in SetupFuture().
  delete future_callback_data;
  future_callback_data = nullptr;
}

// The function called by the Java thread when a result completes.
template <typename T>
void FederatedAuthProviderFutureCallback(JNIEnv* env, jobject result,
                                         util::FutureResult result_code,
                                         const char* status_message,
                                         void* callback_data) {
  FutureCallbackData<T>* future_callback_data =
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
  future_callback_data->future_impl->Complete(
      future_callback_data->handle, error, status_message,
      [result, success, future_callback_data](void* user_data) {
        if (future_callback_data->future_data_read_fn != nullptr) {
          future_callback_data->future_data_read_fn(
              result, future_callback_data, success, user_data);
        }
      });

  // Remove the callback structure that was allocated when the callback was
  // created in SetupFuture().
  delete future_callback_data;
  future_callback_data = nullptr;
}

// Ensure `FutureCallback` gets called when `pending_result` completes.
// Inside `FutureCallback`, we call `read_result_fn` to grab the Future result
// data from Java, and then complete the Future for `handle`.
template <typename T>
void RegisterCallback(
    AuthData* auth_data, jobject pending_result, SafeFutureHandle<T> handle,
    const std::string& future_api_id, ReferenceCountedFutureImpl* future_impl,
    typename FutureCallbackData<T>::ReadFutureResultFn read_result_fn) {
  // The FutureCallbackData structure is deleted in FutureCallback().
  util::RegisterCallbackOnTask(
      Env(auth_data), pending_result, FutureCallback<T>,
      new FutureCallbackData<T>(auth_data, handle, future_impl, read_result_fn),
      future_api_id.c_str());
}

// Akin to RegisterCallback above, but has a special callback handler
// to detect specific error codes associated with the phone's
// Web Activity implementation. This lets us map SDK-specific error idioms
// to consistent error codes for both iOS and Android without interfering
// with the existing API behavior for other sign in events.
template <typename T>
void RegisterFederatedAuthProviderCallback(
    AuthData* auth_data, jobject pending_result, SafeFutureHandle<T> handle,
    const std::string& future_api_id, ReferenceCountedFutureImpl* future_impl,
    typename FutureCallbackData<T>::ReadFutureResultFn read_result_fn) {
  // The FutureCallbackData structure is deleted in
  // FederatedAuthProviderFutureCallback().
  util::RegisterCallbackOnTask(
      Env(auth_data), pending_result, FederatedAuthProviderFutureCallback<T>,
      new FutureCallbackData<T>(auth_data, handle, future_impl, read_result_fn),
      future_api_id.c_str());
}

// Checks if there was an error, and if so, completes the given future with the
// proper error message. Returns true if there was an error and the future was
// completed.
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

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_ANDROID_COMMON_ANDROID_H_
