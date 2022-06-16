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

#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "gtest/gtest.h"

#include <string>
#include <sstream>
#include <iomanip>

namespace firebase {
namespace {

using firebase::heartbeat::HeartbeatController;
using firebase::heartbeat::HeartbeatStorageDesktop;
using firebase::heartbeat::LoggedHeartbeats;


class HeartbeatControllerDesktopTest : public ::testing::Test {
 public:
  HeartbeatControllerDesktopTest() : logger_(nullptr), app_id_("app_id"), storage_("app_id", logger_) {
    // For the sake of testing, clear any pre-existing stored heartbeats.
    LoggedHeartbeats empty_heartbeats_struct;
    storage_.Write(empty_heartbeats_struct);
  }

 protected:
  HeartbeatStorageDesktop storage_;
  std::string app_id_;
  Logger logger_;

  // TODO: Provide a way to mock out the current time/date provider for testing.
  std::string GetCurrentDate() {
    std::time_t t = std::time(nullptr);
    std::tm* tm = std::localtime(&t);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d");
    return ss.str();
  }
};

TEST_F(HeartbeatControllerDesktopTest, WriteAndRead) {
  HeartbeatController controller(app_id_, logger_);
  // TODO: for the sake of testing, clear any pre-existing stored heartbeats.
  controller.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  std::string today = GetCurrentDate();
  ASSERT_EQ(read_heartbeats.last_logged_date, today);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for(auto const& entry: read_heartbeats.heartbeats) {
    std::string user_agent = entry.first;
    // TODO: Verify that the user agent contains some known substring
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], today);
  }
}


}  // namespace
}  // namespace firebase
