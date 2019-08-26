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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_COMMON_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_COMMON_H_

#include "auth/src/data.h"

namespace firebase {
namespace auth {

// Enumeration for Credential API functions that return a Future.
// This allows us to hold a Future for the most recent call to that API.
enum CredentialApiFunction {
  kCredentialFn_GameCenterGetCredential,

  kNumCredentialFunctions
};

// Platform-specific method to create the wrapped Auth class.
void* CreatePlatformAuth(App* app);

// Platform-specific method to initialize AuthData.
void InitPlatformAuth(AuthData* auth_data);

// Platform-specific method to destroy the wrapped Auth class.
void DestroyPlatformAuth(AuthData* auth_data);

// All the result functions are similar.
// Just return the local Future, cast to the proper result type.
#define AUTH_RESULT_FN(class_name, fn_name, result_type)                  \
  Future<result_type> class_name::fn_name##LastResult() const {           \
    return static_cast<const Future<result_type>&>(                       \
        auth_data_->future_impl.LastResult(k##class_name##Fn_##fn_name)); \
  }

// Returns true if `auth_data` has a user that's currently active.
inline bool ValidUser(const AuthData* auth_data) {
  return auth_data->user_impl != nullptr;
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

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_COMMON_H_
