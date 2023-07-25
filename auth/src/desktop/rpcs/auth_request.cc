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
  // The user agent strings are cached in static variables here to avoid
  // dependencies upon other parts of this library.  This complication is due to
  // the way the tests are currently configured where each library has minimal
  // dependencies.

  CheckEmulator();
  static std::string auth_user_agent;           // NOLINT
  static std::string extended_auth_user_agent;  // NOLINT
  static Mutex* user_agent_mutex = new Mutex();
  MutexLock lock(*user_agent_mutex);
  if (auth_user_agent.empty()) {
    std::string sdk;
    std::string version;
    app_common::GetOuterMostSdkAndVersion(&sdk, &version);
    // Set the user agent similar to the iOS SDK.  Format:
    // FirebaseAuth.<platform>/<sdk_version>
    assert(!(sdk.empty() || version.empty()));
    std::string sdk_type(sdk.substr(sizeof(FIREBASE_USER_AGENT_PREFIX) - 1));
    auth_user_agent = std::string("FirebaseAuth.") + sdk_type + "/" + version;
    // Generage the extended header to set the format specified by b/28531026
    // and b/64693042 to include the platform and framework.
    // <environment>/<sdk_implementation>/<sdk_version>/<framework>
    // where <framework> is '(FirebaseCore|FirebaseUI)'.
    extended_auth_user_agent = std::string(app_common::kOperatingSystem) + "/" +
                               sdk + "/" + version + "/" + "FirebaseCore-" +
                               sdk_type;
  }
  // TODO(b/244643516): Remove the User-Agent and X-Client-Version headers.
  if (!auth_user_agent.empty()) {
    add_header("User-Agent", auth_user_agent.c_str());
    add_header("X-Client-Version", extended_auth_user_agent.c_str());
  }
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
  if (emulator_url.empty()) {
    std::string url(kHttps);
    url += kServerURL;
    LogDebug("AuthRequest::GetUrl(Prod): %s", url.c_str());
    return url;
  } else {
    std::string url(kHttp);
    url += emulator_url;
    url += kServerURL;
    LogDebug("AuthRequest::GetUrl(Emulator): %s", url.c_str());
    return url;
  }
}

void AuthRequest::CheckEmulator() {
  if (!emulator_url.empty()) {
    LogDebug("Emulator Url already set: %s", emulator_url.c_str());
    return;
  }
  // Use emulator as long as this env variable is set, regardless its value.
  if (std::getenv("USE_AUTH_EMULATOR") == nullptr) {
    LogDebug("Using Firestore Prod for testing.");
    return;
  }

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
