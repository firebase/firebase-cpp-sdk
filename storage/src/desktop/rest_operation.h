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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_REST_OPERATION_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_REST_OPERATION_H_

#include <memory>

#include "app/memory/unique_ptr.h"
#include "app/rest/controller_interface.h"
#include "app/rest/request.h"
#include "app/rest/transport_curl.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/mutex.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

class BlockingResponse;
class Notifier;

// Structure containing the data we need to keep track of, (and later clean up)
// when we spin up a new async request.
class RestOperation {
 private:
  // See Start().
  RestOperation(StorageInternal* storage_internal,
                const StorageReference& storage_reference,
                rest::Request* request, Notifier* request_notifier,
                BlockingResponse* response, Listener* listener,
                FutureHandle handle, Controller* controller_out);

 public:
  ~RestOperation();

  // Get the notifier used to cleanup this object.
  CleanupNotifier& cleanup() { return cleanup_; }

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

  // Whether this operation is complete and can be deleted.
  bool is_complete() const;

 private:
  // Notify the listener of progress.
  void NotifyListenerOfProgress();

 public:
  // Takes ownership of request and response, copies storage_reference
  // and adds references to storage_internal and listener.
  // If storage_internal or listener are destroyed they call this object to
  // remove references to them.  storage_internal owns the lifetime of this
  // object created by this method through its cleanup notifier.
  // If provided, controller_out is populated with the controller used to manage
  // the rest call.
  static void Start(StorageInternal* storage_internal,
                    const StorageReference& storage_reference,
                    rest::Request* request, Notifier* request_notifier,
                    BlockingResponse* response, Listener* listener,
                    FutureHandle handle, Controller* controller_out) {
    RestOperation* operation = new RestOperation(
        storage_internal, storage_reference, request, request_notifier,
        response, listener, handle, controller_out);
    (void)operation;  // After creation the operation is owned by
                      // storage_internal.
  }

 private:
  StorageInternal* storage_internal_;
  UniquePtr<rest::Request> request_;
  Notifier* request_notifier_;
  UniquePtr<BlockingResponse> response_;
  // Guards this object's state.
  mutable Mutex mutex_;
  Listener* listener_;
  FutureHandle handle_;
  CleanupNotifier cleanup_;
  rest::TransportCurl transport_;
  flatbuffers::unique_ptr<rest::Controller> rest_controller_;
  // Storage controller that delegates to this object.
  storage::Controller controller_;
  bool is_complete_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_REST_OPERATION_H_
