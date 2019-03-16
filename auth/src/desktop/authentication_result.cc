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

#include "auth/src/desktop/authentication_result.h"

#include <vector>
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/user_view.h"

namespace firebase {
namespace auth {

void AuthenticationResult::SetAccountInfo(const GetAccountInfoResult& info) {
  user_account_info_ = info;
}

std::string AuthenticationResult::uid() const {
  // Use value from GetAccountInfoResponse, because VerifyCustomTokenResponse
  // doesn't contain local_id field (from which uid is taken), and in other
  // cases it doesn't matter from which response uid is taken (they're supposed
  // to be identical).
  return user_account_info_.IsValid() ? user_account_info_.user_impl_.uid
                                      : user_impl_.uid;
}

SignInResult AuthenticationResult::SetAsCurrentUser(
    AuthData* const auth_data) const {
  FIREBASE_ASSERT_RETURN(SignInResult(), auth_data);
  if (!IsValid()) {
    return SignInResult();
  }

  // Save the previous user state to be able to check whether listeners should
  // be notified later on.
  UserData previous_user;
  // Don't call Auth::current_user() to avoid locking the mutex twice.
  User* api_user_to_return = nullptr;
  {
    UserView::Writer writer =
        UserView::ResetUser(auth_data, user_impl_, &previous_user);
    if (user_account_info_.IsValid()) {
      user_account_info_.MergeToUser(writer);
    }
    api_user_to_return = &auth_data->current_user;
  }

  if (previous_user.uid != uid()) {
    NotifyAuthStateListeners(auth_data);
  }
  if (previous_user.id_token != id_token()) {
    NotifyIdTokenListeners(auth_data);
  }

  SignInResult result;
  result.user = api_user_to_return;
  result.info = info_;
  return result;
}

void AuthenticationResult::SignOut(AuthData* const auth_data) {
  FIREBASE_ASSERT_RETURN_VOID(auth_data);

  // Save the previous user state to be able to check whether listeners should
  // be notified later on.
  UserData previous_user;
  UserView::ClearUser(auth_data, &previous_user);

  if (!previous_user.uid.empty()) {
    NotifyAuthStateListeners(auth_data);
  }
  if (!previous_user.id_token.empty()) {
    NotifyIdTokenListeners(auth_data);
  }
}

}  // namespace auth
}  // namespace firebase
