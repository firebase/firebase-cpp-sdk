/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_REQUEST_H_
#define FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_REQUEST_H_

#include <memory>

#include "app/src/include/firebase/app.h"
#include "auth/request_generated.h"
#include "auth/request_resource.h"
#include "auth/src/desktop/rpcs/auth_request.h"

namespace firebase {
namespace auth {

// Represents the request payload for the signUp HTTP API. Use this to
// upgrade anonymous accounts with email and password. The full specification of
// the HTTP API can be found at
// https://cloud.google.com/identity-platform/docs/reference/rest/v1/accounts/signUp
class SignUpRequest : public AuthRequest {
 private:
  explicit SignUpRequest(::firebase::App& app, const char* api_key);
  static std::unique_ptr<SignUpRequest> CreateRequest(::firebase::App& app,
                                                      const char* api_key);

 public:
  // Initializer for linking an email and password to an account.
  static std::unique_ptr<SignUpRequest> CreateLinkWithEmailAndPasswordRequest(
      ::firebase::App& app, const char* api_key, const char* email,
      const char* password);

  void SetIdToken(const char* id_token) {
    if (id_token) {
      application_data_->idToken = id_token;
      UpdatePostFields();
    } else {
      LogError("No id token given.");
    }
  }
};

}  // namespace auth
}  // namespace firebase

#endif  // FIREBASE_AUTH_SRC_DESKTOP_RPCS_SIGN_UP_REQUEST_H_
