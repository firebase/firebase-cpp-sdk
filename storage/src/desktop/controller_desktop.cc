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
  std::shared_ptr<RestOperation::OperationHandle> old_handle;
  bool cancelled = false;
  {
    std::scoped_lock lock(mutex_);
    old_handle = std::move(current_);
    current_ = handle;
    cancelled = cancelled_;
  }
  old_handle.reset();
  if (cancelled && handle) {
    std::scoped_lock op_lock(handle->mutex);
    if (handle->op != nullptr) {
      handle->op->Cancel();
    }
  }
}

void ControllerBinding::MarkFinished() {
  std::scoped_lock lock(mutex_);
  finished_ = true;
}

bool ControllerBinding::Cancel() {
  std::shared_ptr<RestOperation::OperationHandle> handle;
  bool prevented_retry = false;
  {
    std::scoped_lock lock(mutex_);
    prevented_retry = !finished_ && !cancelled_;
    cancelled_ = true;
    handle = current_;
  }
  bool op_cancelled = false;
  if (handle) {
    std::scoped_lock op_lock(handle->mutex);
    op_cancelled = handle->op != nullptr && handle->op->Cancel();
  }
  return op_cancelled || prevented_retry;
}

bool ControllerBinding::is_cancelled() const {
  std::scoped_lock lock(mutex_);
  return cancelled_;
}

std::shared_ptr<RestOperation::OperationHandle> ControllerBinding::current()
    const {
  std::scoped_lock lock(mutex_);
  return current_;
}

// Note: ControllerInternal is a private Pimpl implementation class owned by
// firebase::storage::Controller via a heap-allocated pointer
// (Controller::internal_). Its size and member layout are internal to
// libfirebase_storage and never part of the public SDK ABI.
//
// ControllerInternal deliberately does not register with
// RestOperation::cleanup(): every access to the operation goes through the
// shared OperationHandle (whose `op` is cleared under its mutex when the
// operation is destroyed, preserving the final byte counts), so there is no
// raw pointer to invalidate. This also avoids any lock-order dependency on
// CleanupNotifier::UnregisterObject(), which blocks while the object's
// cleanup callback is running on another thread.

ControllerInternal::ControllerInternal() = default;

ControllerInternal::~ControllerInternal() = default;

ControllerInternal::ControllerInternal(State&& state) noexcept
    : binding_(std::move(state.binding)),
      reference_(std::move(state.reference)),
      op_handle_(std::move(state.op_handle)),
      cached_handle_(std::move(state.cached_handle)),
      bytes_transferred_(state.bytes_transferred),
      total_byte_count_(state.total_byte_count),
      is_valid_(state.is_valid) {}

ControllerInternal::ControllerInternal(const ControllerInternal& other)
    : ControllerInternal(other.Snapshot()) {}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (this == &other) {
    return *this;
  }
  // Snapshot `other` (under other.mutex_ only) before locking mutex_, so the
  // two controllers' mutexes are never held at the same time.
  Assign(other.Snapshot());
  return *this;
}

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept
    : ControllerInternal(other.TakeState()) {}

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  Assign(other.TakeState());
  return *this;
}

ControllerInternal::State ControllerInternal::Snapshot() const {
  State state;
  {
    std::scoped_lock lock(mutex_);
    state.binding = binding_;
    state.reference = reference_;
    state.op_handle = op_handle_;
    state.cached_handle = cached_handle_;
    state.bytes_transferred = bytes_transferred_;
    state.total_byte_count = total_byte_count_;
    state.is_valid = is_valid_;
  }

  // Refresh the byte counts from the live (or finished) operation, resolving
  // the active handle the same way UpdateFromOperation() does: the binding's
  // current attempt if there is one, otherwise the initially bound operation.
  std::shared_ptr<RestOperation::OperationHandle> active = state.op_handle;
  if (state.binding) {
    std::shared_ptr<RestOperation::OperationHandle> bound =
        state.binding->current();
    if (bound) {
      active = std::move(bound);
    }
  }
  if (active) {
    int64_t op_transferred = -1;
    int64_t op_total = -1;
    ReadHandle(*active, &op_transferred, &op_total);
    state.cached_handle = active;
    if (op_transferred >= 0) state.bytes_transferred = op_transferred;
    if (op_total >= 0) state.total_byte_count = op_total;
  }
  return state;
}

