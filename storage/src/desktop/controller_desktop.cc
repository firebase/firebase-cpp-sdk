// Copyright 2017 Google LLC
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

#include "storage/src/desktop/controller_desktop.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

#include "storage/src/desktop/storage_reference_desktop.h"

namespace firebase::storage::internal {

void ControllerBinding::Bind(
    std::shared_ptr<RestOperation::OperationHandle> handle) {
  bool cancelled = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    current_ = handle;
    cancelled = cancelled_;
  }
  if (cancelled && handle) {
    std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
    if (handle->op != nullptr) {
      handle->op->Cancel();
    }
  }
}

void ControllerBinding::MarkFinished() {
  std::lock_guard<std::mutex> lock(mutex_);
  finished_ = true;
}

bool ControllerBinding::Cancel() {
  std::shared_ptr<RestOperation::OperationHandle> handle;
  bool prevented_retry = false;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    prevented_retry = !finished_ && !cancelled_;
    cancelled_ = true;
    handle = current_;
  }
  bool op_cancelled = false;
  if (handle) {
    std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
    op_cancelled = handle->op != nullptr && handle->op->Cancel();
  }
  return op_cancelled || prevented_retry;
}

bool ControllerBinding::is_cancelled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return cancelled_;
}

std::shared_ptr<RestOperation::OperationHandle> ControllerBinding::current()
    const {
  std::lock_guard<std::mutex> lock(mutex_);
  return current_;
}

// ControllerInternal deliberately does not register with
// RestOperation::cleanup(): every access to the operation goes through the
// shared OperationHandle (whose `op` is cleared under its mutex when the
// operation is destroyed, preserving the final byte counts), so there is no
// raw pointer to invalidate. This also avoids any lock-order dependency on
// CleanupNotifier::UnregisterObject(), which blocks while the object's
// cleanup callback is running on another thread.

ControllerInternal::ControllerInternal() = default;

ControllerInternal::~ControllerInternal() = default;

ControllerInternal::ControllerInternal(const ControllerInternal& other) {
  *this = other;
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (this == &other) {
    return *this;
  }
  StorageReference other_ref;
  std::shared_ptr<RestOperation::OperationHandle> other_handle;
  std::shared_ptr<ControllerBinding> other_binding;
  int64_t other_transferred = 0;
  int64_t other_total = -1;
  bool other_valid = false;
  {
    MutexLock other_lock(other.mutex_);
    other_ref = other.reference_;
    other_handle = other.op_handle_;
    other_binding = other.binding_;
    other_transferred = other.bytes_transferred_;
    other_total = other.total_byte_count_;
    other_valid = other.is_valid_;
  }

  // Refresh the byte counts from the live (or finished) operation.
  if (other_handle) {
    int64_t op_transferred = -1;
    int64_t op_total = -1;
    ReadHandle(*other_handle, &op_transferred, &op_total);
    if (op_transferred >= 0) other_transferred = op_transferred;
    if (op_total >= 0) other_total = op_total;
  }

  MutexLock lock(mutex_);
  reference_ = std::move(other_ref);
  op_handle_ = std::move(other_handle);
  binding_ = std::move(other_binding);
  bytes_transferred_ = other_transferred;
  total_byte_count_ = other_total;
  is_valid_ = other_valid;
  return *this;
}

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept {
  *this = std::move(other);
}

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  *this = static_cast<const ControllerInternal&>(other);
  other.Initialize(StorageReference(), nullptr);
  return *this;
}

std::shared_ptr<RestOperation::OperationHandle>
ControllerInternal::CurrentHandle() const {
  std::shared_ptr<ControllerBinding> binding;
  {
    MutexLock lock(mutex_);
    if (!binding_) {
      return op_handle_;
    }
    binding = binding_;
  }
  return binding->current();
}

bool ControllerInternal::Pause() {
  std::shared_ptr<RestOperation::OperationHandle> handle = CurrentHandle();
  if (!handle) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Pause();
}

bool ControllerInternal::Resume() {
  std::shared_ptr<RestOperation::OperationHandle> handle = CurrentHandle();
  if (!handle) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Resume();
}

bool ControllerInternal::Cancel() {
  std::shared_ptr<ControllerBinding> binding;
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    MutexLock lock(mutex_);
    binding = binding_;
    handle = op_handle_;
  }
  if (binding) {
    return binding->Cancel();
  }
  if (!handle) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Cancel();
}

bool ControllerInternal::is_paused() const {
  std::shared_ptr<RestOperation::OperationHandle> handle = CurrentHandle();
  if (!handle) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->is_paused();
}

int64_t ControllerInternal::bytes_transferred() const {
  int64_t transferred = 0;
  UpdateFromOperation(&transferred, nullptr);
  return transferred;
}

