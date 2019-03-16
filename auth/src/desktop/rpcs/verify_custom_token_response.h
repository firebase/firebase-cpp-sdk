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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_

#include <string>
#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class VerifyCustomTokenResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(VerifyCustomTokenResponse,
                                         AuthResponse)

  // Either an authorization code suitable for performing an STS token exchange,
  // or the access token from Secure Token Service
  std::string id_token() const { return application_data_->idToken; }

  // The refresh token from Secure Token Service.
  std::string refresh_token() const { return application_data_->refreshToken; }

  // TODO(varconst): see if it needs implementing. Unlike other responses,
  // VerifyCustomTokenResponse *doesn't* contain local_id as a field; instead,
  // you're supposed to parse JWT, which needs base64-decoding and JSON parsing
  // (see e.g. how it's done on Android: http://shortn/_EqJt1rsi2O).
  // However, since the desktop implementation always calls GetAccountInfo
  // before resolving the future, the result will be overridden anyway.
  std::string local_id() const { return std::string(); }

  bool is_new_user() const { return application_data_->isNewUser; }

  // The number of seconds till the access token expires.
  int expires_in() const {
    if (application_data_->expiresIn.empty()) {
      return 0;
    }
    return std::stoi(application_data_->expiresIn);
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_VERIFY_CUSTOM_TOKEN_RESPONSE_H_
