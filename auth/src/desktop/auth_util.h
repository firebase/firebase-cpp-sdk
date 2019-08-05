// Copyright 2017 Google LLC
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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_UTIL_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_UTIL_H_

#include <memory>
#include <vector>

#include "app/rest/request.h"
#include "app/rest/transport_builder.h"
#include "app/src/assert.h"
#include "app/src/callback.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/authentication_result.h"
#include "auth/src/desktop/promise.h"
#include "auth/src/desktop/rpcs/error_codes.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

using firebase::callback::NewCallback;

const char* GetApiKey(const AuthData& auth_data);

// Completes the given promise by translating the AuthenticationResult. If the
// AuthenticationResult is successful, promise will be completed successfully
// with the currently signed-in user. If the AuthenticationResult is failed, the
// promise will be failed, too, by extracting error from the
// AuthenticationResult.
void CompletePromise(Promise<User*>* promise,
                     const SignInResult& sign_in_result);

// Same as the version for Promise<User*>, but additionally extracts
// AdditionalUserInfo from AuthenticationResult if it was successful.
void CompletePromise(Promise<SignInResult>* promise,
                     const SignInResult& sign_in_result);

void CompletePromise(Promise<void>* promise,
                     const SignInResult& sign_in_result);

// Fails the given promise with the given error_code and also provides
// a human-readable description corresponding to the error code.
template <typename T>
void FailPromise(Promise<T>* promise, AuthError error_code);

// Invokes the given callback on another thread and passes the rest of the
// arguments to the invocation.
template <typename ResultT, typename RequestT>
Future<ResultT> CallAsync(
    AuthData* auth_data, Promise<ResultT> promise,
    std::unique_ptr<RequestT> request,
    const typename AuthDataHandle<ResultT, RequestT>::CallbackT callback);

// Sends the given request on the network and returns the response. The response
// is cast to the specified T without any checks, so it's the caller's
// responsibility to ensure the correct type is given.
//
// Note: this is a blocking call! Use in the callback given to CallAsync, or
// otherwise on a separate thread.
template <typename T>
T GetResponse(const rest::Request& request);

// Implementation

template <typename ResultT, typename RequestT>
inline Future<ResultT> CallAsync(
    AuthData* const auth_data, Promise<ResultT> promise,
    std::unique_ptr<RequestT> request,
    const typename AuthDataHandle<ResultT, RequestT>::CallbackT callback) {
  // Note: it's okay for the caller to pass a null request - they may want to
  // create the request inside the callback invocation, and this function
  // doesn't need to access the request anyway.
  FIREBASE_ASSERT_RETURN(Future<ResultT>(), auth_data && callback);

  typedef AuthDataHandle<ResultT, RequestT> HandleT;

  auto scheduler_callback = NewCallback(
      [](HandleT* const raw_auth_data_handle) {
        std::unique_ptr<HandleT> handle(raw_auth_data_handle);
        handle->callback(handle.get());
      },
      new HandleT(auth_data, promise, std::move(request), callback));
  auto auth_impl = static_cast<AuthImpl*>(auth_data->auth_impl);
  auth_impl->scheduler_.Schedule(scheduler_callback);

  return promise.future();
}

template <typename T>
inline T GetResponse(const rest::Request& request) {
  T response;
  firebase::rest::CreateTransport()->Perform(request, &response);
  return response;
}

template <typename T>
inline void FailPromise(Promise<T>* const promise, const AuthError error_code) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->Fail(error_code, GetAuthErrorMessage(error_code));
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_UTIL_H_
