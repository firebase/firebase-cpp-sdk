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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_

#include <string>

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class VerifyCustomTokenResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(VerifyCustomTokenResponse,
                                         AuthResponse)

  // The ID token from Secure Token Service.
  std::string id_token() const {
    return application_data_ ? application_data_->idToken : std::string();
  }

  // The refresh token from Secure Token Service.
  std::string refresh_token() const {
    return application_data_ ? application_data_->refreshToken : std::string();
  }

  // VerifyCustomTokenResponse does not always contain local_id as a field; the
  // desktop implementation calls GetAccountInfo before resolving the future,
  // which populates the user's local_id.
  std::string local_id() const {
    return application_data_ ? application_data_->localId : std::string();
  }

  bool is_new_user() const {
    return application_data_ ? application_data_->isNewUser : false;
  }

  // The number of seconds until the ID token expires.
  int expires_in() const {
    return application_data_ ? ParseExpiresIn(application_data_->expiresIn) : 0;
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_
