/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_APP_SRC_MUTEX_H_
#define FIREBASE_APP_SRC_MUTEX_H_
#include <errno.h>

#include "app/src/include/firebase/internal/platform.h"

#if !FIREBASE_PLATFORM_WINDOWS
#include <pthread.h>
#else
#include <windows.h>
#endif  // !FIREBASE_PLATFORM_WINDOWS

#include "app/src/assert.h"

namespace firebase {

/// @brief A simple synchronization lock. Only one thread at a time can Acquire.
class Mutex {
 public:
  // Bitfield that describes the mutex configuration.
  enum Mode {
    kModeNonRecursive = (0 << 0),
    kModeRecursive = (1 << 0),
  };

  Mutex() { Initialize(kModeRecursive); }

  explicit Mutex(Mode mode) { Initialize(mode); }

  ~Mutex() {
#if !FIREBASE_PLATFORM_WINDOWS
    int ret = pthread_mutex_destroy(&mutex_);
    FIREBASE_ASSERT(ret == 0);
    (void)ret;
#else
    CloseHandle(synchronization_object_);
#endif  // !FIREBASE_PLATFORM_WINDOWS
  }

  void Acquire() {
#if !FIREBASE_PLATFORM_WINDOWS
    int ret = pthread_mutex_lock(&mutex_);
    if (ret == EINVAL) {
      return;
    }
#if defined(__APPLE__)
    // Lock / unlock will fail in a static initializer on OSX and iOS.
    FIREBASE_ASSERT(ret == 0 || ret == EINVAL);
#else
    FIREBASE_ASSERT(ret == 0);
#endif  // defined(__APPLE__)
    (void)ret;
#else
    DWORD ret = WaitForSingleObject(synchronization_object_, INFINITE);
    FIREBASE_ASSERT(ret == WAIT_OBJECT_0);
    (void)ret;
#endif  // !FIREBASE_PLATFORM_WINDOWS
  }

  void Release() {
#if !FIREBASE_PLATFORM_WINDOWS
    int ret = pthread_mutex_unlock(&mutex_);
#if defined(__APPLE__)
    // Lock / unlock will fail in a static initializer on OSX and iOS.
    FIREBASE_ASSERT(ret == 0 || ret == EINVAL);
#else
    FIREBASE_ASSERT(ret == 0);
#endif  // defined(__APPLE__)
    (void)ret;
#else
    if (mode_ & kModeRecursive) {
      ReleaseMutex(synchronization_object_);
    } else {
      ReleaseSemaphore(synchronization_object_, 1, 0);
    }
#endif  // !FIREBASE_PLATFORM_WINDOWS
  }

// Returns the implementation-defined native mutex handle.
// Used by firebase::Thread implementation.
#if !FIREBASE_PLATFORM_WINDOWS
  pthread_mutex_t* native_handle() { return &mutex_; }
#else
  HANDLE* native_handle() { return &synchronization_object_; }
#endif

 private:
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Initialize(Mode mode) {
#if !FIREBASE_PLATFORM_WINDOWS
    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);
    FIREBASE_ASSERT(ret == 0);
    if (mode & kModeRecursive) {
      ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
      FIREBASE_ASSERT(ret == 0);
    }
    ret = pthread_mutex_init(&mutex_, &attr);
    FIREBASE_ASSERT(ret == 0);
    ret = pthread_mutexattr_destroy(&attr);
    FIREBASE_ASSERT(ret == 0);
#else
    mode_ = mode;
    if (mode & kModeRecursive) {
      synchronization_object_ = CreateMutex(nullptr, FALSE, nullptr);
    } else {
      synchronization_object_ = CreateSemaphore(nullptr, 1, 1, nullptr);
    }
#endif  // !FIREBASE_PLATFORM_WINDOWS
  }

#if !FIREBASE_PLATFORM_WINDOWS
  pthread_mutex_t mutex_;
#else
  HANDLE synchronization_object_;
  Mode mode_;
#endif  // !FIREBASE_PLATFORM_WINDOWS
};

/// @brief Acquire and hold a /ref Mutex, while in scope.
///
/// Example usage:
///   \code{.cpp}
///   Mutex syncronization_mutex;
///   void MyFunctionThatRequiresSynchronization() {
///     MutexLock lock(syncronization_mutex);
///     // ... logic ...
///   }
///   \endcode
class MutexLock {
 public:
  explicit MutexLock(Mutex& mutex) : mutex_(&mutex) { mutex_->Acquire(); }
  ~MutexLock() { mutex_->Release(); }

 private:
  // Copy is disallowed.
  MutexLock(const MutexLock& rhs);  // NOLINT
  MutexLock& operator=(const MutexLock& rhs);

  Mutex* mutex_;
};

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_MUTEX_H_
