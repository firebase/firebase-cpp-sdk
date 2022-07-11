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

#include <string>

#include "app/src/app_common.h"
#include "app/src/heartbeat/date_provider.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/logger.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/json_util.h"

namespace firebase {
namespace {

using ::firebase::heartbeat::HeartbeatController;
using ::firebase::heartbeat::HeartbeatStorageDesktop;
using ::firebase::heartbeat::LoggedHeartbeats;
using ::firebase::testing::cppsdk::EqualsJson;
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
  HeartbeatControllerDesktopTest()
      : mock_date_provider_(),
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
  EXPECT_THAT(date, MatchesRegex("^....-..-..$"));
}

TEST_F(HeartbeatControllerDesktopTest, LogSingleHeartbeat) {
  // Register libraries so that app's user agent is not empty.
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);

  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(1).WillOnce(Return(today));

  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that
  // the log succeeded.
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
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(2)
      .WillRepeatedly(Return(today));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();

  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that
  // the log succeeded.
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
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(2)
      .WillOnce(Return(day1))
      .WillOnce(Return(day2));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that
  // the log succeeded.
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

TEST_F(HeartbeatControllerDesktopTest, LogOlderDatesOneEntry) {
  std::string day1 = "2000-01-24";
  std::string day2 = "2000-01-22";
  std::string day3 = "1987-11-29";
  std::string day4 = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(4)
      .WillOnce(Return(day1))
      .WillOnce(Return(day2))
      .WillOnce(Return(day3))
      .WillOnce(Return(day4));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that
  // the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, day1);
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 1);
    // All dates after the first are earlier and should not be logged.
    EXPECT_EQ(dates[0], day1);
  }
}

TEST_F(HeartbeatControllerDesktopTest, LogTwoUserAgentsOnDifferentDays) {
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  std::string day3 = "2000-01-25";
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(3)
      .WillOnce(Return(day1))
      .WillOnce(Return(day2))
      .WillOnce(Return(day3));

  // Log a heartbeat for UserAgent1 on day1
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Log a heartbeat for UserAgent2 on day2
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent2);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  // Log a heartbeat for UserAgent1 on day3
  app_common::RegisterLibrariesFromUserAgent(kCustomUserAgent1);
  controller_.LogHeartbeat();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

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
    ::testing::InSequence seq;
    for (int month = 1; month <= 3; month++) {
      for (int day = 1; day <= 30; day++) {
        // YYYY-MM-DD\0
        char date[11];
        snprintf(date, sizeof(date), "2000-%02d-%02d", month, day);
        std::string date_string(date);
        EXPECT_CALL(mock_date_provider_, GetDate())
            .WillOnce(Return(date_string));
      }
    }
  }
  for (int i = 1; i <= 90; i++) {
    controller_.LogHeartbeat();
  }
  // Since LogHeartbeat is done asynchronously, wait a bit before verifying that
  // the log succeeded.
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "2000-03-30");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  // Even though heartbeat logging is asynchronous, it happens in the order that
  // it is scheduled So the heartbeats should be logged in the correct order.
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    ASSERT_EQ(dates.size(), 30);
    EXPECT_EQ(dates[0], "2000-03-01");
    EXPECT_EQ(dates[29], "2000-03-30");
  }
}

TEST_F(HeartbeatControllerDesktopTest, DestroyControllerWhileWorkIsScheduled) {
  std::string today = "2000-01-23";
  EXPECT_CALL(mock_date_provider_, GetDate()).WillRepeatedly(Return(today));
  for (int i = 1; i <= 1000; i++) {
    // For the sake of testing, clear any pre-existing stored heartbeats.
    LoggedHeartbeats empty_heartbeats_struct;
    storage_.Write(empty_heartbeats_struct);

    HeartbeatController* destructible_controller =
        new HeartbeatController(kAppId, logger_, mock_date_provider_);
    // InSequence guarantees that all of the expected calls occur in order.
    destructible_controller->LogHeartbeat();

    // Trigger the controller's destructor before async work has completed.
    // The destructor will join with the worker thread.
    delete destructible_controller;
  }
}

// This test is temporarily disabled because a lack of file locking can result
// in changes to the file between read and write operations.
// TODO(b/237003018): Support multiple controllers for the same app id.
TEST_F(HeartbeatControllerDesktopTest,
       DISABLED_MultipleControllersForSameAppId) {
  MockDateProvider mock_date_provider1;
  MockDateProvider mock_date_provider2;
  HeartbeatController* controller1 =
      new HeartbeatController(kAppId, logger_, mock_date_provider1);
  HeartbeatController* controller2 =
      new HeartbeatController(kAppId, logger_, mock_date_provider2);
  // InSequence guarantees that all of the expected calls occur in order.
  // Both mock date provider's will return the same sequence of dates.
  {
    ::testing::InSequence seq;
    for (int year = 2001; year <= 2100; year++) {
      std::string date_string = std::to_string(year) + "-01-01";
      EXPECT_CALL(mock_date_provider1, GetDate()).WillOnce(Return(date_string));
    }
  }
  {
    ::testing::InSequence seq;
    for (int year = 2001; year <= 2100; year++) {
      std::string date_string = std::to_string(year) + "-01-01";
      EXPECT_CALL(mock_date_provider2, GetDate()).WillOnce(Return(date_string));
    }
  }
  for (int i = 1; i <= 100; i++) {
    controller1->LogHeartbeat();
    controller2->LogHeartbeat();
  }

  // Wait a bit for all heartbeats to be logged.
  std::this_thread::sleep_for(std::chrono::milliseconds(2000));

  // Read from the storage class to verify
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "2100-01-01");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
  // Even though heartbeat logging is asynchronous, it happens in the order that
  // it is scheduled So the heartbeats should be logged in the correct order.
  for (auto const& entry : read_heartbeats.heartbeats) {
    std::vector<std::string> dates = entry.second;
    // Only the last 30 entries will remain in storage.
    ASSERT_EQ(dates.size(), 30);
    EXPECT_EQ(dates[0], "2071-01-01");
    EXPECT_EQ(dates[29], "2100-01-01");
  }
}

