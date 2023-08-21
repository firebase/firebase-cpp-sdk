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
#include "firebase/log.h"

namespace firebase {
namespace auth {

// Key name for header when sending language code data.
const char* kHeaderFirebaseLocale = "X-Firebase-Locale";

AuthRequest::AuthRequest(::firebase::App& app, const char* schema,
                         bool deliver_heartbeat)
    : RequestJson(schema) {
  CheckEmulator();
}

std::string AuthRequest::GetUrl() {
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

void AuthRequest::CheckEmulator() {
  if (!emulator_url.empty()) {
    LogInfo("Emulator Url already set: %s", emulator_url.c_str());
    return;
  }
  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_AUTH_EMULATOR") == nullptr) {
    LogInfo("Using Auth Prod for testing.");
    return;
  }
  LogInfo("Using Auth Emulator.");
  emulator_url.append(kEmulatorLocalHost);
  emulator_url.append(":");
  // Use AUTH_EMULATOR_PORT if it is set to non empty string,
  // otherwise use the default port.
  if (std::getenv("AUTH_EMULATOR_PORT") == nullptr) {
    emulator_url.append(kEmulatorPort);
  } else {
    emulator_url.append(std::getenv("AUTH_EMULATOR_PORT"));
  }
}

}  // namespace auth
}  // namespace firebase
