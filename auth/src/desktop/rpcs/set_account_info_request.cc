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
    : AuthRequest(app, request_resource_data, true) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] =
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());

  application_data_->returnSecureToken = true;
}

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUpdateEmailRequest(::firebase::App& app,
                                                const char* const api_key,
                                                const char* const email) {
  auto request = CreateRequest(app, api_key);
  if (email) {
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
  if (language_code != nullptr) {
    request->add_header(kHeaderFirebaseLocale, language_code);
  }
  if (password) {
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

std::unique_ptr<SetAccountInfoRequest>
SetAccountInfoRequest::CreateUpdateProfileRequest(
    ::firebase::App& app, const char* const api_key,
    const char* const set_display_name, const char* const set_photo_url) {
  auto request = CreateRequest(app, api_key);

  // It's fine for either set_photo_url or set_photo_url to be null.
  if (set_display_name) {
    const std::string display_name = set_display_name;
    if (!display_name.empty()) {
      request->application_data_->displayName = display_name;
    } else {
      request->application_data_->deleteAttribute.push_back("DISPLAY_NAME");
    }
  }

  if (set_photo_url) {
    const std::string photo_url = set_photo_url;
    if (!photo_url.empty()) {
      request->application_data_->photoUrl = photo_url;
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
  if (provider) {
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
