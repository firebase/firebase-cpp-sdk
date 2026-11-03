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

#ifndef FIREBASE_STORAGE_SRC_IOS_LISTENER_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_LISTENER_IOS_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"

namespace firebase {
namespace storage {
namespace internal {
class ListenerInternal;
}  // namespace internal
}  // namespace storage
}  // namespace firebase

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"

// Obj-C object that owns the per-task synchronization lock and coordinates
// callbacks from observer blocks with Listener detachment and destruction.
@interface FIRCPPStorageListenerHandle : NSObject

- (nonnull instancetype)
    initWithListener:(firebase::storage::Listener* _Nonnull)listener
             storage:
                 (firebase::storage::internal::StorageInternal* _Nonnull)storage
                task:(FIRStorageObservableTask<FIRStorageTaskManagement>*
                          _Nonnull)task;

// Marks the handle detached under its internal recursive mutex so no
// subsequent observer blocks will invoke Listener callbacks.
- (void)markDetached;

// Waits until any in-flight callbacks on other threads have completed.
- (void)waitForCallbacks;

- (void)notifyPaused;

- (void)notifyProgress:(FIRStorageTaskSnapshot* _Nullable)snapshot;

@end
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class ::firebase::storage::internal::NSStringPointer, which
// is a C++-compatible wrapper around the NSString Obj-C class, which is needed
// to use the FIRStorageHandle Obj-C type used by ListenerInternal.
OBJ_C_PTR_WRAPPER(NSString);
// FIRStorageHandle is typedefed as an NSString*, so alias the wrapper type.
using FIRStorageHandlePointer = NSStringPointer;
// This defines the class FIRCPPStorageListenerHandlePointer, which is a
// C++-compatible wrapper around the FIRCPPStorageListenerHandle Obj-C class.
OBJ_C_PTR_WRAPPER(FIRCPPStorageListenerHandle);

class ListenerInternal {
 public:
  explicit ListenerInternal(Listener* _Nonnull listener);
  ~ListenerInternal();

  ListenerInternal(const ListenerInternal&) = delete;
  ListenerInternal& operator=(const ListenerInternal&) = delete;
  ListenerInternal(ListenerInternal&&) = delete;
  ListenerInternal& operator=(ListenerInternal&&) = delete;

  void Detach();

#ifdef __OBJC__
  // Attach this listener to the specified task, if the StorageInternal that
  // owns `storage_token` is still alive. Must not be called with
  // storage_token->mutex held (see the lock order on mutex_).
  void AttachTask(
      const std::shared_ptr<StorageInternal::LifetimeToken>& storage_token,
      FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task);

  // Remove this listener from the currently attached task.
  void DetachTask();

  // Thread-safe reference for asynchronous main-queue / completion blocks.
  // Defined in listener_ios.mm, where SafeReferenceHolder is complete.
  const firebase::util::SafeReference<ListenerInternal>& safe_reference()
      const;
#endif  // __OBJC__

 private:
#ifdef __OBJC__
  // State taken out of the listener by DetachTaskLocked(); FinishDetach()
  // completes the detach once mutex_ has been released.
  struct DetachedTask {
    // Retains the Obj-C handle across FinishDetach() so -waitForCallbacks
    // never messages a deallocated instance after observer blocks are removed.
    std::unique_ptr<FIRCPPStorageListenerHandlePointer> handle;
    // Token of the StorageInternal this listener was registered with, or null
    // if it was not registered.
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
    // Opaque CleanupContext* registered with storage_token's CleanupNotifier.
    void* _Nullable cleanup_ctx = nullptr;
  };

  // Detaches the current task (if any) under mutex_. Does not wait for
  // in-flight callbacks or unregister from the StorageInternal: both must
  // happen after releasing mutex_, via FinishDetach().
  DetachedTask DetachTaskLocked();

  // Unregisters from the old StorageInternal (through its lifetime token, so a
  // dying StorageInternal is never touched) and waits for in-flight callbacks
  // of the detached handle. Must be called without holding mutex_.
  void FinishDetach(DetachedTask& detached);

  // Task monitored by this listener.
  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable task() const {
    return task_ ? static_cast<
                       FIRStorageObservableTask<FIRStorageTaskManagement>*>(
                       task_->get())
                 : nil;
  }
  // Handle to the pause observer block, used to unregister pause notifications.
  FIRStorageHandle _Nullable pause_observer_handle() const {
    return pause_observer_handle_ ? pause_observer_handle_->get() : nil;
  }
  // Handle to the progress observer block, used to unregister progress
  // notifications.
  FIRStorageHandle _Nullable progress_observer_handle() const {
    return progress_observer_handle_ ? progress_observer_handle_->get() : nil;
  }
  // Obj-C handle for the currently attached task.
  FIRCPPStorageListenerHandle* _Nullable listener_handle() const {
    return listener_handle_ ? listener_handle_->get() : nil;
  }
#endif  // __OBJC__

  // Guards every field below except safe_reference_holder_ (set only in the
  // constructor).
  //
  // Lock order: the SafeReference lock (held by SafeReference::Run() callers),
  // then a StorageInternal::LifetimeToken mutex, then mutex_. AttachTask() and
  // FinishDetach() acquire the token mutex themselves, before mutex_, and must
  // not be called with it held. The StorageInternal cleanup callback acquires
  // the SafeReference lock and mutex_ (blocking); that is deadlock-free
  // because CleanupNotifier::UnregisterObject() is only called under a live
  // StorageInternal::LifetimeToken (RunWhileAlive()), when no cleanup callback
  // can be in flight.
  mutable std::recursive_mutex mutex_;
  Listener* _Nonnull listener_;
  // Lifetime token of the StorageInternal whose cleanup() this listener is
  // registered with, or null when not registered.
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token_;
  // Incremented on every cleanup registration so a callback belonging to a
  // superseded registration can tell it no longer owns the attachment.
  uint64_t cleanup_generation_ = 0;
  bool destroying_ = false;

  // Task monitored by this listener.
  std::unique_ptr<FIRStorageObservableTaskPointer> task_;
  // Handle to the pause observer block, used to unregister pause notifications.
  std::unique_ptr<FIRStorageHandlePointer> pause_observer_handle_;
  // Handle to the progress observer block, used to unregister progress
  // notifications.
  std::unique_ptr<FIRStorageHandlePointer> progress_observer_handle_;
  // Per-task Obj-C callback synchronization handle.
  std::unique_ptr<FIRCPPStorageListenerHandlePointer> listener_handle_;

  // Owns the firebase::util::SafeReference<ListenerInternal> handed to async
  // blocks. firebase::util::SafeReference is only declared for Obj-C++ (it
  // lives inside the __OBJC__ section of app/src/util_ios.h), so it is held
  // behind this opaque type. Every data member of ListenerInternal MUST be
  // declared unconditionally so the class has an identical size and layout in
  // pure C++ TUs (e.g. storage/src/common/listener.cc, which allocates and
  // deletes ListenerInternal) and Obj-C++ TUs. The holder is defined, created
  // and destroyed only in listener_ios.mm, where the constructor and
  // destructor of ListenerInternal are defined. Never null after construction.
  struct SafeReferenceHolder;
  std::unique_ptr<SafeReferenceHolder> safe_reference_holder_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_LISTENER_IOS_H_
