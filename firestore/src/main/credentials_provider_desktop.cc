/*
 * Copyright 2021 Google LLC
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

#include "firestore/src/main/credentials_provider_desktop.h"

#include <string>
#include <utility>

#include "Firestore/core/src/util/status.h"
#include "app/src/function_registry.h"
#include "app/src/reference_counted_future_impl.h"
#include "firebase/auth/types.h"
#include "firebase/firestore/firestore_errors.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/common/hard_assert_common.h"

#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <atomic>

namespace firebase {
namespace firestore {
namespace {

std::mutex& ListenerMutex() {
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}

std::unordered_map<uint64_t, std::weak_ptr<FirebaseCppCredentialsProvider::Contents>>& ValidContexts() {
  static auto* contexts = new std::unordered_map<uint64_t, std::weak_ptr<FirebaseCppCredentialsProvider::Contents>>();
  return *contexts;
}

uint64_t GetNextListenerId() {
  static std::atomic<uint64_t> next_id{1};
  return next_id.fetch_add(1);
}


using credentials::AuthToken;
using credentials::CredentialChangeListener;
using credentials::TokenListener;
using credentials::User;
using firebase::auth::AuthError;
using util::Status;
using util::StatusOr;

/**
 * Takes an integer that represents an `AuthError` enum value, and returns a
 * `firestore::Error` that best describes the given `AuthError`.
 */
Error FirestoreErrorFromAuthError(int error) {
  switch (error) {
    case AuthError::kAuthErrorNone:
      return kErrorNone;
    case AuthError::kAuthErrorUnimplemented:
      return kErrorUnimplemented;
    case AuthError::kAuthErrorFailure:
      return kErrorInternal;
    case AuthError::kAuthErrorNetworkRequestFailed:
      return kErrorUnavailable;
    case AuthError::kAuthErrorCancelled:
      return kErrorCancelled;
    case AuthError::kAuthErrorInvalidCustomToken:
    case AuthError::kAuthErrorInvalidCredential:
    case AuthError::kAuthErrorUserDisabled:
    case AuthError::kAuthErrorUserNotFound:
    case AuthError::kAuthErrorInvalidUserToken:
    case AuthError::kAuthErrorUserTokenExpired:
    case AuthError::kAuthErrorNoSignedInUser:
      return kErrorUnauthenticated;
    default:
      return kErrorUnknown;
  }
}

/**
 * Returns a Future that, when completed, will contain the token for the
 * current user or an error. An empty token means that the current user is
 * unauthenticated.
 */
Future<std::string> GetAuthTokenAsync(App& app, bool force_refresh) {
  Future<std::string> result;

  bool success = app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetTokenAsync, &app, &force_refresh, &result);

  if (success) {
    return result;
  }

  // If CallFunction does not succeed, it's because either Auth has not
  // registered implementations for the FunctionId we're using or because there
  // isn't an Auth instance for this App. In either case this means Auth is
  // unavailable and we should treat the current user as unauthenticated.
  return SuccessfulFuture<std::string>("");
}

User GetCurrentUser(App& app) {
  std::string uid;
  bool success = app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentUserUid, &app, /* args= */ nullptr,
      &uid);

  // If Auth is unavailable, treat the user as unauthenticated.
  if (!success) return {};

  return User(uid);
}

StatusOr<AuthToken> ConvertToken(const Future<std::string>& future,
                                 const User& user) {
  if (future.error() != Error::kErrorOk) {
    // `AuthError` is a different error domain from go/canonical-codes that
    // `Status` uses. We map `AuthError` values to Firestore `Error` values in
    // order to be able to perform retries when appropriate.
    const char* msg = future.error_message();
    return Status(FirestoreErrorFromAuthError(future.error()),
                  std::string(msg ? msg : "") + " (AuthError " +
                      std::to_string(future.error()) + ")");
  }

  const std::string* token = future.result();
  if (!token) {
    return AuthToken::Unauthenticated();
  }
  return AuthToken(*token, user);
}

// Converts the result of the given future into an
// `firestore::credentials::AuthToken` and invokes the `listener` with the
// token. If the future is failed, invokes the `listener` with the error. If the
// current token generation is higher than `expected_generation`, will invoke
// the `listener` with "aborted" error. `future_token` must be a completed
// future.
void OnToken(const Future<std::string>& future_token,
             App& app,
             int token_generation,
             const TokenListener<AuthToken>& listener,
             int expected_generation) {
  SIMPLE_HARD_ASSERT(
      future_token.status() == FutureStatus::kFutureStatusComplete,
      "Expected to receive a completed future");

  if (expected_generation != token_generation) {
    // Cancel the request since the user may have changed while the request was
    // outstanding, so the response is likely for a previous user (which user,
    // we can't be sure).
    listener(Status(Error::kErrorAborted,
                    "GetToken() aborted due to token change."));
    return;
  }

  listener(ConvertToken(future_token, GetCurrentUser(app)));
}

}  // namespace

