/*
 * Copyright 2020 Google LLC
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

#include "app/src/heartbeat_info_desktop.h"

#include "app/memory/shared_ptr.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/include/firebase/app.h"

namespace firebase {

HeartbeatInfo::Code HeartbeatInfo::GetHeartbeatCode(
    SharedPtr<heartbeat::HeartbeatController> heartbeat_controller) {
  std::string payload =
      heartbeat_controller->GetAndResetTodaysStoredHeartbeats();
  if (!payload.empty()) {
    return Code::Combined;
  }
  return Code::None;
}

}  // namespace firebase
