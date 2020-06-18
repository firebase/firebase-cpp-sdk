// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <chrono>              // NOLINT
#include <condition_variable>  // NOLINT
#include <thread>              // NOLINT
#include "gtest/gtest.h"

#include "remote_config/src/desktop/notification_channel.h"

namespace firebase {
namespace remote_config {
namespace internal {

class NotificationChannelTest : public ::testing::Test {
 protected:
  int times_ = 0;
  NotificationChannel channel_;
};

TEST_F(NotificationChannelTest, All) {
  std::thread thread([this]() {
    while (channel_.Get()) {
      times_++;
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
  });

  EXPECT_EQ(times_, 0);

  // Thread will get `notification`.
  channel_.Put();
  // Thread will get `notification` in short period of time
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Expect thread got one notification. Thread is processing something now.
  EXPECT_EQ(times_, 1);

  // Thread will get `notification` afrer current loop iteration.
  channel_.Put();
  // Thread will get notification in short period of time
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  // Expect thread got one `notification` total. It is processing something.
  EXPECT_EQ(times_, 1);

  // Thread is doing something. It will get notification after finish first loop
  // iteration. So channel will ignore this Put() call.
  channel_.Put();
  // Thread will finish second loop iteration after sleep.
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // Expect thread got two `notification`s total.
  EXPECT_EQ(times_, 2);

  // Thread will get notification that channel is closed. Thread will be closed.
  channel_.Close();
  // Wait until thread will get `close notification`.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));

  // Thread should be closed, because channel is closed.
  channel_.Put();
  // Still expect that thread got two `notification`s total.
  EXPECT_EQ(times_, 2);

  thread.join();
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
