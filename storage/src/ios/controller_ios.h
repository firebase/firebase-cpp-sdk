// Copyright 2016 Google LLC

#ifndef FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_

#include <cstdint>
#include <memory>
#include <mutex>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/ios/storage_ios.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
#import "FirebaseStorage-Swift.h"
#endif  // __OBJC__

namespace firebase::storage::internal {

class ControllerInternal {
 public:
  struct LifetimeHandle {
    struct CleanupContext {
      std::weak_ptr<LifetimeHandle> handle;
      uint64_t generation;
    };

    std::recursive_mutex mutex;
    StorageInternal* _Nullable storage = nullptr;
    std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
    CleanupContext* _Nullable cleanup_ctx = nullptr;
    uint64_t cleanup_generation = 0;
    std::unique_ptr<FIRStorageObservableTaskPointer> task_impl;
    uint64_t action_generation = 0;
    bool pending_valid = false;
    bool pending_cancel = false;
    bool pending_pause = false;
    bool pending_resume = false;

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
        StorageInternal* _Nullable new_storage,
        FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable new_task);
#endif  // __OBJC__
  };

  ControllerInternal();
  ControllerInternal(const ControllerInternal& other);
  ControllerInternal& operator=(const ControllerInternal& other);
  ControllerInternal(ControllerInternal&& other) noexcept;
  ControllerInternal& operator=(ControllerInternal&& other) noexcept;
  ~ControllerInternal();

  bool Pause();
  bool Resume();
  bool Cancel();
  bool is_paused() const;
  int64_t bytes_transferred() const;
  int64_t total_byte_count() const;
  StorageReferenceInternal* _Nullable GetReference() const;

#ifdef __OBJC__
  void AssignTask(
      StorageInternal* _Nullable storage,
      FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable task_impl);
#endif  // __OBJC__

  bool is_valid() const;
  void set_pending_valid(bool pending_valid);
  std::shared_ptr<LifetimeHandle> lifetime_handle() const noexcept {
    return state_;
  }

  bool operator==(const ControllerInternal& other) const;
  bool operator!=(const ControllerInternal& other) const {
    return !(*this == other);
  }

 private:
  std::shared_ptr<LifetimeHandle> state_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_IOS_CONTROLLER_IOS_H_
