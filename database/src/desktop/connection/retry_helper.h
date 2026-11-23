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
class RetryHelper {
 public:
  // Returns a uniform random double in [0.0, 1.0).
  using RandomGenerator = std::function<double()>;

  static constexpr uint64_t kDefaultMinRetryDelayAfterFailureMs = 1000;
  static constexpr uint64_t kDefaultMaxRetryDelayMs = 30000;
  static constexpr double kDefaultRetryExponent = 1.3;
  static constexpr double kDefaultJitterFactor = 0.7;

  RetryHelper(scheduler::Scheduler* scheduler, Logger* logger,
              uint64_t min_retry_delay_after_failure_ms =
                  kDefaultMinRetryDelayAfterFailureMs,
              uint64_t max_retry_delay_ms = kDefaultMaxRetryDelayMs,
              double retry_exponent = kDefaultRetryExponent,
              double jitter_factor = kDefaultJitterFactor,
              RandomGenerator random_generator = nullptr);
  ~RetryHelper();

  // RetryHelper is neither copyable nor movable.
  RetryHelper(const RetryHelper&) = delete;
  RetryHelper& operator=(const RetryHelper&) = delete;

  // Schedules |callback| to run after the next backoff delay, cancelling any
  // previously scheduled pending retry.
  void Retry(std::function<void()> callback);

  // Resets backoff state after a successful connection or interrupt.
  void SignalSuccess();

  // Jumps the current backoff delay directly to |max_retry_delay_ms_|.
  void SetMaxDelay();

  // Cancels any pending scheduled retry and resets the current retry delay.
  void Cancel();

  // Inspection helpers for unit tests.
  uint64_t current_retry_delay_ms() const;
  uint64_t last_scheduled_delay_ms() const;
  bool last_was_success() const;

 private:
  typedef firebase::internal::SafeReference<RetryHelper> ThisRef;
  typedef firebase::internal::SafeReferenceLock<RetryHelper> ThisRefLock;

  double NextRandom();

  scheduler::Scheduler* scheduler_;
  Logger* logger_;
  const uint64_t min_retry_delay_after_failure_ms_;
  const uint64_t max_retry_delay_ms_;
  const double retry_exponent_;
  const double jitter_factor_;
  RandomGenerator random_generator_;
  std::mt19937_64 rng_;

  mutable ThisRef safe_this_;
  scheduler::RequestHandle scheduled_retry_;
  uint64_t scheduled_retry_id_;
  uint64_t next_task_id_;
  uint64_t current_retry_delay_;
  uint64_t last_scheduled_delay_ms_;
  bool last_was_success_;
};

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CONNECTION_RETRY_HELPER_H_
