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

#include <utility>

#include "app/rest/controller_curl.h"
#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/log.h"
#include "app/src/mutex.h"

namespace firebase {
namespace rest {

ControllerCurl::ControllerCurl(TransportCurl* transport,
                               TransferDirection direction, Response* response)
    : transport_(transport),
      direction_(direction),
      is_paused_(false),
      response_(response),
      transferring_(false),
      bytes_transferred_(0),
      transfer_size_(0),
      this_handle_(nullptr),
      this_handle_mutex_(nullptr) {}

ControllerCurl::~ControllerCurl() {
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    *this_handle_ = nullptr;
  }
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
ControllerCurl::ControllerCurl(ControllerCurl&& other) noexcept {
  *this = std::move(other);
}

ControllerCurl& ControllerCurl::operator=(ControllerCurl&& other) noexcept {
  direction_ = other.direction_;
  transport_ = other.transport_;
  is_paused_ = other.is_paused_;
  response_ = other.response_;
  transferring_ = other.transferring_;
  bytes_transferred_ = other.bytes_transferred_;
  transfer_size_ = other.transfer_size_;
  this_handle_ = other.this_handle_;
  this_handle_mutex_ = other.this_handle_mutex_;
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    *this_handle_ = this;
  }
  other.this_handle_mutex_ = nullptr;
  other.transport_ = nullptr;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

bool ControllerCurl::Pause() {
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    if (transferring_ && !is_paused_) {
      transport_->PauseRequest(response_);
      is_paused_ = true;
      return true;
    }
  }
  return false;
}

bool ControllerCurl::Resume() {
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    if (transferring_ && is_paused_) {
      transport_->ResumeRequest(response_);
      is_paused_ = false;
      return true;
    }
  }
  return false;
}

bool ControllerCurl::IsPaused() {
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    return is_paused_;
  }
  return false;
}

bool ControllerCurl::Cancel() {
  if (this_handle_mutex_) {
    MutexLock lock(*this_handle_mutex_);
    if (transferring_) {
      transport_->CancelRequest(response_);
      transferring_ = false;
      return true;
    }
  }
  return false;
}

float ControllerCurl::Progress() {
  auto transferred = static_cast<float>(BytesTransferred());
  auto size = static_cast<float>(TransferSize());
  return transferred / size;
}

int64_t ControllerCurl::TransferSize() { return transfer_size_; }

int64_t ControllerCurl::BytesTransferred() { return bytes_transferred_; }

void ControllerCurl::set_bytes_transferred(int64_t bytes_transferred) {
  // Only set the bytes transferred if it is larger than the current value.
  // Curl reports -1 for the bytes transferred if the transfer is just starting
  // or is complete.
  if (bytes_transferred > bytes_transferred_) {
    bytes_transferred_ = bytes_transferred;
  }
}

void ControllerCurl::set_transfer_size(int64_t transfer_size) {
  // Only set the transfer size if it is larger than the current value.
  // Curl reports -1 for the transfer size if the transfer is just starting or
  // is complete.
  if (transfer_size > transfer_size_) {
    transfer_size_ = transfer_size;
  }
}

}  // namespace rest
}  // namespace firebase
