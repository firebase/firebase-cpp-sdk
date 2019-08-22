// Copyright 2018 Google LLC
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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_

#include "app/memory/unique_ptr.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
#import "FIRStorageConstants.h"
#import "FIRStorageObservableTask.h"
#import "FIRStorageTask.h"

// Obj-C object that provides a handle to ListenerInternal.  This is used to
// synchronize callbacks from blocks with the potential destruction of the
// Listener.
@class FIRCPPStorageListenerHandle;
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class ::firebase::storage::internal::NSRecursiveLockPointer,
// which is a C++-compatible wrapper around the NSRecursiveLock Obj-C class,
// used by ListenerInternal.
OBJ_C_PTR_WRAPPER(NSRecursiveLock);
// This defines the class ::firebase::storage::internal::NSStringPointer, which
// is a C++-compatible wrapper around the NSString Obj-C class, which is needed
// to use the FIRStorageHandler Obj-C class used by ListenerInternal.
OBJ_C_PTR_WRAPPER(NSString);
// FIRStorageHandle is typedefed as a NSString*, so we can't use
// OBJ_C_PTR_WRAPPER to wrap it. Instead, alias the type.
typedef NSStringPointer FIRStorageHandlePointer;
// This defines the class FIRCPPStorageListenerHandlePointer, which is a
// C++-compatible wrapper around the FIRCPPStorageListenerHandlePointer Obj-C
// class.
OBJ_C_PTR_WRAPPER(FIRCPPStorageListenerHandle);

class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* _Nonnull listener);
  ~ListenerInternal();

#ifdef __OBJC__
  // Attach this listener to the specified task.
  void AttachTask(StorageInternal* _Nonnull storage,
                  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task);

  // Remove this listener from the currently attached task.
  void DetachTask();
#endif  // __OBJC__

 private:
  Listener* _Nonnull listener_;
  StorageInternal* _Nonnull storage_;
#ifdef __OBJC__
  // Guards task_, pause_observer_handle_, progress_observer_handle_,
  // listener_handle_.
  NSRecursiveLock* _Nonnull listener_handle_lock() const { return listener_handle_lock_->get(); }
  // Task monitored by this listener.
  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable task() const {
    return (FIRStorageObservableTask<FIRStorageTaskManagement>*)(task_->get());
  }
  // Handle to the pause observer block, used to unregister pause notifications.
  FIRStorageHandle _Nullable pause_observer_handle() const { return pause_observer_handle_->get(); }
  // Handle to the progress observer block, used to unregister progress
  // notifications.
  FIRStorageHandle _Nullable progress_observer_handle() const {
    return progress_observer_handle_->get();
  }
  // Obj-C reference to this object.
  FIRCPPStorageListenerHandle* _Nullable listener_handle() const { return listener_handle_->get(); }
#endif  // __OBJC__

  // Guards task_, pause_observer_handle_, progress_observer_handle_,
  // listener_handle.
  UniquePtr<NSRecursiveLockPointer> listener_handle_lock_;
  // Task monitored by this listener.
  UniquePtr<FIRStorageObservableTaskPointer> task_;
  // Handle to the pause observer block, used to unregister pause notifications.
  UniquePtr<FIRStorageHandlePointer> pause_observer_handle_;
  // Handle to the progress observer block, used to unregister progress
  // notifications.
  UniquePtr<FIRStorageHandlePointer> progress_observer_handle_;
  // Obj-C reference to this object.
  UniquePtr<FIRCPPStorageListenerHandlePointer> listener_handle_;
  // Previously seen byte count from progress updates.
  // Used to debounce duplicate progress updates.
  int64_t previous_progress_count_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_LISTENER_IOS_H_
