/*
 * Copyright 2020 Google LLC
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

#include "app/src/heartbeat_info_desktop.h"

#include <ctime>
#include <future>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)
#include <vector>

#include "app/src/heartbeat_date_storage_desktop.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

const char kSdkTag[] = "fire-iid";
const char kGlobalTag[] = "GLOBAL";

std::time_t Now() {
  std::time_t now = 0;
  std::time(&now);
  return now;
}

std::time_t DayAgo() {
  return Now() - 86400 - 1;  // A day plus one second.
}

class HeartbeatInfoTest : public ::testing::Test {
 public:
  HeartbeatInfoTest() {
    SetStaleDate(kSdkTag);
    SetStaleDate(kGlobalTag);
  }

 protected:
  void ReadHeartbeat() {
    bool read_ok = storage_.ReadPersisted();
    if (!read_ok) {
      FAIL() << "Unable to read the heartbeat file";
    }
  }

  void WriteHeartbeat() {
    bool write_ok = storage_.WritePersisted();
    if (!write_ok) {
      FAIL() << "Unable to write the heartbeat file";
    }
  }

  void SetRecentDate(const char* tag) {
    ReadHeartbeat();
    storage_.Set(tag, Now());
    WriteHeartbeat();
  }

  void SetStaleDate(const char* tag) {
    ReadHeartbeat();
    storage_.Set(tag, DayAgo());
    WriteHeartbeat();
  }

 private:
  HeartbeatDateStorage storage_;
};

TEST_F(HeartbeatInfoTest, CombinedHeartbeat) {
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(kSdkTag),
            HeartbeatInfo::Code::Combined);
}

TEST_F(HeartbeatInfoTest, SdkOnlyHeartbeat) {
  SetRecentDate(kGlobalTag);
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(kSdkTag),
            HeartbeatInfo::Code::Sdk);
}

TEST_F(HeartbeatInfoTest, GlobalOnlyHeartbeat) {
  SetRecentDate(kSdkTag);
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(kSdkTag),
            HeartbeatInfo::Code::Global);
}

TEST_F(HeartbeatInfoTest, NoHeartbeat) {
  SetRecentDate(kSdkTag);
  SetRecentDate(kGlobalTag);
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(kSdkTag),
            HeartbeatInfo::Code::None);
}

TEST_F(HeartbeatInfoTest, ParallelRequests) {
  std::promise<void> signal_promise;
  std::future<void> signal = signal_promise.get_future();

  std::vector<std::thread> threads;
  for (int i = 0; i != 10; ++i) {
    threads.emplace_back([&] {
      signal.wait();

      for (int i = 0; i != 1000; ++i) {
        volatile auto code = HeartbeatInfo::GetHeartbeatCode("test-name");
        (void)code;
      }
    });
  }

  signal_promise.set_value();

  for (auto& t : threads) {
    t.join();
  }
}

}  // namespace
}  // namespace firebase
