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

#include "app/rest/controller_curl.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <utility>

#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {
namespace rest {

namespace {

void UpdateMaxAtomic(std::atomic<int64_t>* target, int64_t value) {
  int64_t prev = target->load(std::memory_order_relaxed);
  while (value > prev &&
         !target->compare_exchange_weak(prev, value, std::memory_order_relaxed,
                                        std::memory_order_relaxed)) {
  }
}

}  // namespace

ControllerCurl::ControllerCurl(TransportCurl* transport,
                               TransferDirection direction, Response* response,
                               uint64_t transfer_id)
    : control_(std::make_shared<TransferControl>(transport, direction,
                                                 transfer_id)) {
  (void)response;
}

ControllerCurl::~ControllerCurl() = default;

ControllerCurl::ControllerCurl(ControllerCurl&& other) noexcept = default;

ControllerCurl& ControllerCurl::operator=(ControllerCurl&& other) noexcept =
    default;

bool ControllerCurl::Pause() {
  std::shared_ptr<TransferControl> ctrl = control_;
  if (!ctrl) return false;
  TransportCurl* transport = nullptr;
  uint64_t transfer_id = 0;
  {
    MutexLock lock(ctrl->mutex);
    if (!ctrl->transferring.load(std::memory_order_acquire) ||
        ctrl->is_paused.load(std::memory_order_relaxed) ||
        ctrl->transport == nullptr) {
      return false;
    }
    ctrl->is_paused.store(true, std::memory_order_release);
    transport = ctrl->transport;
    transfer_id = ctrl->transfer_id;
  }
  TransportCurl::PauseRequestById(transport, transfer_id);
  return true;
}

bool ControllerCurl::Resume() {
  std::shared_ptr<TransferControl> ctrl = control_;
  if (!ctrl) return false;
  TransportCurl* transport = nullptr;
  uint64_t transfer_id = 0;
  {
    MutexLock lock(ctrl->mutex);
    if (!ctrl->transferring.load(std::memory_order_acquire) ||
        !ctrl->is_paused.load(std::memory_order_relaxed) ||
        ctrl->transport == nullptr) {
      return false;
    }
    ctrl->is_paused.store(false, std::memory_order_release);
    transport = ctrl->transport;
    transfer_id = ctrl->transfer_id;
  }
  TransportCurl::ResumeRequestById(transport, transfer_id);
  return true;
}

bool ControllerCurl::IsPaused() {
  std::shared_ptr<TransferControl> ctrl = control_;
  if (!ctrl) return false;
  MutexLock lock(ctrl->mutex);
  return ctrl->is_paused.load(std::memory_order_acquire);
}

bool ControllerCurl::Cancel() {
  std::shared_ptr<TransferControl> ctrl = control_;
  if (!ctrl) return false;
  TransportCurl* transport = nullptr;
  uint64_t transfer_id = 0;
  {
    MutexLock lock(ctrl->mutex);
    if (!ctrl->transferring.load(std::memory_order_acquire) ||
        ctrl->transport == nullptr) {
      return false;
    }
    ctrl->transferring.store(false, std::memory_order_release);
    ctrl->is_paused.store(false, std::memory_order_release);
    transport = ctrl->transport;
    transfer_id = ctrl->transfer_id;
  }
  TransportCurl::CancelRequestById(transport, transfer_id);
  return true;
}

float ControllerCurl::Progress() {
  const int64_t size = TransferSize();
  if (size <= 0) {
    return 0.0f;
  }
  const int64_t transferred = BytesTransferred();
  if (transferred <= 0) {
    return 0.0f;
  }
  return static_cast<float>(transferred) / static_cast<float>(size);
}

int64_t ControllerCurl::TransferSize() {
  return control_ ? control_->transfer_size.load(std::memory_order_relaxed) : 0;
}

int64_t ControllerCurl::BytesTransferred() {
  return control_ ? control_->bytes_transferred.load(std::memory_order_relaxed)
                  : 0;
}

void ControllerCurl::set_transferring(bool transferring) {
  if (!control_) return;
  MutexLock lock(control_->mutex);
  control_->transferring.store(transferring, std::memory_order_release);
  if (!transferring) {
    control_->is_paused.store(false, std::memory_order_release);
  }
}

void ControllerCurl::set_bytes_transferred(int64_t bytes_transferred) {
  UpdateBytesTransferred(control_.get(), bytes_transferred);
}

void ControllerCurl::set_transfer_size(int64_t transfer_size) {
  UpdateTransferSize(control_.get(), transfer_size);
}

void ControllerCurl::UpdateBytesTransferred(TransferControl* control,
                                            int64_t bytes_transferred) {
  // Only set the bytes transferred if it is larger than the current value.
  // Curl reports -1 for the bytes transferred if the transfer is just starting
  // or is complete.
  if (control && bytes_transferred > 0) {
    UpdateMaxAtomic(&control->bytes_transferred, bytes_transferred);
  }
}

void ControllerCurl::UpdateTransferSize(TransferControl* control,
                                        int64_t transfer_size) {
  // Only set the transfer size if it is larger than the current value.
  // Curl reports -1 for the transfer size if the transfer is just starting or
  // is complete.
  if (control && transfer_size > 0) {
    UpdateMaxAtomic(&control->transfer_size, transfer_size);
  }
}

}  // namespace rest
}  // namespace firebase
