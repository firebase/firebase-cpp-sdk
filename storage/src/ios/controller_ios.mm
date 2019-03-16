// Copyright 2016 Google LLC
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

#include "storage/src/ios/controller_ios.h"

#import "FIRStorage.h"
#import "FIRStorageDownloadTask.h"
#import "FIRStorageTask.h"
#import "FIRStorageTaskSnapshot.h"
#import "FIRStorageUploadTask.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace storage {
namespace internal {

ControllerInternal::ControllerInternal()
    : storage_(nullptr),
      task_impl_(new FIRStorageObservableTaskPointer(nil)),
      pending_valid_(false),
      pending_cancel_(false),
      pending_pause_(false) {}

ControllerInternal::ControllerInternal(const ControllerInternal& other)
    : storage_(other.storage_),
      task_impl_(new FIRStorageObservableTaskPointer(other.task_impl())),
      pending_valid_(other.pending_valid_),
      pending_cancel_(other.pending_cancel_),
      pending_pause_(other.pending_pause_) {}

ControllerInternal::~ControllerInternal() {
  // Destructor is necessary for ARC garbage collection.
}

bool ControllerInternal::Pause() {
  MutexLock mutex(pending_calls_mutex_);
  if (task_impl()) {
    util::DispatchAsyncSafeMainQueue(^() {
      [task_impl() pause];
    });
  } else {
    pending_pause_ = true;
  }
  return true;
}

bool ControllerInternal::Resume() {
  MutexLock mutex(pending_calls_mutex_);
  if (task_impl()) {
    util::DispatchAsyncSafeMainQueue(^() {
      [task_impl() resume];
    });
  } else {
    pending_pause_ = false;
  }
  return true;
}

bool ControllerInternal::Cancel() {
  MutexLock mutex(pending_calls_mutex_);
  if (task_impl()) {
    util::DispatchAsyncSafeMainQueue(^() {
      [task_impl() cancel];
    });
  } else {
    pending_cancel_ = true;
  }
  return true;
}

bool ControllerInternal::is_paused() const {
  return (task_impl() ? task_impl().snapshot.status == FIRStorageTaskStatusPause : false) ||
         pending_pause_;
}

int64_t ControllerInternal::bytes_transferred() const {
  return task_impl() ? task_impl().snapshot.progress.completedUnitCount : 0;
}

int64_t ControllerInternal::total_byte_count() const {
  return task_impl() ? task_impl().snapshot.progress.totalUnitCount : 0;
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  FIRStorageTaskSnapshot* snapshot = task_impl() ? task_impl().snapshot : nil;
  if (snapshot) {
    return new StorageReferenceInternal(storage_,
                                        MakeUnique<FIRStorageReferencePointer>(snapshot.reference));
  }
  return nullptr;
}

void ControllerInternal::AssignTask(StorageInternal* storage,
                                    FIRStorageObservableTask<FIRStorageTaskManagement>* task_impl) {
  MutexLock mutex(pending_calls_mutex_);
  storage_ = storage;
  task_impl_.reset(new FIRStorageObservableTaskPointer(task_impl));
  if (pending_cancel_) {
    pending_cancel_ = false;
    Cancel();
  } else if (pending_pause_) {
    pending_pause_ = false;
    Pause();
  }
}

bool ControllerInternal::is_valid() const { return (storage_ && task_impl()) || pending_valid_; }

}  // namespace internal
}  // namespace storage
}  // namespace firebase
