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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SET_ACCOUNT_INFO_RESULT_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SET_ACCOUNT_INFO_RESULT_H_

#include <vector>
#include "app/src/assert.h"
#include "app/src/log.h"
#include "auth/src/common.h"
#include "auth/src/data.h"
#include "auth/src/desktop/provider_user_info.h"
#include "auth/src/desktop/rpcs/set_account_info_response.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

// Extracts tokens from either a response or UserData to avoid having too many
// function overloads of UpdateUserTokensIfChanged.
struct TokenUpdate {
  template <typename ResponseT>
  explicit TokenUpdate(const ResponseT& response)
      : id_token(response.id_token()),
        expiration_date(response.fetch_time() + response.expires_in()),
        refresh_token(response.refresh_token()) {}

  explicit TokenUpdate(const UserData& user)
      : id_token(user.id_token),
        expiration_date(user.access_token_expiration_date),
        refresh_token(user.refresh_token) {}

  // Whether this update contains any non-blank tokens. Use this check to see
  // if there's any need to update user and lock the mutex.
  bool HasUpdate() const { return !id_token.empty() || !refresh_token.empty(); }

  const std::string id_token;
  const std::time_t expiration_date;
  const std::string refresh_token;
};

// Applies new tokens contained in the given token_update (if any) to the given
// user. Returns whether the user's ID token has changed.
bool UpdateUserTokensIfChanged(UserView::Writer& user,
                               const TokenUpdate& token_update);

// Represents results of a setAccountInfo operation, which can then be merged
// into the currently signed-in user.
class SetAccountInfoResult {
 public:
  // Creates an immutable invalid SetAccountInfoResult.
  explicit SetAccountInfoResult(AuthError const error) : error_(error) {
    if (error_ == kAuthErrorNone) {
      LogDebug(
          "When creating invalid SetAccountInfoResult, use error other than "
          "kAuthErrorNone");
    }
  }

  static SetAccountInfoResult FromResponse(
      const SetAccountInfoResponse& response);

  // Whether the operation was successful.
  bool IsValid() const { return error_ == kAuthErrorNone; }
  // Error code associated with this operation.
  AuthError error() const { return error_; }

  // Updates the properties of the currently signed-in user to those returned by
  // the operation this result represents, and returns the pointer to current
  // user which can be easily returned to the API caller.
  //
  // Updates to AuthData are done in a thread-safe manner.
  User* MergeToCurrentUser(AuthData* auth_data) const;

 private:
  friend class AuthenticationResult;

  SetAccountInfoResult() : error_(kAuthErrorNone) {}

  const AuthError error_;

  UserData user_impl_;
  std::vector<UserInfoImpl> provider_data_;
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_SET_ACCOUNT_INFO_RESULT_H_
