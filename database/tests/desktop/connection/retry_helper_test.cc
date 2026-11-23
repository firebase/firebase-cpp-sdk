// Copyright 2026 Google LLC
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

#include "database/src/desktop/connection/retry_helper.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <vector>

#include "app/src/logger.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"
#include "gtest/gtest.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {
namespace {

constexpr int kWaitTimeoutMs = 5000;

class RetryHelperTest : public ::testing::Test {
 protected:
  RetryHelperTest() : logger_(nullptr) {}

  scheduler::Scheduler scheduler_;
  Logger logger_;
};

TEST_F(RetryHelperTest, FirstRetryIsImmediate) {
  RetryHelper helper(
      &scheduler_, &logger_, RetryHelper::kDefaultMinRetryDelayAfterFailureMs,
      RetryHelper::kDefaultMaxRetryDelayMs, RetryHelper::kDefaultRetryExponent,
      RetryHelper::kDefaultJitterFactor, []() { return 0.5; });

  EXPECT_TRUE(helper.last_was_success());
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  Semaphore sem(0);
  helper.Retry([&sem]() { sem.Post(); });

  EXPECT_FALSE(helper.last_was_success());
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);
  EXPECT_TRUE(sem.TimedWait(kWaitTimeoutMs));
}

TEST_F(RetryHelperTest, ExponentialBackoffCurveWithDeterministicRng) {
  const std::vector<uint64_t> expected_base_delays = {
      1000, 1300,  1690,  2197,  2856,  3712,  4825,  6272,
      8153, 10598, 13777, 17910, 23283, 30000, 30000,
  };

  const std::vector<double> rng_values = {0.0, 0.5, 0.999999};

  for (double rng_value : rng_values) {
    RetryHelper helper(&scheduler_, &logger_, 1000, 30000, 1.3, 0.7,
                       [rng_value]() { return rng_value; });

    // Initial retry is immediate (0 ms).
    helper.Retry([]() {});
    EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);
    EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

    for (uint64_t expected_base : expected_base_delays) {
      helper.Retry([]() {});
      EXPECT_EQ(helper.current_retry_delay_ms(), expected_base);

      const uint64_t min_expected = static_cast<uint64_t>(0.3 * expected_base);
      const uint64_t expected_jittered = static_cast<uint64_t>(
          ((1.0 - 0.7) * expected_base) + (0.7 * expected_base * rng_value));

      EXPECT_EQ(helper.last_scheduled_delay_ms(), expected_jittered);
      EXPECT_GE(helper.last_scheduled_delay_ms(), min_expected);
      EXPECT_LT(helper.last_scheduled_delay_ms(), expected_base);
    }
    helper.Cancel();
  }
}

