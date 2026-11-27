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
#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "app/src/logger.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "gtest/gtest.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

class RetryHelperTestPeer {
 public:
  static uint64_t current_retry_delay_ms(const RetryHelper& helper) {
    return helper.current_retry_delay_ms();
  }
  static uint64_t last_scheduled_delay_ms(const RetryHelper& helper) {
    return helper.last_scheduled_delay_ms();
  }
  static bool last_was_success(const RetryHelper& helper) {
    return helper.last_was_success();
  }
};

namespace {

constexpr int kWaitTimeoutMs = 5000;

struct SharedTestState {
  Semaphore sem{0};
  Semaphore extra_sem{0};
  std::atomic<int> count{0};
  std::atomic<int> second_count{0};
  std::atomic<int> lock_executed_count{0};
  std::atomic<uint64_t> cancelled_delay_ms{0};
  std::atomic<bool> flag{false};
  std::atomic<bool> cancelled_cb_ran{false};
};

struct WorkerGate {
  Semaphore entered{0};
  Semaphore release{0};
};

RetryHelper::Options MakeOptions(
    uint64_t min_delay_ms, uint64_t max_delay_ms, double retry_exponent,
    double jitter_factor, RetryHelper::RandomGenerator random_generator) {
  RetryHelper::Options options;
  options.min_retry_delay_after_failure_ms = min_delay_ms;
  options.max_retry_delay_ms = max_delay_ms;
  options.retry_exponent = retry_exponent;
  options.jitter_factor = jitter_factor;
  options.random_generator = std::move(random_generator);
  return options;
}

class RetryHelperTest : public ::testing::Test {
 protected:
  RetryHelperTest() : logger_(nullptr) {}

  // Parks the single Scheduler worker thread on a semaphore so tests can
  // schedule and cancel/supersede/destroy retries with zero preemption race.
  std::shared_ptr<WorkerGate> PauseSchedulerWorker() {
    auto gate = std::make_shared<WorkerGate>();
    scheduler_.Schedule(
        [gate]() {
          gate->entered.Post();
          gate->release.TimedWait(kWaitTimeoutMs);
        },
        0);
    EXPECT_TRUE(gate->entered.TimedWait(kWaitTimeoutMs));
    return gate;
  }

  Logger logger_;
  scheduler::Scheduler scheduler_;
};

TEST_F(RetryHelperTest, FirstRetryIsImmediate) {
  RetryHelper::Options options;
  options.random_generator = []() { return 0.5; };
  RetryHelper helper(&scheduler_, &logger_, std::move(options));

  EXPECT_TRUE(RetryHelperTestPeer::last_was_success(helper));
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);

  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->sem.Post(); });

  EXPECT_FALSE(RetryHelperTestPeer::last_was_success(helper));
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
  EXPECT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));
}

TEST_F(RetryHelperTest, ExponentialBackoffCurveWithDeterministicRng) {
  const std::vector<uint64_t> expected_base_delays = {
      1000, 1300,  1690,  2197,  2856,  3712,  4825,  6272,
      8153, 10598, 13777, 17910, 23283, 30000, 30000,
  };

  const std::vector<double> rng_values = {0.0, 0.5, 0.999999};

  for (double rng_value : rng_values) {
    SCOPED_TRACE(::testing::Message() << "rng_value=" << rng_value);
    RetryHelper helper(&scheduler_, &logger_,
                       MakeOptions(1000, 30000, 1.3, 0.7,
                                   [rng_value]() { return rng_value; }));

    // Initial retry is immediate (0 ms).
    helper.Retry([]() {});
    EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);
    EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);

    for (uint64_t expected_base : expected_base_delays) {
      SCOPED_TRACE(::testing::Message() << "expected_base=" << expected_base);
      helper.Retry([]() {});
      EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper),
                expected_base);

      const double expected_base_double = static_cast<double>(expected_base);
      const uint64_t min_expected =
          static_cast<uint64_t>(0.3 * expected_base_double);
      const uint64_t expected_jittered =
          static_cast<uint64_t>(((1.0 - 0.7) * expected_base_double) +
                                (0.7 * expected_base_double * rng_value));

      EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper),
                expected_jittered);
      EXPECT_GE(RetryHelperTestPeer::last_scheduled_delay_ms(helper),
                min_expected);
      EXPECT_LT(RetryHelperTestPeer::last_scheduled_delay_ms(helper),
                expected_base);
    }
    helper.Cancel();
  }
}

