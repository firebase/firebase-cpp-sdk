/*
 * Copyright 2018 Google LLC
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

#include "app/src/scheduler.h"
#include "app/memory/atomic.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace scheduler {

using ::testing::Eq;

class SchedulerTest : public ::testing::Test {
 protected:
  SchedulerTest() {}

  void SetUp() override {
    atomic_count_.store(0);
    while (callback_sem1_.TryWait()) {}
    while (callback_sem2_.TryWait()) {}
    ordered_value_.clear();
    repeat_period_ms_ = 0;
    repeat_countdown_ = 0;
  }

  static void SemaphorePost1() {
    callback_sem1_.Post();
  }

  static void AddCount() {
    atomic_count_.fetch_add(1);
    callback_sem1_.Post();
  }

  static void AddValueInOrder(int v) {
    ordered_value_.push_back(v);
    callback_sem1_.Post();
  }

  static void RecursiveCallback(Scheduler* scheduler) {
    callback_sem1_.Post();
    --repeat_countdown_;

    if (repeat_countdown_ > 0) {
        scheduler->Schedule(
            new callback::CallbackValue1<Scheduler*>(
                scheduler, RecursiveCallback),
            repeat_period_ms_);
    }
  }

  static compat::Atomic<int> atomic_count_;
  static Semaphore callback_sem1_;
  static Semaphore callback_sem2_;
  static std::vector<int> ordered_value_;
  static int repeat_period_ms_;
  static int repeat_countdown_;

  Scheduler scheduler_;
};

compat::Atomic<int> SchedulerTest::atomic_count_(0);
Semaphore SchedulerTest::callback_sem1_(0);  // NOLINT
Semaphore SchedulerTest::callback_sem2_(0);  // NOLINT
std::vector<int> SchedulerTest::ordered_value_;  // NOLINT
int SchedulerTest::repeat_period_ms_ = 0;
int SchedulerTest::repeat_countdown_ = 0;

// 10000 seems to be a good number to surface racing condition.
const int kThreadTestIteration = 10000;

TEST_F(SchedulerTest, Basic) {
  scheduler_.Schedule(new callback::CallbackVoid(SemaphorePost1));
  EXPECT_TRUE(callback_sem1_.TimedWait(1000));

  scheduler_.Schedule(new callback::CallbackVoid(SemaphorePost1), 1);
  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
}

#ifdef FIREBASE_USE_STD_FUNCTION
TEST_F(SchedulerTest, BasicStdFunction) {
  std::function<void(void)> func = [this](){
    callback_sem1_.Post();
  };

  scheduler_.Schedule(func);
  EXPECT_TRUE(callback_sem1_.TimedWait(1000));

  scheduler_.Schedule(func, 1);
  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
}
#endif

TEST_F(SchedulerTest, TriggerOrderNoDelay) {
  std::vector<int> expected;
  for (int i = 0; i < kThreadTestIteration; ++i)
  {
    scheduler_.Schedule(
        new callback::CallbackValue1<int>(
            i, AddValueInOrder));
    expected.push_back(i);
  }

  for (int i = 0; i < kThreadTestIteration; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
  EXPECT_THAT(ordered_value_, Eq(expected));
}

TEST_F(SchedulerTest, TriggerOrderSameDelay) {
  std::vector<int> expected;
  for (int i = 0; i < kThreadTestIteration; ++i)
  {
    scheduler_.Schedule(
        new callback::CallbackValue1<int>(
            i, AddValueInOrder), 1);
    expected.push_back(i);
  }

  for (int i = 0; i < kThreadTestIteration; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
  EXPECT_THAT(ordered_value_, Eq(expected));
}

TEST_F(SchedulerTest, TriggerOrderDifferentDelay) {
  std::vector<int> expected;
  for (int i = 0; i < 1000; ++i)
  {
    scheduler_.Schedule(
        new callback::CallbackValue1<int>(
            i, AddValueInOrder), i);
    expected.push_back(i);
  }

  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(2000));
  }

  EXPECT_THAT(ordered_value_, Eq(expected));
}

TEST_F(SchedulerTest, ExecuteDuringCallback) {
  scheduler_.Schedule(
      new callback::CallbackValue1<Scheduler*>(
          &scheduler_, [](Scheduler* scheduler){
            callback_sem1_.Post();
            scheduler->Schedule(
                new callback::CallbackValue1<Scheduler*>(
                    scheduler, [](Scheduler* scheduler){
                      callback_sem2_.Post();
                    }));
          }));

  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  EXPECT_TRUE(callback_sem2_.TimedWait(1000));
}

TEST_F(SchedulerTest, ScheduleDuringCallback1) {
  scheduler_.Schedule(
      new callback::CallbackValue1<Scheduler*>(
          &scheduler_, [](Scheduler* scheduler){
            callback_sem1_.Post();
            scheduler->Schedule(
                new callback::CallbackValue1<Scheduler*>(
                    scheduler, [](Scheduler* scheduler){
                      callback_sem2_.Post();
                    }), 1);
          }), 1);

  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  EXPECT_TRUE(callback_sem2_.TimedWait(1000));
}

TEST_F(SchedulerTest, ScheduleDuringCallback100) {
  scheduler_.Schedule(
      new callback::CallbackValue1<Scheduler*>(
          &scheduler_, [](Scheduler* scheduler){
            callback_sem1_.Post();
            scheduler->Schedule(
                new callback::CallbackValue1<Scheduler*>(
                    scheduler, [](Scheduler* scheduler){
                      callback_sem2_.Post();
                    }), 100);
          }), 100);

  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  EXPECT_TRUE(callback_sem2_.TimedWait(1000));
}

TEST_F(SchedulerTest, RecursiveCallbackNoInterval) {
  repeat_period_ms_ = 0;
  repeat_countdown_ = 1000;
  scheduler_.Schedule(
      new callback::CallbackValue1<Scheduler*>(
          &scheduler_, RecursiveCallback),
      repeat_period_ms_);

  for (int i = 0; i < 1000; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
}

TEST_F(SchedulerTest, RecursiveCallbackWithInterval) {
  repeat_period_ms_ = 10;
  repeat_countdown_ = 5;
  scheduler_.Schedule(
      new callback::CallbackValue1<Scheduler*>(
          &scheduler_, RecursiveCallback),
      repeat_period_ms_);

  for (int i = 0; i < 5; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
}

TEST_F(SchedulerTest, RepeatCallbackNoDelay) {
  scheduler_.Schedule(new callback::CallbackVoid(SemaphorePost1), 0, 1);

  // Wait for it to repeat 100 times
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
}

TEST_F(SchedulerTest, RepeatCallbackWithDelay) {
  int delay = 100;
  scheduler_.Schedule(new callback::CallbackVoid(SemaphorePost1), delay, 1);

  auto start = internal::GetTimestamp();
  EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  auto end = internal::GetTimestamp();

  // Test if the first delay actually works.
  int actual_delay = static_cast<int>(end - start);
  int error = abs(actual_delay - delay);
  printf("Delay: %dms. Actual delay: %dms. Error: %dms\n", delay, actual_delay,
         error);
  EXPECT_TRUE(error < 0.1 * internal::kMillisecondsPerSecond);

  // Wait for it to repeat 100 times
  for (int i = 0; i < 100; ++i) {
    EXPECT_TRUE(callback_sem1_.TimedWait(1000));
  }
}

TEST_F(SchedulerTest, CancelImmediateCallback) {
  auto test_func = [](int delay){
    // Use standalone scheduler and counter
    Scheduler scheduler;
    compat::Atomic<int> count(0);
    int success_cancel = 0;
    for (int i = 0; i < kThreadTestIteration; ++i) {
      bool cancelled = scheduler.Schedule(
          new callback::CallbackValue1<compat::Atomic<int>*>(
              &count, [](compat::Atomic<int>* count){
                count->fetch_add(1);
          }), 0).Cancel();
      if (cancelled) {
        ++success_cancel;
      }
    }

    internal::Sleep(10);

    // Does not guarantee 100% successful cancellation
    float success_rate = success_cancel * 100.0f / kThreadTestIteration;
    printf("[Delay %dms] Cancel success rate: %.1f%% (And it is ok if not 100%%"
           ")\n", delay, success_rate);
    EXPECT_THAT(success_cancel + count.load(),
                Eq(kThreadTestIteration));
  };

  // Test without delay
  test_func(0);

  // Test with delay
  test_func(1);
}

// This test can take around 5s ~ 30s depending on the platform
TEST_F(SchedulerTest, CancelRepeatCallback) {
  auto test_func = [](int delay, int repeat, int wait_repeat){
    // Use standalone scheduler and counter for iterations
    Scheduler scheduler;
    compat::Atomic<int> count(0);
    while (callback_sem1_.TryWait()) {}

    RequestHandle handler =
        scheduler.Schedule(new callback::CallbackValue1<compat::Atomic<int>*>(
            &count, [](compat::Atomic<int>* count){
              count->fetch_add(1);
              callback_sem1_.Post();
            }), delay, repeat);
    EXPECT_FALSE(handler.IsCancelled());

    for (int i = 0; i < wait_repeat; ++i) {
      EXPECT_TRUE(callback_sem1_.TimedWait(1000));
      EXPECT_TRUE(handler.IsTriggered());
    }

    // Cancellation of a repeat cb should always be successful, as long as
    // it is not cancelled yet
    EXPECT_TRUE(handler.Cancel());
    EXPECT_TRUE(handler.IsCancelled());
    EXPECT_FALSE(handler.Cancel());

    // Should have no more cb triggered after the cancellation
    int saved_count = count.load();

    internal::Sleep(1);
    EXPECT_THAT(count.load(), Eq(saved_count));
  };

  for (int i = 0; i < 1000; ++i) {
    // No delay and do not wait for the first trigger to cancel it
    test_func(0, 1, 0);
    // No delay and wait for the first trigger, then cancel it
    test_func(0, 1, 1);
    // 1ms delay and do not wait for the first trigger to cancel it
    test_func(1, 1, 0);
    // 1ms delay and wait for the first trigger, then cancel it
    test_func(1, 1, 1);
  }
}

TEST_F(SchedulerTest, CancelAll) {
  Scheduler scheduler;
  for (int i = 0; i < kThreadTestIteration; ++i) {
    scheduler.Schedule(new callback::CallbackVoid(AddCount));
  }
  scheduler.CancelAllAndShutdownWorkerThread();
  // Does not guarantee 0% trigger rate
  float trigger_rate = atomic_count_.load() * 100.0f / kThreadTestIteration;
  printf("Callback trigger rate: %.1f%% (And it is ok if not 0%%)\n",
         trigger_rate);
}

TEST_F(SchedulerTest, DeleteScheduler) {
  for (int i = 0; i < kThreadTestIteration; ++i) {
    Scheduler scheduler;
    scheduler.Schedule(new callback::CallbackVoid(AddCount));
  }

  // Does not guarantee 0% trigger rate
  float trigger_rate = atomic_count_.load() * 100.0f / kThreadTestIteration;
  printf("Callback trigger rate: %.1f%% (And it is ok if not 0%%)\n",
         trigger_rate);
}

}  // namespace scheduler
}  // namespace firebase
