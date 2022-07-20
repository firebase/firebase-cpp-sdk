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
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || \
    FIREBASE_PLATFORM_TVOS || FIREBASE_PLATFORM_WINDOWS
#define SEMAPHORE_LINUX 0
#else
#define SEMAPHORE_LINUX 1
#include <pthread.h>

#include <csignal>
#include <ctime>
#endif

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

  ASSERT_LT(
      labs((finish_ms - start_ms) - firebase::internal::kMillisecondsPerSecond),
      0.20 * firebase::internal::kMillisecondsPerSecond);
}

#if SEMAPHORE_LINUX
// Tests that Timed Wait handles spurious wakeup (Linux/Android specific).
TEST(SemaphoreTest, TimedWaitSpuriousWakeupLinux) {
  firebase::Semaphore sem(0);

  auto signaller = [](void* arg) -> void* {
    struct timespec req, rem;
    req.tv_sec = 0;
    req.tv_nsec = 400000000;
    while (true) {
      auto result = nanosleep(&req, &rem);
      if (result == 0) {
        break;
      } else if (errno == EINTR) {
        req = rem;
      } else {
        ADD_FAILURE() << "nanosleep() failed";
        return NULL;
      }
    }

    pthread_kill(*static_cast<pthread_t*>(arg), SIGUSR1);

    return NULL;
  };

  signal(SIGUSR1, [](int signum) -> void {});

  pthread_t current_thread = pthread_self();
  pthread_t signaller_thread;
  pthread_create(&signaller_thread, NULL, signaller, &current_thread);

  int64_t start_ms = firebase::internal::GetTimestamp();
  EXPECT_FALSE(sem.TimedWait(2 * firebase::internal::kMillisecondsPerSecond));
  int64_t finish_ms = firebase::internal::GetTimestamp();

  pthread_join(signaller_thread, NULL);

  ASSERT_LT(labs((finish_ms - start_ms) -
                 (2 * firebase::internal::kMillisecondsPerSecond)),
            0.20 * firebase::internal::kMillisecondsPerSecond);
}
#endif  // #if SEMAPHORE_LINUX

TEST(SemaphoreTest, MultithreadedStressTest) {
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
