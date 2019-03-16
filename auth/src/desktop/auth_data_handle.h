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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DATA_HANDLE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DATA_HANDLE_H_

#include <memory>
#include "app/rest/request.h"
#include "app/src/include/firebase/future.h"
#include "auth/src/data.h"
#include "auth/src/desktop/promise.h"

namespace firebase {
namespace auth {

// Holds both AuthData and Promise to be passed into async calls. We need
// to create FutureHandle (done within Promise) before an async call
// starts. Otherwise the call to last-result function may execute before a new
// FutureHandle is created and thus return previous last result.
template <typename ResultT, class RequestT>
struct AuthDataHandle {
  typedef void (*CallbackT)(AuthDataHandle*);
  AuthDataHandle(AuthData* const set_auth_data,
                 const Promise<ResultT>& set_promise,
                 std::unique_ptr<RequestT> set_request,
                 const CallbackT set_callback)
      : auth_data(set_auth_data),
        promise(set_promise),
        request(std::move(set_request)),
        callback(set_callback) {}

  AuthData* const auth_data;
  Promise<ResultT> promise;

  std::unique_ptr<RequestT> request;
  CallbackT callback;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_AUTH_DATA_HANDLE_H_
