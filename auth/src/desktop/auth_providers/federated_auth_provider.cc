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

#include "app/src/include/firebase/future.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/sign_in_flow.h"

#include "app/src/mutex.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/types.h"

namespace firebase {
namespace auth {

#ifdef INTERNAL_EXPERIMENTAL

FederatedOAuthProvider::FederatedOAuthProvider() { }

FederatedOAuthProvider::FederatedOAuthProvider(
    const FederatedOAuthProviderData& provider_data) {
  provider_data_ = provider_data;
}

FederatedOAuthProvider::~FederatedOAuthProvider() {
  handler_ = nullptr;
}

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
Future<SignInResult> CreateAuthFuture(AuthData* auth_data,
                                      AuthApiFunction api_function) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), auth_data);
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  MutexLock lock(auth_impl->provider_mutex);
  auto future_base =
      auth_data->future_impl.LastResult(api_function);
  if (future_base.status() == kFutureStatusPending) {
    // There's an operation in progress. Create and return a new failed
    // future.
    SafeFutureHandle<SignInResult> handle =
        auth_data->future_impl.SafeAlloc<SignInResult>(api_function);
    auth_data->future_impl.CompleteWithResult(
        handle, kAuthErrorFederatedProviderAreadyInUse,
        "Provider operation already in progress.",
        /*result=*/{});
    return MakeFuture(&auth_data->future_impl, handle);
  } else if (future_base.status() == kFutureStatusInvalid) {
    // initialize the future.
    SafeFutureHandle<SignInResult> handle =
        auth_data->future_impl.SafeAlloc<SignInResult>(api_function);
    Future<SignInResult> result = MakeFuture(&auth_data->future_impl,
                    SafeFutureHandle<SignInResult>(handle));
    auto future_base =
      auth_data->future_impl.LastResult(api_function);
    return result;
  } else {
    // Construct future.
    return MakeFuture(&auth_data->future_impl,
                      SafeFutureHandle<SignInResult>(future_base.GetHandle()));
  }
}

Future<SignInResult> FederatedOAuthProvider::SignIn(AuthData* auth_data) {
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), handler_);
  assert(auth_data);
  Future<SignInResult> future =
      CreateAuthFuture(auth_data, kAuthFn_SignInWithProvider);
  if (future.status() == kFutureStatusPending) {
    AuthCompletionHandle* auth_completion_handle = new AuthCompletionHandle(
        SafeFutureHandle<SignInResult>(future.GetHandle()), auth_data);
    handler_->OnSignIn(provider_data_, auth_completion_handle);
  }
  return future;
}

Future<SignInResult> FederatedOAuthProvider::Link(AuthData* auth_data) {
  assert(auth_data);
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), handler_);
  Future<SignInResult> future =
      CreateAuthFuture(auth_data, kUserFn_LinkWithProvider);
  if (future.status() == kFutureStatusPending) {
    AuthCompletionHandle* auth_completion_handle = new AuthCompletionHandle(
        SafeFutureHandle<SignInResult>(future.GetHandle()), auth_data);
    handler_->OnLink(provider_data_, auth_completion_handle);
  }
  return future;
}

Future<SignInResult> FederatedOAuthProvider::Reauthenticate(
    AuthData* auth_data) {
  assert(auth_data);
  FIREBASE_ASSERT_RETURN(Future<SignInResult>(), handler_);
  Future<SignInResult> future =
      CreateAuthFuture(auth_data, kUserFn_ReauthenticateWithProvider);
  if (future.status() == kFutureStatusPending) {
    AuthCompletionHandle* auth_completion_handle = new AuthCompletionHandle(
        SafeFutureHandle<SignInResult>(future.GetHandle()), auth_data);
    handler_->OnReauthenticate(provider_data_, auth_completion_handle);
  }
  return future;
}

// Helper function to identify any missing required data from an
// AuthetnicatedUserData struct.
const char* CheckForRequiredAuthenicatedUserData(
    const FederatedAuthProvider::AuthenticatedUserData& user_data) {
  const char* error_message = nullptr;
  if (user_data.uid == nullptr) {
    error_message = "null uid";
  } else if (user_data.provider_id == nullptr) {
    error_message = "null provider_id";
  } else if (user_data.access_token == nullptr) {
    error_message = "null access_token";
  } else if (user_data.refresh_token == nullptr) {
    error_message = "null refresh_token";
  }
  return error_message;
}

// Helper function which uses the AuthCompletionHandle to plumb an
// asynchronous custom-application result into a Future<SignInResult>.
// Note: error_message is an optional parameter.
void CompleteAuthHandle(
    AuthCompletionHandle* completion_handle,
    const FederatedAuthProvider::AuthenticatedUserData& user_data,
    AuthError auth_error, const char* error_message) {
  assert(completion_handle);
  assert(completion_handle->auth_data);
  SignInResult sign_in_result;
  if (auth_error == kAuthErrorNone) {
    error_message = CheckForRequiredAuthenicatedUserData(user_data);
    if (error_message != nullptr) {
      auth_error = kAuthErrorInvalidAuthenticatedUserData;
    } else {
      AuthenticationResult auth_result = CompleteAuthenticedUserSignInFlow(
          completion_handle->auth_data, user_data);
      if (auth_result.IsValid()) {
        sign_in_result =
            auth_result.SetAsCurrentUser(completion_handle->auth_data);
      } else {
        auth_error = kAuthErrorInvalidAuthenticatedUserData;
        error_message = "Internal parse error";
      }
    }
  }
  completion_handle->auth_data->future_impl.CompleteWithResult(
      completion_handle->future_handle, auth_error,
      (error_message) ? error_message : "", sign_in_result);
  delete completion_handle;
}

// Completion handlers for Federated OAuth sign-in.
template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::SignInComplete(
    AuthCompletionHandle* completion_handle,
    const AuthenticatedUserData& user_data, AuthError auth_error,
    const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message);
}

template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::LinkComplete(
    AuthCompletionHandle* completion_handle,
    const AuthenticatedUserData& user_data, AuthError auth_error,
    const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message);
}

template <>
void FederatedAuthProvider::Handler<FederatedOAuthProviderData>::
    ReauthenticateComplete(AuthCompletionHandle* completion_handle,
                           const AuthenticatedUserData& user_data,
                           AuthError auth_error, const char* error_message) {
  FIREBASE_ASSERT_RETURN_VOID(completion_handle);
  CompleteAuthHandle(completion_handle, user_data, auth_error, error_message);
}
#endif  // INTERNAL_EXPERIMENTAL

}  // namespace auth
}  // namespace firebase
