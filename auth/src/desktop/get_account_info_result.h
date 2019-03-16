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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_

#include <vector>
#include "app/src/log.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/provider_user_info.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

// Represents results of a getAccountInfo operation, which can then be merged
// into the currently signed-in user.
class GetAccountInfoResult {
 public:
  // Creates an immutable invalid GetAccountInfoResult.
  explicit GetAccountInfoResult(AuthError const error) : error_(error) {
    if (error_ == kAuthErrorNone) {
      LogDebug(
          "When creating invalid SetAccountInfoResult, use error other than "
          "kAuthErrorNone");
    }
  }

  // Creates result corresponding to the given response; will be invalid
  // (!IsValid()) if the response contains an error.
  static GetAccountInfoResult FromResponse(
      const GetAccountInfoResponse& response);

  // Whether the operation was successful.
  bool IsValid() const { return error_ == kAuthErrorNone; }
  // Error code associated with this operation.
  AuthError error() const { return error_; }

  // Updates the properties of the currently signed-in user to those returned by
  // the operation this result represents.
  //
  // Updates to AuthData are done in a thread-safe manner.
  void MergeToCurrentUser(AuthData* auth_data) const;
  void MergeToUser(UserView::Writer& writer) const;

  // Only contains fields that are returned by getAccountInfo API. Blank if
  // operation failed.
  const UserData& user() const { return user_impl_; }

 private:
  friend class AuthenticationResult;

  GetAccountInfoResult() : error_(kAuthErrorNone) {}

  AuthError error_;

  UserData user_impl_;
  std::vector<UserInfoImpl> provider_data_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_
