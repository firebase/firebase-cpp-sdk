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

#include <errno.h>

#include "app/src/assert.h"
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

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/pthread_condvar.h"
#include "app/src/util_apple.h"

#define FIREBASE_SEMAPHORE_DEFAULT_PREFIX "/firebase-"
#endif  //  FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS ||
        //  FIREBASE_PLATFORM_TVOS

namespace firebase {

class Semaphore {
 public:
  explicit Semaphore(int initial_count) {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    // MacOS requires named semaphores, and does not support unnamed.
    // Generate a unique string for the semaphore name:

    // String length of the pointer, when printed to a string.
    static const int kPointerStringLength = 16;
    // Buffer size.  the +1 is for the null terminator.
    static const int kBufferSize = kPointerStringLength + 1;

    char buffer[kBufferSize];
    snprintf(buffer, kBufferSize, "%016llx",
             static_cast<unsigned long long>(  // NOLINT
                 reinterpret_cast<intptr_t>(this)));
#if FIREBASE_PLATFORM_OSX
    // Append custom semaphore names, if present, to support macOS Sandbox
    // mode.
    std::string semaphore_name = util::GetSandboxModeSemaphorePrefix();
    if (semaphore_name.empty()) {
      semaphore_name = FIREBASE_SEMAPHORE_DEFAULT_PREFIX;
    }
#else
    std::string semaphore_name = FIREBASE_SEMAPHORE_DEFAULT_PREFIX;
#endif  // FIREBASE_PLATFORM_OSX

    semaphore_name.append(buffer);
    semaphore_ = sem_open(semaphore_name.c_str(),
                          O_CREAT | O_EXCL,   // Create if it doesn't exist.
                          S_IRUSR | S_IWUSR,  // Only the owner can read/write.
                          initial_count);

    // Check for errors.
#if FIREBASE_PLATFORM_OSX
    FIREBASE_ASSERT_MESSAGE(
        semaphore_ != SEM_FAILED,
        "Firebase failed to create semaphore. If running in sandbox mode be "
        "sure to configure FBAppGroupEntitlementName in your app's "
        "Info.plist.");
#endif  // FIREBASE_PLATFORM_OSX

    assert(semaphore_ != SEM_FAILED);
    assert(semaphore_ != nullptr);

    // Remove the semaphore from the system registry, so other processes can't
    // grab it.  (These are designed to just be passed around internally by
    // pointer, like unnamed semaphores.  Mac OSX targets don't support unnamed
    // semaphores though, so we have to use named, and just treat them like
    // unnamed.
    bool success = (sem_unlink(semaphore_name.c_str()) == 0);
    assert(success);
    (void)success;
#elif !FIREBASE_PLATFORM_WINDOWS
    // Android requires unnamed semaphores, and does not support sem_open
    semaphore_ = &semaphore_value_;
    bool success = sem_init(semaphore_, 0, initial_count) == 0;
    assert(success);
    (void)success;
    assert(semaphore_ != nullptr);
#else
    semaphore_ = CreateSemaphore(nullptr,        // default security attributes
                                 initial_count,  // initial count
                                 LONG_MAX,       // maximum count
                                 nullptr);       // unnamed semaphore
    assert(semaphore_ != nullptr);
#endif
  }

  ~Semaphore() {
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
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
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS ||
        // FIREBASE_PLATFORM_TVOS

#if !FIREBASE_PLATFORM_WINDOWS
    bool success = (sem_post(semaphore_) == 0);
    assert(success);
    (void)success;
#else
    bool success = (ReleaseSemaphore(semaphore_, 1, nullptr) != 0);
    assert(success);
    (void)success;
#endif

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    // Notify any potential timedWait calls that are waiting for this.
    cond_.NotifyAll();
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS ||
        // FIREBASE_PLATFORM_TVOS
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
#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
    MutexLock lock(cond_mutex_);
    return cond_.TimedWait(
        cond_mutex_.native_handle(), [this] { return this->TryWait(); },
        milliseconds);
#elif FIREBASE_PLATFORM_WINDOWS
    return WaitForSingleObject(semaphore_, milliseconds) == 0;
#else  // not windows and not mac - should be Linux.
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
          assert("sem_timedwait() failed with EINVAL" == 0);
          return false;
        case EDEADLK:
          assert("sem_timedwait() failed with EDEADLK" == 0);
          return false;
        default:
          assert("sem_timedwait() failed with an unknown error" == 0);
          return false;
      }
    }
#endif
  }

 private:
#if !FIREBASE_PLATFORM_WINDOWS
  sem_t* semaphore_;
#else
  HANDLE semaphore_;
#endif

#if FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  // Apple implementations need a condition variable to make TimedWait() work.
  Mutex cond_mutex_;
  internal::ConditionVariable cond_;
#elif !FIREBASE_PLATFORM_WINDOWS
  // On non-Mac POSIX systems, we keep our own sem_t object.
  sem_t semaphore_value_;
#endif  // FIREBASE_PLATFORM_OSX || FIREBASE_PLATFORM_IOS ||
        // FIREBASE_PLATFORM_TVOS
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SEMAPHORE_H_
