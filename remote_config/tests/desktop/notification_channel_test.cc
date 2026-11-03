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

#include "remote_config/src/desktop/notification_channel.h"

#include <atomic>
#include <chrono>              // NOLINT
#include <condition_variable>  // NOLINT
#include <mutex>               // NOLINT
#include <thread>              // NOLINT

#include "gtest/gtest.h"

namespace firebase {
namespace remote_config {
namespace internal {

class NotificationChannelTest : public ::testing::Test {
 protected:
  std::atomic<int> times_{0};
  NotificationChannel channel_;
};

TEST_F(NotificationChannelTest, All) {
  std::mutex sync_mutex;
  std::condition_variable entered_cv;
  std::condition_variable proceed_cv;
  bool in_iteration_1 = false;
  bool allow_finish_iteration_1 = false;

  std::thread thread([&]() {
    while (channel_.Get()) {
      int current = times_.fetch_add(1) + 1;
      if (current == 1) {
        std::unique_lock<std::mutex> lock(sync_mutex);
        in_iteration_1 = true;
        entered_cv.notify_all();
        proceed_cv.wait(lock, [&]() { return allow_finish_iteration_1; });
      }
    }
  });

  EXPECT_EQ(times_.load(), 0);

  // Thread will get `notification` and pause inside the first iteration.
  channel_.Put();
  {
    std::unique_lock<std::mutex> lock(sync_mutex);
    entered_cv.wait(lock, [&]() { return in_iteration_1; });
  }
  // Expect thread got one notification. Thread is processing something now.
  EXPECT_EQ(times_.load(), 1);

  // Thread will get `notification` after current loop iteration.
  channel_.Put();
  // Expect thread got one `notification` total. It is processing something.
  EXPECT_EQ(times_.load(), 1);

  // Thread is doing something. It will get notification after finishing the
  // first loop iteration. So channel will coalesce this Put() call.
  channel_.Put();
  EXPECT_EQ(times_.load(), 1);

  {
    std::lock_guard<std::mutex> lock(sync_mutex);
    allow_finish_iteration_1 = true;
  }
  proceed_cv.notify_all();

  for (int i = 0; i < 100 && times_.load() < 2; ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  // Expect thread got two `notification`s total.
  EXPECT_EQ(times_.load(), 2);

  // Thread will get notification that channel is closed. Thread will be closed.
  channel_.Close();
  thread.join();

  // Thread should be closed, because channel is closed.
  channel_.Put();
  // Still expect that thread got two `notification`s total.
  EXPECT_EQ(times_.load(), 2);
}

}  // namespace internal
}  // namespace remote_config
}  // namespace firebase
