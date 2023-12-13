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

#include "auth/src/desktop/rpcs/sign_up_request.h"

#include <memory>
#include <string>

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace auth {

SignUpRequest::SignUpRequest(::firebase::App& app, const char* api_key)
    : AuthRequest(app, request_resource_data, true) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  std::string url(
      "https://identitytoolkit.googleapis.com/v1/accounts:signUp?key=");
  url.append(api_key);
  set_url(url.c_str());

  application_data_->returnSecureToken = true;
}

std::unique_ptr<SignUpRequest>
SignUpRequest::CreateLinkWithEmailAndPasswordRequest(::firebase::App& app,
                                                     const char* api_key,
                                                     const char* email,
                                                     const char* password) {
  auto request =
      std::unique_ptr<SignUpRequest>(new SignUpRequest(app, api_key));

  if (email) {
    request->application_data_->email = email;
  } else {
    LogError("No email given");
  }
  if (password) {
    request->application_data_->password = password;
  } else {
    LogError("No password given");
  }
  request->UpdatePostFields();
  return request;
}

}  // namespace auth
}  // namespace firebase
