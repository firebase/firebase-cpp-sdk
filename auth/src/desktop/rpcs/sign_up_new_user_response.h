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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_NEW_USER_RESPONSE_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_NEW_USER_RESPONSE_H_

#include <string>

#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

// Represents the response payload for the signUpNewUser HTTP API. Only relevant
// fields are exposed by getter functions.
class SignUpNewUserResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(SignUpNewUserResponse, AuthResponse)

  // The ID token from Secure Token Service.
  std::string id_token() const {
    return application_data_ ? application_data_->idToken : std::string();
  }

  // The refresh token from Secure Token Service.
  std::string refresh_token() const {
    return application_data_ ? application_data_->refreshToken : std::string();
  }

  // The local id of the new user.
  std::string local_id() const {
    return application_data_ ? application_data_->localId : std::string();
  }

  // The email of the new user; empty if the user is anonymous.
  std::string email() const {
    return application_data_ ? application_data_->email : std::string();
  }

  // Whether the newly created user is anonymous.
  bool IsAnonymousUser() const {
    return application_data_ ? application_data_->email.empty() : false;
  }

  // The number of seconds until the ID token expires.
  int expires_in() const {
    return application_data_ ? ParseExpiresIn(application_data_->expiresIn) : 0;
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_NEW_USER_RESPONSE_H_
