// Copyright 2018 Google LLC
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

#include "auth/src/desktop/rpcs/auth_request.h"

#include <assert.h>

#include <memory>
#include <string>

#include "app/src/app_common.h"
#include "app/src/app_state.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/service_interfaces.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "firebase/log.h"

namespace {
const int kAppCheckTokenTimeoutMs = 10000;
}  // namespace

namespace firebase {
namespace auth {

// Key name for header when sending language code data.
const char* kHeaderFirebaseLocale = "X-Firebase-Locale";

AuthRequest::AuthRequest(const ::firebase::App& app, const char* schema,
                         bool deliver_heartbeat)
    : RequestJson(schema) {
  if (deliver_heartbeat) {
    std::shared_ptr<heartbeat::HeartbeatController> heartbeat_controller =
        app.GetHeartbeatController();
    if (heartbeat_controller) {
      std::string payload = heartbeat_controller->GetAndResetStoredHeartbeats();
      std::string gmp_app_id = app.options().app_id();
      if (!payload.empty()) {
        add_header(app_common::kApiClientHeader, payload.c_str());
        add_header(app_common::kXFirebaseGmpIdHeader, gmp_app_id.c_str());
      }
    }
  }

  if (auto state = ::firebase::internal::AppState::FromApp(app)) {
    if (auto* auth_data = state->FindService<AuthData>(
            &::firebase::internal::kAuthServiceTag, "")) {
      if (auth_data->auth_impl != nullptr) {
        assigned_emulator_url_ =
            static_cast<AuthImpl*>(auth_data->auth_impl)->assigned_emulator_url;
      }
    }

    // Add AppCheck attestation token if available.
    // This is required when AppCheck enforcement is enabled on the project.
    if (auto* app_check = state->app_check_token_provider()) {
      Future<std::string> app_check_future = app_check->GetTokenAsync();
      if (app_check_future.status() != kFutureStatusInvalid) {
        const std::string* token =
            app_check_future.Await(kAppCheckTokenTimeoutMs);
        if (token && app_check_future.error() == 0 && !token->empty()) {
          add_header("X-Firebase-AppCheck", token->c_str());
        }
      }
    }
  }
}

std::string AuthRequest::GetUrl(const char* server_url) {
  if (server_url == nullptr) {
    return "";
  }

  if (assigned_emulator_url_.empty()) {
    std::string url(kHttps);
    url += server_url;
    return url;
  } else {
    std::string url(kHttp);
    url += assigned_emulator_url_;
    url += "/";
    url += server_url;
    return url;
  }
}

}  // namespace auth
}  // namespace firebase
