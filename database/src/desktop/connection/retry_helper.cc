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
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <utility>

#include "app/src/assert.h"

namespace firebase {
namespace database {
namespace internal {
namespace connection {
namespace {

// Mixes several non-blocking, non-throwing sources of entropy to seed the RNG
// for jitter calculations, which does not require cryptographic-strength
// randomness.
std::minstd_rand::result_type SeedRng(const void* instance_addr) {
  static std::atomic<uint64_t> counter{0};
  const uint64_t time_ticks = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch().count());
  const uint64_t addr_bits =
      static_cast<uint64_t>(reinterpret_cast<std::uintptr_t>(instance_addr));
  const uint64_t count = counter.fetch_add(1, std::memory_order_relaxed) + 1;
  uint64_t z = time_ticks ^ (addr_bits * 0x9e3779b97f4a7c15ULL) ^
               (count * 0xbf58476d1ce4e5b9ULL);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  z = z ^ (z >> 31);
  return static_cast<std::minstd_rand::result_type>(
      (z % (std::minstd_rand::modulus - 1)) + 1);
}

}  // namespace

RetryHelper::RetryHelper(scheduler::Scheduler* scheduler, Logger* logger)
    : RetryHelper(scheduler, logger, Options()) {}

RetryHelper::RetryHelper(scheduler::Scheduler* scheduler, Logger* logger,
                         Options options)
    : scheduler_(scheduler),
      logger_(logger),
      min_retry_delay_after_failure_ms_(
          options.min_retry_delay_after_failure_ms),
      max_retry_delay_ms_(options.max_retry_delay_ms),
      retry_exponent_(options.retry_exponent),
      jitter_factor_(options.jitter_factor),
      rng_(options.random_generator ? std::minstd_rand::default_seed
                                    : SeedRng(this)),
      random_generator_(options.random_generator
                            ? std::move(options.random_generator)
                            : [this]() {
                                std::uniform_real_distribution<double> dist(
                                    0.0, 1.0);
                                return dist(rng_);
                              }),
      safe_this_(this),
      scheduled_retry_id_(0),
      next_task_id_(0),
      current_retry_delay_ms_(0),
      last_scheduled_delay_ms_(0),
      last_was_success_(true) {
  FIREBASE_ASSERT(scheduler_ != nullptr);
  FIREBASE_ASSERT(logger_ != nullptr);
  FIREBASE_ASSERT(min_retry_delay_after_failure_ms_ <= max_retry_delay_ms_);
  FIREBASE_ASSERT(std::isfinite(retry_exponent_) && retry_exponent_ >= 1.0);
  FIREBASE_ASSERT(jitter_factor_ >= 0.0 && jitter_factor_ <= 1.0);
}

RetryHelper::~RetryHelper() {
  // Clear SafeReference first so any callback checking/clearing state completes
  // under SafeReferenceLock before this object is torn down, and any unstarted
  // callback becomes a no-op.
  safe_this_.ClearReference();
  if (scheduled_retry_.IsValid()) {
    scheduled_retry_.Cancel();
  }
}

void RetryHelper::Retry(std::function<void()> callback) {
  scheduler::RequestHandle previous_retry;
  {
    ThisRefLock lock(&safe_this_);

    // Cancel the in-progress or scheduled retry, if there is one.
    if (scheduled_retry_.IsValid()) {
      logger_->LogDebug("[RetryHelper] Cancelling previous scheduled retry");
      std::swap(previous_retry, scheduled_retry_);
      scheduled_retry_id_ = 0;
    }

    uint64_t delay_ms = 0;
    if (last_was_success_) {
      delay_ms = 0;
    } else {
      if (current_retry_delay_ms_ == 0) {
        current_retry_delay_ms_ = min_retry_delay_after_failure_ms_;
      } else {
        const double new_delay_double =
            static_cast<double>(current_retry_delay_ms_) * retry_exponent_;
        if (new_delay_double >= static_cast<double>(max_retry_delay_ms_)) {
          current_retry_delay_ms_ = max_retry_delay_ms_;
        } else {
          current_retry_delay_ms_ = static_cast<uint64_t>(new_delay_double);
        }
      }
      if (jitter_factor_ == 0.0 || current_retry_delay_ms_ == 0) {
        delay_ms = current_retry_delay_ms_;
      } else {
        const double random_value = NextRandom();
        const double base_double = static_cast<double>(current_retry_delay_ms_);
        const double jittered = ((1.0 - jitter_factor_) * base_double) +
                                (jitter_factor_ * base_double * random_value);
        if (jittered <= 0.0) {
          delay_ms = 0;
        } else if (jittered >= base_double) {
          delay_ms = current_retry_delay_ms_;
        } else {
          delay_ms = static_cast<uint64_t>(jittered);
        }
      }
    }

    last_was_success_ = false;
    last_scheduled_delay_ms_ = delay_ms;
    logger_->LogDebug("[RetryHelper] Scheduling retry in %" PRIu64 "ms",
                      delay_ms);

    const uint64_t task_id = ++next_task_id_;
    scheduled_retry_id_ = task_id;
    scheduled_retry_ = scheduler_->Schedule(
        [safe_ref = safe_this_, task_id,
         callback = std::move(callback)]() mutable {
          {
            ThisRefLock callback_lock(&safe_ref);
            RetryHelper* helper = callback_lock.GetReference();
            if (!helper || helper->scheduled_retry_id_ != task_id) {
              return;
            }
            // Clear scheduled_retry_ before invoking the user callback so
            // re-entrant Retry() or Cancel() calls will not invoke
            // RequestHandle::Cancel() on the currently running request while
            // Scheduler::TriggerCallback() holds status->mutex.
            helper->scheduled_retry_ = scheduler::RequestHandle();
            helper->scheduled_retry_id_ = 0;
          }
          if (callback) {
            callback();
          }
        },
        delay_ms);
  }

  // Cancel the previous handle outside safe_this_ lock to avoid lock-order
  // inversion with Scheduler::TriggerCallback() (which holds status->mutex).
  if (previous_retry.IsValid()) {
    previous_retry.Cancel();
  }
}

void RetryHelper::SignalSuccess() {
  ThisRefLock lock(&safe_this_);
  last_was_success_ = true;
  current_retry_delay_ms_ = 0;
}

void RetryHelper::SetMaxDelay() {
  ThisRefLock lock(&safe_this_);
  current_retry_delay_ms_ = max_retry_delay_ms_;
}

void RetryHelper::Cancel() {
  scheduler::RequestHandle to_cancel;
  {
    ThisRefLock lock(&safe_this_);
    if (scheduled_retry_.IsValid()) {
      logger_->LogDebug("[RetryHelper] Cancelling existing retry attempt");
      std::swap(to_cancel, scheduled_retry_);
      scheduled_retry_id_ = 0;
    } else {
      logger_->LogDebug("[RetryHelper] No existing retry attempt to cancel");
    }
    current_retry_delay_ms_ = 0;
  }

  if (to_cancel.IsValid()) {
    to_cancel.Cancel();
  }
}

uint64_t RetryHelper::current_retry_delay_ms() const {
  ThisRefLock lock(&safe_this_);
  return current_retry_delay_ms_;
}

uint64_t RetryHelper::last_scheduled_delay_ms() const {
  ThisRefLock lock(&safe_this_);
  return last_scheduled_delay_ms_;
}

bool RetryHelper::last_was_success() const {
  ThisRefLock lock(&safe_this_);
  return last_was_success_;
}

double RetryHelper::NextRandom() {
  const double val = random_generator_();
  // Allow a closed upper bound (<= 1.0) merely to accept buggy standard
  // library implementations (e.g. LWG 2524) that occasionally return 1.0.
  FIREBASE_DEV_ASSERT(std::isfinite(val) && val >= 0.0 && val <= 1.0);
  if (!std::isfinite(val) || val < 0.0) {
    return 0.0;
  }
  if (val >= 1.0) {
    return std::nextafter(1.0, 0.0);
  }
  return val;
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
