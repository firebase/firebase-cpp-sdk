// Copyright 2018 Google LLC

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

namespace firebase::storage::internal {
class ListenerInternal;
}  // namespace firebase::storage::internal

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"

@interface FIRCPPStorageListenerHandle : NSObject

- (nonnull instancetype)
    initWithListener:(firebase::storage::Listener* _Nonnull)listener
             storage:
                 (firebase::storage::internal::StorageInternal* _Nonnull)storage
                task:(FIRStorageObservableTask<FIRStorageTaskManagement>*
                          _Nonnull)task;

- (void)markDetached;
- (void)waitForCallbacks;
- (void)notifyPaused;
- (void)notifyProgress:(FIRStorageTaskSnapshot* _Nullable)snapshot;

@end
#endif  // __OBJC__

namespace firebase::storage::internal {

OBJ_C_PTR_WRAPPER(NSString);
using FIRStorageHandlePointer = NSStringPointer;
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
  void AttachTask(
      const std::shared_ptr<StorageInternal::LifetimeToken>& storage_token,
      FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task);
  void DetachTask();
  const firebase::util::SafeReference<ListenerInternal>& safe_reference() const;
#endif  // __OBJC__

 private:
#ifdef __OBJC__
  struct DetachedTask {
    std::unique_ptr<FIRCPPStorageListenerHandlePointer> handle;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
    void* _Nullable cleanup_ctx = nullptr;
  };

  DetachedTask DetachTaskLocked();
  void FinishDetach(DetachedTask& detached);

  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable task() const {
    return task_ ? static_cast<
                       FIRStorageObservableTask<FIRStorageTaskManagement>*>(
                       task_->get())
                 : nil;
  }
  FIRStorageHandle _Nullable pause_observer_handle() const {
    return pause_observer_handle_ ? pause_observer_handle_->get() : nil;
  }
  FIRStorageHandle _Nullable progress_observer_handle() const {
    return progress_observer_handle_ ? progress_observer_handle_->get() : nil;
  }
  FIRCPPStorageListenerHandle* _Nullable listener_handle() const {
    return listener_handle_ ? listener_handle_->get() : nil;
  }
#endif  // __OBJC__

  mutable std::recursive_mutex mutex_;
  Listener* _Nonnull listener_;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token_;
  uint64_t cleanup_generation_ = 0;
  bool destroying_ = false;

  std::unique_ptr<FIRStorageObservableTaskPointer> task_;
  std::unique_ptr<FIRStorageHandlePointer> pause_observer_handle_;
  std::unique_ptr<FIRStorageHandlePointer> progress_observer_handle_;
  std::unique_ptr<FIRCPPStorageListenerHandlePointer> listener_handle_;

  struct SafeReferenceHolder;
  std::unique_ptr<SafeReferenceHolder> safe_reference_holder_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_IOS_LISTENER_IOS_H_
