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

#include "storage/src/desktop/rest_operation.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/listener_desktop.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase::storage::internal {

// Note: RestOperation is an internal desktop-only implementation class within
// libfirebase_storage and is never exposed in the public SDK ABI.

namespace {

// Rejects a Start() call that has no StorageInternal to own the operation.
// Ownership of request and response is already held by the caller's
// std::unique_ptrs, which destroy them automatically on return (destroying an
// uncompleted BlockingResponse fails its future with kErrorCancelled).
bool RejectStartWithoutStorage(StorageInternal* storage_internal,
                               Controller* controller_out) {
  if (storage_internal != nullptr) {
    return false;
  }
  if (controller_out) {
    *controller_out = Controller();
  }
  return true;
}

}  // namespace

// RAII helper registering the current thread as an in-flight notifier. It only
// holds a shared_ptr to NotifyState, so its destructor never touches the
// RestOperation, which may have been destroyed by the user callback.
class RestOperation::NotifyScope {
 public:
  // Registers the calling thread unless the operation is shutting down;
  // check active() before invoking the callback.
  explicit NotifyScope(std::shared_ptr<NotifyState> state)
      : state_(std::move(state)), id_(std::this_thread::get_id()) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->shutting_down) {
      state_->threads.push_back(id_);
      active_ = true;
    }
  }
  ~NotifyScope() {
    if (!active_) {
      return;
    }
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it = std::find(state_->threads.begin(), state_->threads.end(), id_);
    if (it != state_->threads.end()) {
      state_->threads.erase(it);
    }
    state_->cv.notify_all();
  }

  NotifyScope(const NotifyScope&) = delete;
  NotifyScope& operator=(const NotifyScope&) = delete;

  bool active() const { return active_; }

 private:
  std::shared_ptr<NotifyState> state_;
  std::thread::id id_;
  bool active_ = false;
};

void RestOperation::Start(StorageInternal* storage_internal,
                          const StorageReference& storage_reference,
                          rest::Request* request, Notifier* request_notifier,
                          BlockingResponse* response, Listener* listener,
                          FutureHandle handle, Controller* controller_out) {
  std::unique_ptr<rest::Request> request_holder(request);
  std::unique_ptr<BlockingResponse> response_holder(response);
  if (RejectStartWithoutStorage(storage_internal, controller_out)) {
    return;
  }
  std::shared_ptr<ListenerSharedState> listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  // Ownership is transferred to storage_internal (operations list + cleanup
  // notifier), which deletes the operation once it completes or on shutdown.
  new RestOperation(storage_internal, storage_reference,
                    std::move(request_holder), request_notifier,
                    std::move(response_holder), listener, listener_state,
                    handle, controller_out);
}

void RestOperation::Start(
    StorageInternal* storage_internal,
    const StorageReference& storage_reference, rest::Request* request,
    Notifier* request_notifier, BlockingResponse* response,
    const std::shared_ptr<ListenerSharedState>& listener_state,
    FutureHandle handle, Controller* controller_out) {
  std::unique_ptr<rest::Request> request_holder(request);
  std::unique_ptr<BlockingResponse> response_holder(response);
  if (RejectStartWithoutStorage(storage_internal, controller_out)) {
    return;
  }
  // Ownership is transferred to storage_internal (see above).
  new RestOperation(storage_internal, storage_reference,
                    std::move(request_holder), request_notifier,
                    std::move(response_holder), nullptr, listener_state,
                    handle, controller_out);
}

