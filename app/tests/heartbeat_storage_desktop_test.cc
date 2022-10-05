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

#include "app/src/heartbeat/heartbeat_storage_desktop.h"

#include <fstream>
#include <future>
#include <thread>

#include "app/logged_heartbeats_generated.h"
#include "app/src/filesystem.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

using firebase::heartbeat::HeartbeatStorageDesktop;
using firebase::heartbeat::LoggedHeartbeats;

class HeartbeatStorageDesktopTest : public ::testing::Test {
 public:
  HeartbeatStorageDesktopTest() : logger_(nullptr) {}

 protected:
  Logger logger_;
};

TEST_F(HeartbeatStorageDesktopTest, WriteAndRead) {
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop("app_id", logger_);
  std::string user_agent = "user_agent";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-23";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date2;
  heartbeats.heartbeats[user_agent].push_back(date1);
  heartbeats.heartbeats[user_agent].push_back(date2);
  bool write_ok = storage.Write(heartbeats);
  ASSERT_TRUE(write_ok);

  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, date2);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent].size(), 2);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][0], date1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][1], date2);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadDifferentStorageInstance) {
  std::string app_id = "app_id";
  HeartbeatStorageDesktop storage1 = HeartbeatStorageDesktop(app_id, logger_);
  std::string user_agent = "user_agent";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-23";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date2;
  heartbeats.heartbeats[user_agent].push_back(date1);
  heartbeats.heartbeats[user_agent].push_back(date2);
  bool write_ok = storage1.Write(heartbeats);
  ASSERT_TRUE(write_ok);

  HeartbeatStorageDesktop storage2 = HeartbeatStorageDesktop(app_id, logger_);
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage2.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, date2);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent].size(), 2);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][0], date1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][1], date2);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadDifferentAppIds) {
  std::string app_id = "app_id";
  std::string different_app_id = "different_app_id";
  std::string user_agent1 = "user_agent1";
  std::string user_agent2 = "user_agent2";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-02";
  // Write using app_id
  HeartbeatStorageDesktop storage1 = HeartbeatStorageDesktop(app_id, logger_);
  LoggedHeartbeats heartbeats1;
  heartbeats1.last_logged_date = date1;
  heartbeats1.heartbeats[user_agent1].push_back(date1);
  bool write_ok = storage1.Write(heartbeats1);
  ASSERT_TRUE(write_ok);

  // Write different heartbeats using different_app_id
  HeartbeatStorageDesktop storage2 =
      HeartbeatStorageDesktop(different_app_id, logger_);
  LoggedHeartbeats heartbeats2;
  heartbeats2.last_logged_date = date2;
  heartbeats2.heartbeats[user_agent2].push_back(date2);
  write_ok = storage2.Write(heartbeats2);
  ASSERT_TRUE(write_ok);

  // Read using app_id and verify it contains the original heartbeats.
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage1.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, date1);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent1].size(), 1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent1][0], date1);
}

TEST_F(HeartbeatStorageDesktopTest, ReadNonexistentFile) {
  // ReadHeartbeats should return a default instance
  HeartbeatStorageDesktop storage =
      HeartbeatStorageDesktop("nonexistent_app_id", logger_);
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0);
}

TEST_F(HeartbeatStorageDesktopTest, FilenameIgnoresSymbolsInAppId) {
  std::string app_id = "idstart/\\?%*:|\"<>.,;=idend";
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop(app_id, logger_);
  std::string filename = storage.GetFilename();
  // Verify that the actual filename contains the non-symbol characters in
  // app_id.
  EXPECT_TRUE(filename.find("idstartidend") != std::string::npos) << filename;
}

TEST_F(HeartbeatStorageDesktopTest, ReadCorruptedData) {
  std::string app_id = "app_id";
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop(app_id, logger_);
  // Write non-flatbuffer data to the file and then try to read from that file.
  std::ofstream file(storage.GetFilename());
  file << "this is not a flatbuffer";
  file.close();

  // ReadHeartbeats should return a default instance
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0);
}

}  // namespace
}  // namespace firebase