TEST_F(RetryHelperTest, CapIsAppliedBeforeJitter) {
  // With rng = 0.0 and max = 30000, jittered delay at cap is 0.3 * 30000 =
  // 9000, never exceeding 30000 and never computing jitter on an uncapped
  // 23283 * 1.3 = 30267.
  double rng_value = 0.0;
  RetryHelper helper(
      &scheduler_, &logger_,
      MakeOptions(1000, 30000, 1.3, 0.7, [&rng_value]() { return rng_value; }));

  // Advance to the step just before cap (23283) and then to cap (30000).
  helper.Retry([]() {});  // 0
  for (int i = 0; i < 13; ++i) {
    helper.Retry([]() {});
  }
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 23283u);

  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 9000u);

  rng_value = 0.999999;
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 29999u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, SignalSuccessResetsBackoffAndMakesNextRetryImmediate) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(1000, 30000, 1.3, 0.7, []() { return 0.5; }));

  helper.Retry([]() {});  // 0
  helper.Retry([]() {});  // 1000 -> 650
  helper.Retry([]() {});  // 1300 -> 845
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 1300u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 845u);

  helper.SignalSuccess();
  EXPECT_TRUE(RetryHelperTestPeer::last_was_success(helper));
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);

  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->sem.Post(); });
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));

  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 1000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 650u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, SetMaxDelayJumpsToMaxDelay) {
  double rng_value = 0.5;
  RetryHelper helper(
      &scheduler_, &logger_,
      MakeOptions(1000, 30000, 1.3, 0.7, [&rng_value]() { return rng_value; }));

  // Simulate initial attempt that failed auth before delay built up.
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);

  helper.SetMaxDelay();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);

  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 19500u);

  rng_value = 0.0;
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 9000u);

  // When last_was_success_ is true, first retry is still 0 ms, then max.
  helper.SignalSuccess();
  helper.SetMaxDelay();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 30000u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 9000u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, CancelPreventsPendingTaskAndResetsCurrentDelay) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(5, 1000, 1.3, 0.0, []() { return 0.0; }));

  // Cancel when nothing is scheduled should be safe, leave delay at 0, and
  // preserve last_was_success_ == true.
  helper.Cancel();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
  EXPECT_TRUE(RetryHelperTestPeer::last_was_success(helper));
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);

  // First retry is immediate; wait for it to finish so second retry is delayed.
  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->sem.Post(); });
  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));

  auto gate = PauseSchedulerWorker();
  helper.Retry([state]() { state->flag.store(true); });
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 5u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 5u);

  helper.Cancel();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
  gate->release.Post();

  // Wait past the 5ms delay via sentinel barrier to verify the cancelled
  // callback never runs.
  scheduler_.Schedule([state]() { state->extra_sem.Post(); }, 6);
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_FALSE(state->flag.load());

  // Next Retry() after Cancel() (without SignalSuccess) starts at min delay.
  helper.Retry([]() {});
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 5u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 5u);
  helper.Cancel();
}

