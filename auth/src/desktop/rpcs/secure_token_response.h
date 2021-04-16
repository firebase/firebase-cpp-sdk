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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SECURE_TOKEN_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SECURE_TOKEN_RESPONSE_H_

#include <string>

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class SecureTokenResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(SecureTokenResponse, AuthResponse)

  // The access token.
  std::string access_token() const { return application_data_->access_token; }

  // The refresh token.
  std::string refresh_token() const { return application_data_->refresh_token; }

  // The id token.
  std::string id_token() const { return application_data_->id_token; }

  // The number of seconds till the access token expires.
  int expires_in() const {
    if (application_data_->expires_in.empty()) {
      return 0;
    } else {
      return std::stoi(application_data_->expires_in);
    }
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_SECURE_TOKEN_RESPONSE_H_
