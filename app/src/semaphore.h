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

#ifndef FIREBASE_APP_SRC_SEMAPHORE_H_
#define FIREBASE_APP_SRC_SEMAPHORE_H_

#include <cassert>
#include <cerrno>
#include <cstdint>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/time.h"

#if FIREBASE_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <limits.h>
#include <windows.h>
#elif FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/pthread_condvar.h"
#else
#include <semaphore.h>
#include <time.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {

class Semaphore {
 public:
  explicit Semaphore(int initial_count)
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
      : cond_mutex_(Mutex::kModeNonRecursive), count_(initial_count) {
    FIREBASE_ASSERT(initial_count >= 0);
  }
#elif !FIREBASE_PLATFORM_WINDOWS
  {
    // Android and Linux use unnamed POSIX semaphores.
    semaphore_ = &semaphore_value_;
    bool success = sem_init(semaphore_, 0, initial_count) == 0;
    assert(success);
    (void)success;
    assert(semaphore_ != nullptr);
  }
#else
  {
    semaphore_ = CreateSemaphore(nullptr,        // default security attributes
                                 initial_count,  // initial count
                                 LONG_MAX,       // maximum count
                                 nullptr);       // unnamed semaphore
    assert(semaphore_ != nullptr);
  }
#endif

  ~Semaphore() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    // Synchronize with any concurrent Post() call that just woke a waiter so
    // cond_mutex_ and cond_ are not destroyed while Post() is still unlocking.
    MutexLock lock(cond_mutex_);
#elif !FIREBASE_PLATFORM_WINDOWS
    // Because we use unnamed semaphores otherwise, we need to destroy them.
    bool success = (sem_destroy(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    CloseHandle(semaphore_);
#endif
  }

  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  void Post() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
    ++count_;
    cond_.NotifyOne();
#elif !FIREBASE_PLATFORM_WINDOWS
    bool success = (sem_post(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    bool success = (ReleaseSemaphore(semaphore_, 1, nullptr) != 0);
    assert(success);
    (void)success;
#endif
  }

  void Wait() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
    cond_.Wait(cond_mutex_.native_handle(), [this] { return count_ > 0; });
    --count_;
#elif !FIREBASE_PLATFORM_WINDOWS
    int ret;
    do {
      ret = sem_wait(semaphore_);
    } while (ret != 0 && errno == EINTR);
    bool success = (ret == 0);
    assert(success);
    (void)success;
#else
    // Wait forever, until the signal occurs.
    DWORD ret = WaitForSingleObject(semaphore_, INFINITE);
    assert(ret == WAIT_OBJECT_0);
    (void)ret;
#endif
  }

  // Returns false if it was unable to acquire a lock.  True otherwise.
  bool TryWait() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
    if (count_ > 0) {
      --count_;
      return true;
    }
    return false;
#elif !FIREBASE_PLATFORM_WINDOWS
    int ret;
    do {
      ret = sem_trywait(semaphore_);
    } while (ret != 0 && errno == EINTR);
    if (ret != 0) {
      // If we couldn't lock the semaphore, make sure it's because there are
      // no resources left, and not some other error.  (Then return false.)
      return false;
    }
    // Got the lock, return true!
    return true;
#else
    // Return true if we got the lock.
    DWORD ret = WaitForSingleObject(semaphore_, 0L);
    return (ret == WAIT_OBJECT_0);
#endif
  }

  // Milliseconds describes how long to wait (from the function invocation)
  // before returning.
  // Returns false if it was unable to acquire a lock, true otherwise.
  bool TimedWait(int milliseconds) {
    if (milliseconds < 0) {
      milliseconds = 0;
    }
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
    if (!cond_.TimedWait(
            cond_mutex_.native_handle(), [this] { return count_ > 0; },
            milliseconds)) {
      return false;
    }
    --count_;
    return true;
#elif FIREBASE_PLATFORM_WINDOWS
    return WaitForSingleObject(semaphore_, static_cast<DWORD>(milliseconds)) ==
           WAIT_OBJECT_0;
#else  // not windows and not mac - should be Linux/Android.
    timespec t = internal::MsToAbsoluteTimespec(milliseconds);
    while (true) {
      int result = sem_timedwait(semaphore_, &t);
      if (result == 0) {
        // Return success, since we successfully locked the semaphore.
        return true;
      }
      switch (errno) {
        case EINTR:
          // Restart the wait because we were woken up spuriously.
          continue;
        case ETIMEDOUT:
          // Return failure, since the timeout expired.
          return false;
        case EINVAL:
          assert(!"sem_timedwait() failed with EINVAL");
          return false;
        case EDEADLK:
          assert(!"sem_timedwait() failed with EDEADLK");
          return false;
        default:
          assert(!"sem_timedwait() failed with an unknown error");
          return false;
      }
    }
#endif
  }

 private:
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  Mutex cond_mutex_;
  internal::ConditionVariable cond_;
  int count_;
#elif !FIREBASE_PLATFORM_WINDOWS
  sem_t* semaphore_;
  sem_t semaphore_value_;
#else
  HANDLE semaphore_;
#endif
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SEMAPHORE_H_
