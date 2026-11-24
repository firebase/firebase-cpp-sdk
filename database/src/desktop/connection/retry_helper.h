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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_RETRY_HELPER_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_RETRY_HELPER_H_

#include <cstdint>
#include <functional>
#include <random>

#include "app/src/logger.h"
#include "app/src/safe_reference.h"
#include "app/src/scheduler.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {

// Schedules retry attempts with exponential backoff and random jitter on a
// Scheduler, matching the Android/iOS RTDB connection retry behavior.
// Callbacks passed to Retry() execute on the Scheduler worker thread without
// holding RetryHelper's internal lock and must guard any external lifetimes
// (e.g., via SafeReference or std::shared_ptr).
// Lifetime: |scheduler| and |logger| must be non-null and outlive RetryHelper.
class RetryHelper {
 public:
  // Returns a uniform random double in [0.0, 1.0).
  // Runs synchronously under RetryHelper's internal lock, so implementations
  // must be lightweight and non-reentrant.
  using RandomGenerator = std::function<double()>;

  static constexpr uint64_t kDefaultMinRetryDelayAfterFailureMs = 1000;
  static constexpr uint64_t kDefaultMaxRetryDelayMs = 30000;
  static constexpr double kDefaultRetryExponent = 1.3;
  static constexpr double kDefaultJitterFactor = 0.7;

  struct Options {
    uint64_t min_retry_delay_after_failure_ms =
        kDefaultMinRetryDelayAfterFailureMs;
    uint64_t max_retry_delay_ms = kDefaultMaxRetryDelayMs;
    double retry_exponent = kDefaultRetryExponent;
    double jitter_factor = kDefaultJitterFactor;
    // Optional deterministic/custom RNG. Executed synchronously while holding
    // RetryHelper's internal lock; must be lightweight and non-reentrant.
    RandomGenerator random_generator = nullptr;
  };

  // |scheduler| and |logger| must be non-null and outlive RetryHelper.
  // Invalid |options| (min > max, non-finite or < 1.0 exponent, or jitter
  // outside [0.0, 1.0]) fail via release-active FIREBASE_ASSERT.
  RetryHelper(scheduler::Scheduler* scheduler, Logger* logger);
  RetryHelper(scheduler::Scheduler* scheduler, Logger* logger, Options options);
  ~RetryHelper();

  // RetryHelper is neither copyable nor movable.
  RetryHelper(const RetryHelper&) = delete;
  RetryHelper& operator=(const RetryHelper&) = delete;

  // Schedules |callback| to run after the next backoff delay, cancelling any
  // previously scheduled pending retry.
  // Note: |callback| executes without holding RetryHelper's internal lock, so
  // it may run concurrently with RetryHelper destruction and must guard any
  // external object lifetimes (e.g., via SafeReference or std::shared_ptr).
  void Retry(std::function<void()> callback);

  // Resets backoff state after a successful connection or interrupt.
  // Note: Matching Android's RetryHelper, this does NOT cancel an
  // already-scheduled pending retry.
  void SignalSuccess();

  // Jumps the current backoff delay directly to |max_retry_delay_ms_|.
  // Note: Matching Android's RetryHelper, this does not clear
  // |last_was_success_|; if |last_was_success_| is true, the next Retry()
  // still runs at 0 ms before subsequent failures use |max_retry_delay_ms_|.
  void SetMaxDelay();

  // Cancels any pending scheduled retry and resets the current retry delay.
  // Note: This does NOT reset |last_was_success_| and does NOT wait for an
  // already-executing callback to finish.
  void Cancel();

 private:
  friend class RetryHelperTestPeer;

  using ThisRef = firebase::internal::SafeReference<RetryHelper>;
  using ThisRefLock = firebase::internal::SafeReferenceLock<RetryHelper>;

  // Inspection helpers for unit tests via RetryHelperTestPeer.
  uint64_t current_retry_delay_ms() const;
  uint64_t last_scheduled_delay_ms() const;
  bool last_was_success() const;

  double NextRandom();

  scheduler::Scheduler* const scheduler_;
  Logger* const logger_;
  const uint64_t min_retry_delay_after_failure_ms_;
  const uint64_t max_retry_delay_ms_;
  const double retry_exponent_;
  const double jitter_factor_;
  std::minstd_rand rng_;
  const RandomGenerator random_generator_;

  mutable ThisRef safe_this_;
  scheduler::RequestHandle scheduled_retry_;
  uint64_t scheduled_retry_id_;
  uint64_t next_task_id_;
  uint64_t current_retry_delay_ms_;
  uint64_t last_scheduled_delay_ms_;
  bool last_was_success_;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_RETRY_HELPER_H_
