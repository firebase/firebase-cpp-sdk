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
#include <cstdint>
#include <memory>
#include <utility>

#include "app/src/time.h"

namespace firebase {
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

Scheduler::Scheduler() : thread_(nullptr), impl_(std::make_shared<Impl>()) {}

Scheduler::~Scheduler() { CancelAllAndShutdownWorkerThread(); }

void Scheduler::CancelAllAndShutdownWorkerThread() {
  bool is_self = false;
  Thread* thread_to_finish = nullptr;
  {
    // Notify the worker thread to stop processing any more requests.
    MutexLock lock(impl_->request_mutex_);
    if (impl_->terminating_) return;
    impl_->terminating_ = true;
    is_self = impl_->worker_thread_id_set_ &&
              Thread::IsCurrentThread(impl_->worker_thread_id_);
    thread_to_finish = thread_;
    thread_ = nullptr;
  }

  // Signal the thread to wake if it is sleeping due to no callbacks in queue.
  impl_->sleep_sem_.Post();

  if (thread_to_finish) {
    if (is_self) {
      // A callback running on this worker thread triggered shutdown (e.g., by
      // dropping the last ref-counted handle). Detach rather than self-joining;
      // the worker thread retains its own shared_ptr<Impl> until it returns.
      thread_to_finish->Detach();
    } else {
      thread_to_finish->Join();
    }
    delete thread_to_finish;
  }
}

RequestHandle Scheduler::Schedule(callback::Callback* callback,
                                  ScheduleTimeMs delay /* = 0 */,
                                  ScheduleTimeMs repeat /* = 0 */) {
  assert(callback);

  MutexLock lock(impl_->request_mutex_);

  if (!thread_ && !impl_->terminating_) {
    auto* thread_arg = new std::shared_ptr<Impl>(impl_);
    thread_ = new Thread(WorkerThreadRoutine, thread_arg);
  }

  RequestDataPtr request(
      new RequestData(++impl_->next_request_id_, callback, delay, repeat));

  RequestHandle handler(request->status);

  impl_->AddToQueue(std::move(request), internal::GetTimestamp(), delay);

  // Increase semaphore count by one for unfinished request
  impl_->sleep_sem_.Post();

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
  std::unique_ptr<std::shared_ptr<Impl>> arg(
      static_cast<std::shared_ptr<Impl>*>(data));
  std::shared_ptr<Impl> impl = std::move(*arg);
  arg.reset();
  assert(impl);

  {
    MutexLock lock(impl->request_mutex_);
    impl->worker_thread_id_ = Thread::CurrentId();
    impl->worker_thread_id_set_ = true;
  }

  while (true) {
    uint64_t current = internal::GetTimestamp();

    uint64_t sleep_time = 0;
    RequestDataPtr request;

    // Check if the top request in the queue is due.
    {
      MutexLock lock(impl->request_mutex_);
      if (impl->terminating_) {
        return;
      }
      if (!impl->request_queue_.empty()) {
        auto due = impl->request_queue_.top()->due_timestamp;
        if (due <= current) {
          request = std::move(
              const_cast<RequestDataPtr&>(impl->request_queue_.top()));
          impl->request_queue_.pop();
        } else {
          sleep_time = due - current;
        }
      }
    }

    // If there is no request to process now, there can be 2 cases:
    // 1. The queue is empty -> Wait forever
    // 2. The top request in the queue is not due yet.
    if (!request) {
      if (sleep_time > 0) {
        impl->sleep_sem_.TimedWait(sleep_time);
      } else {
        impl->sleep_sem_.Wait();
      }

      // Drain the semaphore after wake
      while (impl->sleep_sem_.TryWait()) {
      }

      // Check if the scheduler is terminating after sleep.
      MutexLock lock(impl->request_mutex_);
      if (impl->terminating_) {
        return;
      }
    }

    // If the top request is due, trigger the callback.  If the repeat interval
    // is non-zero, move it back to queue.
    if (request && impl->TriggerCallback(request)) {
      MutexLock lock(impl->request_mutex_);
      if (impl->terminating_) {
        return;
      }
      ScheduleTimeMs repeat = request->repeat_ms;
      impl->AddToQueue(std::move(request), current, repeat);
    }
  }
}

void Scheduler::Impl::AddToQueue(RequestDataPtr request, uint64_t current,
                                 ScheduleTimeMs after) {
  // Calculate the future timestamp
  request->due_timestamp = current + after;

  // Push the request to the priority queue
  request_queue_.push(std::move(request));
}

bool Scheduler::Impl::TriggerCallback(const RequestDataPtr& request) {
  MutexLock lock(request->status->mutex);
  if (request->cb && !request->status->cancelled) {
    request->cb->Run();
    request->status->triggered = true;

    // return true if this callback repeats and should be pushed back to queue
    if (request->repeat_ms > 0) {
      return true;
    }
  }

  return false;
}

}  // namespace scheduler
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase
