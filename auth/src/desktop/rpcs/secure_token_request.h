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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_SECURE_TOKEN_REQUEST_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_SECURE_TOKEN_REQUEST_H_

#include "auth/request_generated.h"
#include "auth/request_resource.h"
#include "auth/src/desktop/rpcs/auth_request.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace auth {

class SecureTokenRequest : public AuthRequest {
 public:
  // Set request by using refresh token.
  SecureTokenRequest( ::firebase::App& app, const char* api_key, const char* refresh_token);
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_SECURE_TOKEN_REQUEST_H_
