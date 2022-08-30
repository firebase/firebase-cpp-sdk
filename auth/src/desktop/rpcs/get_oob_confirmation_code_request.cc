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

#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace auth {

GetOobConfirmationCodeRequest::GetOobConfirmationCodeRequest(
    ::firebase::App& app, const char* api_key)
    : AuthRequest(app, request_resource_data, true) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] =
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getOobConfirmationCode?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());
}

std::unique_ptr<GetOobConfirmationCodeRequest>
GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest(
    ::firebase::App& app, const char* api_key, const char* language_code) {
  auto request = std::unique_ptr<GetOobConfirmationCodeRequest>(  // NOLINT
      new GetOobConfirmationCodeRequest(app, api_key));
  request->application_data_->requestType = "VERIFY_EMAIL";
  if (language_code != nullptr) {
    request->add_header(kHeaderFirebaseLocale, language_code);
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<GetOobConfirmationCodeRequest>
GetOobConfirmationCodeRequest::CreateSendPasswordResetEmailRequest(
    ::firebase::App& app, const char* api_key, const char* email,
    const char* language_code) {
  auto request = std::unique_ptr<GetOobConfirmationCodeRequest>(  // NOLINT
      new GetOobConfirmationCodeRequest(app, api_key));
  request->application_data_->requestType = "PASSWORD_RESET";
  if (language_code != nullptr) {
    request->add_header(kHeaderFirebaseLocale, language_code);
  }
  if (email) {
    request->application_data_->email = email;
  } else {
    LogError("No email token given.");
  }
  request->UpdatePostFields();
  return request;
}

}  // namespace auth
}  // namespace firebase
