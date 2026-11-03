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
    const std::thread::id self_id = std::this_thread::get_id();
    state->cv.wait(lock, [&state, self_id]() {
      return std::all_of(
          state->callback_threads.begin(), state->callback_threads.end(),
          [self_id](const std::thread::id& id) { return id == self_id; });
    });
  }
  if (old_op && old_handle) {
    std::lock_guard<std::recursive_mutex> op_lock(old_handle->mutex);
    if (old_handle->op == old_op) {
      old_op->cleanup().UnregisterObject(this);
      old_op->DetachListener(this);
    }
  }
}

namespace {

// Removes the calling thread from `state->callback_threads` and wakes any
// thread blocked in ListenerInternal::Detach() when it goes out of scope, even
// if the user callback throws.
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

  CallbackThreadScope(const CallbackThreadScope&) = delete;
  CallbackThreadScope& operator=(const CallbackThreadScope&) = delete;

 private:
  std::shared_ptr<ListenerSharedState> state_;
  std::thread::id id_;
};

}  // namespace

void ListenerInternal::NotifyProgress(Controller* controller) {
  // Copy state_ so the callee owns a reference independent of `this`.
  NotifyProgress(std::shared_ptr<SharedState>(state_), controller);
}

void ListenerInternal::NotifyProgress(std::shared_ptr<SharedState> state,
                                      Controller* controller) {
  if (!state || !controller) {
    return;
  }
  Listener* target = nullptr;
  const std::thread::id self_id = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->listener) {
      return;
    }
    const int64_t new_bytes_transferred = controller->bytes_transferred();
    const int64_t new_total_byte_count = controller->total_byte_count();
    if (new_bytes_transferred > state->bytes_transferred ||
        (state->bytes_transferred < 0 && new_bytes_transferred >= 0) ||
        new_total_byte_count != state->total_byte_count) {
      if (new_bytes_transferred >= state->bytes_transferred) {
        state->bytes_transferred = new_bytes_transferred;
      }
      state->total_byte_count = new_total_byte_count;
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
  // Copy state_ so the callee owns a reference independent of `this`.
  NotifyPaused(std::shared_ptr<SharedState>(state_), controller);
}

void ListenerInternal::NotifyPaused(std::shared_ptr<SharedState> state,
                                    Controller* controller) {
  if (!state || !controller) {
    return;
  }
  Listener* target = nullptr;
  const std::thread::id self_id = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (!state->listener) {
      return;
    }
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
  std::shared_ptr<RestOperation::OperationHandle> new_handle =
      operation ? operation->operation_handle() : nullptr;
  {
    std::lock_guard<std::mutex> lock(state->mutex);
    if (state->listener == nullptr && operation != nullptr) {
      return;
    }
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
    if (prev_handle->op == prev_op) {
      prev_op->cleanup().UnregisterObject(this);
      prev_op->DetachListener(this);
    }
  }
  if (operation && new_handle) {
    std::lock_guard<std::recursive_mutex> op_lock(new_handle->mutex);
    if (new_handle->op == operation) {
      bool should_register = false;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->rest_operation == operation && state->listener != nullptr) {
          should_register = true;
        }
      }
      if (should_register) {
        operation->AttachListenerState(this, state);
        operation->cleanup().RegisterObjectFunction(
            this, [state, operation](void*) {
              std::lock_guard<std::mutex> lock(state->mutex);
              if (state->rest_operation == operation) {
                state->rest_operation = nullptr;
                state->op_handle.reset();
              }
            });
      }
    }
  }
}

}  // namespace firebase::storage::internal