RestOperation::RestOperation(
    StorageInternal* storage_internal,
    const StorageReference& storage_reference,
    std::unique_ptr<rest::Request> request, Notifier* request_notifier,
    std::unique_ptr<BlockingResponse> response, Listener* listener,
    const std::shared_ptr<ListenerSharedState>& listener_state,
    FutureHandle handle, Controller* controller_out)
    : storage_internal_(storage_internal),
      request_(std::move(request)),
      request_notifier_(request_notifier),
      response_(std::move(response)),
      handle_(handle) {
  op_handle_->op = this;
  transport_.set_is_async(true);

  if (!controller_.internal_) {
    controller_.internal_ = new ControllerInternal();
  }
  controller_.internal_->Initialize(storage_reference, this);
  listener_controller_ = std::make_shared<storage::Controller>(controller_);

  if (listener) {
    set_listener(listener);
  } else if (listener_state) {
    MutexLock lock(mutex_);
    std::lock_guard<std::mutex> state_lock(listener_state->mutex);
    if (listener_state->listener != nullptr &&
        listener_state->listener_internal != nullptr) {
      listener_internal_ = listener_state->listener_internal;
      listener_state_ = listener_state;
      listener_state->rest_operation = this;
      listener_state->op_handle = op_handle_;
      listener_state->bytes_transferred = -1;
      listener_state->total_byte_count = -1;
    }
  }
  cleanup_.RegisterObject(this, [](void* op_void) {
    auto* op = static_cast<RestOperation*>(op_void);
    std::shared_ptr<ListenerSharedState> state;
    {
      MutexLock lock(op->mutex_);
      op->listener_internal_ = nullptr;
      state = std::move(op->listener_state_);
    }
    if (state) {
      std::lock_guard<std::mutex> state_lock(state->mutex);
      if (state->rest_operation == op) {
        state->rest_operation = nullptr;
        state->op_handle.reset();
      }
    }
  });

  // Start() guarantees a non-null storage_internal_, which owns this object.
  // The cleanup callback identifies this operation by its unique id rather
  // than by address, so it can never delete a different operation that was
  // later allocated at the same address.
  {
    StorageInternal* owner = storage_internal_;
    operation_id_ = owner->AddOperation(this);
    owner->cleanup().RegisterObject(this, [](void* operation) {
      auto* op = static_cast<RestOperation*>(operation);
      if (op->storage_internal_) {
        op->storage_internal_->DeleteOperation(op, op->operation_id_);
      }
    });
  }

  if (response_) {
    response_->set_update_callback(
        [](Notifier::UpdateCallbackType update_type, void* data) {
          RestOperation* operation = reinterpret_cast<RestOperation*>(data);
          switch (update_type) {
            case Notifier::kUpdateCallbackTypeProgress:
              operation->NotifyListenerOfProgress();
              break;
            case Notifier::kUpdateCallbackTypeComplete:
            case Notifier::kUpdateCallbackTypeFailed:
              // Mark operation complete so
              // StorageInternal::CleanupCompletedOperations() can delete it
              // once construction has finished.
              operation->is_complete_.store(true);
              break;
          }
        },
        this);
  }
  if (request_notifier_) {
    request_notifier_->set_update_callback(
        [](Notifier::UpdateCallbackType update_type, void* data) {
          RestOperation* operation = reinterpret_cast<RestOperation*>(data);
          if (update_type == Notifier::kUpdateCallbackTypeProgress) {
            operation->NotifyListenerOfProgress();
          }
        },
        this);
  }

  if (controller_out) {
    *controller_out = controller_;
  }

  // Publish rest_controller_ atomically with starting the transfer, so that
  // Pause()/Resume()/Cancel()/bytes_transferred() work from the very first
  // progress callback. TransportCurl::PerformInternal() populates
  // *controller_out *before* it posts the transfer to the curl I/O thread, and
  // mutex_ is held until Perform() returns, so:
  //  * every progress callback on the I/O thread (which starts with
  //    NotifyListenerOfProgress() taking mutex_) and every Pause()/Resume()/
  //    Cancel()/byte-count query from another thread observes the controller;
  //  * nobody can read rest_controller_ while Perform() is writing it.
  // In the normal (async) path Perform() only allocates and posts the
  // transfer; it takes no lock but leaf locks inside the transport, so holding
  // mutex_ across it cannot invert any lock order. Perform() only runs
  // callbacks synchronously if the curl I/O loop has already stopped
  // (AbortWithoutLoop()), which cannot happen while storage_internal_ (which
  // holds a reference on the loop from its constructor until the very end of
  // its destructor, after every operation is gone) is alive. Even then, mutex_
  // is recursive, and NotifyListenerOfProgress() suppresses listener dispatch
  // on this thread while perform_thread_ is set, so no user callback ever runs
  // with mutex_ held.
  {
    MutexLock lock(mutex_);
    perform_thread_ = std::this_thread::get_id();
    transport_.Perform(request_.get(), response_.get(), &rest_controller_);
    perform_thread_ = std::thread::id();
  }
  is_started_.store(true);
}

RestOperation::~RestOperation() {
  std::shared_ptr<NotifyState> notify_state = notify_state_;
  {
    std::lock_guard<std::mutex> lock(notify_state->mutex);
    notify_state->shutting_down = true;
  }
  if (response_) {
    response_->set_update_callback(nullptr, nullptr);
  }
  if (request_notifier_) {
    request_notifier_->set_update_callback(nullptr, nullptr);
  }
  Cancel();

  // Wait for any in-flight progress/pause notification on other threads to
  // finish before destroying controller_.
  // A notification on this thread (i.e. the user callback destroyed this
  // operation) is not waited for; its NotifyScope only touches notify_state.
  {
    std::unique_lock<std::mutex> lock(notify_state->mutex);
    const std::thread::id self_id = std::this_thread::get_id();
    notify_state->cv.wait(lock, [&notify_state, self_id]() {
      return std::all_of(
          notify_state->threads.begin(), notify_state->threads.end(),
          [self_id](const auto& id) { return id == self_id; });
    });
  }

  {
    std::lock_guard<std::recursive_mutex> op_lock(op_handle_->mutex);
    op_handle_->final_bytes_transferred = bytes_transferred();
    op_handle_->final_total_byte_count = total_byte_count();
    op_handle_->op = nullptr;
  }

  set_listener(nullptr);
  cleanup_.CleanupAll();
  if (storage_internal_) {
    storage_internal_->cleanup().UnregisterObject(this);
    storage_internal_->RemoveOperation(this);
  }
}

