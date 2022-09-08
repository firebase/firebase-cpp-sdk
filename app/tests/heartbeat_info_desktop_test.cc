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

#include "app/src/app_common.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

class HeartbeatInfoTest : public ::testing::Test {
 public:
  HeartbeatInfoTest() : app_(testing::CreateApp()) {}

 protected:
  std::unique_ptr<App> app_;

  void SetUp() override {
    Logger logger(nullptr);
    heartbeat::HeartbeatStorageDesktop storage(app_->name(), logger);
    // For the sake of testing, clear any pre-existing stored heartbeats.
    heartbeat::LoggedHeartbeats empty_heartbeats_struct;
    storage.Write(empty_heartbeats_struct);
  }
};

TEST_F(HeartbeatInfoTest, GlobalOnlyHeartbeat) {
  app_->GetHeartbeatController()->LogHeartbeat();
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(app_->GetHeartbeatController()),
            HeartbeatInfo::Code::Combined);
}

TEST_F(HeartbeatInfoTest, NoHeartbeat) {
  EXPECT_EQ(HeartbeatInfo::GetHeartbeatCode(app_->GetHeartbeatController()),
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
        volatile auto code =
            HeartbeatInfo::GetHeartbeatCode(app_->GetHeartbeatController());
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
