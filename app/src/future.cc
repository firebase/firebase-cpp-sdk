/*
 * Copyright 2019 Google LLC
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

#include "app/src/include/firebase/future.h"

#include <atomic>
#include <memory>

#include "app/src/semaphore.h"

namespace firebase {

namespace {

struct WaitState {
  WaitState() : semaphore(0), completed(false), cleaned_up(false) {}
  Semaphore semaphore;
  std::atomic<bool> completed;
  std::atomic<bool> cleaned_up;
};

// Callback function installed by FutureBase::Wait().
// This will be called when the future is complete.
void WaitCallback(const FutureBase& /*future*/, void* user_data) {
  auto* state_ptr = static_cast<std::shared_ptr<WaitState>*>(user_data);
  if (state_ptr != nullptr && *state_ptr != nullptr) {
    (*state_ptr)->completed.store(true, std::memory_order_release);
    // Wake up the thread that called Wait().
    (*state_ptr)->semaphore.Post();
  }
}

void DeleteWaitState(void* user_data) {
  auto* state_ptr = static_cast<std::shared_ptr<WaitState>*>(user_data);
  if (state_ptr != nullptr) {
    if (*state_ptr != nullptr) {
      (*state_ptr)->cleaned_up.store(true, std::memory_order_release);
      // Always post on cleanup so a waiting thread wakes up even if the
      // callback was evicted/cleared (e.g. during API teardown) without
      // WaitCallback running.
      (*state_ptr)->semaphore.Post();
    }
    delete state_ptr;
  }
}

}  // namespace

const int FutureBase::kWaitTimeoutInfinite = -1;

bool FutureBase::Wait(int timeout_milliseconds) const {
  detail::FutureApiInterface* api = nullptr;
  FutureHandle handle;
  {
    MutexLock lock(mutex_);
    if (api_ == nullptr) {
      return false;
    }
    FutureStatus current_status = api_->GetFutureStatus(handle_);
    if (current_status == kFutureStatusInvalid) {
      return false;
    }
    if (current_status == kFutureStatusComplete) {
      return true;
    }
    api = api_;
    handle = handle_;
  }

  auto state = std::make_shared<WaitState>();
  auto* user_data = new std::shared_ptr<WaitState>(state);
  CompletionCallbackHandle callback_handle = api->AddCompletionCallback(
      handle, WaitCallback, user_data, DeleteWaitState,
      /*clear_existing_callbacks=*/false);

  if (!callback_handle.is_valid()) {
    if (!state->cleaned_up.load(std::memory_order_acquire)) {
      delete user_data;
    }
    return state->completed.load(std::memory_order_acquire) ||
           status() == kFutureStatusComplete;
  }

  if (timeout_milliseconds < 0) {
    state->semaphore.Wait();
    return state->completed.load(std::memory_order_acquire) ||
           status() == kFutureStatusComplete;
  } else {
    bool signaled = state->semaphore.TimedWait(timeout_milliseconds);
    if (!signaled) {
      RemoveOnCompletion(callback_handle);
      return state->completed.load(std::memory_order_acquire);
    }
    return state->completed.load(std::memory_order_acquire) ||
           status() == kFutureStatusComplete;
  }
}

}  // namespace firebase
