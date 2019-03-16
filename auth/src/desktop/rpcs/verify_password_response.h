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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_PASSWORD_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_PASSWORD_RESPONSE_H_

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class VerifyPasswordResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(VerifyPasswordResponse, AuthResponse)

  std::string local_id() const { return application_data_->localId; }

  std::string email() const { return application_data_->email; }

  std::string display_name() const { return application_data_->displayName; }

  std::string id_token() const { return application_data_->idToken; }

  std::string refresh_token() const { return application_data_->refreshToken; }

  std::string photo_url() const { return application_data_->photoUrl; }

  // The number of seconds til the access token expires.
  int expires_in() const {
    if (application_data_->expiresIn.empty()) {
      return 0;
    } else {
      return std::stoi(application_data_->expiresIn);
    }
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_PASSWORD_RESPONSE_H_
