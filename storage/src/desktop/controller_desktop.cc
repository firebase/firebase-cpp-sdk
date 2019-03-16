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

#include <stdint.h>
#include <string.h>

namespace firebase {
namespace storage {
namespace internal {

ControllerInternal::ControllerInternal() : operation_(nullptr) {}

ControllerInternal::~ControllerInternal() {
  Initialize(StorageReference(), nullptr);
}

ControllerInternal::ControllerInternal(const ControllerInternal& other)
    : operation_(nullptr) {
  *this = other;
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  MutexLock lock(other.mutex_);
  Initialize(other.reference_, other.operation_);
  bytes_transferred_ = other.bytes_transferred_;
  total_byte_count_ = other.total_byte_count_;
  return *this;
}

// Pauses the operation currently in progress.
bool ControllerInternal::Pause() {
  MutexLock lock(mutex_);
  return operation_ && operation_->Pause();
}

// Resumes the operation that is paused.
bool ControllerInternal::Resume() {
  MutexLock lock(mutex_);
  return operation_ && operation_->Resume();
}

// Cancels the operation currently in progress.
bool ControllerInternal::Cancel() {
  MutexLock lock(mutex_);
  return operation_ && operation_->Cancel();
}

// Returns true if the operation is paused.
bool ControllerInternal::is_paused() const {
  MutexLock lock(mutex_);
  return operation_ && operation_->is_paused();
}

// Returns the number of bytes transferred so far.
int64_t ControllerInternal::bytes_transferred() {
  int64_t transferred;
  UpdateFromOperation(&transferred, nullptr);
  return transferred;
}

// Returns the total bytes to be transferred.
int64_t ControllerInternal::total_byte_count() {
  int64_t total;
  UpdateFromOperation(nullptr, &total);
  return total;
}

// Returns the StorageReference associated with this Controller.
StorageReferenceInternal* ControllerInternal::GetReference() const {
  MutexLock lock(mutex_);
  return reference_.is_valid()
             ? new StorageReferenceInternal(*reference_.internal_)
             : nullptr;
}

bool ControllerInternal::is_valid() {
  MutexLock lock(mutex_);
  return operation_ != nullptr;
}

void ControllerInternal::Initialize(const StorageReference& reference,
                                    RestOperation* operation) {
  MutexLock lock(mutex_);
  bytes_transferred_ = 0;
  total_byte_count_ = -1;
  reference_ = reference;
  if (operation_) operation_->cleanup().UnregisterObject(this);
  operation_ = operation;
  if (operation_) {
    operation_->cleanup().RegisterObject(this, RemoveRestOperationReference);
    UpdateFromOperation(nullptr, nullptr);
  }
}

void ControllerInternal::UpdateFromOperation(
    int64_t* transferred, int64_t* total) {
  MutexLock lock(mutex_);
  int64_t new_value = bytes_transferred_;
  if (operation_) new_value = operation_->bytes_transferred();
  if (new_value > 0 && new_value != bytes_transferred_) {
    bytes_transferred_ = new_value;
  }
  new_value = total_byte_count_;
  if (operation_) new_value = operation_->total_byte_count();
  if (new_value > 0 && new_value != total_byte_count_) {
    total_byte_count_ = new_value;
  }
  if (transferred) *transferred = bytes_transferred_;
  if (total) *total = total_byte_count_;
}

void ControllerInternal::RemoveRestOperationReference(void* object) {
  reinterpret_cast<ControllerInternal*>(object)->Initialize(StorageReference(),
                                                            nullptr);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