TEST_F(RetryHelperTest, OverlappingRetryCancelsPreviousTask) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(5, 1000, 1.3, 0.0, []() { return 0.0; }));

  // Burn the immediate 0ms first attempt.
  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->extra_sem.Post(); });
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));

  // Park the scheduler worker so the first delayed retry cannot fire before
  // the superseding Retry() cancels it and advances backoff (5 -> 6 ms).
  auto gate = PauseSchedulerWorker();
  helper.Retry([state]() { state->count.fetch_add(1); });
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 5u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 5u);

  helper.Retry([state]() {
    state->second_count.fetch_add(1);
    state->sem.Post();
  });
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 6u);
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 6u);
  gate->release.Post();

  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));

  scheduler_.Schedule([state]() { state->extra_sem.Post(); }, 7);
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(state->count.load(), 0);
  EXPECT_EQ(state->second_count.load(), 1);
}

TEST_F(RetryHelperTest, SignalSuccessDoesNotCancelPendingRetry) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(5, 1000, 1.3, 0.0, []() { return 0.0; }));

  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->extra_sem.Post(); });
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));

  auto gate = PauseSchedulerWorker();
  helper.Retry([state]() {
    state->count.fetch_add(1);
    state->sem.Post();
  });
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 5u);

  // Matching Android's RetryHelper, SignalSuccess() resets backoff state but
  // does NOT cancel an already-scheduled pending retry.
  helper.SignalSuccess();
  EXPECT_TRUE(RetryHelperTestPeer::last_was_success(helper));
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
  gate->release.Post();

  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(state->count.load(), 1);
}

TEST_F(RetryHelperTest, ReentrantRetryInsideCallbackDoesNotDeadlock) {
  auto state = std::make_shared<SharedTestState>();
  auto helper = std::make_shared<RetryHelper>(
      &scheduler_, &logger_,
      MakeOptions(10, 100, 1.3, 0.0, []() { return 0.0; }));
  std::weak_ptr<RetryHelper> weak_helper = helper;

  helper->Retry([weak_helper, state]() {
    state->count.fetch_add(1);
    if (auto locked_helper = weak_helper.lock()) {
      state->lock_executed_count.fetch_add(1);
      locked_helper->Retry([state]() {
        state->count.fetch_add(1);
        state->sem.Post();
      });
    }
    state->extra_sem.Post();
  });

  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(state->lock_executed_count.load(), 1);
  EXPECT_EQ(state->count.load(), 2);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(*helper), 10u);
}

TEST_F(RetryHelperTest, ReentrantCancelInsideCallbackDoesNotDeadlock) {
  auto state = std::make_shared<SharedTestState>();
  auto helper = std::make_shared<RetryHelper>(
      &scheduler_, &logger_,
      MakeOptions(10, 100, 1.3, 0.0, []() { return 0.0; }));
  std::weak_ptr<RetryHelper> weak_helper = helper;

  // Prime a non-zero current_retry_delay_ms so the assertion after Cancel()
  // verifies Cancel() actually reset it.
  helper->SetMaxDelay();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(*helper), 100u);

  helper->Retry([weak_helper, state]() {
    if (auto locked_helper = weak_helper.lock()) {
      state->lock_executed_count.fetch_add(1);
      EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(*locked_helper),
                100u);
      locked_helper->Cancel();
    }
    state->extra_sem.Post();
  });
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(state->lock_executed_count.load(), 1);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(*helper), 0u);

  helper->Retry([weak_helper, state]() {
    if (auto locked_helper = weak_helper.lock()) {
      state->lock_executed_count.fetch_add(1);
      // Re-entrantly schedule and then cancel inside the active callback.
      locked_helper->Retry([state]() { state->cancelled_cb_ran.store(true); });
      state->cancelled_delay_ms.store(
          RetryHelperTestPeer::last_scheduled_delay_ms(*locked_helper));
      locked_helper->Cancel();
    }
    state->sem.Post();
  });

  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));
  EXPECT_EQ(state->lock_executed_count.load(), 2);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(*helper), 0u);
  const uint64_t cancelled_delay = state->cancelled_delay_ms.load();
  ASSERT_GT(cancelled_delay, 0u);

  scheduler_.Schedule([state]() { state->extra_sem.Post(); },
                      cancelled_delay + 1);
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_FALSE(state->cancelled_cb_ran.load());
}