TEST_F(RetryHelperTest, CapIsAppliedBeforeJitter) {
  // With rng = 0.0 and max = 30000, jittered delay at cap is 0.3 * 30000 =
  // 9000, never exceeding 30000 and never computing jitter on an uncapped
  // 23283 * 1.3 = 30267.
  double rng_value = 0.0;
  RetryHelper helper(&scheduler_, &logger_, 1000, 30000, 1.3, 0.7,
                     [&rng_value]() { return rng_value; });

  // Advance to the step just before cap (23283) and then to cap (30000).
  helper.Retry([]() {});  // 0
  for (int i = 0; i < 13; ++i) {
    helper.Retry([]() {});
  }
  EXPECT_EQ(helper.current_retry_delay_ms(), 23283u);

  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 9000u);

  rng_value = 0.999999;
  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 29999u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, SignalSuccessResetsBackoffAndMakesNextRetryImmediate) {
  RetryHelper helper(&scheduler_, &logger_, 1000, 30000, 1.3, 0.7,
                     []() { return 0.5; });

  helper.Retry([]() {});  // 0
  helper.Retry([]() {});  // 1000 -> 650
  helper.Retry([]() {});  // 1300 -> 845
  EXPECT_EQ(helper.current_retry_delay_ms(), 1300u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 845u);

  helper.SignalSuccess();
  EXPECT_TRUE(helper.last_was_success());
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  Semaphore sem(0);
  helper.Retry([&sem]() { sem.Post(); });
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);
  EXPECT_TRUE(sem.TimedWait(kWaitTimeoutMs));

  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 1000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 650u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, SetMaxDelayJumpsToMaxDelay) {
  double rng_value = 0.5;
  RetryHelper helper(&scheduler_, &logger_, 1000, 30000, 1.3, 0.7,
                     [&rng_value]() { return rng_value; });

  // Simulate initial attempt that failed auth before delay built up.
  helper.Retry([]() {});
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);

  helper.SetMaxDelay();
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);

  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 19500u);

  rng_value = 0.0;
  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 9000u);

  // When last_was_success_ is true, first retry is still 0 ms, then max.
  helper.SignalSuccess();
  helper.SetMaxDelay();
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  helper.Retry([]() {});
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);
  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 30000u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 9000u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, CancelPreventsPendingTaskAndResetsCurrentDelay) {
  RetryHelper helper(&scheduler_, &logger_, 100, 1000, 1.3, 0.0,
                     []() { return 0.0; });

  // Cancel when nothing is scheduled should be safe and leave delay at 0.
  helper.Cancel();
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  // First retry is immediate; wait for it to finish so second retry is delayed.
  Semaphore first_sem(0);
  helper.Retry([&first_sem]() { first_sem.Post(); });
  EXPECT_TRUE(first_sem.TimedWait(kWaitTimeoutMs));

  std::atomic<bool> second_ran(false);
  helper.Retry([&second_ran]() { second_ran.store(true); });
  EXPECT_EQ(helper.current_retry_delay_ms(), 100u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 100u);

  helper.Cancel();
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  // Wait past the 100ms delay to verify the cancelled callback never runs.
  ::firebase::internal::Sleep(200);
  EXPECT_FALSE(second_ran.load());

  // Next Retry() after Cancel() (without SignalSuccess) starts at min delay.
  helper.Retry([]() {});
  EXPECT_EQ(helper.current_retry_delay_ms(), 100u);
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 100u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, OverlappingRetryCancelsPreviousTask) {
  RetryHelper helper(&scheduler_, &logger_, 200, 1000, 1.3, 0.0,
                     []() { return 0.0; });

  // Burn the immediate 0ms first attempt.
  Semaphore init_sem(0);
  helper.Retry([&init_sem]() { init_sem.Post(); });
  EXPECT_TRUE(init_sem.TimedWait(kWaitTimeoutMs));

  std::atomic<int> first_count(0);
  std::atomic<int> second_count(0);
  Semaphore second_sem(0);

  // Schedule a 200ms retry, then immediately supersede it with another Retry().
  helper.Retry([&first_count]() { first_count.fetch_add(1); });
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 200u);

  // Reset to 0ms via SignalSuccess so the replacement runs immediately while
  // ensuring the cancelled 200ms task never fires even after sleeping past it.
  helper.SignalSuccess();
  helper.Retry([&second_count, &second_sem]() {
    second_count.fetch_add(1);
    second_sem.Post();
  });
  EXPECT_EQ(helper.last_scheduled_delay_ms(), 0u);
  EXPECT_TRUE(second_sem.TimedWait(kWaitTimeoutMs));

  ::firebase::internal::Sleep(250);
  EXPECT_EQ(first_count.load(), 0);
  EXPECT_EQ(second_count.load(), 1);
}

