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

#include <cstdint>
#include <utility>

#include "app/rest/transport_curl.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"

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
  Mutex* mutex = this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
    if (this_handle_) {
      *this_handle_ = nullptr;
      this_handle_ = nullptr;
    }
  }
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
ControllerCurl::ControllerCurl(ControllerCurl&& other) noexcept
    : transport_(nullptr),
      direction_(kTransferDirectionDownload),
      is_paused_(false),
      response_(nullptr),
      transferring_(false),
      bytes_transferred_(0),
      transfer_size_(0),
      this_handle_(nullptr),
      this_handle_mutex_(nullptr) {
  *this = std::move(other);
}

ControllerCurl& ControllerCurl::operator=(ControllerCurl&& other) noexcept {
  if (this == &other) return *this;
  Mutex* mutex = other.this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
    direction_ = other.direction_;
    transport_ = other.transport_;
    is_paused_ = other.is_paused_;
    response_ = other.response_;
    transferring_ = other.transferring_;
    bytes_transferred_.store(
        other.bytes_transferred_.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    transfer_size_.store(other.transfer_size_.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    this_handle_ = other.this_handle_;
    this_handle_mutex_.store(
        other.this_handle_mutex_.load(std::memory_order_relaxed),
        std::memory_order_release);
    if (this_handle_) {
      *this_handle_ = this;
    }
    other.this_handle_ = nullptr;
    other.this_handle_mutex_.store(nullptr, std::memory_order_release);
    other.transport_ = nullptr;
  } else {
    direction_ = other.direction_;
    transport_ = other.transport_;
    is_paused_ = other.is_paused_;
    response_ = other.response_;
    transferring_ = other.transferring_;
    bytes_transferred_.store(
        other.bytes_transferred_.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    transfer_size_.store(other.transfer_size_.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    this_handle_ = nullptr;
    this_handle_mutex_.store(nullptr, std::memory_order_release);
    other.transport_ = nullptr;
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

bool ControllerCurl::Pause() {
  Mutex* mutex = this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
    if (transferring_ && !is_paused_) {
      transport_->PauseRequest(response_);
      is_paused_ = true;
      return true;
    }
  }
  return false;
}

bool ControllerCurl::Resume() {
  Mutex* mutex = this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
    if (transferring_ && is_paused_) {
      transport_->ResumeRequest(response_);
      is_paused_ = false;
      return true;
    }
  }
  return false;
}

bool ControllerCurl::IsPaused() {
  Mutex* mutex = this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
    return is_paused_;
  }
  return false;
}

bool ControllerCurl::Cancel() {
  Mutex* mutex = this_handle_mutex_.load(std::memory_order_acquire);
  if (mutex) {
    MutexLock lock(*mutex);
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

int64_t ControllerCurl::TransferSize() {
  return transfer_size_.load(std::memory_order_relaxed);
}

int64_t ControllerCurl::BytesTransferred() {
  return bytes_transferred_.load(std::memory_order_relaxed);
}

void ControllerCurl::set_bytes_transferred(int64_t bytes_transferred) {
  // Only set the bytes transferred if it is larger than the current value.
  // Curl reports -1 for the bytes transferred if the transfer is just starting
  // or is complete.
  int64_t current = bytes_transferred_.load(std::memory_order_relaxed);
  while (bytes_transferred > current &&
         !bytes_transferred_.compare_exchange_weak(current, bytes_transferred,
                                                   std::memory_order_relaxed)) {
  }
}

void ControllerCurl::set_transfer_size(int64_t transfer_size) {
  // Only set the transfer size if it is larger than the current value.
  // Curl reports -1 for the transfer size if the transfer is just starting or
  // is complete.
  int64_t current = transfer_size_.load(std::memory_order_relaxed);
  while (transfer_size > current &&
         !transfer_size_.compare_exchange_weak(current, transfer_size,
                                               std::memory_order_relaxed)) {
  }
}

}  // namespace rest
}  // namespace firebase