TEST_F(RetryHelperTest, DestructorCancelsPendingTaskWithoutUaf) {
  auto state = std::make_shared<SharedTestState>();
  {
    auto helper = std::make_unique<RetryHelper>(
        &scheduler_, &logger_,
        MakeOptions(5, 1000, 1.3, 0.0, []() { return 0.0; }));

    helper->Retry([state]() { state->sem.Post(); });
    ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));

    auto gate = PauseSchedulerWorker();
    helper->Retry([state]() { state->flag.store(true); });
    EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(*helper), 5u);
    helper.reset();  // Destroy RetryHelper while 5ms task is queued.
    gate->release.Post();
  }

  scheduler_.Schedule([state]() { state->extra_sem.Post(); }, 6);
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));
  EXPECT_FALSE(state->flag.load());
}

TEST_F(RetryHelperTest,
       CancelSupersedeAndDestructionReleaseCapturedCallbackImmediately) {
  auto gate = PauseSchedulerWorker();
  std::weak_ptr<int> weak3;
  {
    RetryHelper helper(
        &scheduler_, &logger_,
        MakeOptions(30000, 30000, 1.3, 0.0, []() { return 0.0; }));

    // Cancel() immediately destroys the captured callback.
    auto resource1 = std::make_shared<int>(1);
    std::weak_ptr<int> weak1 = resource1;
    helper.Retry([resource1]() {});
    resource1.reset();
    ASSERT_FALSE(weak1.expired());
    helper.Cancel();
    EXPECT_TRUE(weak1.expired());

    // Superseding Retry() immediately destroys the previous callback.
    auto resource2 = std::make_shared<int>(2);
    std::weak_ptr<int> weak2 = resource2;
    helper.Retry([resource2]() {});
    resource2.reset();
    ASSERT_FALSE(weak2.expired());

    auto resource3 = std::make_shared<int>(3);
    weak3 = resource3;
    helper.Retry([resource3]() {});
    resource3.reset();
    EXPECT_TRUE(weak2.expired());
    ASSERT_FALSE(weak3.expired());
  }
  // Destruction immediately releases the pending callback even while the
  // Scheduler worker is still paused.
  EXPECT_TRUE(weak3.expired());
  gate->release.Post();
}

TEST_F(RetryHelperTest, NextRandomClampsClosedUpperBoundAndSupportsFullJitter) {
  // rng == 1.0 (e.g., LWG 2524 buggy stdlib) is clamped strictly below 1.0.
  {
    RetryHelper helper(
        &scheduler_, &logger_,
        MakeOptions(1000, 30000, 1.3, 0.7, []() { return 1.0; }));
    helper.Retry([]() {});  // 0 ms
    helper.Retry([]() {});  // 1000 ms, rng == 1.0 clamped without asserting
    EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 1000u);
    EXPECT_GE(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 999u);
    EXPECT_LE(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 1000u);
    helper.Cancel();
  }

  // jitter_factor == 1.0 spans [0, base) from rng == 0.0 to rng == 1.0.
  {
    double rng_value = 0.0;
    RetryHelper helper(&scheduler_, &logger_,
                       MakeOptions(1000, 30000, 1.3, 1.0,
                                   [&rng_value]() { return rng_value; }));
    helper.Retry([]() {});  // 0 ms
    helper.Retry([]() {});  // 1000 ms * 0.0 -> 0 ms
    EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 1000u);
    EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 0u);

    rng_value = 1.0;
    helper.Retry([]() {});  // 1300 ms * nextafter(1.0, 0.0) -> 1299 ms
    EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 1300u);
    EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 1299u);
    helper.Cancel();
  }
}