TEST_F(RetryHelperTest, ReentrantRetryInsideCallbackDoesNotDeadlock) {
  RetryHelper helper(&scheduler_, &logger_, 10, 100, 1.3, 0.0,
                     []() { return 0.0; });

  Semaphore second_sem(0);
  std::atomic<int> count(0);

  helper.Retry([&helper, &second_sem, &count]() {
    count.fetch_add(1);
    helper.Retry([&second_sem, &count]() {
      count.fetch_add(1);
      second_sem.Post();
    });
  });

  EXPECT_TRUE(second_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(count.load(), 2);
  EXPECT_EQ(helper.current_retry_delay_ms(), 10u);
}

TEST_F(RetryHelperTest, ReentrantCancelInsideCallbackDoesNotDeadlock) {
  RetryHelper helper(&scheduler_, &logger_, 10, 100, 1.3, 0.0,
                     []() { return 0.0; });

  Semaphore cancel_only_sem(0);
  helper.Retry([&helper, &cancel_only_sem]() {
    helper.Cancel();
    cancel_only_sem.Post();
  });
  EXPECT_TRUE(cancel_only_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  Semaphore sem(0);
  helper.Retry([&helper, &sem]() {
    // Re-entrantly schedule and then cancel inside the active callback.
    helper.Retry(
        []() { FAIL() << "Cancelled re-entrant retry should not run"; });
    helper.Cancel();
    sem.Post();
  });

  EXPECT_TRUE(sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);

  ::firebase::internal::Sleep(50);
}

TEST_F(RetryHelperTest, DestructorCancelsPendingTaskWithoutUaf) {
  std::atomic<bool> callback_ran(false);
  {
    auto helper = std::make_unique<RetryHelper>(
        &scheduler_, &logger_, 100, 1000, 1.3, 0.0, []() { return 0.0; });

    Semaphore init_sem(0);
    helper->Retry([&init_sem]() { init_sem.Post(); });
    EXPECT_TRUE(init_sem.TimedWait(kWaitTimeoutMs));

    helper->Retry([&callback_ran]() { callback_ran.store(true); });
    EXPECT_EQ(helper->last_scheduled_delay_ms(), 100u);
  }  // Destroy RetryHelper while 100ms task is queued.

  ::firebase::internal::Sleep(200);
  EXPECT_FALSE(callback_ran.load());
}

TEST_F(RetryHelperTest, DestructorWaitsForInFlightCallbackWithoutUaf) {
  auto helper = std::make_unique<RetryHelper>(&scheduler_, &logger_, 10, 100,
                                              1.3, 0.0, []() { return 0.0; });

  Semaphore entered(0);
  std::atomic<bool> destroyed_after_callback(false);
  std::atomic<bool> callback_finished(false);

  helper->Retry([&entered, &destroyed_after_callback, &callback_finished]() {
    entered.Post();
    ::firebase::internal::Sleep(80);
    EXPECT_FALSE(destroyed_after_callback.load());
    callback_finished.store(true);
  });

  EXPECT_TRUE(entered.TimedWait(kWaitTimeoutMs));
  helper.reset();
  destroyed_after_callback.store(true);

  EXPECT_TRUE(callback_finished.load());
}

TEST_F(RetryHelperTest, DefaultRngProducesValuesWithinExpectedJitterBounds) {
  RetryHelper helper(&scheduler_, &logger_);

  std::set<uint64_t> observed_delays;
  for (int i = 0; i < 64; ++i) {
    helper.SignalSuccess();
    helper.Retry([]() {});  // 0 ms
    helper.Retry([]() {});  // 1000 ms * [0.3, 1.0) -> [300, 1000]
    uint64_t delay = helper.last_scheduled_delay_ms();
    EXPECT_GE(delay, 300u);
    EXPECT_LE(delay, 1000u);
    observed_delays.insert(delay);
    helper.Cancel();
  }
  // With 64 uniform draws over [300, 1000), collisions across all 64 draws
  // are astronomically unlikely.
  EXPECT_GT(observed_delays.size(), 1u);
}

TEST_F(RetryHelperTest, SecondRetryRespectsScheduledDelay) {
  RetryHelper helper(&scheduler_, &logger_, 80, 500, 1.3, 0.0,
                     []() { return 0.0; });

  Semaphore first_sem(0);
  helper.Retry([&first_sem]() { first_sem.Post(); });
  EXPECT_TRUE(first_sem.TimedWait(kWaitTimeoutMs));

  const uint64_t start_ms = ::firebase::internal::GetTimestamp();
  Semaphore second_sem(0);
  helper.Retry([&second_sem]() { second_sem.Post(); });
  EXPECT_TRUE(second_sem.TimedWait(kWaitTimeoutMs));
  const uint64_t elapsed_ms = ::firebase::internal::GetTimestamp() - start_ms;

  // Allow slight OS timer granularity slack while ensuring it actually waited.
  EXPECT_GE(elapsed_ms, 60u);
}

TEST_F(RetryHelperTest, HandlesNullLoggerAndEmptyCallbackSafely) {
  RetryHelper helper(&scheduler_, /*logger=*/nullptr, 10, 100, 1.3, 0.0,
                     []() { return 0.0; });

  // Empty callback at 0 ms and delayed backoff should execute without crashing,
  // and Cancel() with nullptr logger should be safe.
  helper.Retry(nullptr);
  helper.Retry(nullptr);
  EXPECT_EQ(helper.current_retry_delay_ms(), 10u);

  Semaphore sem(0);
  helper.SignalSuccess();
  helper.Retry([&sem]() { sem.Post(); });
  EXPECT_TRUE(sem.TimedWait(kWaitTimeoutMs));

  helper.Cancel();
  EXPECT_EQ(helper.current_retry_delay_ms(), 0u);
}

}  // namespace
}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
