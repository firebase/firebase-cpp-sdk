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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_
#define FIREBASE_AUTH_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_

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

class GetAccountInfoResult {
 public:
  explicit GetAccountInfoResult(AuthError const error) : error_(error) {
    if (error_ == kAuthErrorNone) {
      LogDebug(
          "When creating invalid GetAccountInfoResult, use error other than "
          "kAuthErrorNone");
    }
  }

  static GetAccountInfoResult FromResponse(
      const GetAccountInfoResponse& response);

  bool IsValid() const { return error_ == kAuthErrorNone; }
  AuthError error() const { return error_; }
  void MergeToCurrentUser(AuthData* auth_data) const;
  void MergeToUser(UserView::Writer& writer) const;
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

#endif  // FIREBASE_AUTH_SRC_DESKTOP_GET_ACCOUNT_INFO_RESULT_H_
