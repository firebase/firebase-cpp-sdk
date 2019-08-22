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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_CONTROLLER_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_CONTROLLER_IOS_H_

#include <cstdint>

#include "app/memory/unique_ptr.h"
#include "app/src/util_ios.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
#import "FIRStorage.h"
#import "FIRStorageDownloadTask.h"
#import "FIRStorageTask.h"
#import "FIRStorageTaskSnapshot.h"
#import "FIRStorageUploadTask.h"
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

class ControllerInternal {
 public:
  ControllerInternal();
  ControllerInternal(const ControllerInternal &other);

  ~ControllerInternal();

  // Pauses the operation currently in progress.
  bool Pause();

  // Resumes the operation that is paused.
  bool Resume();

  // Cancels the operation currently in progress.
  bool Cancel();

  // Returns true if the operation is paused.
  bool is_paused() const;

  // Returns the number of bytes transferred so far.
  int64_t bytes_transferred() const;

  // Returns the total bytes to be transferred.
  int64_t total_byte_count() const;

  // Returns the StorageReference associated with this Controller.
  StorageReferenceInternal *_Nullable GetReference() const;

#ifdef __OBJC__
  // Assign implementation pointers.
  //
  // We don't assign these in the constructor because Controllers can be
  // constructed by the user of the library, and those controllers are not
  // associated with a specific operation until passed to a Read or Write call.
  void AssignTask(
      StorageInternal *_Nonnull storage,
      FIRStorageObservableTask<FIRStorageTaskManagement> *_Nonnull task_impl);
#endif  // __OBJC__

  bool is_valid() const;

  void set_pending_valid(bool pending_valid) { pending_valid_ = pending_valid; }

 private:
#ifdef __OBJC__
  FIRStorageObservableTask<FIRStorageTaskManagement> *_Nullable task_impl()
      const {
    return (FIRStorageObservableTask<FIRStorageTaskManagement> *)
        task_impl_->get();
  }
#endif  // __OBJC__
  StorageInternal *_Nullable storage_;

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRStorageObservableTaskPointer> task_impl_;

  Mutex pending_calls_mutex_;

  bool pending_valid_;
  bool pending_cancel_;
  bool pending_pause_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_CONTROLLER_IOS_H_
