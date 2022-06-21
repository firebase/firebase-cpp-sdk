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

#include <string>

#include "app/src/app_common.h"
#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

using firebase::heartbeat::HeartbeatController;
using firebase::heartbeat::HeartbeatStorageDesktop;
using firebase::heartbeat::LoggedHeartbeats;
using ::testing::MatchesRegex;
using ::testing::Return;

const char kAppId[] = "app_id";
const char kDefaultUserAgent[] = "agent/1";
const char kCustomUserAgent1[] = "agent/2";
const char kCustomUserAgent2[] = "agent/3";

class MockDateProvider : public heartbeat::DateProvider {
 public:
  MOCK_METHOD(std::string, GetDate, (), (const override));
};

class HeartbeatControllerDesktopTest : public ::testing::Test {
 public:
  HeartbeatControllerDesktopTest() :
      mock_date_provider_(),
      logger_(nullptr),
      storage_(kAppId, logger_),
      controller_(kAppId, logger_, mock_date_provider_) {
    // For the sake of testing, clear any pre-existing stored heartbeats.
    LoggedHeartbeats empty_heartbeats_struct;
    storage_.Write(empty_heartbeats_struct);
    // Default to registering a user agent with version 1
    app_common::RegisterLibrariesFromUserAgent(kDefaultUserAgent);
  }

 protected:
  MockDateProvider mock_date_provider_;
  Logger logger_;
  HeartbeatStorageDesktop storage_;
  HeartbeatController controller_;
};

TEST_F(HeartbeatControllerDesktopTest, DateProvider) {
  firebase::heartbeat::DateProviderImpl actualDateProvider;
  std::string date = actualDateProvider.GetDate();
  // Verify that the date is in the form YYYY-MM-DD
  EXPECT_THAT(date, MatchesRegex("^[0-9]{4}-[0-9]{2}-[0-9]{2}$"));
}

TEST_F(HeartbeatControllerDesktopTest, LogSingleHeartbeat) {
  // Register libraries so that app's user agent is not empty.
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);

  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(1).WillOnce(Return(today));

  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, today);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::string user_agent = entry.first;
    EXPECT_EQ(user_agent, kCustomUserAgent1);
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], today);
  }
}

TEST_F(HeartbeatControllerDesktopTest, LogSameDateTwiceOneEntry) {
  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(2).WillRepeatedly(Return(today));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();

  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, today);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 1);
    EXPECT_EQ(dates[0], today);
  }
}

TEST_F(HeartbeatControllerDesktopTest, LogTwoDatesTwoEntries) {
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(2)
    .WillOnce(Return(day1)).WillOnce(Return(day2));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, day2);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 2);
    EXPECT_EQ(dates[0], day1);
    EXPECT_EQ(dates[1], day2);
  }
}

TEST_F(HeartbeatControllerDesktopTest, LogTwoUserAgentsOnDifferentDays) {
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  std::string day3 = "2000-01-25";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(3)
    .WillOnce(Return(day1)).WillOnce(Return(day2)).WillOnce(Return(day3));

  // Log a heartbeat for UserAgent1 on day1
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Log a heartbeat for UserAgent2 on day2
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent2);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Log a heartbeat for UserAgent1 on day3
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, day3);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 2);
  // UserAgent1 was logged on day1 and day3
  ASSERT_EQ(read_heartbeats.heartbeats[kCustomUserAgent1].size(), 2);
  EXPECT_EQ(read_heartbeats.heartbeats[kCustomUserAgent1][0], day1);
  EXPECT_EQ(read_heartbeats.heartbeats[kCustomUserAgent1][1], day3);
  // UserAgent2 was logged on day2
  ASSERT_EQ(read_heartbeats.heartbeats[kCustomUserAgent2].size(), 1);
  EXPECT_EQ(read_heartbeats.heartbeats[kCustomUserAgent2][0], day2);
}

TEST_F(HeartbeatControllerDesktopTest, LogMoreThan30DaysRemovesOldEntries) {
  {
    // InSequence guarantees that all of the expected calls occur in order.
    testing::InSequence seq;
    for (int month = 1; month <= 3; month++) {
      for (int day = 1; day <= 30; day++) {
        // YYYY-MM-DD\0
        char date[11];
        snprintf(date, 11, "2000-%02d-%02d", month, day);
        std::string date_string(date);
        EXPECT_CALL(mock_date_provider_, GetDate()).WillOnce(Return(date_string));
      }
    }
  }
  for (int i = 1; i <= 90; i++) {
    controller_.LogHeartbeat();
  }
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "2000-03-30");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  // Even though heartbeat logging is asynchronous, it happens in the order that it is scheduled
  // So the heartbeats should be logged in the correct order.
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 30);
    EXPECT_EQ(dates[0], "2000-03-01");
    EXPECT_EQ(dates[29], "2000-03-30");
  }
}

}  // namespace
}  // namespace firebase
