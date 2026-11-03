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
#include <climits>
#include <cstdint>
#include <utility>

#include "app/src/time.h"

namespace firebase {
namespace scheduler {

namespace {
// The Scheduler whose worker routine runs on the current thread, or null. A
// Scheduler destroyed from one of its own callbacks clears this, telling the
// worker routine to exit without touching the freed Scheduler.
thread_local Scheduler* t_worker_scheduler = nullptr;
}  // namespace

bool RequestHandle::Cancel() {
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
  if (!IsValid()) {
    return false;
  }
  MutexLock lock(status_->mutex);
  return status_->cancelled;
}

bool RequestHandle::IsTriggered() const {
  if (!IsValid()) {
    return false;
  }
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

Scheduler::~Scheduler() { CancelAllAndShutdownWorkerThread(); }

void Scheduler::CancelAllAndShutdownWorkerThread() {
  Thread* thread_to_join = nullptr;
  {
    // Notify the worker thread to stop processing anymore requests.
    MutexLock lock(request_mutex_);
    if (terminating_) return;
    terminating_ = true;
    while (!request_queue_.empty()) {
      RequestDataPtr request =
          std::move(const_cast<RequestDataPtr&>(request_queue_.top()));
      request_queue_.pop();
      if (request && request->status) {
        MutexLock status_lock(request->status->mutex);
        request->status->cancelled = true;
      }
    }
    thread_to_join = thread_;
    thread_ = nullptr;
  }

  // Signal the thread to wake if it is sleeping due to no callbacks in queue
  sleep_sem_.Post();

  if (thread_to_join) {
    if (t_worker_scheduler == this) {
      // Called from one of our own callbacks: joining would deadlock, so let
      // the worker routine exit on its own once the callback returns.
      t_worker_scheduler = nullptr;
      thread_to_join->Detach();
    } else {
      thread_to_join->Join();
    }
    delete thread_to_join;
  }
}

RequestHandle Scheduler::Schedule(callback::Callback* callback,
                                  ScheduleTimeMs delay /* = 0 */,
                                  ScheduleTimeMs repeat /* = 0 */) {
  if (!callback) {
    return RequestHandle();
  }

  MutexLock lock(request_mutex_);
  if (terminating_) {
    delete callback;
    return RequestHandle();
  }

  if (!thread_) {
    thread_ = new Thread(WorkerThreadRoutine, this);
  }

  RequestDataPtr request(
      new RequestData(++next_request_id_, callback, delay, repeat));

  RequestHandle handler(request->status);

  AddToQueue(std::move(request), internal::GetTimestamp(), delay);

  // Increase semaphore count by one for unfinished request
  sleep_sem_.Post();

  return handler;
}

#ifdef FIREBASE_USE_STD_FUNCTION
RequestHandle Scheduler::Schedule(const std::function<void(void)>& callback,
                                  ScheduleTimeMs delay /* = 0 */,
                                  ScheduleTimeMs repeat /* = 0 */) {
  if (!callback) {
    return RequestHandle();
  }
  return Schedule(new callback::CallbackStdFunction(callback), delay, repeat);
}
#endif

void Scheduler::WorkerThreadRoutine(void* data) {
  Scheduler* scheduler = static_cast<Scheduler*>(data);
  assert(scheduler);
  t_worker_scheduler = scheduler;

  while (true) {
    uint64_t current = internal::GetTimestamp();

    uint64_t sleep_time = 0;
    RequestDataPtr request;

    // Check if the top request in the queue is due.
    {
      MutexLock lock(scheduler->request_mutex_);
      if (scheduler->terminating_) {
        return;
      }
      if (!scheduler->request_queue_.empty()) {
        auto due = scheduler->request_queue_.top()->due_timestamp;
        if (due <= current) {
          // We do a const_cast below because priority_queue.top() returns a
          // const reference, but we need to std::move the contents because it's
          // a unique_ptr which cannot be copied. Fortunately, this operation is
          // safe because we immediately pop that same element.
          //
          // More information here:
          // https://stackoverflow.com/questions/20149471/move-out-element-of-std-priority-queue-in-c11
          request = std::move(
              const_cast<RequestDataPtr&>(scheduler->request_queue_.top()));
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
        int wait_ms = sleep_time > static_cast<uint64_t>(INT_MAX)
                          ? INT_MAX
                          : static_cast<int>(sleep_time);
        scheduler->sleep_sem_.TimedWait(wait_ms);
      } else {
        scheduler->sleep_sem_.Wait();
      }

      // Drain the semaphore after wake
      while (scheduler->sleep_sem_.TryWait()) {
      }

      // Check if the scheduler is terminating after sleep.
      MutexLock lock(scheduler->request_mutex_);
      if (scheduler->terminating_) {
        return;
      }
    }

    // If the top request is due, trigger the callback.  If the repeat interval
    // is non-zero, move it back to queue.
    const bool repeat = request && scheduler->TriggerCallback(request);
    if (t_worker_scheduler != scheduler) {
      return;  // The callback destroyed the scheduler.
    }
    if (repeat) {
      MutexLock lock(scheduler->request_mutex_);
      if (!scheduler->terminating_) {
        ScheduleTimeMs repeat = request->repeat_ms;
        request->id = ++scheduler->next_request_id_;
        scheduler->AddToQueue(std::move(request), internal::GetTimestamp(),
                              repeat);
        scheduler->sleep_sem_.Post();
      }
    }
  }
}

void Scheduler::AddToQueue(RequestDataPtr request, uint64_t current,
                           ScheduleTimeMs after) {
  // Calculate the future timestamp, clamping on unsigned overflow.
  request->due_timestamp =
      (UINT64_MAX - current < after) ? UINT64_MAX : (current + after);

  // Push the request to the priority queue
  request_queue_.push(std::move(request));
}

bool Scheduler::TriggerCallback(const RequestDataPtr& request) {
  if (!request->cb) {
    return false;
  }
  {
    MutexLock lock(request->status->mutex);
    if (request->status->cancelled) {
      return false;
    }
    request->status->triggered = true;
  }

  request->cb->Run();

  {
    MutexLock lock(request->status->mutex);
    return request->repeat_ms > 0 && !request->status->cancelled;
  }
}

}  // namespace scheduler
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
