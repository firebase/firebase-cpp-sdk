/*
 * Copyright 2017 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_CURL_H_
#define FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_CURL_H_

#include <limits>
#include <memory>
#include <vector>

#include "app/rest/transport_interface.h"
#include "app/src/mutex.h"
#include "app/src/semaphore.h"
#include "flatbuffers/stl_emulation.h"

namespace firebase {
namespace rest {

// This must be called before performing any curl operations. Calls to this
// function are reference counted, so it is safe to call multiple times.
void InitTransportCurl();
// This must be called when curl operations are complete to clean up remaining
// resources. This should be called once for every call to InitTransportCurl.
void CleanupTransportCurl();

// Implement the transport layer, based on curl library.
class TransportCurl : public Transport {
 public:
  TransportCurl();
  ~TransportCurl() override;

  TransportCurl(const TransportCurl& transport);
  TransportCurl& operator=(const TransportCurl& transport);

  // Sets whether this transport operation will be performed asynchronously.
  // This should only be modified when an operation on this transport is not
  // currently running.
  // NOTE: This should be set once before starting any transfers.
  // TODO(b/67899466): This is just a stop-gap until we can make all operations
  // async all the time. Since some code currently relies on operations being
  // synchronous this defaults to false and must be manually turned on.
  void set_is_async(bool is_async) { is_async_ = is_async; }
  bool is_async() { return is_async_; }

  // Perform a HTTP request and put result in response.
  // This function does not actually perform the transfer, but rather informs
  // the background thread that there is a transfer waiting to be performed.
  // If a controller pointer is provided, it will be populated with a controller
  // that can pause, resume, cancel and query that status of the transfer. The
  // caller owns the returned controller.
  // NOTE: The controller should *not* be destroyed while a transfer is active.
  //
  // When the transfer is complete Response::MarkComplete will be called, and if
  // there was an error the error code on the response will be populated. This
  // call does not take ownership of any arguments passed in. The request and
  // response and must both live until Response::MarkComplete is called.
  void PerformInternal(
      Request* request, Response* response,
      flatbuffers::unique_ptr<Controller>* controller_out) override;

 private:
  friend class ControllerCurl;
  friend class BackgroundTransportCurl;

  // Used by the ControllerCurl to cancel a running transfer.
  void CancelRequest(Response* response);
  // Used by the ControllerCurl to pause a running transfer.
  void PauseRequest(Response* response);
  // Used by the ControllerCurl to resumt a running transfer.
  void ResumeRequest(Response* response);

  // Signal that a scheduled transfer is complete or canceled.
  void SignalTransferComplete();
  // Wait for all requests associated with this transport to complete.
  void WaitForAllTransfersToComplete();

  // The Curl handle. This class owns the handle.
  void* curl_;

  // Whether this request should be made asynchronously.
  bool is_async_;

  // Guards running_transfers.
  Mutex running_transfers_mutex_;
  // Number of ongoing transfers.
  int running_transfers_;
  // Signaled when a transfer is complete.
  Semaphore running_transfers_semaphore_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_TRANSPORT_CURL_H_
