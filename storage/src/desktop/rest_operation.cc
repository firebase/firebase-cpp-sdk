// Copyright 2018 Google LLC

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

class RestOperation::NotifyScope {
 public:
  NotifyScope() = default;
  explicit NotifyScope(std::shared_ptr<NotifyState> state) {
    Enter(std::move(state));
  }
  ~NotifyScope() {
    if (!active_) return;
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it = std::find(state_->threads.begin(), state_->threads.end(), id_);
    if (it != state_->threads.end()) state_->threads.erase(it);
    state_->cv.notify_all();
  }

  NotifyScope(const NotifyScope&) = delete;
  NotifyScope& operator=(const NotifyScope&) = delete;

  bool Enter(std::shared_ptr<NotifyState> state) {
    if (active_ || !state) return active_;
    state_ = std::move(state);
    id_ = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (!state_->shutting_down) {
      state_->threads.push_back(id_);
      active_ = true;
    }
    return active_;
  }
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
  std::unique_ptr<rest::Request> req_holder(request);
  std::unique_ptr<BlockingResponse> resp_holder(response);
  if (!storage_internal) {
    if (controller_out) *controller_out = Controller();
    return;
  }
  auto listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  new RestOperation(storage_internal, storage_reference, std::move(req_holder),
                    request_notifier, std::move(resp_holder), listener,
                    listener_state, handle, controller_out);
}

void RestOperation::Start(
    StorageInternal* storage_internal,
    const StorageReference& storage_reference, rest::Request* request,
    Notifier* request_notifier, BlockingResponse* response,
    const std::shared_ptr<ListenerSharedState>& listener_state,
    FutureHandle handle, Controller* controller_out) {
  std::unique_ptr<rest::Request> req_holder(request);
  std::unique_ptr<BlockingResponse> resp_holder(response);
  if (!storage_internal) {
    if (controller_out) *controller_out = Controller();
    return;
  }
  new RestOperation(storage_internal, storage_reference, std::move(req_holder),
                    request_notifier, std::move(resp_holder), nullptr,
                    listener_state, handle, controller_out);
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

  if (!controller_.internal_) controller_.internal_ = new ControllerInternal();
  controller_.internal_->Initialize(storage_reference, this);
  listener_controller_ = std::make_shared<storage::Controller>(controller_);

  if (listener) {
    set_listener(listener);
  } else if (listener_state) {
    MutexLock lock(mutex_);
    std::lock_guard<std::mutex> state_lock(listener_state->mutex);
    if (listener_state->listener && listener_state->listener_internal) {
      listener_internal_ = listener_state->listener_internal;
      listener_state_ = listener_state;
      listener_state->rest_operation = this;
      listener_state->op_handle = op_handle_;
      listener_state->bytes_transferred = -1;
      listener_state->total_byte_count = -1;
    }
  }

  operation_id_ = storage_internal_->AddOperation(this);

  if (response_) {
    response_->set_update_callback(
        [](Notifier::UpdateCallbackType update_type, void* data) {
          auto* op = static_cast<RestOperation*>(data);
          if (update_type == Notifier::kUpdateCallbackTypeProgress) {
            op->NotifyListenerOfProgress();
          } else {
            op->is_complete_.store(true);
          }
        },
        this);
  }
  if (request_notifier_) {
    request_notifier_->set_update_callback(
        [](Notifier::UpdateCallbackType update_type, void* data) {
          if (update_type == Notifier::kUpdateCallbackTypeProgress) {
            static_cast<RestOperation*>(data)->NotifyListenerOfProgress();
          }
        },
        this);
  }

  if (controller_out) *controller_out = controller_;
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
  if (response_) response_->set_update_callback(nullptr, nullptr);
  if (request_notifier_) request_notifier_->set_update_callback(nullptr, nullptr);
  Cancel();

  {
    std::unique_lock<std::mutex> lock(notify_state->mutex);
    const auto self_id = std::this_thread::get_id();
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
  {
    std::shared_ptr<ListenerSharedState> state;
    {
      MutexLock lock(mutex_);
      listener_internal_ = nullptr;
      state = std::move(listener_state_);
    }
    if (state) {
      std::lock_guard<std::mutex> state_lock(state->mutex);
      if (state->rest_operation == this) {
        state->rest_operation = nullptr;
        state->op_handle.reset();
      }
    }
  }
  if (storage_internal_) storage_internal_->RemoveOperation(this);
}

bool RestOperation::Pause() {
  NotifyScope scope;
  bool paused = false;
  std::shared_ptr<ListenerSharedState> state;
  std::shared_ptr<storage::Controller> controller;
  {
    MutexLock lock(mutex_);
    paused = rest_controller_ ? rest_controller_->Pause() : false;
    if (paused && listener_state_ && scope.Enter(notify_state_)) {
      state = listener_state_;
      controller = listener_controller_;
    }
  }
  if (scope.active()) {
    ListenerInternal::NotifyPaused(std::move(state), controller.get());
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
  NotifyScope scope;
  std::shared_ptr<ListenerSharedState> state;
  std::shared_ptr<storage::Controller> controller;
  std::shared_ptr<OperationHandle> handle;
  std::shared_ptr<std::atomic<bool>> pending;
  StorageInternal* storage_internal = nullptr;
  {
    MutexLock lock(mutex_);
    if (perform_thread_ == std::this_thread::get_id() || !listener_state_ ||
        !storage_internal_ || !scope.Enter(notify_state_)) {
      return;
    }
    state = listener_state_;
    controller = listener_controller_;
    handle = op_handle_;
    pending = progress_pending_;
    storage_internal = storage_internal_;
  }
  if (pending->exchange(true)) return;
  const bool posted = storage_internal->listener_dispatcher().Post(
      [state = std::move(state), controller = std::move(controller),
       handle = std::move(handle), pending]() {
        pending->store(false);
        ListenerInternal::NotifyProgress(state, controller.get(), handle.get());
      });
  if (!posted) pending->store(false);
}

void RestOperation::set_listener(Listener* listener) {
  MutexLock lifecycle_lock(listener_mutex_);
  ListenerInternal* new_impl = listener ? listener->impl_ : nullptr;
  auto new_state = new_impl ? new_impl->shared_state() : nullptr;
  ListenerInternal* old_impl = nullptr;
  std::shared_ptr<ListenerSharedState> old_state;
  {
    MutexLock lock(mutex_);
    if (listener_internal_ == new_impl) return;
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
  if (new_impl) new_impl->set_rest_operation(this);
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