FirebaseCppCredentialsProvider::FirebaseCppCredentialsProvider(App& app)
    : contents_(std::make_shared<Contents>(app)) {}

FirebaseCppCredentialsProvider::~FirebaseCppCredentialsProvider() {
  RemoveAuthStateListener();
}

void FirebaseCppCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<User> listener) {
  bool should_remove = false;
  bool should_add = false;
  CredentialChangeListener<User> old_listener;
  User current_user;

  if (!listener) {
    {
      std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
      SIMPLE_HARD_ASSERT(contents_->auth_listener,
                         "Change listener removed without being set!");
      old_listener = std::move(contents_->auth_listener);
      contents_->auth_listener = {};
      should_remove = true;
    }
  } else {
    current_user = GetCurrentUser(contents_->app);
    {
      std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
      SIMPLE_HARD_ASSERT(!contents_->auth_listener, "Set change listener twice!");
      contents_->auth_listener = listener;
      should_add = true;
    }
  }

  if (should_remove) {
    RemoveAuthStateListener();
  }

  if (should_add) {
    listener(current_user);
    AddAuthStateListener();
  }
}

void FirebaseCppCredentialsProvider::GetToken(
    TokenListener<AuthToken> listener) {
  RequestToken(std::move(listener));
}

void FirebaseCppCredentialsProvider::InvalidateToken() {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
  force_refresh_ = true;
}

void FirebaseCppCredentialsProvider::AddAuthStateListener() {
  App& app = contents_->app;
  auto callback = reinterpret_cast<void*>(OnAuthStateChanged);
  uint64_t listener_id = GetNextListenerId();
  auth_listener_context_ = reinterpret_cast<std::weak_ptr<Contents>*>(static_cast<uintptr_t>(listener_id));
  {
    std::lock_guard<std::mutex> lock(ListenerMutex());
    ValidContexts()[listener_id] = std::weak_ptr<Contents>(contents_);
  }
  app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthAddAuthStateListener, &app, callback, auth_listener_context_);
}

void FirebaseCppCredentialsProvider::RemoveAuthStateListener() {
  if (!auth_listener_context_) return;
  App& app = contents_->app;
  auto callback = reinterpret_cast<void*>(OnAuthStateChanged);
  uint64_t listener_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(auth_listener_context_));
  {
    std::lock_guard<std::mutex> lock(ListenerMutex());
    ValidContexts().erase(listener_id);
  }
  app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthRemoveAuthStateListener, &app, callback,
      auth_listener_context_);
  auth_listener_context_ = nullptr;
}

void FirebaseCppCredentialsProvider::OnAuthStateChanged(void* context) {
  uint64_t listener_id = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(context));
  std::shared_ptr<Contents> contents;
  {
    std::lock_guard<std::mutex> lock(ListenerMutex());
    auto it = ValidContexts().find(listener_id);
    if (it == ValidContexts().end()) return;
    contents = it->second.lock();
  }
  
  if (!contents) return;

  User current_user = GetCurrentUser(contents->app);
  CredentialChangeListener<User> listener;

  {
    std::lock_guard<std::recursive_mutex> lock(contents->mutex);
    ++(contents->token_generation);
    listener = contents->auth_listener;
  }

  if (listener) {
    listener(std::move(current_user));
  }
}

// Private member functions

void FirebaseCppCredentialsProvider::RequestToken(
    TokenListener<AuthToken> listener) {
  bool force_refresh;
  int expected_generation;
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);
    force_refresh = force_refresh_;
    force_refresh_ = false;
    expected_generation = contents_->token_generation;
  }

  if (!IsSignedIn()) {
    listener(AuthToken::Unauthenticated());
    return;
  }

  auto future = GetAuthTokenAsync(contents_->app, force_refresh);
  if (future.status() == FutureStatus::kFutureStatusInvalid) {
    listener(AuthToken::Unauthenticated());
    return;
  }

  std::weak_ptr<Contents> weak_contents(contents_);

  future.OnCompletion(
      [listener = std::move(listener), expected_generation,
       weak_contents](const Future<std::string>& future_token) mutable {
        auto contents = weak_contents.lock();
        if (!contents) {
          if (listener) {
            listener(Status(Error::kErrorAborted,
                            "Credentials provider was destroyed."));
          }
          return;
        }

        int current_generation;
        {
          std::lock_guard<std::recursive_mutex> lock(contents->mutex);
          current_generation = contents->token_generation;
        }
        OnToken(future_token, contents->app, current_generation,
                listener, expected_generation);
      });
}

bool FirebaseCppCredentialsProvider::IsSignedIn() const {
  return GetCurrentUser(contents_->app).is_authenticated();
}

}  // namespace firestore
}  // namespace firebase
