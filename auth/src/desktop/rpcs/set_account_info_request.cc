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

#include "auth/src/desktop/rpcs/set_account_info_request.h"

#include "app/src/assert.h"
#include "app/src/include/firebase/app.h"

namespace firebase {
namespace auth {

SetAccountInfoRequest::SetAccountInfoRequest(::firebase::App& app,
                                             const char* const api_key)
    : AuthRequest(app, request_resource_data, true, "setAccountInfo?key=",
                  api_key) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);
  application_data_->returnSecureToken = true;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUpdateEmailRequest(
    ::firebase::App& app, const char* const api_key, const char* const email,
    const char* const language_code) {
  auto request = CreateRequest(app, api_key);
  request->SetLocaleHeader(language_code);
  if (email && *email != '\0') {
    request->application_data_->email = email;
  } else {
    LogError("No email given");
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUpdatePasswordRequest(
    ::firebase::App& app, const char* const api_key, const char* const password,
    const char* const language_code) {
  auto request = CreateRequest(app, api_key);
  request->SetLocaleHeader(language_code);
  if (password && *password != '\0') {
    request->application_data_->password = password;
  } else {
    LogError("No password given");
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateLinkWithEmailAndPasswordRequest(
    ::firebase::App& app, const char* const api_key, const char* const email,
    const char* const password) {
  auto request = CreateRequest(app, api_key);
  if (email && *email != '\0') {
    request->application_data_->email = email;
  } else {
    LogError("No email given");
  }
  if (password && *password != '\0') {
    request->application_data_->password = password;
  } else {
    LogError("No password given");
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUpdateProfileRequest(
    ::firebase::App& app, const char* const api_key,
    const char* const set_display_name, const char* const set_photo_url) {
  auto request = CreateRequest(app, api_key);
  // It's fine for either set_display_name or set_photo_url to be null.
  if (set_display_name) {
    if (*set_display_name != '\0') {
      request->application_data_->displayName = set_display_name;
    } else {
      request->application_data_->deleteAttribute.push_back("DISPLAY_NAME");
    }
  }
  if (set_photo_url) {
    if (*set_photo_url != '\0') {
      request->application_data_->photoUrl = set_photo_url;
    } else {
      request->application_data_->deleteAttribute.push_back("PHOTO_URL");
    }
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUnlinkProviderRequest(::firebase::App& app,
                                                   const char* const api_key,
                                                   const char* const provider) {
  auto request = CreateRequest(app, api_key);
  if (provider && *provider != '\0') {
    request->application_data_->deleteProvider.push_back(provider);
  }
  request->UpdatePostFields();
  return request;
}

std::unique_ptr<SetAccountInfoRequest> SetAccountInfoRequest::CreateRequest(
    ::firebase::App& app, const char* const api_key) {
  return std::unique_ptr<SetAccountInfoRequest>(
      new SetAccountInfoRequest(app, api_key));  // NOLINT
}

}  // namespace auth
}  // namespace firebase
