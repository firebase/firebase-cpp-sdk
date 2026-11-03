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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_LISTENER_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_LISTENER_DESKTOP_H_

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase::storage::internal {

struct ListenerSharedState {
  std::mutex mutex;
  std::condition_variable cv;
  Listener* listener = nullptr;
  ListenerInternal* listener_internal = nullptr;
  RestOperation* rest_operation = nullptr;
  std::shared_ptr<RestOperation::OperationHandle> op_handle;
  std::vector<std::thread::id> callback_threads;
  int64_t bytes_transferred = -1;
  int64_t total_byte_count = -1;
};

class ListenerInternal {
 public:
  using SharedState = ListenerSharedState;

  explicit ListenerInternal(Listener* listener);
  ~ListenerInternal();

  ListenerInternal(const ListenerInternal&) = delete;
  ListenerInternal& operator=(const ListenerInternal&) = delete;
  ListenerInternal(ListenerInternal&&) = delete;
  ListenerInternal& operator=(ListenerInternal&&) = delete;

  // Detach this listener from any active operation and wait for in-flight
  // callbacks on other threads to complete.
  void Detach();

  // Notify the listener associated with this class with a progress update.
  // This will debounce updates if the controller doesn't report progress.
  void NotifyProgress(Controller* controller);

  // Takes `state` by value so the shared state stays alive (and is never
  // reached through a reference into a ListenerInternal) even if the user
  // callback destroys the ListenerInternal. If `expected_handle` is non-null,
  // the update is dropped unless the listener is still attached to that
  // operation (used for notifications delivered asynchronously).
  static void NotifyProgress(
      std::shared_ptr<SharedState> state, Controller* controller,
      const RestOperation::OperationHandle* expected_handle = nullptr);

  // Notify the listener that the operation was paused.
  void NotifyPaused(Controller* controller);

  // See NotifyProgress(std::shared_ptr<SharedState>, Controller*).
  static void NotifyPaused(std::shared_ptr<SharedState> state,
                           Controller* controller);

  // Attach this listener to the specified rest operation.
  // If the rest operation is destroyed it will remove a reference to this
  // listener.
  void set_rest_operation(RestOperation* operation);

  const std::shared_ptr<SharedState>& shared_state() const { return state_; }

 private:
  std::shared_ptr<SharedState> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_LISTENER_DESKTOP_H_
