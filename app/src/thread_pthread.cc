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

#include <memory>

#include "app/src/assert.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/pthread_condvar.h"

namespace firebase {

namespace {

// Heap-allocated synchronization state shared between the parent constructor
// and the newly spawned pthread so the parent never destroys a stack-allocated
// Mutex/ConditionVariable while the child is still unlocking it.
struct ContextHolder {
  ContextHolder(Thread::UnsafeRoutine in_routine,
                Thread::AdapterRoutine in_adapter_routine, void* in_context)
      : routine(in_routine),
        adapter_routine(in_adapter_routine),
        user_context(in_context),
        started(false),
        mutex(Mutex::kModeNonRecursive) {}

  Thread::UnsafeRoutine routine;
  Thread::AdapterRoutine adapter_routine;
  void* user_context;
  bool started;
  Mutex mutex;
  internal::ConditionVariable cond_var;
};

void* InitializeAndRun(void* data) {
  std::unique_ptr<std::shared_ptr<ContextHolder>> holder_ptr(
      static_cast<std::shared_ptr<ContextHolder>*>(data));
  std::shared_ptr<ContextHolder> holder = *holder_ptr;
  holder_ptr.reset();

  auto routine = holder->routine;
  auto adapter_routine = holder->adapter_routine;
  void* ctx = holder->user_context;

  {
    MutexLock lock(holder->mutex);
    holder->started = true;
    holder->cond_var.NotifyOne();
  }
  holder.reset();

  adapter_routine(routine, ctx);
  return nullptr;
}

struct NoArgContext {
  Thread::NoArgRoutine routine;
};

}  // namespace

Thread::Thread() : thread_{}, is_joinable_(false) {}

Thread::~Thread() {
  FIREBASE_ASSERT_MESSAGE(!is_joinable_,
                          "Thread destructor called on a joinable thread. It "
                          "must be either join()'ed or detach()'ed before the "
                          "thread can be destructed.");
}

Thread::Thread(Thread&& other) noexcept
    : thread_(other.thread_), is_joinable_(other.is_joinable_) {
  other.thread_ = {};
  other.is_joinable_ = false;
}

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg)
    : Thread(start_routine, arg,
             [](Thread::UnsafeRoutine routine, void* context) {
               routine(context);
             }) {}

Thread::Thread(Thread::NoArgRoutine start_routine)
    : Thread(
          nullptr, new NoArgContext{start_routine},
          [](Thread::UnsafeRoutine, void* context) {
            std::unique_ptr<NoArgContext> holder(
                static_cast<NoArgContext*>(context));
            if (holder->routine) {
              holder->routine();
            }
          }) {}

Thread::Thread(Thread::UnsafeRoutine start_routine, void* arg,
               Thread::AdapterRoutine adapter_routine)
    : thread_{}, is_joinable_(false) {
  auto holder =
      std::make_shared<ContextHolder>(start_routine, adapter_routine, arg);
  auto* child_holder = new std::shared_ptr<ContextHolder>(holder);

  int creation_result =
      pthread_create(&thread_, nullptr, &InitializeAndRun, child_holder);
  if (creation_result != 0) {
    delete child_holder;
  }
  FIREBASE_ASSERT_MESSAGE(creation_result == 0, "Could not start thread");

  // Wait for the thread to initialize and start executing start_routine.
  {
    MutexLock lock(holder->mutex);
    holder->cond_var.Wait(holder->mutex.native_handle(),
                          [&holder] { return holder->started; });
  }
  is_joinable_ = true;
}

Thread& Thread::operator=(Thread&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  FIREBASE_ASSERT_MESSAGE(
      !is_joinable_,
      "Cannot unassign a joinable thread. Join or detach it first.");

  thread_ = other.thread_;
  is_joinable_ = other.is_joinable_;
  other.thread_ = {};
  other.is_joinable_ = false;
  return *this;
}

bool Thread::Joinable() const { return is_joinable_; }

void Thread::Join() {
  FIREBASE_ASSERT_MESSAGE(is_joinable_,
                          "Not allowed to join a non-joinable thread");
  is_joinable_ = false;
  int join_err = pthread_join(thread_, nullptr);
  FIREBASE_ASSERT_MESSAGE(join_err == 0, "Could not join thread");
}

void Thread::Detach() {
  FIREBASE_ASSERT_MESSAGE(is_joinable_,
                          "Not allowed to detach a non-joinable thread");
  is_joinable_ = false;
  int detach_err = pthread_detach(thread_);
  FIREBASE_ASSERT_MESSAGE(detach_err == 0, "Could not detach thread");
}

// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // defined(_STLPORT_VERSION)
