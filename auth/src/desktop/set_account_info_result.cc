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

#include "auth/src/desktop/set_account_info_result.h"

#include "app/rest/util.h"
#include "auth/src/desktop/auth_util.h"
#include "auth/src/desktop/provider_user_info.h"

namespace firebase {
namespace auth {

bool UpdateUserTokensIfChanged(UserView::Writer& user,
                               const TokenUpdate& token_update) {
  bool has_token_changed = false;

  // Update access token.
  if (!token_update.id_token.empty() &&
      token_update.id_token != user->id_token) {
    user->id_token = user->access_token = token_update.id_token;
    user->access_token_expiration_date = token_update.expiration_date;
    has_token_changed = true;
  }
  // Update refresh token.
  if (!token_update.refresh_token.empty()) {
    user->refresh_token = token_update.refresh_token;
  }

  return has_token_changed;
}

SetAccountInfoResult SetAccountInfoResult::FromResponse(
    const SetAccountInfoResponse& response) {
  if (!response.IsSuccessful()) {
    return SetAccountInfoResult(response.error_code());
  }

  SetAccountInfoResult result;

  result.user_impl_.uid = response.local_id();
  result.user_impl_.email = response.email();
  result.user_impl_.display_name = response.display_name();
  result.user_impl_.photo_url = response.photo_url();
  result.user_impl_.has_email_password_credential =
      response.password_hash().length() > 0;

  // No need to check whether response contains non-blank tokens in this case,
  // because the user being updated was blank anyway, so there is no danger of
  // overriding valid tokens with blanks.
  result.user_impl_.id_token = result.user_impl_.access_token =
      response.id_token();
  result.user_impl_.access_token_expiration_date =
      response.fetch_time() + response.expires_in();
  result.user_impl_.refresh_token = response.refresh_token();

  result.provider_data_ = ParseProviderUserInfo(response);

  return result;
}

User* SetAccountInfoResult::MergeToCurrentUser(
    AuthData* const auth_data) const {
  FIREBASE_ASSERT_RETURN(nullptr, auth_data);
  if (!IsValid()) {
    return nullptr;
  }

  bool has_token_changed = false;
  User* api_user_to_return = nullptr;
  {
    UserView::Writer user = UserView::GetWriter(auth_data);
    if (!user.IsValid()) {
      return nullptr;
    }

    has_token_changed =
        UpdateUserTokensIfChanged(user, TokenUpdate(user_impl_));

    user->uid = user_impl_.uid;
    user->email = user_impl_.email;
    user->display_name = user_impl_.display_name;
    user->photo_url = user_impl_.photo_url;
    user->has_email_password_credential =
        user_impl_.has_email_password_credential;

    // If email was linked to an anonymous account, it's no longer anonymous.
    // Note: both checks are necessary, the backend is happy to update email or
    // password separately on an anonymous account. Unless both are set, user
    // won't be able to log in with email credential and therefore is still
    // effectively anonymous.
    if (!user_impl_.email.empty() && user_impl_.has_email_password_credential) {
      user->is_anonymous = false;
    }

    user.ResetUserInfos(provider_data_);

    api_user_to_return = &auth_data->current_user;
  }

  if (has_token_changed) {
    NotifyIdTokenListeners(auth_data);
  }
  return api_user_to_return;
}

}  // namespace auth
}  // namespace firebase
