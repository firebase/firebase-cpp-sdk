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

namespace firebase {
namespace firestore {
namespace {

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

StatusOr<AuthToken> ConvertToken(const Future<std::string>& future, App& app) {
  if (future.error() != Error::kErrorOk) {
    // `AuthError` is a different error domain from go/canonical-codes that
    // `Status` uses. We map `AuthError` values to Firestore `Error` values in
    // order to be able to perform retries when appropriate.
    return Status(FirestoreErrorFromAuthError(future.error()),
                  std::string(future.error_message()) + " (AuthError " +
                      std::to_string(future.error()) + ")");
  }

  return AuthToken(*future.result(), GetCurrentUser(app));
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

  listener(ConvertToken(future_token, app));
}

}  // namespace

FirebaseCppCredentialsProvider::FirebaseCppCredentialsProvider(App& app)
    : contents_(std::make_shared<Contents>(app)) {}

FirebaseCppCredentialsProvider::~FirebaseCppCredentialsProvider() {
  RemoveAuthStateListener();
}

void FirebaseCppCredentialsProvider::SetCredentialChangeListener(
    CredentialChangeListener<User> listener) {
  {
    std::lock_guard<std::recursive_mutex> lock(contents_->mutex);

    if (!listener) {
      SIMPLE_HARD_ASSERT(change_listener_,
                         "Change listener removed without being set!");
      change_listener_ = {};
      RemoveAuthStateListener();
      return;
    }

    SIMPLE_HARD_ASSERT(!change_listener_, "Set change listener twice!");
    change_listener_ = std::move(listener);
    change_listener_(GetCurrentUser(contents_->app));
  }

  // Note: make sure to only register the Auth listener _after_ calling
  // `Auth::current_user` for the first time. The reason is that upon the first
  // call only, `Auth::current_user` might block as it would asynchronously
  // notify Auth listeners; getting the Firestore listener notified while
  // `Auth::current_user` is pending can lead to a deadlock.
  AddAuthStateListener();
}

void FirebaseCppCredentialsProvider::GetToken(
    TokenListener<AuthToken> listener) {
  std::lock_guard<std::recursive_mutex> lock(contents_->mutex);

  if (IsSignedIn()) {
    RequestToken(std::move(listener));
  } else {
    listener(AuthToken::Unauthenticated());
  }
}

void FirebaseCppCredentialsProvider::AddAuthStateListener() {
  App& app = contents_->app;
  auto callback = reinterpret_cast<void*>(OnAuthStateChanged);
  app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthAddAuthStateListener, &app, callback, this);
}

void FirebaseCppCredentialsProvider::RemoveAuthStateListener() {
  App& app = contents_->app;
  auto callback = reinterpret_cast<void*>(OnAuthStateChanged);
  app.function_registry()->CallFunction(
      ::firebase::internal::FnAuthRemoveAuthStateListener, &app, callback,
      this);
}

void FirebaseCppCredentialsProvider::OnAuthStateChanged(void* context) {
  auto provider = static_cast<FirebaseCppCredentialsProvider*>(context);
  Contents* contents = provider->contents_.get();

  std::lock_guard<std::recursive_mutex> lock(contents->mutex);

  // The currently signed-in user may have changed, increase the token
  // generation to reflect that.
  ++(contents->token_generation);

  if (provider->change_listener_) {
    provider->change_listener_(GetCurrentUser(contents->app));
  }
}

// Private member functions

void FirebaseCppCredentialsProvider::RequestToken(
    TokenListener<AuthToken> listener) {
  SIMPLE_HARD_ASSERT(IsSignedIn(),
                     "Cannot get token when there is no signed-in user");

  // Take note of the current value of `token_generation` so that this request
  // can fail if there is a token change while the request is outstanding.
  int expected_generation = contents_->token_generation;

  bool force_refresh = force_refresh_;
  force_refresh_ = false;
  auto future = GetAuthTokenAsync(contents_->app, force_refresh);

  std::weak_ptr<Contents> weak_contents(contents_);

  // Note: if the future happens to be already completed (either because the
  // token was readily available, or theoretically because the Auth token
  // request finished so quickly), this completion will be invoked
  // synchronously. Because the mutex is recursive, it's okay to lock it again
  // in that case.
  future.OnCompletion(
      // TODO(c++14): move `listener` into the lambda.
      [listener, expected_generation,
       weak_contents](const Future<std::string>& future_token) {
        auto contents = weak_contents.lock();
        // Auth may invoke the callback when credentials provider has already
        // been destroyed.
        if (!contents) {
          return;
        }

        std::lock_guard<std::recursive_mutex> lock(contents->mutex);
        OnToken(future_token, contents->app, contents->token_generation,
                std::move(listener), expected_generation);
      });
}

bool FirebaseCppCredentialsProvider::IsSignedIn() const {
  return GetCurrentUser(contents_->app).is_authenticated();
}

}  // namespace firestore
}  // namespace firebase
