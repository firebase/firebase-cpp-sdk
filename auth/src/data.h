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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DATA_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DATA_H_

#include <string>

#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

// Enumeration for API functions that return a Future.
// This allows us to hold a Future for the most recent call to that API.
enum AuthApiFunction {
  // External functions in the Auth API.
  kAuthFn_FetchProvidersForEmail,
  kAuthFn_SignInWithCustomToken,
  kAuthFn_SignInWithCredential,
  kAuthFn_SignInAndRetrieveDataWithCredential,
  kAuthFn_SignInAnonymously,
  kAuthFn_SignInWithEmailAndPassword,
  kAuthFn_SignInWithProvider,
  kAuthFn_CreateUserWithEmailAndPassword,
  kAuthFn_SendPasswordResetEmail,

  // External functions in the User API.
  kUserFn_GetToken,
  kUserFn_UpdateEmail,
  kUserFn_UpdatePassword,
  kUserFn_Reauthenticate,
  kUserFn_ReauthenticateAndRetrieveData,
  kUserFn_SendEmailVerification,
  kUserFn_ConfirmEmailVerification,
  kUserFn_UpdateUserProfile,
  kUserFn_LinkWithCredential,
  kUserFn_LinkAndRetrieveDataWithCredential,
  kUserFn_LinkWithProvider,
  kUserFn_ReauthenticateWithProvider,
  kUserFn_Unlink,
  kUserFn_UpdatePhoneNumberCredential,
  kUserFn_Reload,
  kUserFn_Delete,

  // Internal functions that are still handles, but are only used internally:
  kInternalFn_GetTokenForRefresher,
  kInternalFn_GetTokenForFunctionRegistry,

  kNumAuthFunctions
};

/// Delete all the user_infos in auth_data and reset the length to zero.
void ClearUserInfos(AuthData* auth_data);

/// The pimpl data for the Auth and User classes.
/// The same pimpl is referred to by both classes, since the two implementations
/// are tightly linked (there can only be one User per Auth).
struct AuthData {
  AuthData()
      : app(nullptr),
        auth(nullptr),
        future_impl(kNumAuthFunctions),
        current_user(this),
        auth_impl(nullptr),
        user_impl(nullptr),
        listener_impl(nullptr),
        id_token_listener_impl(nullptr),
        expect_id_token_listener_callback(false),
        persistent_cache_load_pending(true),
        destructing(false) {}

  ~AuthData() {
    ClearUserInfos(this);

    // Reset the listeners so that they don't try to unregister themselves
    // when they are destroyed.
    ClearListeners();

    app = nullptr;
    auth = nullptr;
    auth_impl = nullptr;
    user_impl = nullptr;
    listener_impl = nullptr;
    id_token_listener_impl = nullptr;
  }

  void ClearListeners() {
    while (!listeners.empty()) {
      auth->RemoveAuthStateListener(listeners.back());
    }
    while (!id_token_listeners.empty()) {
      auth->RemoveIdTokenListener(id_token_listeners.back());
    }
  }

  /// The Firebase App this auth is connected to.
  App* app;

  /// Backpointer to the external Auth class that holds this internal data.
  Auth* auth;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl;

  /// Identifier used to track futures associated with future_impl.
  std::string future_api_id;

  /// Unique user for this Auth. Note: we only support one user per Auth.
  User current_user;

  /// Platform-dependent implementation of Auth (that we're wrapping).
  /// For example, on Android `jobject`.
  void* auth_impl;

  /// Platform-dependent implementation of User (that we're wrapping).
  /// For example, on iOS `FIRUser`.
  void* user_impl;

  /// Platform-dependent implementation of AuthStateListener (that we're
  /// wrapping). For example, on Android `jobject`.
  void* listener_impl;

  /// Platform-dependent implementation of IdTokenListener (that we're
  /// wrapping). For example, on Android `jobject`
  void* id_token_listener_impl;

  /// Backing data for the return value of User::ProviderData().
  /// These point to WrappedUserInfos.
  std::vector<UserInfoInterface*> user_infos;

  /// User-supplied listener classes. All of these are updated when Auth's
  /// sign in state changes.
  std::vector<AuthStateListener*> listeners;

  /// User-supplied ID token listeners. All of these are updated when Auth's
  /// ID token changes.
  std::vector<IdTokenListener*> id_token_listeners;

  /// Unique phone provider for this Auth. Initialized the first time
  /// PhoneAuthProvider::GetInstance() is called.
  PhoneAuthProvider phone_auth_provider;

  /// Mutex protecting the `listeners`, `id_token_listeners`, and
  /// `phone_auth_provider.data_->listeners vectors`.
  /// 'listeners' are added and removed and accessed on different threads.
  /// This is also protecting persistent_cache_load_pending flag.
  Mutex listeners_mutex;

  // Mutex for changes to the internal token listener state.
  Mutex token_listener_mutex;

  // Tracks if the Id Token listener is expecting a callback to occur.
  bool expect_id_token_listener_callback;

  // Tracks if the persistent cache load is pending.
  bool persistent_cache_load_pending;

  // Mutex protecting `expect_id_token_listener_callback`
  Mutex expect_id_token_mutex;

  // Tracks if auth is being destroyed.
  bool destructing;

  // Mutex protecting destructing
  Mutex desctruting_mutex;

  // Sets if the Id Token Listener is expecting a callback.
  // Used to workaround an issue where the Id Token is not reset with a new one,
  // and thus not triggered correctly.
  void SetExpectIdTokenListenerCallback(bool expect) {
    MutexLock lock(expect_id_token_mutex);
    expect_id_token_listener_callback = expect;
  }

  // Returns if the Id Token Listener is expecting a callback, and clears the
  // flag.
  bool ShouldTriggerIdTokenListenerCallback() {
    MutexLock lock(expect_id_token_mutex);
    bool result = expect_id_token_listener_callback;
    expect_id_token_listener_callback = false;
    return result;
  }
};

// Called automatically whenever anyone refreshes the auth token.
// (Only used by desktop builds at the moment.)
void ResetTokenRefreshCounter(AuthData* auth_data);

void EnableTokenAutoRefresh(AuthData* auth_data);
void DisableTokenAutoRefresh(AuthData* auth_data);
void InitializeTokenRefresher(AuthData* auth_data);
void DestroyTokenRefresher(AuthData* auth_data);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DATA_H_
