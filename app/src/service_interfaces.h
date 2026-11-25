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

#include <functional>
#include <string>
#include <utility>

#include "app/src/include/firebase/future.h"

namespace firebase {
namespace internal {

// Teardown priority constants for AppState::DestroyServices().
// Higher values are destroyed first; ties are broken in reverse creation order.
inline constexpr int kDefaultServiceTeardownPriority = 0;
inline constexpr int kProviderSubServiceTeardownPriority = -5;
inline constexpr int kProviderServiceTeardownPriority = -10;

// Move-only RAII subscription handle returned by AppState::SubscribeAuthState
// and AppState::SubscribeAppCheckToken.
//
// Destroying or calling Reset() on a TokenSubscription synchronously detaches
// the listener, releases any associated token-refresh lease, and blocks until
// any in-flight notification callback executing on another thread finishes.
class TokenSubscription {
 public:
  TokenSubscription() = default;
  explicit TokenSubscription(std::function<void()> unsubscribe)
      : unsubscribe_(std::move(unsubscribe)) {}

  ~TokenSubscription() { Reset(); }

  TokenSubscription(const TokenSubscription&) = delete;
  TokenSubscription& operator=(const TokenSubscription&) = delete;

  TokenSubscription(TokenSubscription&& other) noexcept
      : unsubscribe_(std::move(other.unsubscribe_)) {
    other.unsubscribe_ = nullptr;
  }

  TokenSubscription& operator=(TokenSubscription&& other) noexcept {
    if (this != &other) {
      Reset();
      unsubscribe_ = std::move(other.unsubscribe_);
      other.unsubscribe_ = nullptr;
    }
    return *this;
  }

  void Reset() {
    if (unsubscribe_) {
      auto fn = std::move(unsubscribe_);
      unsubscribe_ = nullptr;
      fn();
    }
  }

  explicit operator bool() const noexcept {
    return static_cast<bool>(unsubscribe_);
  }

 private:
  std::function<void()> unsubscribe_;
};

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

// Zero-RTTI type-safe service tag helper for single-module services.
template <typename T>
inline const void* ServiceTypeTag() {
  static constexpr char kTag = 0;
  return &kTag;
}

// Typed per-App interface exposed by Firebase Auth to other products on the
// same App (Database, Storage, Functions, Firestore) without link dependencies
// or void* casts.
class AuthTokenProvider : public Service {
 public:
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

  // Ref-counted background token refresh thread management (paired
  // automatically by AppState::SubscribeAuthState).
  virtual void StartTokenListener() = 0;
  virtual void StopTokenListener() = 0;
};

// Typed per-App interface exposed by Firebase App Check to other products on
// the same App (Database, Storage, Functions, Firestore).
class AppCheckTokenProvider : public Service {
 public:
  int teardown_priority() const override {
    return kProviderServiceTeardownPriority;
  }

  // Asynchronously fetches a standard App Check token string.
  virtual Future<std::string> GetTokenAsync() = 0;

  // Asynchronously fetches a limited-use App Check token string.
  virtual Future<std::string> GetLimitedUseTokenAsync() = 0;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SERVICE_INTERFACES_H_
