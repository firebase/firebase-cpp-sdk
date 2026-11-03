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

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "app/src/include/firebase/internal/platform.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

using firebase::heartbeat::HeartbeatStorageDesktop;
using firebase::heartbeat::LoggedHeartbeats;

void RemoveHeartbeatFileForAppId(const std::string& app_id,
                                 const Logger& logger) {
  HeartbeatStorageDesktop storage(app_id, logger);
  auto filename = storage.GetFilename();
  if (filename.empty()) {
    return;
  }
#if FIREBASE_PLATFORM_WINDOWS
  _wremove(filename.c_str());
#else
  std::remove(filename.c_str());
#endif
}

class HeartbeatStorageDesktopTest : public ::testing::Test {
 public:
  HeartbeatStorageDesktopTest() : logger_(nullptr) {}

  void SetUp() override { CleanUpTestFiles(); }

  void TearDown() override { CleanUpTestFiles(); }

 protected:
  void CleanUpTestFiles() {
    const char* const kTestAppIds[] = {
        "storage_test_write_and_read_app_id",
        "storage_test_different_instance_app_id",
        "storage_test_different_app_ids_1",
        "storage_test_different_app_ids_2",
        "storage_test_nonexistent_app_id",
        "idstart/\\?%*:|\"<>.,;=idend",
        "storage_test_corrupted_app_id",
    };
    for (const char* app_id : kTestAppIds) {
      RemoveHeartbeatFileForAppId(app_id, logger_);
    }
  }

  Logger logger_;
};

TEST_F(HeartbeatStorageDesktopTest, WriteAndRead) {
  HeartbeatStorageDesktop storage("storage_test_write_and_read_app_id",
                                  logger_);
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
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent].size(), 2u);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][0], date1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][1], date2);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadDifferentStorageInstance) {
  std::string app_id = "storage_test_different_instance_app_id";
  HeartbeatStorageDesktop storage1(app_id, logger_);
  std::string user_agent = "user_agent";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-23";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date2;
  heartbeats.heartbeats[user_agent].push_back(date1);
  heartbeats.heartbeats[user_agent].push_back(date2);
  bool write_ok = storage1.Write(heartbeats);
  ASSERT_TRUE(write_ok);

  HeartbeatStorageDesktop storage2(app_id, logger_);
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage2.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, date2);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent].size(), 2u);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][0], date1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][1], date2);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadDifferentAppIds) {
  std::string app_id = "storage_test_different_app_ids_1";
  std::string different_app_id = "storage_test_different_app_ids_2";
  std::string user_agent1 = "user_agent1";
  std::string user_agent2 = "user_agent2";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-02";
  // Write using app_id
  HeartbeatStorageDesktop storage1(app_id, logger_);
  LoggedHeartbeats heartbeats1;
  heartbeats1.last_logged_date = date1;
  heartbeats1.heartbeats[user_agent1].push_back(date1);
  bool write_ok = storage1.Write(heartbeats1);
  ASSERT_TRUE(write_ok);

  // Write different heartbeats using different_app_id
  HeartbeatStorageDesktop storage2(different_app_id, logger_);
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
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent1].size(), 1u);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent1][0], date1);
}

TEST_F(HeartbeatStorageDesktopTest, ReadNonexistentFile) {
  // ReadHeartbeats should return a default instance
  HeartbeatStorageDesktop storage("storage_test_nonexistent_app_id", logger_);
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0u);
}

TEST_F(HeartbeatStorageDesktopTest, FilenameIgnoresSymbolsInAppId) {
  std::string app_id = "idstart/\\?%*:|\"<>.,;=idend";
  HeartbeatStorageDesktop storage(app_id, logger_);
#if FIREBASE_PLATFORM_WINDOWS
  std::wstring filename = storage.GetFilename();
  // Verify that the actual filename contains the non-symbol characters in
  // app_id.
  EXPECT_TRUE(filename.find(L"idstartidend") != std::wstring::npos) << filename;
#else
  std::string filename = storage.GetFilename();
  // Verify that the actual filename contains the non-symbol characters in
  // app_id.
  EXPECT_TRUE(filename.find("idstartidend") != std::string::npos) << filename;
#endif
}

TEST_F(HeartbeatStorageDesktopTest, ReadCorruptedData) {
  std::string app_id = "storage_test_corrupted_app_id";
  HeartbeatStorageDesktop storage(app_id, logger_);
  // Write non-flatbuffer data to the file and then try to read from that file.
  std::ofstream file(storage.GetFilename(),
                     std::ios_base::trunc | std::ios_base::binary);
  ASSERT_TRUE(file.is_open());
  file << "this is not a flatbuffer";
  file.flush();
  ASSERT_TRUE(file.good());
  file.close();

  // ReadHeartbeats should return a default instance
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(read_heartbeats);
  ASSERT_TRUE(read_ok);
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0u);
}

}  // namespace
}  // namespace firebase
