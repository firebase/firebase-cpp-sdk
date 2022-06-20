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

#include "app/src/app_common.h"
#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/logger.h"

#include <string>
#include <sstream>
#include <iomanip>

namespace firebase {
namespace heartbeat {

HeartbeatController::HeartbeatController(
  const std::string& app_id, const Logger& logger, const DateProvider& date_provider)
  : storage_(app_id, logger), scheduler_(), last_logged_date_(""), date_provider_(date_provider)
{}

HeartbeatController::~HeartbeatController() {}

void HeartbeatController::LogHeartbeat() {
  std::string user_agent = App::GetUserAgent();

  std::function<void(void)> log_heartbeat_funct = [&, user_agent]() {
    std::string current_date = date_provider_.GetDate();
    // Return early if a heartbeat has already been logged today.
    if (this->last_logged_date_ == current_date) {
      return;
    }
    LoggedHeartbeats logged_heartbeats;
    bool read_succeeded = this->storage_.ReadTo(logged_heartbeats);
    // TODO: handle read failure
    // If last logged timestamp is not todays date, add a heartbeat
    if (logged_heartbeats.last_logged_date != current_date) {
      logged_heartbeats.last_logged_date = current_date;
      logged_heartbeats.heartbeats[user_agent].push_back(current_date);
      // TODO: remove any old heartbeats (30 days old)
    }
    bool write_succeeded = this->storage_.Write(logged_heartbeats);
    // TODO: handle write failure
    if (write_succeeded) {
      this->last_logged_date_ = current_date;
    }
  };

  scheduler_.Schedule(log_heartbeat_funct);
}

}  // namespace heartbeat
}  // namespace firebase
