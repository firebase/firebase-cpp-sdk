// Copyright 2016 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_DATA_H_
#define FIREBASE_AUTH_SRC_DATA_H_

#include <string>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/user.h"

namespace firebase {
namespace auth {

enum AuthApiFunction {
  kAuthFn_FetchProvidersForEmail,
  kAuthFn_SignInWithCustomToken,
  kAuthFn_SignInWithCredential,
  kAuthFn_SignInAndRetrieveDataWithCredential,
  kAuthFn_SignInAnonymously,
  kAuthFn_SignInWithEmailAndPassword,
  kAuthFn_SignInWithProvider,
  kAuthFn_CreateUserWithEmailAndPassword,
  kAuthFn_SendPasswordResetEmail,
  kUserFn_GetToken,
  kUserFn_UpdatePassword,
  kUserFn_Reauthenticate,
  kUserFn_ReauthenticateAndRetrieveData,
  kUserFn_SendEmailVerification,
  kUserFn_SendEmailVerificationBeforeUpdatingEmail,
  kUserFn_UpdateUserProfile,
  kUserFn_LinkWithCredential,
  kUserFn_LinkWithProvider,
  kUserFn_ReauthenticateWithProvider,
  kUserFn_Unlink,
  kUserFn_UpdatePhoneNumberCredential,
  kUserFn_Reload,
  kUserFn_Delete,
  kUserFn_GetIdTokenResult,
  kInternalFn_GetTokenForRefresher,
  kInternalFn_GetTokenForFunctionRegistry,
  kNumAuthFunctions
};

void ClearUserInfos(AuthData* auth_data);

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
        expect_id_token_listener_callback_count(0),
        persistent_cache_load_pending(false),
        destructing(false) {}

  ~AuthData() {
    ClearUserInfos(this);
    ClearListeners();
    app = nullptr;
    auth = nullptr;
    auth_impl = nullptr;
    user_impl = nullptr;
    listener_impl = nullptr;
    id_token_listener_impl = nullptr;
  }

  void ClearListeners() {
    if (!auth) return;
    while (!listeners.empty()) {
      auth->RemoveAuthStateListener(listeners.back());
    }
    while (!id_token_listeners.empty()) {
      auth->RemoveIdTokenListener(id_token_listeners.back());
    }
  }

  App* app;
  Auth* auth;
  ReferenceCountedFutureImpl future_impl;
  std::string future_api_id;
  User current_user;
  void* auth_impl;
  void* user_impl;
  void* listener_impl;
  void* id_token_listener_impl;
  std::vector<UserInfoInterface*> user_infos;
  std::vector<AuthStateListener*> listeners;
  std::vector<IdTokenListener*> id_token_listeners;
  PhoneAuthProvider phone_auth_provider;
  Mutex listeners_mutex;
  int expect_id_token_listener_callback_count;
  bool persistent_cache_load_pending;
  Mutex expect_id_token_mutex;
  bool destructing;
  Mutex destructing_mutex;

  void SetExpectIdTokenListenerCallback(bool expect) {
    MutexLock lock(expect_id_token_mutex);
    if (expect) {
      ++expect_id_token_listener_callback_count;
    } else if (expect_id_token_listener_callback_count > 0) {
      --expect_id_token_listener_callback_count;
    }
  }

  bool ShouldTriggerIdTokenListenerCallback() {
    MutexLock lock(expect_id_token_mutex);
    if (expect_id_token_listener_callback_count > 0) {
      --expect_id_token_listener_callback_count;
      return true;
    }
    return false;
  }
};

void ResetTokenRefreshCounter(AuthData* auth_data);
void InitializeTokenRefresher(AuthData* auth_data);
void DestroyTokenRefresher(AuthData* auth_data);
void EnableTokenAutoRefresh(AuthData* auth_data);
void DisableTokenAutoRefresh(AuthData* auth_data);

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DATA_H_
