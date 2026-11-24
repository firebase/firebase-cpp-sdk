// Copyright 2022 Google LLC
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

#ifndef FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_TOKEN_REQUEST_H_
#define FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_TOKEN_REQUEST_H_

#include <string>
#include <string_view>
#include <utility>

#include "app/rest/request_json.h"
#include "app_check/debug_token_request_generated.h"
#include "app_check/debug_token_request_resource.h"
#include "firebase/app.h"

namespace firebase {
namespace app_check {
namespace internal {

// The server url to exchange the debug token with for an attestation token.
inline constexpr const char* kDebugTokenRequestServerUrlBase =
    "https://firebaseappcheck.googleapis.com/v1/projects/";
// The header used to pass the project's API key.
inline constexpr const char* kDebugTokenRequestHeader = "X-Goog-Api-Key";

// Request to exchange the user provided Debug Token with a valid attestation
// token.
class DebugTokenRequest
    : public firebase::rest::RequestJson<fbs::DebugTokenRequest,
                                         fbs::DebugTokenRequestT> {
 public:
  DebugTokenRequest(std::string_view project_id, std::string_view app_id,
                    std::string_view api_key)
      : RequestJson(debug_token_request_resource_data) {
    std::string server_url(kDebugTokenRequestServerUrlBase);
    server_url.append(project_id);
    server_url.append("/apps/");
    server_url.append(app_id);
    server_url.append(":exchangeDebugToken");
    set_url(server_url.c_str());

    add_header(kDebugTokenRequestHeader, std::string(api_key).c_str());
  }

  explicit DebugTokenRequest(const App& app)
      : DebugTokenRequest(app.options().project_id(), app.options().app_id(),
                          app.options().api_key()) {}

  void SetDebugToken(std::string debug_token) {
    application_data_->debug_token = std::move(debug_token);
    UpdatePostFields();
  }

  void SetLimitedUse(bool limited_use) {
    application_data_->limited_use = limited_use;
    UpdatePostFields();
  }
};

}  // namespace internal
}  // namespace app_check
}  // namespace firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_TOKEN_REQUEST_H_