bool RestOperation::Pause() {
  bool paused = false;
  std::shared_ptr<ListenerSharedState> state;
  {
    MutexLock lock(mutex_);
    paused = rest_controller_ ? rest_controller_->Pause() : false;
    state = listener_state_;
  }
  if (paused && state) {
    // The user callback may destroy this operation, so only stack-owned values
    // (scope, state, paused) are used once it has been invoked.
    NotifyScope scope(notify_state_);
    if (scope.active()) {
      ListenerInternal::NotifyPaused(std::move(state), &controller_);
    }
  }
  return paused;
}

bool RestOperation::Resume() {
  MutexLock lock(mutex_);
  return rest_controller_ ? rest_controller_->Resume() : false;
}

bool RestOperation::Cancel() {
  MutexLock lock(mutex_);
  return rest_controller_ ? rest_controller_->Cancel() : false;
}

bool RestOperation::is_paused() const {
  MutexLock lock(mutex_);
  return rest_controller_ ? rest_controller_->IsPaused() : false;
}

int64_t RestOperation::bytes_transferred() const {
  MutexLock lock(mutex_);
  return rest_controller_ ? rest_controller_->BytesTransferred() : 0;
}

int64_t RestOperation::total_byte_count() const {
  MutexLock lock(mutex_);
  return rest_controller_ ? rest_controller_->TransferSize() : -1;
}

bool RestOperation::is_complete() const {
  return is_started_.load() && is_complete_.load();
}

void RestOperation::NotifyListenerOfProgress() {
  // Runs on the curl I/O thread. The user's Listener is never invoked here:
  // a Listener may delete its Storage, App or this operation, and doing so on
  // the I/O thread would block it on itself (~TransportCurl() waits for
  // transfers that only the I/O thread can finish). Instead the notification
  // is posted to the StorageInternal's CallbackDispatcher, with shared copies
  // of everything it needs, so it never touches `this`.
  std::shared_ptr<ListenerSharedState> state;
  {
    MutexLock lock(mutex_);
    if (perform_thread_ == std::this_thread::get_id()) {
      // Synchronous callback from inside transport_.Perform() in the
      // constructor, which holds mutex_ (see there). Skip this progress
      // update; the final result is still reported through the response's
      // future.
      return;
    }
    state = listener_state_;
  }
  if (!state || storage_internal_ == nullptr) {
    return;
  }
  // ~RestOperation() waits for this scope, so storage_internal_ and the
  // members read below stay valid until it ends.
  NotifyScope scope(notify_state_);
  if (!scope.active()) {
    return;
  }
  if (progress_pending_->exchange(true)) {
    return;  // A queued notification will report the latest progress.
  }
  const bool posted = storage_internal_->listener_dispatcher().Post(
      [state = std::move(state), controller = listener_controller_,
       handle = op_handle_, pending = progress_pending_]() {
        // Cleared before sampling, so progress made from here on is
        // reported by a new notification.
        pending->store(false);
        // Dropped if the listener was detached from, or re-attached to
        // another operation than, this one in the meantime.
        ListenerInternal::NotifyProgress(state, controller.get(),
                                         handle.get());
      });
  if (!posted) {
    progress_pending_->store(false);
  }
}

void RestOperation::set_listener(Listener* listener) {
  MutexLock lifecycle_lock(listener_mutex_);
  ListenerInternal* new_impl = listener ? listener->impl_ : nullptr;
  std::shared_ptr<ListenerSharedState> new_state =
      new_impl ? new_impl->shared_state() : nullptr;
  ListenerInternal* old_impl = nullptr;
  std::shared_ptr<ListenerSharedState> old_state;
  {
    MutexLock lock(mutex_);
    if (listener_internal_ == new_impl) {
      return;
    }
    old_impl = listener_internal_;
    old_state = std::move(listener_state_);
    listener_internal_ = new_impl;
    listener_state_ = new_state;
  }
  if (old_state && old_impl) {
    std::lock_guard<std::mutex> state_lock(old_state->mutex);
    if (old_state->rest_operation == this) {
      old_state->rest_operation = nullptr;
      old_state->op_handle.reset();
    }
  }
  if (new_impl) {
    new_impl->set_rest_operation(this);
  }
}

void RestOperation::AttachListenerState(
    ListenerInternal* listener_internal,
    const std::shared_ptr<ListenerSharedState>& listener_state) {
  MutexLock lock(mutex_);
  listener_internal_ = listener_internal;
  listener_state_ = listener_state;
}

void RestOperation::DetachListener(ListenerInternal* listener_internal) {
  MutexLock lock(mutex_);
  if (listener_internal_ == listener_internal) {
    listener_internal_ = nullptr;
    listener_state_.reset();
  }
}

}  // namespace firebase::storage::internal
