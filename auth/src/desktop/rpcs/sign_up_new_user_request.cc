// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "auth/src/desktop/rpcs/sign_up_new_user_request.h"

#include "app/src/assert.h"

namespace firebase {
namespace auth {

SignUpNewUserRequest::SignUpNewUserRequest(const char* api_key)
    : AuthRequest(request_resource_data) {
  SetUrl(api_key);
  application_data_->returnSecureToken = true;
  UpdatePostFields();
}

SignUpNewUserRequest::SignUpNewUserRequest(const char* api_key,
                                           const char* email,
                                           const char* password,
                                           const char* display_name)
    : AuthRequest(request_resource_data) {
  SetUrl(api_key);
  // It's fine for any of the additional parameters to be null, in case it's an
  // anonymous sign up.
  if (email) {
    application_data_->email = email;
  }
  if (password) {
    application_data_->password = password;
  }
  if (display_name) {
    application_data_->displayName = display_name;
  }
  application_data_->returnSecureToken = true;
  UpdatePostFields();
}

void SignUpNewUserRequest::SetUrl(const char* api_key) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] =
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());
}

}  // namespace auth
}  // namespace firebase
