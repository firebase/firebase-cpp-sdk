// Copyright 2017 Google LLC

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
  bool pause = false;
  {
    std::scoped_lock lock(mutex_);
    old_handle = std::move(current_);
    current_ = handle;
    cancelled = cancelled_;
    pause = pending_pause_ && handle != nullptr;
    if (handle) pending_pause_ = false;
  }
  old_handle.reset();
  if (!handle) return;
  std::scoped_lock op_lock(handle->mutex);
  if (!handle->op) return;
  if (cancelled) {
    handle->op->Cancel();
  } else if (pause) {
    handle->op->Pause();
  }
}

bool ControllerBinding::Pause() {
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    std::scoped_lock lock(mutex_);
    if (cancelled_ || finished_) return false;
    if (!current_) {
      pending_pause_ = true;
      return true;
    }
    handle = current_;
  }
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Pause();
}

bool ControllerBinding::Resume() {
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    std::scoped_lock lock(mutex_);
    if (!current_) return std::exchange(pending_pause_, false);
    handle = current_;
  }
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->Resume();
}

bool ControllerBinding::IsPaused() const {
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    std::scoped_lock lock(mutex_);
    if (!current_) return pending_pause_;
    handle = current_;
  }
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && handle->op->is_paused();
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
  if (this != &other) Assign(other.Snapshot());
  return *this;
}

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept
    : ControllerInternal(other.TakeState()) {}

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept {
  if (this != &other) Assign(other.TakeState());
  return *this;
}

ControllerInternal::State ControllerInternal::Snapshot() const {
  State state;
  {
    std::scoped_lock lock(mutex_);
    state = {binding_,     reference_,         op_handle_,
             cached_handle_, bytes_transferred_, total_byte_count_,
             is_valid_};
  }
  auto active = state.op_handle;
  if (state.binding) {
    if (auto bound = state.binding->current()) active = std::move(bound);
  }
  if (active) {
    int64_t op_transferred = -1, op_total = -1;
    ReadHandle(*active, &op_transferred, &op_total);
    state.cached_handle = active;
    if (op_transferred >= 0) state.bytes_transferred = op_transferred;
    if (op_total >= 0) state.total_byte_count = op_total;
  }
  return state;
}

ControllerInternal::State ControllerInternal::TakeState() noexcept {
  std::scoped_lock lock(mutex_);
  State state{std::move(binding_),       std::move(reference_),
              std::move(op_handle_),     std::move(cached_handle_),
              bytes_transferred_,        total_byte_count_,
              is_valid_};
  reference_ = StorageReference();
  bytes_transferred_ = 0;
  total_byte_count_ = -1;
  is_valid_ = false;
  return state;
}

void ControllerInternal::Assign(State&& state) noexcept {
  State retired;
  std::scoped_lock lock(mutex_);
  retired = {std::move(binding_), std::move(reference_),
             std::move(op_handle_), std::move(cached_handle_)};
  binding_ = std::move(state.binding);
  reference_ = std::move(state.reference);
  op_handle_ = std::move(state.op_handle);
  cached_handle_ = std::move(state.cached_handle);
  bytes_transferred_ = state.bytes_transferred;
  total_byte_count_ = state.total_byte_count;
  is_valid_ = state.is_valid;
}

template <typename BindingFn, typename OpFn>
bool ControllerInternal::WithTarget(BindingFn binding_fn, OpFn op_fn) const {
  std::shared_ptr<ControllerBinding> binding;
  std::shared_ptr<RestOperation::OperationHandle> handle;
  {
    std::scoped_lock lock(mutex_);
    binding = binding_;
    handle = op_handle_;
  }
  if (binding) return binding_fn(*binding);
  if (!handle) return false;
  std::scoped_lock op_lock(handle->mutex);
  return handle->op != nullptr && op_fn(*handle->op);
}

bool ControllerInternal::Pause() {
  return WithTarget([](auto& b) { return b.Pause(); },
                    [](auto& op) { return op.Pause(); });
}

bool ControllerInternal::Resume() {
  return WithTarget([](auto& b) { return b.Resume(); },
                    [](auto& op) { return op.Resume(); });
}

bool ControllerInternal::Cancel() {
  return WithTarget([](auto& b) { return b.Cancel(); },
                    [](auto& op) { return op.Cancel(); });
}

bool ControllerInternal::is_paused() const {
  return WithTarget([](const auto& b) { return b.IsPaused(); },
                    [](const auto& op) { return op.is_paused(); });
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
  return ref_copy.is_valid()
             ? new StorageReferenceInternal(*ref_copy.internal_)
             : nullptr;
}

bool ControllerInternal::is_valid() const {
  std::scoped_lock lock(mutex_);
  return is_valid_ || binding_ != nullptr;
}

void ControllerInternal::SetBinding(std::shared_ptr<ControllerBinding> binding) {
  std::shared_ptr<ControllerBinding> old_binding;
  std::scoped_lock lock(mutex_);
  old_binding = std::move(binding_);
  binding_ = std::move(binding);
}

std::shared_ptr<RestOperation::OperationHandle>
ControllerInternal::operation_handle() const {
  std::scoped_lock lock(mutex_);
  return op_handle_;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) return true;
  StorageReference this_ref, other_ref;
  std::shared_ptr<RestOperation::OperationHandle> this_handle, other_handle;
  std::shared_ptr<ControllerBinding> this_binding, other_binding;
  bool this_valid = false, other_valid = false;
  {
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
  if (!this_valid && !other_valid) return this_ref == other_ref;
  if (this_valid != other_valid) return false;
  if (this_binding || other_binding) {
    return this_binding == other_binding && this_ref == other_ref;
  }
  return this_handle == other_handle && this_ref == other_ref;
}

void ControllerInternal::Initialize(const StorageReference& reference,
                                    RestOperation* operation) {
  auto new_handle = operation ? operation->operation_handle() : nullptr;
  int64_t op_transferred = -1, op_total = -1;
  if (new_handle) ReadHandle(*new_handle, &op_transferred, &op_total);
  Assign({nullptr,
          reference,
          new_handle,
          new_handle,
          (op_transferred >= 0) ? op_transferred : 0,
          (op_total >= 0) ? op_total : -1,
          new_handle != nullptr});
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
    if (auto bound = binding->current()) handle = std::move(bound);
  }
  if (!handle) {
    std::scoped_lock lock(mutex_);
    if (transferred) *transferred = bytes_transferred_;
    if (total) *total = total_byte_count_;
    return;
  }

  int64_t op_transferred = -1, op_total = -1;
  ReadHandle(*handle, &op_transferred, &op_total);
  const auto current_bound = binding ? binding->current() : nullptr;

  std::scoped_lock lock(mutex_);
  const bool still_bound =
      binding ? (binding_ == binding &&
                 (current_bound ? current_bound == handle
                                : op_handle_ == handle))
              : (!binding_ && op_handle_ == handle);
  if (still_bound) {
    if (cached_handle_ != handle) {
      cached_handle_ = handle;
      bytes_transferred_ = (op_transferred >= 0) ? op_transferred : 0;
      total_byte_count_ = (op_total >= 0) ? op_total : -1;
    } else {
      if (op_transferred > bytes_transferred_) bytes_transferred_ = op_transferred;
      if (op_total >= 0) total_byte_count_ = op_total;
    }
  }
  if (transferred) *transferred = bytes_transferred_;
  if (total) *total = total_byte_count_;
}

}  // namespace firebase::storage::internal
