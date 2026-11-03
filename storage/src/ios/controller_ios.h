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

#ifndef FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

class ControllerInternal {
 public:
  struct LifetimeHandle {
    // std::recursive_mutex (not firebase::Mutex) so the StorageInternal cleanup
    // callback can try_lock() it; see AssignTask().
    std::recursive_mutex mutex;
    // > 0 while AssignTask() is blocked in CleanupNotifier::UnregisterObject()
    // with `mutex` held.
    std::atomic<int> unregistering{0};
    StorageInternal* _Nullable storage = nullptr;
    std::unique_ptr<FIRStorageObservableTaskPointer> task_impl;
    bool pending_valid = false;
    bool pending_cancel = false;
    bool pending_pause = false;

    LifetimeHandle();
    ~LifetimeHandle();

    LifetimeHandle(const LifetimeHandle&) = delete;
    LifetimeHandle& operator=(const LifetimeHandle&) = delete;

#ifdef __OBJC__
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable get_task()
        const {
      return task_impl
                 ? static_cast<
                       FIRStorageObservableTask<FIRStorageTaskManagement>*>(
                       task_impl->get())
                 : nil;
    }

    static void AssignTask(
        const std::shared_ptr<LifetimeHandle>& self,
        StorageInternal* _Nonnull new_storage,
        FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull new_task);
#endif  // __OBJC__
  };

  ControllerInternal();
  ControllerInternal(const ControllerInternal& other);
  ControllerInternal& operator=(const ControllerInternal& other);
  ControllerInternal(ControllerInternal&& other) noexcept;
  ControllerInternal& operator=(ControllerInternal&& other) noexcept;

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
  StorageReferenceInternal* _Nullable GetReference() const;

#ifdef __OBJC__
  // Assign implementation pointers.
  //
  // We don't assign these in the constructor because Controllers can be
  // constructed by the user of the library, and those controllers are not
  // associated with a specific operation until passed to a Read or Write call.
  void AssignTask(
      StorageInternal* _Nonnull storage,
      FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task_impl);
#endif  // __OBJC__

  bool is_valid() const;

  void set_pending_valid(bool pending_valid);

  std::shared_ptr<LifetimeHandle> lifetime_handle() const;

  bool operator==(const ControllerInternal& other) const;
  bool operator!=(const ControllerInternal& other) const {
    return !(*this == other);
  }

 private:
  mutable std::shared_ptr<LifetimeHandle> state_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_
