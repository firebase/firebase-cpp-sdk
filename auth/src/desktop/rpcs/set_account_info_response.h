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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_RESPONSE_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_RESPONSE_H_

#include <memory>
#include <string>
#include <vector>

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class SetAccountInfoResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(SetAccountInfoResponse, AuthResponse)

  std::string local_id() const {
    return application_data_ ? application_data_->localId : std::string();
  }

  std::string id_token() const {
    return application_data_ ? application_data_->idToken : std::string();
  }

  std::string refresh_token() const {
    return application_data_ ? application_data_->refreshToken : std::string();
  }

  // The number of seconds until the ID token expires.
  int expires_in() const {
    return application_data_ ? ParseExpiresIn(application_data_->expiresIn) : 0;
  }

  std::string email() const {
    return application_data_ ? application_data_->email : std::string();
  }

  bool email_verified() const {
    return application_data_ ? application_data_->emailVerified : false;
  }

  std::string display_name() const {
    return application_data_ ? application_data_->displayName : std::string();
  }

  std::string photo_url() const {
    return application_data_ ? application_data_->photoUrl : std::string();
  }

  std::string password_hash() const {
    return application_data_ ? application_data_->passwordHash : std::string();
  }

  const std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>&
  providerUserInfos() const {
    if (application_data_) {
      return application_data_->providerUserInfo;
    }
    static const auto* const kFallback =
        new std::vector<flatbuffers::unique_ptr<fbs::ProviderUserInfoT>>();
    return *kFallback;
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_SET_ACCOUNT_INFO_RESPONSE_H_
