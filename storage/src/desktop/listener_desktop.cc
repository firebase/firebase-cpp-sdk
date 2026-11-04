// Copyright 2018 Google LLC

#include "storage/src/desktop/listener_desktop.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase::storage::internal {
namespace {

class CallbackThreadScope {
 public:
  CallbackThreadScope(std::shared_ptr<ListenerSharedState> state,
                      std::thread::id id)
      : state_(std::move(state)), id_(id) {}
  ~CallbackThreadScope() {
    std::lock_guard<std::mutex> lock(state_->mutex);
    auto it = std::find(state_->callback_threads.begin(),
                        state_->callback_threads.end(), id_);
    if (it != state_->callback_threads.end()) {
      state_->callback_threads.erase(it);
    }
    state_->cv.notify_all();
  }

 private:
  std::shared_ptr<ListenerSharedState> state_;
  std::thread::id id_;
};

}  // namespace

ListenerInternal::ListenerInternal(Listener* listener)
    : state_(std::make_shared<SharedState>()) {
  state_->listener = listener;
  state_->listener_internal = this;
}

ListenerInternal::~ListenerInternal() { Detach(); }

void ListenerInternal::Detach() {
  std::shared_ptr<SharedState> state = state_;
  RestOperation* old_op = nullptr;
  std::shared_ptr<RestOperation::OperationHandle> old_handle;
  {
    std::unique_lock<std::mutex> lock(state->mutex);
    state->listener = nullptr;
    state->listener_internal = nullptr;
    old_op = state->rest_operation;
    old_handle = std::move(state->op_handle);
    state->rest_operation = nullptr;
    const auto self_id = std::this_thread::get_id();
    state->cv.wait(lock, [&state, self_id]() {
      return std::all_of(state->callback_threads.begin(),
                         state->callback_threads.end(),
                         [self_id](const auto& id) { return id == self_id; });
    });
  }
  if (old_op && old_handle) {
    std::lock_guard<std::recursive_mutex> op_lock(old_handle->mutex);
    if (old_handle->op == old_op) old_op->DetachListener(this);
  }
}

void ListenerInternal::NotifyProgress(Controller* controller) {
  NotifyProgress(std::shared_ptr<SharedState>(state_), controller);
}

void ListenerInternal::NotifyProgress(
    std::shared_ptr<SharedState> state, Controller* controller,
    const RestOperation::OperationHandle* expected_handle) {
  if (!state || !controller) return;
  if (expected_handle != nullptr) {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->op_handle.get() != expected_handle) return;
  }
  const int64_t new_bytes = controller->bytes_transferred();
  const int64_t new_total = controller->total_byte_count();

  Listener* target = nullptr;
  const auto self_id = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->listener ||
        (expected_handle && state->op_handle.get() != expected_handle)) {
      return;
    }
    if (new_bytes >= state->bytes_transferred &&
        (new_bytes > state->bytes_transferred ||
         new_total != state->total_byte_count)) {
      state->bytes_transferred = new_bytes;
      state->total_byte_count = new_total;
      target = state->listener;
      state->callback_threads.push_back(self_id);
    }
  }
  if (target) {
    CallbackThreadScope scope(state, self_id);
    target->OnProgress(controller);
  }
}

void ListenerInternal::NotifyPaused(Controller* controller) {
  NotifyPaused(std::shared_ptr<SharedState>(state_), controller);
}

void ListenerInternal::NotifyPaused(std::shared_ptr<SharedState> state,
                                    Controller* controller) {
  if (!state || !controller) return;
  Listener* target = nullptr;
  const auto self_id = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->listener) return;
    target = state->listener;
    state->callback_threads.push_back(self_id);
  }
  CallbackThreadScope scope(state, self_id);
  target->OnPaused(controller);
}

void ListenerInternal::set_rest_operation(RestOperation* operation) {
  std::shared_ptr<SharedState> state = state_;
  RestOperation* prev_op = nullptr;
  std::shared_ptr<RestOperation::OperationHandle> prev_handle;
  auto new_handle = operation ? operation->operation_handle() : nullptr;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->listener && operation) return;
    prev_op = state->rest_operation;
    prev_handle = std::move(state->op_handle);
    state->rest_operation = operation;
    state->op_handle = new_handle;
    if (operation) {
      state->bytes_transferred = -1;
      state->total_byte_count = -1;
    }
  }
  if (prev_op && prev_op != operation && prev_handle) {
    std::lock_guard<std::recursive_mutex> op_lock(prev_handle->mutex);
    if (prev_handle->op == prev_op) prev_op->DetachListener(this);
  }
  if (operation && new_handle) {
    std::lock_guard<std::recursive_mutex> op_lock(new_handle->mutex);
    if (new_handle->op == operation) {
      bool should_register = false;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        should_register =
            (state->rest_operation == operation && state->listener != nullptr);
      }
      if (should_register) operation->AttachListenerState(this, state);
    }
  }
}

}  // namespace firebase::storage::internal
