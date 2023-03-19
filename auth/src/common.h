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

#ifndef FIREBASE_AUTH_SRC_COMMON_H_
#define FIREBASE_AUTH_SRC_COMMON_H_

#include <string>

#include "auth/src/data.h"

namespace firebase {
namespace auth {

// Error messages used for completing futures. These match the error codes in
// the AdErrorCode enumeration in the C++ API.
extern const char* kUserNotInitializedErrorMessage;
extern const char* kPhoneAuthNotSupportedErrorMessage;

// Enumeration for Credential API functions that return a Future.
// This allows us to hold a Future for the most recent call to that API.
enum CredentialApiFunction {
  kCredentialFn_GameCenterGetCredential,

  kNumCredentialFunctions
};

// Hold backing data for returned Futures.
struct FutureData {
  explicit FutureData(int num_functions_that_return_futures)
      : future_impl(num_functions_that_return_futures) {}

  // Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl;
};

template <class T>
struct FutureCallbackData {
  FutureData* future_data;
  SafeFutureHandle<T> future_handle;
};

// Create a future and update the corresponding last result.
template <class T>
SafeFutureHandle<T> CreateFuture(int fn_idx, FutureData* future_data) {
  return future_data->future_impl.SafeAlloc<T>(fn_idx);
}

// Mark a Future<void> as complete.
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<void> handle, FutureData* future_data);

// Mark a Future<std::string> as complete
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<std::string> handle,
                    FutureData* future_data, const std::string& result);

// Mark a Future<User *> as complete
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<User*> handle, FutureData* future_data,
                    User* user);

// Mark a Future<SignInResult> as complete
void CompleteFuture(int error, const char* error_msg,
                    SafeFutureHandle<SignInResult> handle,
                    FutureData* future_data, SignInResult result);

// For calls that aren't asynchronous, create and complete a Future<void> at
// the same time.
Future<void> CreateAndCompleteFuture(int fn_idx, int error,
                                     const char* error_msg,
                                     FutureData* future_data);

// For calls that aren't asynchronous, create and complete a
// Future<std::string> at the same time.
Future<std::string> CreateAndCompleteFuture(int fn_idx, int error,
                                            const char* error_msg,
                                            FutureData* future_data,
                                            const std::string& result);

// Platform-specific method to create the wrapped Auth class.
void* CreatePlatformAuth(App* app);

// Platform-specific method to initialize AuthData.
void InitPlatformAuth(AuthData* auth_data);

// Platform-specific method to destroy the wrapped Auth class.
void DestroyPlatformAuth(AuthData* auth_data);

// Platform-specific method that causes a heartbeat to be logged.
// See go/firebase-platform-logging-design for more information.
void LogHeartbeat(Auth* auth);

// All the result functions are similar.
// Just return the local Future, cast to the proper result type.
#define AUTH_RESULT_FN(class_name, fn_name, result_type)                  \
  Future<result_type> class_name::fn_name##LastResult() const {           \
    return static_cast<const Future<result_type>&>(                       \
        auth_data_->future_impl.LastResult(k##class_name##Fn_##fn_name)); \
  }

// All the result functions are similar.
// Just return the local Future, cast to the proper result type.
#define AUTH_RESULT_DEPRECATED_FN(class_name, fn_name, result_type)        \
  Future<result_type> class_name::fn_name##LastResult_DEPRECATED() const { \
    return static_cast<const Future<result_type>&>(                        \
        auth_data_->future_impl.LastResult(                                \
            k##class_name##Fn_##fn_name##_DEPRECATED));                    \
  }

// Returns true if `auth_data` has a user that's currently active.
inline bool ValidUser(const AuthData* auth_data) {
  return auth_data->deprecated_fields.user_deprecated->is_valid();
}

// Notify all the listeners of the state change.
void NotifyAuthStateListeners(AuthData* auth_data);

// Notify all the listeners of the ID token change.
void NotifyIdTokenListeners(AuthData* auth_data);

// Synchronize the current user.
void UpdateCurrentUser(AuthData* auth_data);

// Get a FutureImpl to use for Credential methods that return Futures.
ReferenceCountedFutureImpl* GetCredentialFutureImpl();

// Cleanup the static CredentialFutureImpl that may have been generated.
void CleanupCredentialFutureImpl();

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_COMMON_H_
