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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_SEMAPHORE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_SEMAPHORE_H_

#include <errno.h>

#include "app/src/include/firebase/internal/platform.h"
#include "app/src/time.h"

#if FIREBASE_PLATFORM_WINDOWS
#include <limits.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <time.h>

#include <string>
#endif  // FIREBASE_PLATFORM_WINDOWS

#include <cassert>

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
#include "app/src/mutex.h"
#include "app/src/pthread_condvar.h"
#endif  //  FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

class Semaphore {
 public:
  explicit Semaphore(int initial_count) {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
    // MacOS requires named semaphores, and does not support unnamed.
    // Generate a unique string for the semaphore name:
    static const char kPrefix[] = "/firebase-";
    // String length of the name prefix.
    static const int kPprefixLen = sizeof(kPrefix);
    // String length of the pointer, when printed to a string.
    static const int kPointerStringLength = 16;
    // Buffer size.  the +1 is for the null terminator.
    static const int kBufferSize = kPprefixLen + kPointerStringLength + 1;

    char buffer[kBufferSize];
    snprintf(buffer, kBufferSize, "%s%016llx", kPrefix,
             static_cast<unsigned long long>(  // NOLINT
                 reinterpret_cast<intptr_t>(this)));

    semaphore_ = sem_open(buffer,
                          O_CREAT | O_EXCL,   // Create if it doesn't exist.
                          S_IRUSR | S_IWUSR,  // Only the owner can read/write.
                          initial_count);
    // Remove the semaphore from the system registry, so other processes can't
    // grab it.  (These are designed to just be passed around internally by
    // pointer, like unnamed semaphores.  Mac OSX targets don't support unnamed
    // semaphores though, so we have to use named, and just treat them like
    // unnamed.
    bool success = (sem_unlink(buffer) == 0);
    assert(success);
    (void)success;
#elif !FIREBASE_PLATFORM_WINDOWS
    // Android requires unnamed semaphores, and does not support sem_open
    semaphore_ = &semaphore_value_;
    bool success = sem_init(semaphore_, 0, initial_count) == 0;
    assert(success);
    (void)success;
#else
    semaphore_ = CreateSemaphore(nullptr,        // default security attributes
                                 initial_count,  // initial count
                                 LONG_MAX,       // maximum count
                                 nullptr);       // unnamed semaphore
#endif
    assert(semaphore_ != nullptr);
  }

  ~Semaphore() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
    // Because we use named semaphores on MacOS, we need to close them.
    bool success = (sem_close(semaphore_) == 0);
    assert(success);
    (void)success;
#elif !FIREBASE_PLATFORM_WINDOWS
    // Because we use unnamed semaphores otherwise, we need to destroy them.
    bool success = (sem_destroy(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    CloseHandle(semaphore_);
#endif
  }

  void Post() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
    MutexLock lock(cond_mutex_);
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS

#if !FIREBASE_PLATFORM_WINDOWS
    bool success = (sem_post(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    bool success = (ReleaseSemaphore(semaphore_, 1, nullptr) != 0);
    assert(success);
    (void)success;
#endif

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
    // Notify any potential timedWait calls that are waiting for this.
    cond_.NotifyAll();
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
  }

  void Wait() {
#if !FIREBASE_PLATFORM_WINDOWS
    bool success = (sem_wait(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    // Wait forever, until the signal occurs.
    DWORD ret = WaitForSingleObject(semaphore_, INFINITE);
    assert(ret == WAIT_OBJECT_0);
#endif
  }

  // Returns false if it was unable to acquire a lock.  True otherwise.
  bool TryWait() {
#if !FIREBASE_PLATFORM_WINDOWS
    int ret = sem_trywait(semaphore_);
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
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
    MutexLock lock(cond_mutex_);
    return cond_.TimedWait(
        cond_mutex_.native_handle(), [this] { return this->TryWait(); },
        milliseconds);
#elif FIREBASE_PLATFORM_WINDOWS
    return WaitForSingleObject(semaphore_, milliseconds) == 0;
#else  // not windows and not mac - should be Linux.
    timespec t = internal::MsToAbsoluteTimespec(milliseconds);
    return sem_timedwait(semaphore_, &t) == 0;
#endif
  }

 private:
#if !FIREBASE_PLATFORM_WINDOWS
  sem_t* semaphore_;
#else
  HANDLE semaphore_;
#endif

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
  // Apple implementations need a condition variable to make TimedWait() work.
  Mutex cond_mutex_;
  internal::ConditionVariable cond_;
#elif !FIREBASE_PLATFORM_WINDOWS
  // On non-Mac POSIX systems, we keep our own sem_t object.
  sem_t semaphore_value_;
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SEMAPHORE_H_
