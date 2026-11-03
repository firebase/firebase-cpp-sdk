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

#ifndef FIREBASE_APP_REST_CONTROLLER_CURL_H_
#define FIREBASE_APP_REST_CONTROLLER_CURL_H_

#include <atomic>
#include <cstdint>
#include <memory>

#include "app/rest/controller_interface.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {
namespace rest {

class Response;
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
  // Shared control block between ControllerCurl and RestTransfer so either
  // object can safely outlive or be moved independently of the other without
  // raw back-pointers or use-after-free hazards.
  struct TransferControl {
    TransferControl(TransportCurl* transport_in, TransferDirection direction_in,
                    uint64_t transfer_id_in)
        : transport(transport_in),
          direction(direction_in),
          transfer_id(transfer_id_in) {}

    Mutex mutex;
    TransportCurl* transport = nullptr;
    TransferDirection direction = kTransferDirectionDownload;
    uint64_t transfer_id = 0;
    std::atomic<bool> is_paused{false};
    std::atomic<bool> transferring{false};
    std::atomic<int64_t> bytes_transferred{0};
    std::atomic<int64_t> transfer_size{0};
  };

  // Transport and Response are pointers to the transport layer and curl
  // response, respectively. ControllerCurl does not take ownership of these,
  // but uses a shared TransferControl block to coordinate safely with the
  // active RestTransfer on the curl I/O loop.
  ControllerCurl(TransportCurl* transport, TransferDirection direction,
                 Response* response, uint64_t transfer_id = 0);

  ~ControllerCurl() override;

  ControllerCurl(const ControllerCurl& other) = delete;
  ControllerCurl& operator=(const ControllerCurl& other) = delete;

  ControllerCurl(ControllerCurl&& other) noexcept;
  ControllerCurl& operator=(ControllerCurl&& other) noexcept;

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
  void set_transferring(bool transferring);

  // Set the current number of bytes transferred.
  void set_bytes_transferred(int64_t bytes_transferred);

  // Set the total size of the transfer.
  void set_transfer_size(int64_t transfer_size);

  // Direction of the transfer.
  TransferDirection direction() const {
    return control_ ? control_->direction : kTransferDirectionDownload;
  }

  // Returns the shared control block used by RestTransfer on the I/O thread.
  const std::shared_ptr<TransferControl>& control() const { return control_; }

  static void UpdateBytesTransferred(TransferControl* control,
                                     int64_t bytes_transferred);
  static void UpdateTransferSize(TransferControl* control,
                                 int64_t transfer_size);

 private:
  std::shared_ptr<TransferControl> control_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CONTROLLER_CURL_H_
