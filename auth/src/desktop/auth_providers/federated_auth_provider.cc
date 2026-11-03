// Copyright 2019 Google LLC
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

#include <cassert>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/types.h"

namespace firebase {
namespace auth {

FederatedOAuthProvider::FederatedOAuthProvider() : handler_(nullptr) {}

FederatedOAuthProvider::FederatedOAuthProvider(
    const FederatedOAuthProviderData& provider_data)
    : handler_(nullptr) {
  provider_data_ = provider_data;
}

FederatedOAuthProvider::~FederatedOAuthProvider() { handler_ = nullptr; }

void FederatedOAuthProvider::SetAuthHandler(AuthHandler* handler) {
  handler_ = handler;
}

void FederatedOAuthProvider::SetProviderData(
    const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

// Helper function which returns a Future for the corresponding auth
// api function. Or, if that operation is already in progress,
// returns a Future in an error state instead, thereby blocking duplicate
// operations on the same auth instance.
Future<AuthResult> CreateAuthFuture(AuthData* auth_data,
                                    AuthApiFunction api_function) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auto future_base = auth_data->future_impl.LastResult(api_function);
  if (future_base.status() == kFutureStatusPending) {
    // There's an operation in progress. Create and return a new failed
    // future without overwriting the pending LastResult slot.
    SafeFutureHandle<AuthResult> handle =
        auth_data->future_impl.SafeAlloc<AuthResult>();
    auth_data->future_impl.CompleteWithResult(
        handle, kAuthErrorFederatedProviderAlreadyInUse,
        "Provider operation already in progress.",
        /*result=*/{});
    return MakeFuture(&auth_data->future_impl, handle);
  }
  SafeFutureHandle<AuthResult> handle =
      auth_data->future_impl.SafeAlloc<AuthResult>(api_function);
  return MakeFuture(&auth_data->future_impl, handle);
}

Future<AuthResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), auth_data);
  Future<AuthResult> future =
      CreateAuthFuture(auth_data, kAuthFn_SignInWithProvider);
  if (future.status() == kFutureStatusPending) {
    if (handler_ != nullptr) {
      AuthResultCompletionHandle* auth_completion_handle =
          new AuthResultCompletionHandle(
              SafeFutureHandle<AuthResult>(future.GetHandle()), auth_data);
      handler_->OnSignIn(provider_data_, auth_completion_handle);
    } else {
      auth_data->future_impl.CompleteWithResult(
          SafeFutureHandle<AuthResult>(future.GetHandle()),
          kAuthErrorUnimplemented,
          "Operation is not supported on non-mobile systems.", AuthResult());
    }
  }
  return future;
}

Future<AuthResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), auth_data);
  Future<AuthResult> future =
      CreateAuthFuture(auth_data, kUserFn_LinkWithProvider);
  if (future.status() == kFutureStatusPending) {
    if (handler_ != nullptr) {
      AuthResultCompletionHandle* auth_completion_handle =
          new AuthResultCompletionHandle(
              SafeFutureHandle<AuthResult>(future.GetHandle()), auth_data);
      handler_->OnLink(provider_data_, auth_completion_handle);
    } else {
      auth_data->future_impl.CompleteWithResult(
          SafeFutureHandle<AuthResult>(future.GetHandle()),
          kAuthErrorUnimplemented,
          "Operation is not supported on non-mobile systems.", AuthResult());
    }
  }
  return future;
}

Future<AuthResult> FederatedOAuthProvider::Reauthenticate(AuthData* auth_data) {
  FIREBASE_ASSERT_RETURN(Future<AuthResult>(), auth_data);
  Future<AuthResult> future =
      CreateAuthFuture(auth_data, kUserFn_ReauthenticateWithProvider);
  if (future.status() == kFutureStatusPending) {
    if (handler_ != nullptr) {
      AuthResultCompletionHandle* auth_completion_handle =
          new AuthResultCompletionHandle(
              SafeFutureHandle<AuthResult>(future.GetHandle()), auth_data);
      handler_->OnReauthenticate(provider_data_, auth_completion_handle);
    } else {
      auth_data->future_impl.CompleteWithResult(
          SafeFutureHandle<AuthResult>(future.GetHandle()),
          kAuthErrorUnimplemented,
          "Operation is not supported on non-mobile systems.", AuthResult());
    }
  }
  return future;
}