TEST_F(RetryHelperTest,
       DestructorCompletesWhileCallbackIsInFlightWithoutDeadlockOrUaf) {
  struct InFlightState {
    Semaphore entered{0};
    Semaphore allow_finish{0};
    Semaphore finished{0};
    std::atomic<bool> allow_finish_succeeded{false};
    std::atomic<bool> callback_finished{false};
  };
  auto state = std::make_shared<InFlightState>();
  auto helper = std::make_unique<RetryHelper>(
      &scheduler_, &logger_,
      MakeOptions(10, 100, 1.3, 0.0, []() { return 0.0; }));

  helper->Retry([state]() {
    state->entered.Post();
    state->allow_finish_succeeded.store(
        state->allow_finish.TimedWait(kWaitTimeoutMs));
    state->callback_finished.store(true);
    state->finished.Post();
  });

  ASSERT_TRUE(state->entered.TimedWait(kWaitTimeoutMs));
  helper.reset();
  state->allow_finish.Post();
  ASSERT_TRUE(state->finished.TimedWait(kWaitTimeoutMs));
  EXPECT_TRUE(state->allow_finish_succeeded.load());
  EXPECT_TRUE(state->callback_finished.load());
}

TEST_F(RetryHelperTest, DefaultRngProducesValuesWithinExpectedJitterBounds) {
  RetryHelper helper(&scheduler_, &logger_);

  std::set<uint64_t> observed_delays;
  for (int i = 0; i < 64; ++i) {
    SCOPED_TRACE(::testing::Message() << "iteration=" << i);
    helper.SignalSuccess();
    helper.Retry([]() {});  // 0 ms
    helper.Retry([]() {});  // 1000 ms * [0.3, 1.0) -> [300, 1000]
    uint64_t delay = RetryHelperTestPeer::last_scheduled_delay_ms(helper);
    EXPECT_GE(delay, 300u);
    EXPECT_LE(delay, 1000u);
    observed_delays.insert(delay);
    helper.Cancel();
  }
  // With 64 uniform draws over [300, 1000], collisions across all 64 draws
  // are astronomically unlikely.
  EXPECT_GT(observed_delays.size(), 1u);
}

TEST_F(RetryHelperTest, SecondRetryRespectsScheduledDelay) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(40, 500, 1.3, 0.0, []() { return 0.0; }));

  auto state = std::make_shared<SharedTestState>();
  helper.Retry([state]() { state->extra_sem.Post(); });
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));

  const auto start_time = std::chrono::steady_clock::now();
  helper.Retry([state]() { state->sem.Post(); });
  EXPECT_EQ(RetryHelperTestPeer::last_scheduled_delay_ms(helper), 40u);
  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - start_time)
                              .count();

  // Allow slight OS timer granularity slack while ensuring it actually waited.
  EXPECT_GE(elapsed_ms, 20);
}

TEST_F(RetryHelperTest, HandlesEmptyCallbackSafely) {
  RetryHelper helper(&scheduler_, &logger_,
                     MakeOptions(10, 100, 1.3, 0.0, []() { return 0.0; }));

  auto state = std::make_shared<SharedTestState>();

  // Empty callback at 0 ms should execute without crashing.
  helper.Retry(nullptr);
  scheduler_.Schedule([state]() { state->sem.Post(); }, 0);
  ASSERT_TRUE(state->sem.TimedWait(kWaitTimeoutMs));

  // Empty callback at delayed backoff should execute without crashing.
  helper.Retry(nullptr);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 10u);
  scheduler_.Schedule([state]() { state->extra_sem.Post(); }, 10);
  ASSERT_TRUE(state->extra_sem.TimedWait(kWaitTimeoutMs));

  // Cancel() should be safe both with no pending task and with a pending task.
  helper.Cancel();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);

  helper.Retry(nullptr);
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 10u);
  helper.Cancel();
  EXPECT_EQ(RetryHelperTestPeer::current_retry_delay_ms(helper), 0u);
}

}  // namespace
}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