int64_t ControllerInternal::total_byte_count() const {
  int64_t total = -1;
  UpdateFromOperation(nullptr, &total);
  return total;
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  StorageReference ref_copy;
  {
    MutexLock lock(mutex_);
    ref_copy = reference_;
  }
  return ref_copy.is_valid() ? new StorageReferenceInternal(*ref_copy.internal_)
                             : nullptr;
}

bool ControllerInternal::is_valid() const {
  MutexLock lock(mutex_);
  return is_valid_ || binding_ != nullptr;
}

void ControllerInternal::SetBinding(std::shared_ptr<ControllerBinding> binding) {
  MutexLock lock(mutex_);
  binding_ = std::move(binding);
}

std::shared_ptr<RestOperation::OperationHandle>
ControllerInternal::operation_handle() const {
  MutexLock lock(mutex_);
  return op_handle_;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) {
    return true;
  }
  StorageReference this_ref;
  std::shared_ptr<RestOperation::OperationHandle> this_handle;
  std::shared_ptr<ControllerBinding> this_binding;
  int64_t this_transferred = 0;
  int64_t this_total = -1;
  bool this_valid = false;
  {
    MutexLock lock(mutex_);
    this_ref = reference_;
    this_handle = op_handle_;
    this_binding = binding_;
    this_transferred = bytes_transferred_;
    this_total = total_byte_count_;
    this_valid = is_valid_ || (binding_ != nullptr);
  }

  StorageReference other_ref;
  std::shared_ptr<RestOperation::OperationHandle> other_handle;
  std::shared_ptr<ControllerBinding> other_binding;
  int64_t other_transferred = 0;
  int64_t other_total = -1;
  bool other_valid = false;
  {
    MutexLock lock(other.mutex_);
    other_ref = other.reference_;
    other_handle = other.op_handle_;
    other_binding = other.binding_;
    other_transferred = other.bytes_transferred_;
    other_total = other.total_byte_count_;
    other_valid = other.is_valid_ || (other.binding_ != nullptr);
  }

  if (!this_valid && !other_valid) {
    return true;
  }
  if (this_valid != other_valid) {
    return false;
  }
  if (this_binding != nullptr || other_binding != nullptr) {
    return this_binding == other_binding && this_ref == other_ref;
  }
  if (this_handle != nullptr || other_handle != nullptr) {
    return this_handle == other_handle && this_ref == other_ref;
  }
  return this_ref == other_ref && this_transferred == other_transferred &&
         this_total == other_total;
}

void ControllerInternal::Initialize(const StorageReference& reference,
                                    RestOperation* operation) {
  std::shared_ptr<RestOperation::OperationHandle> new_handle =
      operation ? operation->operation_handle() : nullptr;
  int64_t op_transferred = -1;
  int64_t op_total = -1;
  if (new_handle) {
    ReadHandle(*new_handle, &op_transferred, &op_total);
  }
  MutexLock lock(mutex_);
  reference_ = reference;
  binding_.reset();
  op_handle_ = std::move(new_handle);
  bytes_transferred_ = (op_transferred >= 0) ? op_transferred : 0;
  total_byte_count_ = (op_total >= 0) ? op_total : -1;
  is_valid_ = (op_handle_ != nullptr);
}

void ControllerInternal::ReadHandle(RestOperation::OperationHandle& handle,
                                    int64_t* transferred, int64_t* total) {
  std::lock_guard<std::recursive_mutex> op_lock(handle.mutex);
  if (handle.op != nullptr) {
    *transferred = handle.op->bytes_transferred();
    *total = handle.op->total_byte_count();
  } else {
    *transferred = handle.final_bytes_transferred;
    *total = handle.final_total_byte_count;
  }
}

void ControllerInternal::UpdateFromOperation(int64_t* transferred,
                                             int64_t* total) const {
  std::shared_ptr<ControllerBinding> binding;
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    MutexLock lock(mutex_);
    binding = binding_;
    handle = op_handle_;
  }
  if (binding) {
    std::shared_ptr<RestOperation::OperationHandle> bound = binding->current();
    if (bound) {
      handle = std::move(bound);
    }
  }
  if (!handle) {
    MutexLock lock(mutex_);
    if (transferred) *transferred = bytes_transferred_;
    if (total) *total = total_byte_count_;
    return;
  }

  int64_t op_transferred = -1;
  int64_t op_total = -1;
  ReadHandle(*handle, &op_transferred, &op_total);

  MutexLock lock(mutex_);
  // Only cache values read from the handle this object is still bound to.
  const bool still_bound =
      binding ? (binding_ == binding) : (op_handle_ == handle);
  if (still_bound) {
    if (op_transferred >= 0) {
      bytes_transferred_ = op_transferred;
    }
    if (op_total >= 0) {
      total_byte_count_ = op_total;
    }
  }
  if (transferred) *transferred = bytes_transferred_;
  if (total) *total = total_byte_count_;
}

}  // namespace firebase::storage::internal