ControllerInternal::State ControllerInternal::TakeState() noexcept {
  State state;
  std::scoped_lock lock(mutex_);
  state.binding = std::move(binding_);
  state.reference = std::move(reference_);
  state.op_handle = std::move(op_handle_);
  state.cached_handle = std::move(cached_handle_);
  state.bytes_transferred = bytes_transferred_;
  state.total_byte_count = total_byte_count_;
  state.is_valid = is_valid_;
  // Leave the moved-from object as Initialize(StorageReference(), nullptr).
  binding_.reset();
  reference_ = StorageReference();
  op_handle_.reset();
  cached_handle_.reset();
  bytes_transferred_ = 0;
  total_byte_count_ = -1;
  is_valid_ = false;
  return state;
}

void ControllerInternal::Assign(State&& state) noexcept {
  // Move overwritten members into `retired` under mutex_ and destroy them
  // after releasing mutex_ to prevent any lock-order inversion during
  // destruction of StorageReference or shared_ptrs.
  State retired;
  {
    std::scoped_lock lock(mutex_);
    retired.binding = std::move(binding_);
    retired.reference = std::move(reference_);
    retired.op_handle = std::move(op_handle_);
    retired.cached_handle = std::move(cached_handle_);
    binding_ = std::move(state.binding);
    reference_ = std::move(state.reference);
    op_handle_ = std::move(state.op_handle);
    cached_handle_ = std::move(state.cached_handle);
    bytes_transferred_ = state.bytes_transferred;
    total_byte_count_ = state.total_byte_count;
    is_valid_ = state.is_valid;
  }
}

std::shared_ptr<RestOperation::OperationHandle>
ControllerInternal::CurrentHandle() const {
  std::shared_ptr<ControllerBinding> binding;
  {
    std::scoped_lock lock(mutex_);
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
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Pause();
}

bool ControllerInternal::Resume() {
  std::shared_ptr<RestOperation::OperationHandle> handle = CurrentHandle();
  if (!handle) {
    return false;
  }
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Resume();
}

bool ControllerInternal::Cancel() {
  std::shared_ptr<ControllerBinding> binding;
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    std::scoped_lock lock(mutex_);
    binding = binding_;
    handle = op_handle_;
  }
  if (binding) {
    return binding->Cancel();
  }
  if (!handle) {
    return false;
  }
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Cancel();
}

bool ControllerInternal::is_paused() const {
  std::shared_ptr<RestOperation::OperationHandle> handle = CurrentHandle();
  if (!handle) {
    return false;
  }
  std::scoped_lock op_lock(handle->mutex);
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
    std::scoped_lock lock(mutex_);
    ref_copy = reference_;
  }
  // Ownership is transferred to Controller::GetReference() in
  // storage/src/common/controller.cc, which immediately wraps the raw pointer
  // in a StorageReference (required by the cross-platform ControllerInternal
  // interface).
  return ref_copy.is_valid()
             ? std::make_unique<StorageReferenceInternal>(*ref_copy.internal_)
                   .release()
             : nullptr;
}

bool ControllerInternal::is_valid() const {
  std::scoped_lock lock(mutex_);
  return is_valid_ || binding_ != nullptr;
}

void ControllerInternal::SetBinding(std::shared_ptr<ControllerBinding> binding) {
  std::shared_ptr<ControllerBinding> old_binding;
  {
    std::scoped_lock lock(mutex_);
    old_binding = std::move(binding_);
    binding_ = std::move(binding);
  }
}

