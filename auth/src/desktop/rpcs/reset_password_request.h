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

#ifndef FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_RESET_PASSWORD_REQUEST_H_
#define FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_RESET_PASSWORD_REQUEST_H_

#include "auth/request_generated.h"
#include "auth/request_resource.h"
#include "auth/src/desktop/rpcs/auth_request.h"

namespace firebase {
namespace auth {

class ResetPasswordRequest : public AuthRequest {
 public:
  ResetPasswordRequest(const char* api_key, const char* oob_code,
                       const char* new_password);
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_CLIENT_CPP_SRC_DESKTOP_RPCS_RESET_PASSWORD_REQUEST_H_
