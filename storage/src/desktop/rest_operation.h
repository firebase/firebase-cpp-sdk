// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_REST_OPERATION_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_REST_OPERATION_H_

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/rest/controller_interface.h"
#include "app/rest/request.h"
#include "app/rest/transport_curl.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

class BlockingResponse;
class ListenerInternal;
struct ListenerSharedState;
class Notifier;
class StorageInternal;

// Structure containing the data we need to keep track of (and later clean up)
// when we spin up a new async request.
class RestOperation {
 public:
  // Shared lifetime handle used by ControllerInternal and ListenerInternal to
  // synchronize access to RestOperation* across threads and avoid
  // use-after-free when RestOperation is destroyed.
  struct OperationHandle {
    std::recursive_mutex mutex;
    RestOperation* op = nullptr;
    int64_t final_bytes_transferred = 0;
    int64_t final_total_byte_count = -1;
  };

  ~RestOperation();

  RestOperation(const RestOperation&) = delete;
  RestOperation& operator=(const RestOperation&) = delete;
  RestOperation(RestOperation&&) = delete;
  RestOperation& operator=(RestOperation&&) = delete;

  // Takes ownership of request and response, copies storage_reference, and
  // records non-owning references to storage_internal, request_notifier (owned
  // by request), and listener.
  // If storage_internal or listener are destroyed they call this object to
  // remove references to them. storage_internal owns the lifetime of the
  // object created by this method through its operations list and cleanup
  // notifier. If provided, controller_out is populated with the controller used
  // to manage the rest call.
  // If storage_internal is null nothing is started: request and response are
  // deleted (which fails the response's future with kErrorCancelled) and
  // controller_out, if provided, is reset to an invalid Controller.
  static void Start(StorageInternal* storage_internal,
                    const StorageReference& storage_reference,
                    rest::Request* request, Notifier* request_notifier,
                    BlockingResponse* response, Listener* listener,
                    FutureHandle handle, Controller* controller_out);

  static void Start(StorageInternal* storage_internal,
                    const StorageReference& storage_reference,
                    rest::Request* request, Notifier* request_notifier,
                    BlockingResponse* response,
                    const std::shared_ptr<ListenerSharedState>& listener_state,
                    FutureHandle handle, Controller* controller_out);

  // Get the notifier used to cleanup this object.
  CleanupNotifier& cleanup() { return cleanup_; }

  // Shared handle guarding the lifetime of this RestOperation.
  const std::shared_ptr<OperationHandle>& operation_handle() const {
    return op_handle_;
  }

  // Pauses the operation currently in progress.
  bool Pause();
  // Resumes the operation that is paused.
  bool Resume();
  // Cancels the operation currently in progress.
  bool Cancel();
  // Returns true if the operation is paused.
  bool is_paused() const;
  // Returns the number of bytes transferred so far.
  int64_t bytes_transferred() const;
  // Returns the total bytes to be transferred.
  int64_t total_byte_count() const;

  // Set the listener for this operation.
  void set_listener(Listener* listener);

  // Attach a listener via its shared state (used by set_rest_operation and
  // retry re-attachment).
  void AttachListenerState(
      ListenerInternal* listener_internal,
      const std::shared_ptr<ListenerSharedState>& listener_state);

  // Detach a specific ListenerInternal without locking ListenerInternal again.
  void DetachListener(ListenerInternal* listener_internal);

  // Whether this operation is complete and can be deleted.
  bool is_complete() const;

 private:
  // See Start().
  RestOperation(StorageInternal* storage_internal,
                const StorageReference& storage_reference,
                rest::Request* request, Notifier* request_notifier,
                BlockingResponse* response, Listener* listener,
                const std::shared_ptr<ListenerSharedState>& listener_state,
                FutureHandle handle, Controller* controller_out);

  // Notify the listener of progress.
  void NotifyListenerOfProgress();

  std::shared_ptr<OperationHandle> op_handle_ =
      std::make_shared<OperationHandle>();
  StorageInternal* storage_internal_ = nullptr;
  std::unique_ptr<rest::Request> request_;
  // Non-owning pointer to the Notifier embedded inside request_.
  Notifier* request_notifier_ = nullptr;
  std::unique_ptr<BlockingResponse> response_;
  // Serializes set_listener calls without holding mutex_ across Listener locks.
  Mutex listener_mutex_;
  // Guards rest_controller_ and listener_state_ / notification state.
  mutable Mutex mutex_;
  // Bookkeeping for in-flight listener notifications. Heap allocated and
  // shared so that a notifying thread can finish its bookkeeping through a
  // stack copy of this pointer even if the user callback destroyed `this`.
  struct NotifyState {
    std::mutex mutex;
    std::condition_variable cv;
    // Threads currently inside a listener callback for this operation.
    std::vector<std::thread::id> threads;
    // Set (under mutex) once ~RestOperation() starts; no new notifications
    // are dispatched afterwards.
    bool shutting_down = false;
  };
  // RAII helper that registers the calling thread in NotifyState::threads and
  // removes it (and wakes ~RestOperation()) on scope exit.
  class NotifyScope;
  std::shared_ptr<NotifyState> notify_state_ = std::make_shared<NotifyState>();
  ListenerInternal* listener_internal_ = nullptr;
  std::shared_ptr<ListenerSharedState> listener_state_;
  uint64_t operation_id_ = 0;
  FutureHandle handle_;
  CleanupNotifier cleanup_;
  rest::TransportCurl transport_;
  // Written by transport_.Perform() in the constructor while mutex_ is held,
  // before the transfer is posted to the I/O thread; guarded by mutex_.
  std::unique_ptr<rest::Controller> rest_controller_;
  // Thread currently inside transport_.Perform() in the constructor (holding
  // mutex_), or a default id. Guarded by mutex_.
  std::thread::id perform_thread_;
  // Storage controller that delegates to this object.
  storage::Controller controller_;
  std::atomic<bool> is_complete_{false};
  std::atomic<bool> is_started_{false};
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_REST_OPERATION_H_
