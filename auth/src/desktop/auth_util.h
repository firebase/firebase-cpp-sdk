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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_AUTH_UTIL_H_
#define FIREBASE_AUTH_SRC_DESKTOP_AUTH_UTIL_H_

#include <cassert>
#include <memory>
#include <type_traits>
#include <vector>

#include "app/rest/request.h"
#include "app/rest/transport_builder.h"
#include "app/src/assert.h"
#include "app/src/callback.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "auth/src/common.h"
#include "auth/src/common/promise.h"
#include "auth/src/data.h"
#include "auth/src/desktop/auth_constants.h"
#include "auth/src/desktop/auth_data_handle.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/desktop/authentication_result.h"
#include "auth/src/desktop/rpcs/auth_request.h"
#include "auth/src/desktop/rpcs/error_codes.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

using firebase::callback::NewCallback;

const char* GetApiKey(const AuthData& auth_data);

void CompletePromise(Promise<AuthResult>* promise,
                     const AuthResult& auth_result);
void CompletePromise(Promise<User>* promise, const AuthResult& auth_result);
void CompletePromise(Promise<void>* promise, const AuthResult& auth_result);

template <typename T>
inline void FailPromise(Promise<T>* const promise, const AuthError error_code) {
  FIREBASE_ASSERT_RETURN_VOID(promise);
  promise->Fail(error_code, GetAuthErrorMessage(error_code));
}

template <typename ResultT, typename RequestT>
inline void CallAsync(
    AuthData* const auth_data, Promise<ResultT> promise,
    std::unique_ptr<RequestT> request,
    const typename AuthDataHandle<ResultT, RequestT>::CallbackT callback) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data && callback);
  typedef AuthDataHandle<ResultT, RequestT> HandleT;
  auto scheduler_callback = NewCallback(
      [](HandleT* const raw_auth_data_handle) {
        std::unique_ptr<HandleT> handle(raw_auth_data_handle);
        if constexpr (std::is_base_of_v<AuthRequest, RequestT>) {
          if (handle->request) handle->request->PrepareRequestHeaders();
        }
        handle->callback(handle.get());
      },
      new HandleT(auth_data, promise, std::move(request), callback));
  static_cast<AuthImpl*>(auth_data->auth_impl)
      ->scheduler_.Schedule(scheduler_callback);
}

template <typename T>
inline T GetResponse(AuthRequest& request) {
  request.PrepareRequestHeaders();
  T response;
  firebase::rest::CreateTransport()->Perform(request, &response);
  return response;
}

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_AUTH_UTIL_H_
