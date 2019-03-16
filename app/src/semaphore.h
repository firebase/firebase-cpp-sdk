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
#include "app/src/time.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <semaphore.h>
#include <sys/stat.h>
#include <time.h>
#include <string>
#else
#include <limits.h>
#include <windows.h>
#endif  // !defined(_WIN32)

#ifdef __APPLE__
#include "TargetConditionals.h"
#endif  // __APPLE__

#include <cassert>

#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
#include "app/src/mutex.h"
#include "app/src/pthread_condvar.h"
#endif  // !defined(_WIN32) && !defined(__APPLE__)

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

class Semaphore {
 public:
  explicit Semaphore(int initial_count) {
#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
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
#elif !defined(_WIN32)
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
#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
    // Because we use named semaphores on MacOS, we need to close them.
    bool success = (sem_close(semaphore_) == 0);
    assert(success);
    (void)success;
#elif !defined(_WIN32)
    // Because we use unnamed semaphores otherwise, we need to destroy them.
    bool success = (sem_destroy(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    CloseHandle(semaphore_);
#endif
  }

  void Post() {
#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
    MutexLock lock(cond_mutex_);
#endif  // defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)

#if !defined(_WIN32)
    bool success = (sem_post(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    bool success = (ReleaseSemaphore(semaphore_, 1, nullptr) != 0);
    assert(success);
    (void)success;
#endif

#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
    // Notify any potential timedWait calls that are waiting for this.
    cond_.NotifyAll();
#endif  // defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
  }

  void Wait() {
#if !defined(_WIN32)
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
#if !defined(_WIN32)
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
#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
    MutexLock lock(cond_mutex_);
    return cond_.TimedWait(cond_mutex_.native_handle(),
                           [this] { return this->TryWait(); },
                           milliseconds);
#elif defined(_WIN32)
    return WaitForSingleObject(semaphore_, milliseconds) == 0;
#else  // not windows and not mac - should be Linux.
    timespec t = internal::MsToAbsoluteTimespec(milliseconds);
    return sem_timedwait(semaphore_, &t) == 0;
#endif
  }

 private:
#if !defined(_WIN32)
  sem_t* semaphore_;
#else
  HANDLE semaphore_;
#endif

#if defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
  // Apple implementations need a condition variable to make TimedWait() work.
  Mutex cond_mutex_;
  internal::ConditionVariable cond_;
#elif !defined(_WIN32)
  // On non-Mac POSIX systems, we keep our own sem_t object.
  sem_t semaphore_value_;
#endif  // defined(TARGET_OS_OSX) || defined(TARGET_OS_IOS)
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_SEMAPHORE_H_
