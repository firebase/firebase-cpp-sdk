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

#include <fstream>
#include <future>
#include <thread>

#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/logged_heartbeats_generated.h"
#include "app/src/filesystem.h"

#include "gtest/gtest.h"

namespace firebase {
namespace {

using firebase::heartbeat::LoggedHeartbeats;
using firebase::heartbeat::HeartbeatStorageDesktop;

class HeartbeatStorageDesktopTest : public ::testing::Test {
};

TEST_F(HeartbeatStorageDesktopTest, WriteAndRead) {
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop("app_id");
  std::string user_agent = "user_agent";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-23";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date2;
  heartbeats.heartbeats[user_agent].push_back(date1);
  heartbeats.heartbeats[user_agent].push_back(date2);
  bool write_ok = storage.Write(heartbeats);
  ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage.GetError();

  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage.GetError();
  EXPECT_EQ(read_heartbeats.last_logged_date, date2);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent].size(), 2);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][0], date1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent][1], date2);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadDifferentStorageInstance) {
  std::string app_id = "app_id";
  HeartbeatStorageDesktop storage1 = HeartbeatStorageDesktop(app_id);
  std::string user_agent = "user_agent";
  std::string date1 = "2022-01-01";
  std::string date2 = "2022-02-23";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date2;
  heartbeats.heartbeats[user_agent].push_back(date1);
  heartbeats.heartbeats[user_agent].push_back(date2);
  bool write_ok = storage1.Write(heartbeats);
  ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage1.GetError();

  HeartbeatStorageDesktop storage2 = HeartbeatStorageDesktop(app_id);
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage2.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage2.GetError();
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
  HeartbeatStorageDesktop storage1 = HeartbeatStorageDesktop(app_id);
  LoggedHeartbeats heartbeats1;
  heartbeats1.last_logged_date = date1;
  heartbeats1.heartbeats[user_agent1].push_back(date1);
  bool write_ok = storage1.Write(heartbeats1);
  ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage1.GetError();

  // Write different heartbeats using different_app_id
  HeartbeatStorageDesktop storage2 = HeartbeatStorageDesktop(different_app_id);
  LoggedHeartbeats heartbeats2;
  heartbeats2.last_logged_date = date2;
  heartbeats2.heartbeats[user_agent2].push_back(date2);
  write_ok = storage2.Write(heartbeats2);
  ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage2.GetError();

  // Read using app_id and verify it contains the original heartbeats.
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage1.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage1.GetError();
  EXPECT_EQ(read_heartbeats.last_logged_date, date1);
  ASSERT_EQ(read_heartbeats.heartbeats[user_agent1].size(), 1);
  EXPECT_EQ(read_heartbeats.heartbeats[user_agent1][0], date1);
}

TEST_F(HeartbeatStorageDesktopTest, ReadNonexistentFile) {
  // ReadHeartbeats should return a default instance
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop("nonexistent_app_id");
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage.GetError();
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0);
}

TEST_F(HeartbeatStorageDesktopTest, WriteAndReadWithSymbolsInAppId) {
  std::string app_id = "start/\\?%*:|\"<>.,;=end";
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop(app_id);
  std::string date1 = "2022-01-01";
  LoggedHeartbeats heartbeats;
  heartbeats.last_logged_date = date1;
  bool write_ok = storage.Write(heartbeats);
  ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage.GetError();
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage.GetError();
  EXPECT_EQ(read_heartbeats.last_logged_date, date1);
}

TEST_F(HeartbeatStorageDesktopTest, ReadCorruptedData) {
  // TODO(almostmatt): Make filename visible in tests rather than relying on the test knowing the filename
  std::string app_id = "app_id";
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop(app_id);
  std::string error;
  std::string app_dir =
      AppDataDir("firebase-heartbeat", /*should_create=*/true, &error);
  std::string filename = app_dir + "/heartbeats-"  + app_id;

  std::ofstream file(filename);
  file << "this is not a flatbuffer";
  file.close();

  // ReadHeartbeats should return a default instance
  LoggedHeartbeats read_heartbeats;
  bool read_ok = storage.ReadTo(&read_heartbeats);
  ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage.GetError();
  EXPECT_EQ(read_heartbeats.last_logged_date, "");
  ASSERT_EQ(read_heartbeats.heartbeats.size(), 0);
}

TEST_F(HeartbeatStorageDesktopTest, ConcurrentWritesAndReads) {
  // Perform many write and read requests in parallel.
  // Verify that Read never returns corrupt data.
  HeartbeatStorageDesktop storage = HeartbeatStorageDesktop("app_id");
  std::promise<void> signal_promise;
  std::future<void> signal = signal_promise.get_future();

  std::vector<std::thread> threads;
  for (int i = 0; i < 10; ++i) {
    threads.emplace_back([&] {
      signal.wait();

      for (int i = 0; i != 100; ++i) {
        std::string user_agent = "user_agent" + std::to_string(i);
        std::string date1 = "2022-01-01";
        LoggedHeartbeats heartbeats;
        heartbeats.last_logged_date = date1;
        heartbeats.heartbeats[user_agent].push_back(date1);
        bool write_ok = storage.Write(heartbeats);
        ASSERT_TRUE(write_ok) << "Unable to write the heartbeat file:\n  " + storage.GetError();

        // Verify that read succeeds and that data is not corrupted.
        LoggedHeartbeats read_heartbeats;
        bool read_ok = storage.ReadTo(&read_heartbeats);
        ASSERT_TRUE(read_ok) << "Unable to read the heartbeat file:\n  " + storage.GetError();
        EXPECT_EQ(read_heartbeats.last_logged_date, date1);
        ASSERT_EQ(read_heartbeats.heartbeats.size(), 1);
      }
    });
  }

  // Signal to each of the threads that it is ok to start.
  signal_promise.set_value();
  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace
}  // namespace firebase
