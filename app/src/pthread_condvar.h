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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_PTHREAD_CONDVAR_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_PTHREAD_CONDVAR_H_

#include "app/src/include/firebase/internal/platform.h"

#if !FIREBASE_PLATFORM_WINDOWS

#include <errno.h>
#include <pthread.h>

#include "app/src/time.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace internal {
// A simple RAII wraper around pthread_cond_t.
//
// It is not portable so it lives here.
class ConditionVariable {
 public:
  ConditionVariable() { pthread_cond_init(&cond_, nullptr); }
  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;
  ~ConditionVariable() { pthread_cond_destroy(&cond_); }

  void Wait(pthread_mutex_t* mutex) { pthread_cond_wait(&cond_, mutex); }
  bool TimedWait(pthread_mutex_t* mutex, const timespec* abstime) {
    return pthread_cond_wait(&cond_, mutex) != 0;
  }
  bool TimedWait(pthread_mutex_t* mutex, int milliseconds) {
    timespec abstime = MsToAbsoluteTimespec(milliseconds);
    return pthread_cond_timedwait(&cond_, mutex, &abstime) != 0;
  }
  template <class Predicate>
  void Wait(pthread_mutex_t* lock, Predicate predicate) {
    while (!predicate()) {
      Wait(lock);
    }
  }

  // Waits for the condition variable to go, AND for the predicate to succeed.
  // Returns false if it times out before both those conditions are met.
  // Returns true otherwise
  template <class Predicate>
  bool TimedWait(pthread_mutex_t* lock, Predicate predicate, int milliseconds) {
    timespec end_time, current_time;
    clock_gettime(CLOCK_REALTIME, &end_time);
    end_time.tv_nsec += milliseconds * kNanosecondsPerMillisecond;
    int64_t end_time_ms = TimespecToMs(end_time);
    NormalizeTimespec(&end_time);

    clock_gettime(CLOCK_REALTIME, &current_time);
    int64_t current_time_ms = TimespecToMs(current_time);
    while (!predicate() && current_time_ms < end_time_ms) {
      TimedWait(lock, end_time_ms - current_time_ms);
      clock_gettime(CLOCK_REALTIME, &current_time);
      current_time_ms = TimespecToMs(current_time);
    }
    // If time isn't up, AND the predicate is true, then we return true.
    // False otherwise.
    return current_time_ms < end_time_ms;
  }

  void NotifyOne() { pthread_cond_signal(&cond_); }
  void NotifyAll() { pthread_cond_broadcast(&cond_); }

 private:
  pthread_cond_t cond_;
};

}  // namespace internal
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // !FIREBASE_PLATFORM_WINDOWS

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_PTHREAD_CONDVAR_H_
