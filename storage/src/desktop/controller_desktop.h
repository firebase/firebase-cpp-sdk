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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

// Shared, rebindable target for the Controller of an operation that may be
// retried. Every attempt publishes its RestOperation handle here so that all
// copies of the user's Controller always act on the current attempt, and a
// Cancel() also stops any further retries.
class ControllerBinding {
 public:
  ControllerBinding() = default;
  ControllerBinding(const ControllerBinding&) = delete;
  ControllerBinding& operator=(const ControllerBinding&) = delete;

  // Points the binding at a new attempt. If Cancel() was already called, the
  // new attempt is cancelled immediately so a racing Cancel() is never lost.
  void Bind(std::shared_ptr<RestOperation::OperationHandle> handle);

  // Marks the overall operation finished (the user-facing future completed).
  void MarkFinished();

  // Cancels the current attempt and prevents further retries. Returns true if
  // the current attempt was cancelled or a pending retry was prevented.
  bool Cancel();

  // Pauses the current attempt. Before the first attempt has been bound (it
  // is prepared and started on a background thread), records the pause so
  // Bind() applies it to that attempt.
  bool Pause();

  // Resumes the current attempt, or clears a pause recorded by Pause().
  bool Resume();

  // Whether the current attempt (or, before one is bound, the pending
  // operation) is paused.
  [[nodiscard]] bool IsPaused() const;

  // Whether Cancel() has been called.
  [[nodiscard]] bool is_cancelled() const;

  // Handle of the current attempt, or null if no attempt has been bound.
  [[nodiscard]] std::shared_ptr<RestOperation::OperationHandle> current() const;

 private:
  mutable std::mutex mutex_;
  std::shared_ptr<RestOperation::OperationHandle> current_;
  bool cancelled_ = false;
  bool finished_ = false;
  // Pause requested before the first attempt was bound; applied by Bind().
  bool pending_pause_ = false;
};

// Private Pimpl implementation class owned by firebase::storage::Controller
// via a heap-allocated pointer (Controller::internal_). Not part of the
// public SDK ABI or stack-allocated by user code.
class ControllerInternal {
 public:
  ControllerInternal();

  ~ControllerInternal();

  ControllerInternal(const ControllerInternal& other);

  ControllerInternal& operator=(const ControllerInternal& other);

  ControllerInternal(ControllerInternal&& other) noexcept;

  ControllerInternal& operator=(ControllerInternal&& other) noexcept;

  // Pauses the operation currently in progress.
  bool Pause();

  // Resumes the operation that is paused.
  bool Resume();

  // Cancels the operation currently in progress.
  bool Cancel();

  // Returns true if the operation is paused.
  [[nodiscard]] bool is_paused() const;

  // Returns the number of bytes transferred so far.
  [[nodiscard]] int64_t bytes_transferred() const;

  // Returns the total bytes to be transferred.
  [[nodiscard]] int64_t total_byte_count() const;

  // Returns a new StorageReferenceInternal for the reference associated with
  // this Controller, or null if there is none. Ownership is transferred to
  // the caller (Controller::GetReference() in storage/src/common/controller.cc
  // immediately wraps it in a StorageReference, as required by the
  // cross-platform ControllerInternal interface).
  [[nodiscard]] StorageReferenceInternal* GetReference() const;

  // Returns if a ControllerInternal is valid and ready to be used.
  [[nodiscard]] bool is_valid() const;

  bool operator==(const ControllerInternal& other) const;
  bool operator!=(const ControllerInternal& other) const {
    return !(*this == other);
  }

  // Initialization. Done outside the constructor, because the object is owned
  // (and constructed) by the caller, but needs to be initialized by the
  // storage library.
  // reference is copied by this class and operation is referenced through its
  // shared OperationHandle, so it's safe for operation to be deleted before
  // this class (final byte counts are read from the handle).
  // Clears any ControllerBinding.
  void Initialize(const StorageReference& reference, RestOperation* operation);

  // Routes Pause/Resume/Cancel/progress queries through `binding` (the current
  // attempt of a retried operation) instead of the initially bound operation.
  // Copies of this object share the binding.
  void SetBinding(std::shared_ptr<ControllerBinding> binding);

  // Handle of the RestOperation this controller was initialized with.
  [[nodiscard]] std::shared_ptr<RestOperation::OperationHandle>
  operation_handle() const;

 private:
  // Value snapshot of the guarded members, used to implement copy/move
  // without holding two ControllerInternal mutexes at once.
  struct State {
    std::shared_ptr<ControllerBinding> binding;
    StorageReference reference;
    std::shared_ptr<RestOperation::OperationHandle> op_handle;
    std::shared_ptr<RestOperation::OperationHandle> cached_handle;
    int64_t bytes_transferred = 0;
    int64_t total_byte_count = -1;
    bool is_valid = false;
  };

  explicit ControllerInternal(State&& state) noexcept;

  // Copies the guarded members under mutex_, refreshing the byte counts from
  // the active operation (the binding's current attempt if bound, otherwise
  // op_handle_), exactly like UpdateFromOperation().
  State Snapshot() const;

  // Moves the guarded members out under mutex_, leaving this object in the
  // same state as Initialize(StorageReference(), nullptr).
  State TakeState() noexcept;

  // Replaces the guarded members with `state` under mutex_.
  void Assign(State&& state) noexcept;

  // Update the internal state from the operation optionally returning
  // the current transfer state.
  void UpdateFromOperation(int64_t* transferred, int64_t* total) const;

  // Reads the current (or, once the operation is gone, final) byte counts
  // from handle under handle.mutex.
  static void ReadHandle(RestOperation::OperationHandle& handle,
                         int64_t* transferred, int64_t* total);

  // Guards reference_, op_handle_, cached_handle_, binding_,
  // bytes_transferred_, total_byte_count_, and is_valid_.
  mutable std::mutex mutex_;
  std::shared_ptr<ControllerBinding> binding_;
  StorageReference reference_;
  std::shared_ptr<RestOperation::OperationHandle> op_handle_;
  mutable std::shared_ptr<RestOperation::OperationHandle> cached_handle_;
  mutable int64_t bytes_transferred_ = 0;
  mutable int64_t total_byte_count_ = -1;
  bool is_valid_ = false;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_CONTROLLER_DESKTOP_H_
