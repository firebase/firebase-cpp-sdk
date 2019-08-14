/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_

#include <memory>
#include <string>
#include <vector>
#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class GetAccountInfoResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(GetAccountInfoResponse, AuthResponse)

  std::string local_id() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->localId
               : std::string();
  }

  std::string display_name() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->displayName
               : std::string();
  }

  std::string email() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->email
               : std::string();
  }

  std::string photo_url() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->photoUrl
               : std::string();
  }

  bool email_verified() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->emailVerified
               : false;
  }

  std::string password_hash() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->passwordHash
               : std::string();
  }

  std::string phone_number() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->phoneNumber
               : std::string();
  }

  uint64_t last_login_at() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->lastLoginAt
               : 0;
  }

  uint64_t created_at() const {
    return !application_data_->users.empty()
               ? application_data_->users[0]->createdAt
               : 0;
  }

  const std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>&
  providerUserInfos() const {
    if (!application_data_->users.empty()) {
      return application_data_->users[0]->providerUserInfo;
    }
    static std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>
        fallback;
    return fallback;
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_GET_ACCOUNT_INFO_RESPONSE_H_
