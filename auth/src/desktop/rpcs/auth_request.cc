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

#include "app/src/app_common.h"
#include "app/src/include/firebase/app.h"
#include "app/src/mutex.h"

namespace firebase {
namespace auth {

AuthRequest::AuthRequest(const char* schema) : RequestJson(schema) {
  // The user agent strings are cached in static variables here to avoid
  // dependencies upon other parts of this library.  This complication is due to
  // the way the tests are currently configured where each library has minimal
  // dependencies.
  static std::string auth_user_agent;           // NOLINT
  static std::string extended_auth_user_agent;  // NOLINT
  static Mutex user_agent_mutex;                // NOLINT
  MutexLock lock(user_agent_mutex);
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
  if (!auth_user_agent.empty()) {
    add_header("User-Agent", auth_user_agent.c_str());
    add_header("X-Client-Version", extended_auth_user_agent.c_str());
  }
  add_header(app_common::kApiClientHeader, App::GetUserAgent());
}

}  // namespace auth
}  // namespace firebase
