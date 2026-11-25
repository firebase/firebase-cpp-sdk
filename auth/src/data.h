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

#ifndef FIREBASE_AUTH_SRC_DATA_H_
#define FIREBASE_AUTH_SRC_DATA_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/service_interfaces.h"
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
  kUserFn_SendEmailVerificationBeforeUpdatingEmail,
  kUserFn_ConfirmEmailVerification,
  kUserFn_UpdateUserProfile,
  kUserFn_LinkWithCredential,
  kUserFn_LinkWithProvider,
  kUserFn_ReauthenticateWithProvider,
  kUserFn_Unlink,
  kUserFn_UpdatePhoneNumberCredential,
  kUserFn_Reload,
  kUserFn_Delete,
  kUserFn_GetIdTokenResult,

  // Internal functions that are still handles, but are only used internally:
  kInternalFn_GetTokenForRefresher,
  kInternalFn_GetTokenForFunctionRegistry,

  kNumAuthFunctions
};

/// Delete all the user_infos in auth_data and reset the length to zero.
void ClearUserInfos(AuthData* auth_data);

/// The pimpl service owned by AppState for Auth and User handles.
struct AuthData : public ::firebase::internal::AuthTokenProvider {
  AuthData(::firebase::internal::AppState& state, void* platform_auth_impl);
  ~AuthData() override;

  // Creates an aliasing Auth handle sharing AppState's control block, or an
  // invalid Auth if AppState is already tearing down.
  Auth MakeAuthHandle();

  // Creates a non-owning User handle referring to this AuthData (for storage
  // inside Future<AuthResult> / Future<User>), or an invalid User if no user is
  // signed in or AppState is tearing down.
  User MakeCurrentUserHandle();

  // Creates an owning User handle sharing AppState's control block (for
  // Auth::current_user()), or an invalid User if no user is signed in.
  User MakeStrongCurrentUserHandle();

  // Clones a User as a non-owning weak handle for storage inside Future
  // results.
  static User CloneWeakUser(const User& user) { return User::CloneWeak(user); }

  // Internal listener registration used by Auth-owned helpers (no weak_ptr
  // back-registration needed since AuthData removes them before destroying
  // platform state).
  void AddInternalAuthStateListener(AuthStateListener* listener);
  void RemoveInternalAuthStateListener(AuthStateListener* listener);
  void AddInternalIdTokenListener(IdTokenListener* listener);
  void RemoveInternalIdTokenListener(IdTokenListener* listener);

  void ClearListeners();

  // ::firebase::internal::AuthTokenProvider overrides (implemented per
  // platform: full desktop implementation, no-op stubs on Android/iOS where
  // native SDKs handle cross-product auth).
  std::string GetCurrentToken() override;
  Future<std::string> GetTokenAsync(bool force_refresh) override;
  std::string GetCurrentUserUid() override;
  void StartTokenListener() override;
  void StopTokenListener() override;

  /// Non-owning reference to the owning AppState (guaranteed to outlive this).
  ::firebase::internal::AppState& app_state;

  /// Handle calls from Futures that the API returns.
  ReferenceCountedFutureImpl future_impl;

  /// Identifier used to track futures associated with future_impl.
  std::string future_api_id;

  /// Platform-dependent implementation of Auth (that we're wrapping).
  /// For example, on Android `jobject`.
  void* auth_impl = nullptr;

  /// Platform-dependent implementation of User (that we're wrapping).
  /// For example, on iOS `FIRUser`.
  void* user_impl = nullptr;

  /// Platform-dependent implementation of AuthStateListener (that we're
  /// wrapping). For example, on Android `jobject`.
  void* listener_impl = nullptr;

  /// Platform-dependent implementation of IdTokenListener (that we're
  /// wrapping). For example, on Android `jobject`.
  void* id_token_listener_impl = nullptr;

  /// Lazily created platform-specific data for PhoneAuthProvider (owned here
  /// and released in DestroyPlatformAuth).
  void* phone_auth_provider_data = nullptr;

  /// Backing data for the return value of User::provider_data().
  /// These point to WrappedUserInfos.
  std::vector<UserInfoInterface*> user_infos;

  /// User-supplied and internal AuthStateListeners.
  std::vector<AuthStateListener*> listeners;

  /// User-supplied and internal IdTokenListeners.
  std::vector<IdTokenListener*> id_token_listeners;

  /// Mutex protecting `listeners`, `id_token_listeners`, and
  /// `persistent_cache_load_pending`.
  Mutex listeners_mutex;

  // Mutex for changes to the internal token listener state.
  Mutex token_listener_mutex;

  // Tracks if the Id Token listener is expecting a callback to occur.
  bool expect_id_token_listener_callback = false;

  // Tracks if the persistent cache load is pending.
  bool persistent_cache_load_pending = true;

  // Synchronization for Auth::current_user() callers waiting for initial
  // persistent cache load without registering an AuthStateListener (preventing
  // lock-order inversions with product mutexes during NotifyAllListeners).
  std::atomic<bool> persistent_cache_loaded{false};
  std::mutex persistent_cache_cv_mutex;
  std::condition_variable persistent_cache_cv;

  void MarkPersistentCacheLoaded() {
    if (persistent_cache_loaded.load(std::memory_order_acquire)) return;
    {
      std::lock_guard<std::mutex> lock(persistent_cache_cv_mutex);
      persistent_cache_loaded.store(true, std::memory_order_release);
    }
    persistent_cache_cv.notify_all();
  }

  void WaitForPersistentCacheLoad() {
    if (persistent_cache_loaded.load(std::memory_order_acquire)) return;
    std::unique_lock<std::mutex> lock(persistent_cache_cv_mutex);
    persistent_cache_cv.wait(lock, [this]() {
      return persistent_cache_loaded.load(std::memory_order_acquire);
    });
  }

  // Mutex protecting `expect_id_token_listener_callback`.
  Mutex expect_id_token_mutex;

  // Tracks if auth is being destroyed.
  bool destructing = false;

  // Mutex protecting `destructing`.
  Mutex destructing_mutex;

  // Sets if the Id Token Listener is expecting a callback.
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

#endif  // FIREBASE_AUTH_SRC_DATA_H_
