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

#include "app/rest/request_json.h"

#include "firebase/app.h"
#include "app_check/debug_token_request_generated.h"
#include "app_check/debug_token_request_resource.h"

namespace firebase {
namespace app_check {
namespace internal {

// Request to exchange the user provided Debug Token with a valid attestation token.
class DebugTokenRequest
    : public firebase::rest::RequestJson<fbs::DebugTokenRequest, fbs::DebugTokenRequestT> {
 public:
  explicit DebugTokenRequest(App* app)
      : RequestJson(debug_token_request_resource_data) {
    std::string server_url("https://firebaseappcheck.googleapis.com/v1beta/projects/");
    server_url.append(app->options().project_id());
    server_url.append("/apps/");
    server_url.append(app->options().app_id());
    server_url.append(":exchangeDebugToken");
    set_url(server_url.c_str());

    add_header("X-Goog-Api-Key", app->options().api_key());
  }

  void SetDebugToken(std::string debug_token) {
    application_data_->debug_token = std::move(debug_token);
    UpdatePostFields();
  }
};

}  // internal
}  // app_check
}  // firebase

#endif  // FIREBASE_APP_CHECK_SRC_DESKTOP_DEBUG_TOKEN_REQUEST_H_
