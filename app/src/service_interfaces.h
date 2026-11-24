/*
 * Copyright 2026 Google LLC
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

#ifndef FIREBASE_APP_SRC_SERVICE_INTERFACES_H_
#define FIREBASE_APP_SRC_SERVICE_INTERFACES_H_

#include <string>

#include "app/src/include/firebase/future.h"

namespace firebase {
namespace internal {

// Teardown priority constants for AppState::DestroyServices().
// Higher values are destroyed first; ties are broken in reverse creation order.
inline constexpr int kDefaultServiceTeardownPriority = 0;
inline constexpr int kProviderServiceTeardownPriority = -10;

// Base class for all per-App product implementations and teardown guards owned
// by AppState.
class Service {
 public:
  virtual ~Service() = default;

  // Controls relative teardown order inside ~AppState(). Consumers (Database,
  // Storage, Functions, Firestore, etc.) use the default priority (0) so they
  // are destroyed before token providers (Auth, AppCheck at -10).
  virtual int teardown_priority() const {
    return kDefaultServiceTeardownPriority;
  }
};

// Well-known service type tags for cross-product discovery without link-time
// coupling or RTTI. Defined in app_state.cc.
extern const char kAuthServiceTag;
extern const char kAppCheckServiceTag;

// Typed per-App interface exposed by Firebase Auth to other products on the
// same App (Database, Storage, Functions, Firestore) without link dependencies
// or void* casts.
class AuthTokenProvider : public Service {
 public:
  using AuthStateCallback = void (*)(void* context);

  int teardown_priority() const override {
    return kProviderServiceTeardownPriority;
  }

  // Returns the current user's cached auth token, or empty if unauthenticated.
  virtual std::string GetCurrentToken() = 0;

  // Asynchronously fetches a fresh auth token for the current user, or returns
  // an invalid Future if unavailable.
  virtual Future<std::string> GetTokenAsync(bool force_refresh) = 0;

  // Returns the current user's UID, or empty if unauthenticated.
  virtual std::string GetCurrentUserUid() = 0;

  // Ref-counted background token refresh thread management.
  virtual void StartTokenListener() = 0;
  virtual void StopTokenListener() = 0;

  // Internal auth-state change callbacks.
  virtual void AddAuthStateListener(AuthStateCallback callback,
                                    void* context) = 0;
  virtual void RemoveAuthStateListener(AuthStateCallback callback,
                                       void* context) = 0;
};

// Typed per-App interface exposed by Firebase App Check to other products on
// the same App (Database, Storage, Functions, Firestore).
class AppCheckTokenProvider : public Service {
 public:
  using TokenChangedCallback = void (*)(const std::string& token,
                                        void* context);

  int teardown_priority() const override {
    return kProviderServiceTeardownPriority;
  }

  // Asynchronously fetches a standard App Check token string.
  virtual Future<std::string> GetTokenAsync() = 0;

  // Asynchronously fetches a limited-use App Check token string.
  virtual Future<std::string> GetLimitedUseTokenAsync() = 0;

  // Registers / removes an internal App Check token change listener.
  virtual void AddTokenListener(TokenChangedCallback callback,
                                void* context) = 0;
  virtual void RemoveTokenListener(TokenChangedCallback callback,
                                   void* context) = 0;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SERVICE_INTERFACES_H_
