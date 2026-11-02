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

class RestTransfer;

// Enum used by ControllerCurl to determine whether the controller is operating
// on an upload or a download.
enum TransferDirection {
  kTransferDirectionUpload,
  kTransferDirectionDownload,
};

// Shared state between a ControllerCurl (owned by the caller) and its in-flight
// RestTransfer (owned by the curl I/O thread). Both hold a shared_ptr so either
// may outlive the other without dangling pointers or global transfer tables.
struct TransferControl {
  explicit TransferControl(TransferDirection dir)
      : direction(dir),
        mutex(Mutex::kModeNonRecursive),
        transfer(nullptr),
        transferring(true),
        paused(false),
        canceled(false),
        bytes_transferred(0),
        transfer_size(0) {}

  // Written exclusively on the curl I/O thread; read lock-free by callers.
  // libcurl reports -1 while starting or complete, so keep the high-water mark.
  void SetBytesTransferred(int64_t value) {
    if (value > bytes_transferred.load(std::memory_order_relaxed)) {
      bytes_transferred.store(value, std::memory_order_relaxed);
    }
  }

  void SetTransferSize(int64_t value) {
    if (value > transfer_size.load(std::memory_order_relaxed)) {
      transfer_size.store(value, std::memory_order_relaxed);
    }
  }

  // Defined in transport_curl.cc; invoked on the curl I/O thread.
  static void PauseOnIoThread(RestTransfer* transfer);
  static void ResumeOnIoThread(RestTransfer* transfer);
  static void CancelOnIoThread(RestTransfer* transfer);

  const TransferDirection direction;
  // Guards transfer, transferring, paused, and canceled.
  Mutex mutex;
  RestTransfer* transfer;
  bool transferring;
  bool paused;
  bool canceled;
  std::atomic<int64_t> bytes_transferred;
  std::atomic<int64_t> transfer_size;
};

// Controls a running curl transfer (progress, pause, resume, cancel).
class ControllerCurl : public Controller {
 public:
  explicit ControllerCurl(std::shared_ptr<TransferControl> control);
  ~ControllerCurl() override;

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  ControllerCurl(ControllerCurl&& other) noexcept = default;
  ControllerCurl& operator=(ControllerCurl&& other) noexcept = default;
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ControllerCurl(const ControllerCurl& other) = delete;
  ControllerCurl& operator=(const ControllerCurl& other) = delete;

  bool Pause() override;
  bool Resume() override;
  bool IsPaused() override;
  bool Cancel() override;
  float Progress() override;
  int64_t TransferSize() override;
  int64_t BytesTransferred() override;

 private:
  std::shared_ptr<TransferControl> control_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_REST_CONTROLLER_CURL_H_
