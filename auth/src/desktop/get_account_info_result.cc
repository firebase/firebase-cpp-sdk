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

#include "auth/src/desktop/get_account_info_result.h"

#include "app/rest/util.h"
#include "app/src/assert.h"
#include "auth/src/desktop/auth_util.h"

namespace firebase {
namespace auth {

GetAccountInfoResult GetAccountInfoResult::FromResponse(
    const GetAccountInfoResponse& response) {
  if (!response.IsSuccessful()) {
    return GetAccountInfoResult(response.error_code());
  }

  GetAccountInfoResult result;

  result.user_impl_.uid = response.local_id();
  result.user_impl_.email = response.email();
  result.user_impl_.phone_number = response.phone_number();
  result.user_impl_.display_name = response.display_name();
  result.user_impl_.photo_url = response.photo_url();
  result.user_impl_.is_email_verified = response.email_verified();
  result.user_impl_.has_email_password_credential =
      response.password_hash().length() > 0;
  result.user_impl_.creation_timestamp = response.created_at();
  result.user_impl_.last_sign_in_timestamp = response.last_login_at();

  result.provider_data_ = ParseProviderUserInfo(response);

  return result;
}

void GetAccountInfoResult::MergeToUser(UserView::Writer& user) const {
  if (!IsValid() || !user.IsValid()) {
    return;
  }
  user->uid = user_impl_.uid;
  user->email = user_impl_.email;
  user->display_name = user_impl_.display_name;
  user->photo_url = user_impl_.photo_url;
  user->phone_number = user_impl_.phone_number;
  user->is_email_verified = user_impl_.is_email_verified;
  user->has_email_password_credential =
      user_impl_.has_email_password_credential;
  user->creation_timestamp = user_impl_.creation_timestamp;
  user->last_sign_in_timestamp = user_impl_.last_sign_in_timestamp;

  user.ResetUserInfos(provider_data_);
}

void GetAccountInfoResult::MergeToCurrentUser(AuthData* const auth_data) const {
  auto user = UserView::GetWriter(auth_data);
  MergeToUser(user);
}

}  // namespace auth
}  // namespace firebase
