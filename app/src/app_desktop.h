/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_SRC_APP_DESKTOP_H_
#define FIREBASE_APP_SRC_APP_DESKTOP_H_

#include <atomic>
#include <memory>
#include <string>

#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/version.h"
#include "app/src/logger.h"

namespace firebase {
// DEFINE_FIREBASE_VERSION_STRING(Firebase);

namespace internal {

class AppInternal {
 public:
  // A registry that modules can use to expose functions to each other, without
  // requiring a linkage dependency.
  // todo - make all the implementations use something like this, for internal
  // or implementation-specific code.  b/70229654
  FunctionRegistry function_registry;

  // HeartbeatController provides methods to log heartbeats and fetch payloads.
  // It shares ownership of the DateProvider and Logger it references (see
  // CreateHeartbeatController() in app_desktop.cc), so copies handed out by
  // App::GetHeartbeatController() stay valid after the App is destroyed.
  std::shared_ptr<heartbeat::HeartbeatController> heartbeat_controller_;

  // Per-app automatic data collection flag.
  std::atomic<bool> data_collection_default_enabled_{true};
};

// When Create() is invoked without arguments, it checks for a file named
// google-services-desktop.json, to load options from.  This specifies the
// path to search.
extern std::string g_default_config_path;  // NOLINT

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_DESKTOP_H_
