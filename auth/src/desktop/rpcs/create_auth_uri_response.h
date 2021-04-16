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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_CREATE_AUTH_URI_RESPONSE_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_CREATE_AUTH_URI_RESPONSE_H_

#include <string>
#include <vector>
#include "auth/src/desktop/rpcs/auth_response.h"
#include "auth/src/desktop/visual_studio_compatibility.h"

namespace firebase {
namespace auth {

class CreateAuthUriResponse : public AuthResponse {
 public:
  DEFAULT_AND_MOVE_CTRS_NO_CLASS_MEMBERS(CreateAuthUriResponse, AuthResponse)

  // A list of provider IDs to be used to sign in with.
  const std::vector<std::string>& providers() const {
    return application_data_->allProviders;
  }

  // Whether the user is registered if the identifier is an email.
  bool registered() const { return application_data_->registered; }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_CREATE_AUTH_URI_RESPONSE_H_
