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

#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <string>
#include <sstream>
#include <iomanip>

namespace firebase {
namespace {

using firebase::heartbeat::HeartbeatController;
using firebase::heartbeat::HeartbeatStorageDesktop;
using firebase::heartbeat::LoggedHeartbeats;

class MockDateProvider : public heartbeat::DateProvider {
 public:
  MOCK_METHOD(std::string, GetDate, (), (const override));
};

class HeartbeatControllerDesktopTest : public ::testing::Test {
 public:
  HeartbeatControllerDesktopTest() :
      mock_date_provider_(),
      logger_(nullptr),
      app_id_("app_id"),
      storage_("app_id", logger_),
      controller_("app_id", logger_, mock_date_provider_) {
    // For the sake of testing, clear any pre-existing stored heartbeats.
    LoggedHeartbeats empty_heartbeats_struct;
    storage_.Write(empty_heartbeats_struct);
  }

 protected:
  MockDateProvider mock_date_provider_;
  Logger logger_;
  std::string app_id_;
  HeartbeatStorageDesktop storage_;
  HeartbeatController controller_;
};

TEST_F(HeartbeatControllerDesktopTest, WriteAndRead) {
  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(1).WillOnce(testing::Return(today));

  // TODO: for the sake of testing, clear any pre-existing stored heartbeats.
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
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

TEST_F(HeartbeatControllerDesktopTest, LogSameDateTwiceOneEntry) {
  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(2).WillRepeatedly(testing::Return(today));

  // TODO: for the sake of testing, clear any pre-existing stored heartbeats.
  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
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

TEST_F(HeartbeatControllerDesktopTest, LogTwoDatesTwoEntries) {
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(2)
    .WillOnce(testing::Return(day1)).WillOnce(testing::Return(day2));

  // TODO: for the sake of testing, clear any pre-existing stored heartbeats.
  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  
  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  ASSERT_EQ(read_heartbeats.last_logged_date, day2);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for(auto const& entry: read_heartbeats.heartbeats) {
    std::string user_agent = entry.first;
    // TODO: Verify that the user agent contains some known substring
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 2);
    EXPECT_EQ(dates[0], day1);
    EXPECT_EQ(dates[1], day2);
  }
}

// TODO: add a unit test for the actual implementation of date provider
// Basically just verify length of the string and that certain characters are hyphens

}  // namespace
}  // namespace firebase
