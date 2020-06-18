/*
 * Copyright 2017 Google LLC
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

#include "app/src/semaphore.h"

#include "app/src/thread.h"
#include "app/src/time.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace {

// Basic test of TryWait, to make sure that its successes and failures
// line up with what we'd expect, based on the initial count.
TEST(SemaphoreTest, TryWaitTests) {
  firebase::Semaphore sem(2);

  // First time, should be able to get a value just fine.
  EXPECT_EQ(sem.TryWait(), true);

  // Second time, should still be able to get a value.
  EXPECT_EQ(sem.TryWait(), true);

  // Second time, we should be unable to acquire a lock.
  EXPECT_EQ(sem.TryWait(), false);

  sem.Post();

  // Should be able to get a lock now.
  EXPECT_EQ(sem.TryWait(), true);
}

// Test that semaphores work across threads.
// Blocks, after setting a thread to unlock itself in 1 second.
// If the thread doesn't unblock it, it will wait forever, triggering a test
// failure via timeout after 60 seconds, through the testing framework.
TEST(SemaphoreTest, MultithreadedTest) {
  firebase::Semaphore sem(0);

  firebase::Thread(
      [](void* data_) {
        auto sem = static_cast<firebase::Semaphore*>(data_);
        firebase::internal::Sleep(firebase::internal::kMillisecondsPerSecond);
        sem->Post();
      },
      &sem)
      .Detach();

  // This will block, until the thread releases it.
  sem.Wait();
}

// Tests that Timed Wait works as intended.
TEST(SemaphoreTest, TimedWait) {
  firebase::Semaphore sem(0);

  int64_t start_ms = firebase::internal::GetTimestamp();
  EXPECT_FALSE(sem.TimedWait(firebase::internal::kMillisecondsPerSecond));
  int64_t finish_ms = firebase::internal::GetTimestamp();

  assert(labs((finish_ms - start_ms) -
              firebase::internal::kMillisecondsPerSecond) <
         0.10 * firebase::internal::kMillisecondsPerSecond);
}

TEST(SemaphoreTest, DISABLED_MultithreadedStressTest) {
  for (int i = 0; i < 10000; ++i) {
    firebase::Semaphore sem(0);

    firebase::Thread thread = firebase::Thread(
        [](void* data_) {
          auto sem = static_cast<firebase::Semaphore*>(data_);
          sem->Post();
        },
        &sem);
    // This will block, until the thread releases it or it times out.
    EXPECT_TRUE(sem.TimedWait(100));

    thread.Join();
  }
}

}  // namespace
