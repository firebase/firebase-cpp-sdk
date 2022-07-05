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

#ifndef FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_CONTROLLER_DESKTOP_H_
#define FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_CONTROLLER_DESKTOP_H_

#include <string>

#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "app/src/scheduler.h"

namespace firebase {
namespace heartbeat {

class HeartbeatController {
 public:
  HeartbeatController(const std::string& app_id, const Logger& logger,
                      const DateProvider& date_provider_);
  ~HeartbeatController();

  // Asynchronously log a heartbeat, if needed
  void LogHeartbeat();

  // Synchronously fetches and clears all heartbeats from storage
  std::string GetAndResetStoredHeartbeats();

  // Synchronously fetches and clears today's heartbeat from storage
  std::string GetAndResetTodaysStoredHeartbeats();

  // TODO: figure out where zipping belongs (internal to payload or separate
  // step) Maybe unit test it as well For now compress and decompress are public
  // to make them testable
  // TODO: make test-only visible or refactor to a separate class
  std::string CompressAndEncode(const std::string& input);

  std::string DecodeAndDecompress(const std::string& input);

 private:
  HeartbeatStorageDesktop storage_;
  scheduler::Scheduler scheduler_;
  const DateProvider& date_provider_;

  std::string GetStringPayloadForHeartbeats(LoggedHeartbeats heartbeats);

  // For thread safety, the following variables should only be read or written
  // by the scheduler thread.
  std::string last_logged_date_;
  std::string last_fetched_all_heartbeats_date_;
  std::string last_fetched_todays_heartbeat_date_;
};

}  // namespace heartbeat
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_CONTROLLER_DESKTOP_H_