std::shared_ptr<RestOperation::OperationHandle>
ControllerInternal::operation_handle() const {
  std::scoped_lock lock(mutex_);
  return op_handle_;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) {
    return true;
  }
  StorageReference this_ref;
  std::shared_ptr<RestOperation::OperationHandle> this_handle;
  std::shared_ptr<ControllerBinding> this_binding;
  bool this_valid = false;
  StorageReference other_ref;
  std::shared_ptr<RestOperation::OperationHandle> other_handle;
  std::shared_ptr<ControllerBinding> other_binding;
  bool other_valid = false;
  {
    // Lock both mutexes simultaneously using C++17 deadlock-avoiding
    // std::scoped_lock so the two states cannot diverge between reads.
    std::scoped_lock lock(mutex_, other.mutex_);
    this_ref = reference_;
    this_handle = op_handle_;
    this_binding = binding_;
    this_valid = is_valid_ || (binding_ != nullptr);
    other_ref = other.reference_;
    other_handle = other.op_handle_;
    other_binding = other.binding_;
    other_valid = other.is_valid_ || (other.binding_ != nullptr);
  }

  if (!this_valid && !other_valid) {
    // Two controllers that are not (or no longer) attached to an operation
    // are only equal if they refer to the same storage location.
    return this_ref == other_ref;
  }
  if (this_valid != other_valid) {
    return false;
  }
  if (this_binding != nullptr || other_binding != nullptr) {
    return this_binding == other_binding && this_ref == other_ref;
  }
  return this_handle == other_handle && this_ref == other_ref;
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
  StorageReference new_ref_copy(reference);
  StorageReference old_ref;
  std::shared_ptr<ControllerBinding> old_binding;
  std::shared_ptr<RestOperation::OperationHandle> old_op_handle;
  std::shared_ptr<RestOperation::OperationHandle> old_cached_handle;
  {
    std::scoped_lock lock(mutex_);
    old_ref = std::move(reference_);
    old_binding = std::move(binding_);
    old_op_handle = std::move(op_handle_);
    old_cached_handle = std::move(cached_handle_);
    reference_ = std::move(new_ref_copy);
    cached_handle_ = new_handle;
    op_handle_ = std::move(new_handle);
    bytes_transferred_ = (op_transferred >= 0) ? op_transferred : 0;
    total_byte_count_ = (op_total >= 0) ? op_total : -1;
    is_valid_ = (op_handle_ != nullptr);
  }
}

void ControllerInternal::ReadHandle(RestOperation::OperationHandle& handle,
                                    int64_t* transferred, int64_t* total) {
  std::scoped_lock op_lock(handle.mutex);
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
    std::scoped_lock lock(mutex_);
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
    std::scoped_lock lock(mutex_);
    if (transferred) *transferred = bytes_transferred_;
    if (total) *total = total_byte_count_;
    return;
  }

  int64_t op_transferred = -1;
  int64_t op_total = -1;
  ReadHandle(*handle, &op_transferred, &op_total);

  // Re-verify the active handle on the binding (outside mutex_ to preserve
  // lock order) so a read from a superseded retry attempt is discarded.
  const std::shared_ptr<RestOperation::OperationHandle> current_bound =
      binding ? binding->current() : nullptr;

  std::scoped_lock lock(mutex_);
  const bool still_bound =
      binding ? (binding_ == binding &&
                 (current_bound ? current_bound == handle
                                : op_handle_ == handle))
              : (!binding_ && op_handle_ == handle);
  if (still_bound) {
    if (cached_handle_ != handle) {
      // Bound to a new retry attempt: reset the cached counts for this handle.
      cached_handle_ = handle;
      bytes_transferred_ = (op_transferred >= 0) ? op_transferred : 0;
      total_byte_count_ = (op_total >= 0) ? op_total : -1;
    } else {
      // Within the same attempt, progress is monotonic: never let an older,
      // out-of-order read overwrite a newer cached byte count.
      if (op_transferred > bytes_transferred_) {
        bytes_transferred_ = op_transferred;
      }
      if (op_total >= 0) {
        total_byte_count_ = op_total;
      }
    }
  }
  if (transferred) *transferred = bytes_transferred_;
  if (total) *total = total_byte_count_;
}

}  // namespace firebase::storage::internal
