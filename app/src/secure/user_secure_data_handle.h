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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DATA_HANDLE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DATA_HANDLE_H_

#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"

namespace firebase {
namespace app {
namespace secure {

enum UserSecureFn {
  kUserSecureFnLoad,
  kUserSecureFnSave,
  kUserSecureFnDelete,
  kUserSecureFnDeleteAll,
  kUserSecureFnCount
};

enum UserSecureFutureResult {
  kSuccess,
  kNoEntry,
};

template <typename T>
struct UserSecureDataHandle {
  UserSecureDataHandle(const std::string& appName, const std::string& userData,
                       ReferenceCountedFutureImpl* futureApi,
                       const SafeFutureHandle<T>& futureHandle)
      : app_name(appName),
        user_data(userData),
        future_api(futureApi),
        future_handle(futureHandle) {}

  const std::string app_name;
  const std::string user_data;
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<T> future_handle;
};

}  // namespace secure
}  // namespace app
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SECURE_USER_SECURE_DATA_HANDLE_H_
