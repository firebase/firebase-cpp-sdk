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

#include "storage/src/ios/listener_ios.h"

#include "app/src/mutex.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/ios/controller_ios.h"
#include "storage/src/ios/storage_ios.h"


@interface FIRCPPStorageListenerHandle : NSObject
@property(atomic, nullable) firebase::storage::internal::ListenerInternal
    *listenerInternal;
@end


@implementation FIRCPPStorageListenerHandle
@end


namespace firebase {
namespace storage {
namespace internal {

ListenerInternal::ListenerInternal(Listener* _Nonnull listener)
    : listener_(listener), storage_(nullptr), previous_progress_count_(-1) {
  task_.reset(new FIRStorageObservableTaskPointer(nil));
  pause_observer_handle_.reset(new FIRStorageHandlePointer(nil));
  progress_observer_handle_.reset(new FIRStorageHandlePointer(nil));
  listener_handle_.reset(new FIRCPPStorageListenerHandlePointer(nil));
  listener_handle_lock_.reset(new NSRecursiveLockPointer([[NSRecursiveLock alloc] init]));
}

ListenerInternal::~ListenerInternal() {
  DetachTask();
}

// Attach this listener to the specified task.
void ListenerInternal::AttachTask(
    StorageInternal* _Nonnull storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  DetachTask();
  FIRCPPStorageListenerHandle* listener_handle = [[FIRCPPStorageListenerHandle alloc] init];
  listener_handle.listenerInternal = this;

  // Copy the lock so that "this" isn't referenced from the pause and progress handler blocks.

  NSRecursiveLock* my_listener_handle_lock = listener_handle_lock();
  [my_listener_handle_lock lock];
  listener_handle_.reset(new FIRCPPStorageListenerHandlePointer(listener_handle));
  task_.reset(new FIRStorageObservableTaskPointer(task));
  previous_progress_count_ = -1;

  FIRStorageVoidSnapshot pause_handler = ^(FIRStorageTaskSnapshot* snapshot) {
    [my_listener_handle_lock lock];
    ListenerInternal* listener_internal = listener_handle.listenerInternal;
    if (listener_internal) {
      Controller controller;
      controller.internal_->AssignTask(storage, snapshot.task);
      listener_internal->listener_->OnPaused(&controller);
    }
    [my_listener_handle_lock unlock];
  };

  FIRStorageVoidSnapshot progress_handler = ^(FIRStorageTaskSnapshot* snapshot) {
    [my_listener_handle_lock lock];
    ListenerInternal* listener_internal = listener_handle.listenerInternal;
    if (listener_internal) {
      // FIRStorageTaskSnapshot.progress can be nil when the task is starting so ignore all
      // progress callbacks until progress can be reported.
      NSProgress *progress = snapshot.progress;
      if (progress != nil && progress.totalUnitCount &&
          listener_internal->previous_progress_count_ != progress.completedUnitCount) {
        listener_internal->previous_progress_count_ = progress.completedUnitCount;
        Controller controller;
        controller.internal_->AssignTask(storage, snapshot.task);
        listener_internal->listener_->OnProgress(&controller);
      }
    }
    [my_listener_handle_lock unlock];
  };

  pause_observer_handle_.reset(new FIRStorageHandlePointer(
      [task observeStatus:FIRStorageTaskStatusPause handler:pause_handler]));
  progress_observer_handle_.reset(new FIRStorageHandlePointer(
      [task observeStatus:FIRStorageTaskStatusProgress handler:progress_handler]));
  [my_listener_handle_lock unlock];
}

// Remove this listener from the currently attached task.
void ListenerInternal::DetachTask() {
  [listener_handle_lock() lock];
  if (listener_handle()) {
    listener_handle().listenerInternal = nullptr;
    listener_handle_.reset(new FIRCPPStorageListenerHandlePointer(nil));
  }
  if (pause_observer_handle()) {
    [task() removeObserverWithHandle:pause_observer_handle()];
    pause_observer_handle_.reset(new FIRStorageHandlePointer(nil));
  }
  if (progress_observer_handle()) {
    [task() removeObserverWithHandle:progress_observer_handle()];
    progress_observer_handle_.reset(new FIRStorageHandlePointer(nil));
  }
  [listener_handle_lock() unlock];
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