TEST_F(HeartbeatControllerDesktopTest, EncodeAndDecode) {
  std::string original_str = R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-23"]
        }
      ],
      "version":"2"
    })json";
  std::string encoded = controller_.CompressAndEncode(original_str);
  std::string decoded = controller_.DecodeAndDecompress(encoded);
  EXPECT_NE(encoded, original_str);
  EXPECT_EQ(decoded, original_str);
}

TEST_F(HeartbeatControllerDesktopTest, GetEmptyHeartbeatPayload) {
  std::string today = "2000-01-23";
  // Date provider will be called for both Log and Get
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(1).WillOnce(Return(today));

  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());

  EXPECT_EQ(payload, "");
}

TEST_F(HeartbeatControllerDesktopTest, GetTodaysHeartbeatEmptyPayload) {
  std::string today = "2000-01-23";
  // Date provider will be called for both Log and Get
  EXPECT_CALL(mock_date_provider_, GetDate()).Times(1).WillOnce(Return(today));

  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetTodaysStoredHeartbeats());

  EXPECT_EQ(payload, "");
}

TEST_F(HeartbeatControllerDesktopTest, GetHeartbeatsPayload) {
  app_common::RegisterLibrariesFromUserAgent(kDefaultUserAgent);
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  // Date provider will be called twice for Log and then once for Get
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(3)
      .WillOnce(Return(day1))
      .WillRepeatedly(Return(day2));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());

  EXPECT_THAT(payload, EqualsJson(R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-23", "2000-01-24"]
        }
      ],
      "version":"2"
    })json"));

  // Storage should still have last_logged_date, but the heartbeats should no
  // longer be stored.
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage_.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, day2);
  EXPECT_EQ(read_heartbeats.heartbeats.size(), 0);
}

TEST_F(HeartbeatControllerDesktopTest, GetTodaysHeartbeatThenGetAllHeartbeats) {
  app_common::RegisterLibrariesFromUserAgent(kDefaultUserAgent);
  std::string day1 = "2000-01-23";
  std::string day2 = "2000-01-24";
  // Date provider will be called twice for, Log then twice for Get
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(4)
      .WillOnce(Return(day1))
      .WillRepeatedly(Return(day2));

  controller_.LogHeartbeat();
  controller_.LogHeartbeat();
  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetTodaysStoredHeartbeats());

  EXPECT_THAT(payload, EqualsJson(R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-24"]
        }
      ],
      "version":"2"
    })json"));

  // The heartbeat for the previous day should still be stored (01-23).
  std::string payload2 = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());

  EXPECT_THAT(payload2, EqualsJson(R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-23"]
        }
      ],
      "version":"2"
    })json"));
}

TEST_F(HeartbeatControllerDesktopTest, GetHeartbeatPayloadMultipleTimes) {
  app_common::RegisterLibrariesFromUserAgent(kDefaultUserAgent);
  std::string today = "2000-01-23";
  // Date provider will be called for both Log and Get
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(3)
      .WillRepeatedly(Return(today));

  controller_.LogHeartbeat();
  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string first_payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());
  EXPECT_THAT(first_payload, EqualsJson(R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-23"]
        }
      ],
      "version":"2"
    })json"));

  std::string second_payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());
  EXPECT_EQ(second_payload, "");
}

TEST_F(HeartbeatControllerDesktopTest, GetTodaysHeartbeatPayloadMultipleTimes) {
  app_common::RegisterLibrariesFromUserAgent(kDefaultUserAgent);
  std::string today = "2000-01-23";
  // Date provider will be called for both Log and Get
  EXPECT_CALL(mock_date_provider_, GetDate())
      .Times(3)
      .WillRepeatedly(Return(today));

  controller_.LogHeartbeat();
  // GetAndResetStoredHeartbeats is done synchronously, so there is no need to
  // wait.
  std::string first_payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetTodaysStoredHeartbeats());
  EXPECT_THAT(first_payload, EqualsJson(R"json({
      "heartbeats": [
        {
          agent: "agent/1",
          dates: ["2000-01-23"]
        }
      ],
      "version":"2"
    })json"));

  std::string second_payload = controller_.DecodeAndDecompress(
      controller_.GetAndResetStoredHeartbeats());
  EXPECT_EQ(second_payload, "");
}

}  // namespace
}  // namespace firebase
