// Copyright 2018 Google LLC

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

class RestOperation {
 public:
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

  CleanupNotifier& cleanup() { return cleanup_; }
  const std::shared_ptr<OperationHandle>& operation_handle() const {
    return op_handle_;
  }

  bool Pause();
  bool Resume();
  bool Cancel();
  bool is_paused() const;
  int64_t bytes_transferred() const;
  int64_t total_byte_count() const;

  void set_listener(Listener* listener);
  void AttachListenerState(
      ListenerInternal* listener_internal,
      const std::shared_ptr<ListenerSharedState>& listener_state);
  void DetachListener(ListenerInternal* listener_internal);
  bool is_complete() const;

 private:
  RestOperation(StorageInternal* storage_internal,
                const StorageReference& storage_reference,
                std::unique_ptr<rest::Request> request,
                Notifier* request_notifier,
                std::unique_ptr<BlockingResponse> response, Listener* listener,
                const std::shared_ptr<ListenerSharedState>& listener_state,
                FutureHandle handle, Controller* controller_out);

  void NotifyListenerOfProgress();

  std::shared_ptr<OperationHandle> op_handle_ =
      std::make_shared<OperationHandle>();
  StorageInternal* storage_internal_ = nullptr;
  std::unique_ptr<rest::Request> request_;
  Notifier* request_notifier_ = nullptr;
  std::unique_ptr<BlockingResponse> response_;
  Mutex listener_mutex_;
  mutable Mutex mutex_;
  struct NotifyState {
    std::mutex mutex;
    std::condition_variable cv;
    std::vector<std::thread::id> threads;
    bool shutting_down = false;
  };
  class NotifyScope;
  std::shared_ptr<NotifyState> notify_state_ = std::make_shared<NotifyState>();
  ListenerInternal* listener_internal_ = nullptr;
  std::shared_ptr<ListenerSharedState> listener_state_;
  uint64_t operation_id_ = 0;
  FutureHandle handle_;
  CleanupNotifier cleanup_;
  rest::TransportCurl transport_;
  std::unique_ptr<rest::Controller> rest_controller_;
  std::thread::id perform_thread_;
  storage::Controller controller_;
  std::shared_ptr<storage::Controller> listener_controller_;
  std::shared_ptr<std::atomic<bool>> progress_pending_ =
      std::make_shared<std::atomic<bool>>(false);
  std::atomic<bool> is_complete_{false};
  std::atomic<bool> is_started_{false};
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_REST_OPERATION_H_
