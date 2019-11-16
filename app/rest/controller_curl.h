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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_CURL_H_
#define FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_CURL_H_

#include <cstdint>

#include "app/rest/controller_interface.h"
#include "app/rest/response.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/mutex.h"

namespace firebase {
namespace rest {

class TransportCurl;

// Enum used by ControllerCurl to determine whether the controller is operating
// on an upload or a download.
enum TransferDirection {
  kTransferDirectionUpload,
  kTransferDirectionDownload,
};

// An implementation of a Controller that controls a running curl
// operation. This can be used to monitor progress, pause, restart or cancel a
// running operation.
class ControllerCurl : public Controller {
 public:
  // Transport and Response are pointers to the transport layer and curl
  // response, respectively.  ControllerCurl does not take ownership of these,
  // but needs access to them to perform controller operations.
  // The owner of this class is responsible for ensuring that transport and
  // response remain valid throughout the lifetime of the controller.  (and
  // conversely, that ControllerCurl's methods are not invoked after transport
  // and response are no longer valid.)
  ControllerCurl(TransportCurl* transport, TransferDirection direction,
                 Response* response);

  ~ControllerCurl() override;

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  ControllerCurl(ControllerCurl&& other) noexcept;

  ControllerCurl& operator=(ControllerCurl&& other) noexcept;
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  // Pauses whatever the handle is doing.
  bool Pause() override;

  // Resumes a paused operation.
  bool Resume() override;

  // Returns true if the operation is paused.
  bool IsPaused() override;

  // Cancels a request and disposes of any resources allocated as part of the
  // request.
  bool Cancel() override;

  // Returns a float in the range [0.0 - 1.0], representing the progress of the
  // operation so far, if known. (percent bytes transferred, usually.)
  float Progress() override;

  // Returns the number of bytes being transferred in this operation. (either
  // upload or download.)
  int64_t TransferSize() override;

  // Returns the number of bytes that have been transferred so far.
  int64_t BytesTransferred() override;

  // The following methods are called by TransportCurl to set the status of this
  // object.
  // Set whether a transfer is active.
  void set_transferring(bool transferring) { transferring_ = transferring; }

  // Set the current number of bytes transferred.
  void set_bytes_transferred(int64_t bytes_transferred);

  // Set the total size of the transfer.
  void set_transfer_size(int64_t transfer_size);

  // Set the controller handle and mutex.
  void InitializeControllerHandle(ControllerCurl** handle, Mutex* mutex) {
    this_handle_ = handle;
    this_handle_mutex_ = mutex;
  }

  // Direction of the transfer.
  TransferDirection direction() const { return direction_; }

 private:
  ControllerCurl(const ControllerCurl& other) = delete;
  ControllerCurl& operator=(const ControllerCurl& other) = delete;

  TransportCurl* transport_;

  // Whether this is an upload or a download.
  TransferDirection direction_;

  // Whether this operation is currently paused or not.
  bool is_paused_;

  // The response that this controller is associated with.
  Response* response_;

  // Whether the transfer is running.
  bool transferring_;
    // Number of bytes transferred.
  int64_t bytes_transferred_;
  // Total size of the transfer.
  int64_t transfer_size_;

  // Handle to this controller which is updated when it's moved.
  ControllerCurl** this_handle_;
  // Guards this_handle.
  Mutex* this_handle_mutex_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_CONTROLLER_CURL_H_
