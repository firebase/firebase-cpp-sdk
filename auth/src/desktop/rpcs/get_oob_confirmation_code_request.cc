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
    : AuthRequest(app, request_resource_data, true,
                  "getOobConfirmationCode?key=", api_key) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);
}

std::unique_ptr<GetOobConfirmationCodeRequest>
GetOobConfirmationCodeRequest::CreateRequest(::firebase::App& app,
                                             const char* api_key,
                                             const char* request_type,
                                             const char* language_code) {
  auto request = std::unique_ptr<GetOobConfirmationCodeRequest>(  // NOLINT
      new GetOobConfirmationCodeRequest(app, api_key));
  request->application_data_->requestType = request_type;
  request->SetLocaleHeader(language_code);
  return request;
}

std::unique_ptr<GetOobConfirmationCodeRequest>
GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest(
    ::firebase::App& app, const char* api_key, const char* language_code) {
  auto request = CreateRequest(app, api_key, "VERIFY_EMAIL", language_code);
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<GetOobConfirmationCodeRequest> GetOobConfirmationCodeRequest::
    CreateSendEmailVerificationBeforeUpdatingEmailRequest(
        ::firebase::App& app, const char* api_key, const char* email,
        const char* language_code) {
  auto request =
      CreateRequest(app, api_key, "VERIFY_AND_CHANGE_EMAIL", language_code);
  if (email && *email != '\0') {
    request->application_data_->newEmail = email;
  } else {
    LogError("No email given.");
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<GetOobConfirmationCodeRequest>
GetOobConfirmationCodeRequest::CreateSendPasswordResetEmailRequest(
    ::firebase::App& app, const char* api_key, const char* email,
    const char* language_code) {
  auto request = CreateRequest(app, api_key, "PASSWORD_RESET", language_code);
  if (email && *email != '\0') {
    request->application_data_->email = email;
  } else {
    LogError("No email given.");
  }
  request->UpdatePostFields();
  return request;
}

}  // namespace auth
}  // namespace firebase
