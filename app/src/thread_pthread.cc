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

#if defined(_STLPORT_VERSION)

#include "app/src/assert.h"
#include "app/src/mutex.h"
#include "app/src/pthread_condvar.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {

// Helper POD that holds data needed for the pthread start_routine. See
// InitializeAndRun() for details on how it is used.
struct ContextHolder {
  firebase::Thread::UnsafeRoutine routine;
  firebase::Thread::AdapterRoutine adapter_routine;
  void* user_context;
  bool* is_joinable;
  firebase::Mutex* mutex;
  firebase::internal::ConditionVariable* cond_var;
};

// pthread start_routine. Accomplishes 2 things:
//
// * Adapts the pthread's signature void* (*)(void*) to void (*)(void*) that's
//   passed into the Thread object.
// * Synchronizes the end of Thread's constructor with the beginning of Routine
//   invocation.
void* InitializeAndRun(void* data) {
  auto* holder = static_cast<ContextHolder*>(data);
  auto routine = holder->routine;
  auto adapter_routine = holder->adapter_routine;
  void* ctx = holder->user_context;

  {
    firebase::MutexLock lock(*holder->mutex);
    *holder->is_joinable = true;
    holder->cond_var->NotifyOne();
  }

  adapter_routine(routine, ctx);
  return nullptr;
}
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

namespace FIREBASE_NAMESPACE {

Thread::Thread() : is_joinable_(false) {}

Thread::~Thread() {
  FIREBASE_ASSERT_MESSAGE(!is_joinable_,
                          "Thread destructor called on a joinable thread. It "
                          "must be either join()'ed or detach()'ed before the "
                          "thread can be destructed.");
}

Thread::Thread(Thread&& other) {
  thread_ = other.thread_;
  is_joinable_ = other.is_joinable_;
  other.is_joinable_ = false;
}

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg)
    : Thread(start_routine, arg,
             [](Thread::UnsafeRoutine start_routine, void* arg) {
               start_routine(arg);
             }) {}

Thread::Thread(Thread::NoArgRoutine start_routine)
    : Thread(reinterpret_cast<Thread::UnsafeRoutine>(start_routine), nullptr,
             [](Thread::UnsafeRoutine start_routine, void* arg) {
               reinterpret_cast<Thread::NoArgRoutine>(start_routine)();
             }) {}

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg,
               Thread::AdapterRoutine adapter_routine)
    : is_joinable_(false) {
  Mutex mutex;
  firebase::internal::ConditionVariable cond_var;

  ContextHolder context_holder{start_routine, adapter_routine, arg,
                               &is_joinable_, &mutex,          &cond_var};

  int creation_result =
      pthread_create(&thread_, nullptr, &InitializeAndRun, &context_holder);
  FIREBASE_ASSERT_MESSAGE(creation_result == 0, "Could not start thread");

  // Wait for the thread to initialize and start executing start_routine.
  {
    MutexLock lock(mutex);
    cond_var.Wait(mutex.native_handle(), [this] { return is_joinable_; });
  }
}

Thread& Thread::operator=(Thread&& other) {
  if (this == &other) {
    return *this;
  }
  FIREBASE_ASSERT_MESSAGE(
      !is_joinable_,
      "Cannot unassign a joinable thread. Join or detach it first.");

  thread_ = other.thread_;
  is_joinable_ = other.is_joinable_;
  other.is_joinable_ = false;
  return *this;
}

bool Thread::Joinable() const { return is_joinable_; }

void Thread::Join() {
  FIREBASE_ASSERT_MESSAGE(is_joinable_,
                          "Not allowed to join a non-joinable thread");
  is_joinable_ = false;
  FIREBASE_ASSERT_MESSAGE(pthread_join(thread_, nullptr) == 0,
                          "Could not join thread");
}

void Thread::Detach() {
  FIREBASE_ASSERT_MESSAGE(is_joinable_,
                          "Not allowed to join a non-joinable thread");
  is_joinable_ = false;
  FIREBASE_ASSERT_MESSAGE(pthread_detach(thread_) == 0,
                          "Could not detach thread");
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE

#endif  // defined(_STLPORT_VERSION)
