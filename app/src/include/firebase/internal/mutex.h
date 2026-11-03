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

#ifndef FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_
#define FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_

#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {

#if !defined(DOXYGEN)

/// @brief A simple synchronization lock. Only one thread at a time can Acquire.
class Mutex {
 public:
  // Bitfield that describes the mutex configuration.
  enum Mode {
    kModeNonRecursive = (0 << 0),
    kModeRecursive = (1 << 0),
  };

  Mutex() : Mutex(kModeRecursive) {}

  explicit Mutex(Mode mode);

  ~Mutex();

  // Acquires the lock for this mutex, blocking until it is available.
  void Acquire();

  // Releases the lock for this mutex acquired by a previous `Acquire()` call.
  void Release();

  // Standard BasicLockable interface for compatibility with std::lock_guard.
  void lock() { Acquire(); }
  void unlock() { Release(); }

// Returns the implementation-defined native mutex handle.
// Used by firebase::Thread implementation.
#if FIREBASE_PLATFORM_WINDOWS
  HANDLE* native_handle() { return &synchronization_object_; }
#else
  pthread_mutex_t* native_handle() { return &mutex_; }
#endif  // FIREBASE_PLATFORM_WINDOWS

 private:
  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

#if FIREBASE_PLATFORM_WINDOWS
  HANDLE synchronization_object_;
  Mode mode_;
#else
  pthread_mutex_t mutex_;
#endif  // FIREBASE_PLATFORM_WINDOWS
};

/// @brief Acquire and hold a \ref Mutex, while in scope.
///
/// Example usage:
///   \code{.cpp}
///   Mutex synchronization_mutex;
///   void MyFunctionThatRequiresSynchronization() {
///     MutexLock lock(synchronization_mutex);
///     // ... logic ...
///   }
///   \endcode
class MutexLock {
 public:
  explicit MutexLock(Mutex& mutex) : mutex_(&mutex) { mutex_->Acquire(); }
  ~MutexLock() { mutex_->Release(); }

  MutexLock(const MutexLock& rhs) = delete;
  MutexLock& operator=(const MutexLock& rhs) = delete;

 private:
  Mutex* mutex_;
};

#endif  // !defined(DOXYGEN)

}  // namespace firebase

#endif  // FIREBASE_APP_SRC_INCLUDE_FIREBASE_INTERNAL_MUTEX_H_
