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

#ifdef FIREBASE_TESTING
#include "gtest/gtest.h"
#endif  // FIREBASE_TESTING

namespace firebase {
namespace heartbeat {

class HeartbeatController {
 public:
  HeartbeatController(const std::string& app_id, const Logger& logger,
                      const DateProvider& date_provider_);
  ~HeartbeatController();

  // Asynchronously log a heartbeat, if needed
  void LogHeartbeat();

  // Synchronously fetches and clears all heartbeats from storage and returns
  // a JSON payload that has been compressed with gzip and base64 encoded.
  // If there are no new heartbeats, an empty string is returned instead.
  std::string GetAndResetStoredHeartbeats();

  // Synchronously fetches and clears today's heartbeat from storage and returns
  // a string of user agents. If there is no new heartbeat, an empty string is
  // returned instead.
  std::string GetAndResetTodaysStoredHeartbeats();

 private:
#ifdef FIREBASE_TESTING
  FRIEND_TEST(HeartbeatControllerDesktopTest, PerAppHeartbeatController);
  FRIEND_TEST(HeartbeatControllerDesktopTest, EncodeAndDecode);
  FRIEND_TEST(HeartbeatControllerDesktopTest, CreatePayloadString);
  FRIEND_TEST(HeartbeatControllerDesktopTest, GetExpectedHeartbeatPayload);
  FRIEND_TEST(HeartbeatControllerDesktopTest, GetHeartbeatsPayload);
  FRIEND_TEST(HeartbeatControllerDesktopTest,
              GetTodaysHeartbeatThenGetAllHeartbeats);
  FRIEND_TEST(HeartbeatControllerDesktopTest, GetHeartbeatPayloadMultipleTimes);
  FRIEND_TEST(HeartbeatControllerDesktopTest,
              GetHeartbeatsPayloadTimeBetweenFetches);
  FRIEND_TEST(HeartbeatControllerDesktopTest,
              GetTodaysHeartbeatPayloadMultipleTimes);
#endif  // FIREBASE_TESTING

  // Constructs an encoded string payload from a given LoggedHeartbeats object.
  std::string GetJsonPayloadForHeartbeats(const LoggedHeartbeats& heartbeats);

  // Compress a string with gzip and base 64 encode the result.
  std::string CompressAndEncode(const std::string& input);

  // Decode a base64 encoded string and decompress the result using gzip.
  // This method should only be used in tests.
  std::string DecodeAndDecompress(const std::string& input);

  HeartbeatStorageDesktop storage_;
  scheduler::Scheduler scheduler_;
  const DateProvider& date_provider_;

  std::time_t last_read_all_heartbeats_time_ = 0;
  std::time_t last_read_todays_heartbeat_time_ = 0;

  // For thread safety, the following variables should only be read or written
  // by the scheduler thread.
  std::string last_logged_date_;
  std::string last_flushed_all_heartbeats_date_;
  std::string last_flushed_todays_heartbeat_date_;
};

}  // namespace heartbeat
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_HEARTBEAT_HEARTBEAT_CONTROLLER_DESKTOP_H_
