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

#include "auth/src/desktop/rpcs/create_auth_uri_request.h"

#include "app/src/assert.h"
#include "app/src/log.h"

namespace firebase {
namespace auth {

CreateAuthUriRequest::CreateAuthUriRequest(const char* api_key,
                                           const char* identifier)
    : AuthRequest(request_resource_data) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] =
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "createAuthUri?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());

  if (identifier) {
    application_data_->identifier = identifier;
  } else {
    LogError("No identifier given.");
  }

  // This parameter is only relevant for the web SDK; for desktop, it can have
  // any value as long as it passes the backend validation for a valid URL.
  application_data_->continueUri = "http://localhost";
  UpdatePostFields();
}

}  // namespace auth
}  // namespace firebase
