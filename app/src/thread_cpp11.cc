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

#include "app/src/thread.h"

#if !defined(_STLPORT_VERSION)

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

Thread::Thread() {}

Thread::~Thread() {}

#ifdef FIREBASE_USE_EXPLICIT_DEFAULT_METHODS
Thread::Thread(Thread&& other) = default;
#else
Thread::Thread(Thread&& other) { thread_.swap(other.thread_); }
#endif  // FIREBASE_USE_EXPLICIT_DEFAULT_METHODS

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg)
    : thread_(start_routine, arg) {}

Thread::Thread(Thread::NoArgRoutine start_routine) : thread_(start_routine) {}

#ifdef FIREBASE_USE_EXPLICIT_DEFAULT_METHODS
Thread& Thread::operator=(Thread&& other) = default;
#else
Thread& Thread::operator=(Thread&& other) {
  if (this == &other) {
    return *this;
  }

  thread_.swap(other.thread_);
  return *this;
}
#endif  // FIREBASE_USE_EXPLICIT_DEFAULT_METHODS

bool Thread::Joinable() const { return thread_.joinable(); }

void Thread::Join() { thread_.join(); }

void Thread::Detach() { thread_.detach(); }

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
#endif
