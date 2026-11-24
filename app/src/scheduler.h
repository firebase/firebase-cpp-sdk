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

#ifndef FIREBASE_APP_SRC_SCHEDULER_H_
#define FIREBASE_APP_SRC_SCHEDULER_H_

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>

#include "app/src/callback.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/semaphore.h"
#include "app/src/thread.h"
#include "firebase/internal/common.h"

namespace firebase {
namespace scheduler {

typedef uint64_t ScheduleTimeMs;

// RequestStatusBlock contains the status of a request.  References to this
// block are shared by the queued request and the request handle.  The contents
// of this structure are potentially modified from different thread, hence
// guarded by mutex.
struct RequestStatusBlock {
  explicit RequestStatusBlock(bool repeat)
      : mutex(Mutex::kModeNonRecursive),
        cancelled(false),
        triggered(false),
        repeat(repeat) {}

  // Guard both "cancelled" and "triggered"
  Mutex mutex;

  // Whether the callback is properly cancelled
  bool cancelled;

  // Whether the callback has been triggered.
  bool triggered;

  // Whether this callback will repeat itself again after first trigger.
  const bool repeat;
};

// The handle used to check the status of a scheduled task or to cancel it.
// This handle is safe to be copied or be moved. However, it is NOT safe to
// modify or reference the same handle from different threads.
class RequestHandle {
 public:
  RequestHandle() : status_() {}
  explicit RequestHandle(const std::shared_ptr<RequestStatusBlock>& status)
      : status_(status) {}

  // Attempt to cancel the scheduled task.  return true if success or false if
  // it is cancelled or complete already.
  bool Cancel();

  // Return true if the handler is pointing to a request
  bool IsValid() const { return (status_ != nullptr); }

  // Thread-safe call to check if the scheduled callback has been cancelled.
  bool IsCancelled() const;

  // Thread-safe call to check if the scheduled callback has been triggered
  bool IsTriggered() const;

 private:
  std::shared_ptr<RequestStatusBlock> status_;
};

// Scheduler can be used to trigger a callback from the same worker thread.
// Currently it supports to trigger a callback ASAP using Execute() or with
// a delay using Schedule().
// All the public functions are safe to be called from different thread.
class Scheduler {
 public:
  Scheduler();

  // When a scheduler is deleted, all the future callback will be discarded.
  // The scheduler does not guarantee to trigger any callback scheduled before
  // the deletion or any potentially due callback. Safe to destroy even from
  // within a callback executing on the worker thread itself.
  ~Scheduler();

  // Scheduler is neither copyable nor movable.
  Scheduler(const Scheduler&) = delete;
  Scheduler(Scheduler&&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  Scheduler& operator=(Scheduler&&) = delete;

  // Schedule a callback to be triggered with a delay and optional
  // repeat interval.
  RequestHandle Schedule(callback::Callback* callback, ScheduleTimeMs delay = 0,
                         ScheduleTimeMs repeat = 0);

#ifdef FIREBASE_USE_STD_FUNCTION
  // std::function version of Schedule(callback, delay, repeat)
  RequestHandle Schedule(const std::function<void(void)>& callback,
                         ScheduleTimeMs delay = 0, ScheduleTimeMs repeat = 0);
#endif  // FIREBASE_USE_STD_FUNCTION

  // Cancel all scheduled callbacks and shut down the worker thread.
  void CancelAllAndShutdownWorkerThread();

 private:
  typedef uint64_t RequestId;
  // The request data for all scheduled callback.
  struct RequestData {
    explicit RequestData(RequestId id, callback::Callback* cb,
                         ScheduleTimeMs delay, ScheduleTimeMs repeat);

    // Unique id per scheduler.
    RequestId id;

    // The callback to be triggered.
    std::shared_ptr<callback::Callback> cb;

    // Delay to trigger in milliseconds
    ScheduleTimeMs delay_ms;

    // Repeat interval after first trigger.  Will not repeat if value is 0
    ScheduleTimeMs repeat_ms;

    // The timestamp after the delay in milliseconds.  Used for priority_queue
    uint64_t due_timestamp;

    // Status block shared with handlers
    std::shared_ptr<RequestStatusBlock> status;
  };

  // Request data.
  typedef std::unique_ptr<RequestData> RequestDataPtr;

  struct RequestDataPtrComparer {
    bool operator()(const RequestDataPtr& lhs,
                    const RequestDataPtr& rhs) const {
      return lhs->due_timestamp > rhs->due_timestamp ||
             (lhs->due_timestamp == rhs->due_timestamp && lhs->id > rhs->id);
    }
  };

  // Shared state kept alive by both the Scheduler and its running worker
  // thread, allowing safe detachment if a callback destroys the Scheduler on
  // the worker thread itself.
  struct Impl {
    Impl()
        : next_request_id_(0),
          terminating_(false),
          worker_thread_id_set_(false),
          request_mutex_(Mutex::kModeRecursive),
          sleep_sem_(0) {}

    RequestId next_request_id_;
    bool terminating_;
    Thread::Id worker_thread_id_;
    bool worker_thread_id_set_;
    std::priority_queue<RequestDataPtr, std::vector<RequestDataPtr>,
                        RequestDataPtrComparer>
        request_queue_;
    Mutex request_mutex_;
    Semaphore sleep_sem_;

    void AddToQueue(RequestDataPtr request, uint64_t current,
                    ScheduleTimeMs after);
    bool TriggerCallback(const RequestDataPtr& request);
  };

  // The worker thread to process scheduled callbacks.
  Thread* thread_;

  // Shared worker state.
  std::shared_ptr<Impl> impl_;

  // The main worker thread routine.
  static void WorkerThreadRoutine(void* data);
};

}  // namespace scheduler
// NOLINTNEXTLINE - allow namespace overridden
}  // namespace firebase

#endif  // FIREBASE_APP_SRC_SCHEDULER_H_
