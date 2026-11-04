// Copyright 2018 Google LLC

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

  void Detach();
  void NotifyProgress(Controller* controller);
  static void NotifyProgress(
      std::shared_ptr<SharedState> state, Controller* controller,
      const RestOperation::OperationHandle* expected_handle = nullptr);
  void NotifyPaused(Controller* controller);
  static void NotifyPaused(std::shared_ptr<SharedState> state,
                           Controller* controller);
  void set_rest_operation(RestOperation* operation);
  const std::shared_ptr<SharedState>& shared_state() const { return state_; }

 private:
  std::shared_ptr<SharedState> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_LISTENER_DESKTOP_H_
