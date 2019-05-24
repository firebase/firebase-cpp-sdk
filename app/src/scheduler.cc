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

#include "app/src/scheduler.h"
#include <cassert>
#include "app/src/time.h"

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif

namespace FIREBASE_NAMESPACE {
namespace scheduler {

bool RequestHandle::Cancel() {
  assert(status_);

  if (!IsValid()) {
    return false;
  }

  MutexLock lock(status_->mutex);
  if (status_->cancelled || (!status_->repeat && status_->triggered)) {
    return false;
  }

  status_->cancelled = true;
  return true;
}

bool RequestHandle::IsCancelled() const {
  assert(status_);
  MutexLock lock(status_->mutex);
  return status_->cancelled;
}

bool RequestHandle::IsTriggered() const {
  assert(status_);
  MutexLock lock(status_->mutex);
  return status_->triggered;
}

Scheduler::RequestData::RequestData(RequestId id, callback::Callback* cb,
                                    ScheduleTimeMs delay, ScheduleTimeMs repeat)
    : id(id),
      cb(cb),
      delay_ms(delay),
      repeat_ms(repeat),
      due_timestamp(0),
      status(new RequestStatusBlock(repeat > 0)) {}

Scheduler::Scheduler()
    : thread_(nullptr),
      next_request_id_(0),
      terminating_(false),
      request_mutex_(Mutex::kModeRecursive),
      sleep_sem_(0) {}

Scheduler::~Scheduler() {
  CancelAllAndShutdownWorkerThread();
}

void Scheduler::CancelAllAndShutdownWorkerThread() {
  {
    // Notify the worker thread to stop processing anymore requests.
    MutexLock lock(request_mutex_);
    if (terminating_) return;
    terminating_ = true;
  }

  // Signal the thread to wake if it is sleeping due to no callbacks in queue
  sleep_sem_.Post();

  if (thread_) {
    thread_->Join();
    delete thread_;
    thread_ = nullptr;
  }
}

RequestHandle Scheduler::Schedule(callback::Callback* callback,
                                   ScheduleTimeMs delay /* = 0 */,
                                   ScheduleTimeMs repeat /* = 0 */) {
  assert(callback);

  MutexLock lock(request_mutex_);

  if (!thread_ && !terminating_) {
    thread_ = new Thread(WorkerThreadRoutine, this);
  }

  RequestDataPtr request(
      new RequestData(++next_request_id_, callback, delay, repeat));

  RequestHandle handler(request->status);

  AddToQueue(Move(request), internal::GetTimestamp(), delay);

  // Increase semaphore count by one for unfinished request
  sleep_sem_.Post();

  return handler;
}

#ifdef FIREBASE_USE_STD_FUNCTION
RequestHandle Scheduler::Schedule(const std::function<void(void)>& callback,
                                   ScheduleTimeMs delay /* = 0 */,
                                   ScheduleTimeMs repeat /* = 0 */) {
  return Schedule(new callback::CallbackStdFunction(callback), delay, repeat);
}
#endif

void Scheduler::WorkerThreadRoutine(void* data) {
  Scheduler* scheduler = static_cast<Scheduler*>(data);
  assert(scheduler);

  while (true) {
    uint64_t current = internal::GetTimestamp();

    uint64_t sleep_time = 0;
    RequestDataPtr request;

    // Check if the top request in the queue is due.
    {
      MutexLock lock(scheduler->request_mutex_);
      if (!scheduler->request_queue_.empty()) {
        auto due = scheduler->request_queue_.top()->due_timestamp;
        if (due <= current) {
          request = Move(scheduler->request_queue_.top());
          scheduler->request_queue_.pop();
        } else {
          sleep_time = due - current;
        }
      }
    }

    // If there is no request to process now, there can be 2 cases
    // 1. The queue is empty -> Wait forever
    // 2. The top request in the queue is not due yet.
    if (!request) {
      if (sleep_time > 0) {
        scheduler->sleep_sem_.TimedWait(sleep_time);
      } else {
        scheduler->sleep_sem_.Wait();
      }

      // Drain the semaphore after wake
      while (scheduler->sleep_sem_.TryWait()) {}

      // Check if the scheduler is terminating after sleep.
      MutexLock lock(scheduler->request_mutex_);
      if (scheduler->terminating_) {
        return;
      }
    }

    // If the top request is due, trigger the callback.  If the repeat interval
    // is non-zero, move it back to queue.
    if (request && scheduler->TriggerCallback(request)) {
      MutexLock lock(scheduler->request_mutex_);
      ScheduleTimeMs repeat = request->repeat_ms;
      scheduler->AddToQueue(Move(request), current, repeat);
    }
  }
}

void Scheduler::AddToQueue(RequestDataPtr request,
                           uint64_t current, ScheduleTimeMs after) {
  // Calculate the future timestamp
  request->due_timestamp = current + after;

  // Push the request to the priority queue
  request_queue_.push(Move(request));
}

bool Scheduler::TriggerCallback(const RequestDataPtr& request) {
  MutexLock lock(request->status->mutex);
  if (request->cb && !request->status->cancelled) {
    request->cb->Run();
    request->status->triggered = true;

    // return true if this callback repeats and should be push back to the queue
    if (request->repeat_ms > 0) {
      return true;
    }
  }

  return false;
}

}  // namespace scheduler
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace FIREBASE_NAMESPACE
