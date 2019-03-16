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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_ASSERTION_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_ASSERTION_RESPONSE_H_

#include <string>
#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class VerifyAssertionResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(VerifyAssertionResponse, AuthResponse)

  std::string local_id() const { return application_data_->localId; }

  std::string id_token() const { return application_data_->idToken; }

  std::string refresh_token() const { return application_data_->refreshToken; }

  // The number of seconds until the ID token expires.
  int expires_in() const {
    if (application_data_->expiresIn.empty()) {
      return 0;
    } else {
      return std::stoi(application_data_->expiresIn);
    }
  }

  std::string provider_id() const { return application_data_->providerId; }

  std::string raw_user_info() const { return application_data_->rawUserInfo; }

  bool need_confirmation() const { return application_data_->needConfirmation; }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_ASSERTION_RESPONSE_H_
