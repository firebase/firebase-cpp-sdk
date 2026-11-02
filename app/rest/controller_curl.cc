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

#include "app/rest/curl/curl_io_loop.h"
#include "app/src/include/firebase/internal/mutex.h"

namespace firebase {
namespace rest {

namespace {

void PostToTransfer(const std::shared_ptr<TransferControl>& control,
                    void (*action)(RestTransfer*)) {
  curl::CurlIoLoop::PostIfRunning([control, action]() {
    RestTransfer* transfer = nullptr;
    {
      MutexLock lock(control->mutex);
      transfer = control->transfer;
    }
    if (transfer != nullptr) action(transfer);
  });
}

}  // namespace

ControllerCurl::ControllerCurl(std::shared_ptr<TransferControl> control)
    : control_(std::move(control)) {}

ControllerCurl::~ControllerCurl() = default;

bool ControllerCurl::Pause() {
  if (!control_) return false;
  {
    MutexLock lock(control_->mutex);
    if (!control_->transferring || control_->paused) return false;
    control_->paused = true;
  }
  PostToTransfer(control_, &TransferControl::PauseOnIoThread);
  return true;
}

bool ControllerCurl::Resume() {
  if (!control_) return false;
  {
    MutexLock lock(control_->mutex);
    if (!control_->transferring || !control_->paused) return false;
    control_->paused = false;
  }
  PostToTransfer(control_, &TransferControl::ResumeOnIoThread);
  return true;
}

bool ControllerCurl::IsPaused() {
  if (!control_) return false;
  MutexLock lock(control_->mutex);
  return control_->paused;
}

bool ControllerCurl::Cancel() {
  if (!control_) return false;
  {
    MutexLock lock(control_->mutex);
    if (!control_->transferring) return false;
    control_->transferring = false;
    control_->paused = false;
    control_->canceled = true;
  }
  PostToTransfer(control_, &TransferControl::CancelOnIoThread);
  return true;
}

float ControllerCurl::Progress() {
  auto transferred = static_cast<float>(BytesTransferred());
  auto size = static_cast<float>(TransferSize());
  return transferred / size;
}

int64_t ControllerCurl::TransferSize() {
  return control_ ? control_->transfer_size.load(std::memory_order_relaxed) : 0;
}

int64_t ControllerCurl::BytesTransferred() {
  return control_ ? control_->bytes_transferred.load(std::memory_order_relaxed)
                  : 0;
}

}  // namespace rest
}  // namespace firebase
