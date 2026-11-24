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

#include <memory>

#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"

namespace firebase {
namespace internal {

class AppInternal {
 public:
  // Declared before heartbeat_controller_ so date_provider_ outlives
  // heartbeat_controller_ during destruction.
  firebase::heartbeat::DateProviderImpl date_provider_;

  // HeartbeatController provides methods to log heartbeats and fetch payloads.
  std::shared_ptr<heartbeat::HeartbeatController> heartbeat_controller_;
};

}  // namespace internal
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_APP_DESKTOP_H_
