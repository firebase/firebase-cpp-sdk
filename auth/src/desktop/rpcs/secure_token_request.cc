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

#include "auth/src/desktop/rpcs/secure_token_request.h"

#include "app/src/assert.h"
#include "app/src/log.h"

namespace firebase {
namespace auth {

SecureTokenRequest::SecureTokenRequest(const char* api_key,
                                       const char* refresh_token)
    : AuthRequest(request_resource_data) {
  FIREBASE_ASSERT_RETURN_VOID(api_key);

  const char api_host[] = "https://securetoken.googleapis.com/v1/token?key=";
  std::string url;
  url.reserve(strlen(api_host) + strlen(api_key));
  url.append(api_host);
  url.append(api_key);
  set_url(url.c_str());

  application_data_->grantType = "refresh_token";
  if (refresh_token) {
    application_data_->refreshToken = refresh_token;
  } else {
    LogError("No refresh token given");
  }

  UpdatePostFields();
}

}  // namespace auth
}  // namespace firebase