// Helper function to identify any missing required data from an
// AuthenticatedUserData struct.
const char* CheckForRequiredAuthenicatedUserData(
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  const char* error_message = nullptr;
  if (user_data.uid == nullptr || user_data.uid[0] == '\0') {
    error_message = "null or empty uid";
  } else if (user_data.provider_id == nullptr ||
             user_data.provider_id[0] == '\0') {
    error_message = "null or empty provider_id";
  } else if (user_data.access_token == nullptr ||
             user_data.access_token[0] == '\0') {
    error_message = "null or empty access_token";
  } else if (user_data.refresh_token == nullptr ||
             user_data.refresh_token[0] == '\0') {
    error_message = "null or empty refresh_token";
  }
  return error_message;
}

// Helper function which uses the AuthResultCompletionHandle to plumb an
// asynchronous custom-application result into a Future<AuthResult>.
// Note: error_message is an optional parameter.
void CompleteAuthHandle(
    AuthResultCompletionHandle* completion_handle,
    const FederatedAuthProvider::AuthenticatedUserData& user_data,
    AuthError auth_error, const char* error_message, bool require_signed_in,
    bool require_matching_uid) {
  assert(completion_handle);
  assert(completion_handle->auth_data);
  AuthResult auth_result;
  if (auth_error == kAuthErrorNone) {
    error_message = CheckForRequiredAuthenicatedUserData(user_data);
    if (error_message != nullptr) {
      auth_error = kAuthErrorInvalidAuthenticatedUserData;
    } else {
      std::string current_uid;
      const bool is_signed_in = UserView::TryRead(
          completion_handle->auth_data,
          [&current_uid](const UserView::Reader& user) {
            current_uid = user->uid;
          });
      if (require_signed_in && !is_signed_in) {
        auth_error = kAuthErrorNoSignedInUser;
        error_message = GetAuthErrorMessage(auth_error);
      } else if (require_matching_uid && current_uid != user_data.uid) {
        auth_error = kAuthErrorUserMismatch;
        error_message = GetAuthErrorMessage(auth_error);
      } else {
        AuthenticationResult authentication_result =
            CompleteAuthenticedUserSignInFlow(completion_handle->auth_data,
                                              user_data);
        if (authentication_result.IsValid()) {
          auth_result = authentication_result.SetAsCurrentUser(
              completion_handle->auth_data);
        } else {
          auth_error = authentication_result.error() != kAuthErrorNone
                           ? authentication_result.error()
                           : kAuthErrorInvalidAuthenticatedUserData;
          error_message = "Internal parse error";
        }
      }
    }
  } else if (error_message == nullptr || error_message[0] == '\0') {
    error_message = "Federated auth provider error";
  }
  completion_handle->auth_data->future_impl.CompleteWithResult(
      completion_handle->future_handle, auth_error,
      (error_message) ? error_message : "", auth_result);
  delete completion_handle;
}

// Completion handlers for Federated OAuth sign-in.
template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::SignInComplete(
    AuthResultCompletionHandle* completion_handle,
    const AuthenticatedUserData& user_data, AuthError auth_error,
    const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message,
                     /*require_signed_in=*/false,
                     /*require_matching_uid=*/false);
}

template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::LinkComplete(
    AuthResultCompletionHandle* completion_handle,
    const AuthenticatedUserData& user_data, AuthError auth_error,
    const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message,
                     /*require_signed_in=*/true,
                     /*require_matching_uid=*/false);
}

template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::
    ReauthenticateComplete(AuthResultCompletionHandle* completion_handle,
                           const AuthenticatedUserData& user_data,
                           AuthError auth_error, const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message,
                     /*require_signed_in=*/true,
                     /*require_matching_uid=*/true);
}

}  // namespace auth
}  // namespace firebase
