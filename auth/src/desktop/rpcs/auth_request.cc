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
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "firebase/log.h"

namespace firebase {
namespace auth {

// Key name for header when sending language code data.
const char* kHeaderFirebaseLocale = "X-Firebase-Locale";

AuthRequest::AuthRequest(::firebase::App& app, const char* schema,
                         bool deliver_heartbeat)
    : RequestJson(schema), app(app) {
  CheckEnvEmulator();

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
}

std::string AuthRequest::GetUrl() {
  std::string emulator_url;
  Auth* auth_ptr = Auth::GetAuth(&app);
  std::string assigned_emulator_url =
      static_cast<AuthImpl*>(auth_ptr->auth_data_->auth_impl)
          ->assigned_emulator_url;
  if (assigned_emulator_url.empty()) {
    emulator_url = env_emulator_url;
  } else {
    emulator_url = assigned_emulator_url;
  }

  if (emulator_url.empty()) {
    std::string url(kHttps);
    url += kServerURL;
    return url;
  } else {
    std::string url(kHttp);
    url += emulator_url;
    url += "/";
    url += kServerURL;
    return url;
  }
}

void AuthRequest::CheckEnvEmulator() {
  if (!env_emulator_url.empty()) {
    LogInfo("Environment Emulator Url already set: %s",
            env_emulator_url.c_str());
    return;
  }

  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_AUTH_EMULATOR") == nullptr) {
    LogInfo("USE_AUTH_EMULATOR not set.");
    return;
  }
  env_emulator_url.append(kEmulatorLocalHost);
  env_emulator_url.append(":");
  // Use AUTH_EMULATOR_PORT if it is set to non empty string,
  // otherwise use the default port.
  if (std::getenv("AUTH_EMULATOR_PORT") == nullptr) {
    env_emulator_url.append(kEmulatorPort);
  } else {
    env_emulator_url.append(std::getenv("AUTH_EMULATOR_PORT"));
  }
}

}  // namespace auth
}  // namespace firebase
