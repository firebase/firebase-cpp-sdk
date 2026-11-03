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

#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>  // NOLINT
#include <utility>

#import "FirebaseStorage-Swift.h"

#include "app/src/util_ios.h"
#include "storage/src/ios/storage_ios.h"

namespace firebase {
namespace storage {
namespace internal {

ControllerInternal::LifetimeHandle::LifetimeHandle()
    : storage(nullptr),
      task_impl(std::make_unique<FIRStorageObservableTaskPointer>(nil)),
      pending_valid(false),
      pending_cancel(false),
      pending_pause(false) {}

ControllerInternal::LifetimeHandle::~LifetimeHandle() {
  std::lock_guard<std::recursive_mutex> lock(mutex);
  if (storage != nullptr) {
    storage->cleanup().UnregisterObject(this);
    storage = nullptr;
  }
  task_impl.reset();
}

void ControllerInternal::LifetimeHandle::AssignTask(
    const std::shared_ptr<LifetimeHandle>& self,
    StorageInternal* _Nonnull new_storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull new_task) {
  if (!self) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(self->mutex);
  if (self->storage != nullptr && self->storage != new_storage) {
    // UnregisterObject() blocks while the old storage's cleanup callback for
    // this handle runs on another thread. That callback only try-locks
    // `mutex` and bails out while `unregistering > 0` (we overwrite the
    // binding right below anyway), so this cannot deadlock.
    self->unregistering.fetch_add(1);
    self->storage->cleanup().UnregisterObject(self.get());
    self->unregistering.fetch_sub(1);
  }
  self->storage = new_storage;
  self->task_impl = std::make_unique<FIRStorageObservableTaskPointer>(new_task);
  self->pending_valid = false;
  if (self->storage != nullptr) {
    std::weak_ptr<LifetimeHandle> weak_self(self);
    self->storage->cleanup().RegisterObject(
        self.get(), [weak_self](void* /*obj*/) {
          if (std::shared_ptr<LifetimeHandle> locked = weak_self.lock()) {
            std::unique_lock<std::recursive_mutex> state_lock(locked->mutex,
                                                              std::defer_lock);
            while (!state_lock.try_lock()) {
              if (locked->unregistering.load() > 0) {
                return;  // AssignTask() is rebinding this handle.
              }
              std::this_thread::yield();
            }
            locked->storage = nullptr;
            locked->task_impl.reset();
            locked->pending_valid = false;
            locked->pending_cancel = false;
            locked->pending_pause = false;
          }
        });
  }
  // Native pause/resume/cancel calls are dispatched while holding self->mutex
  // (as in Pause(), Resume() and Cancel()) so their order on the main queue
  // matches the order of the C++ calls. self->mutex is recursive, so the block
  // may re-acquire it when DispatchAsyncSafeMainQueue runs it synchronously on
  // the main thread.
  if (self->pending_cancel) {
    self->pending_cancel = false;
    self->pending_pause = false;
    if (new_task && [new_task respondsToSelector:@selector(cancel)]) {
      util::DispatchAsyncSafeMainQueue(^() {
        [new_task cancel];
      });
    }
  } else if (self->pending_pause) {
    if (new_task && [new_task respondsToSelector:@selector(pause)]) {
      // Leave pending_pause set until the native pause has actually been
      // issued, so is_paused() never transiently reports false between here
      // and the task's snapshot status transitioning to paused.
      std::weak_ptr<LifetimeHandle> weak_self(self);
      util::DispatchAsyncSafeMainQueue(^() {
        [new_task pause];
        if (std::shared_ptr<LifetimeHandle> locked = weak_self.lock()) {
          std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
          // Only clear the flag on behalf of this task: if the handle has
          // since been reassigned or cleaned up, its pending state belongs to
          // someone else.
          if (locked->get_task() == new_task) {
            locked->pending_pause = false;
          }
        }
      });
    } else {
      self->pending_pause = false;
    }
  }
}

ControllerInternal::ControllerInternal()
    : state_(std::make_shared<LifetimeHandle>()) {}

ControllerInternal::ControllerInternal(const ControllerInternal& other) =
    default;

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) = default;

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept =
    default;

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept = default;

ControllerInternal::~ControllerInternal() = default;

bool ControllerInternal::Pause() {
  if (!state_) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
  if (task) {
    if ([task respondsToSelector:@selector(pause)]) {
      util::DispatchAsyncSafeMainQueue(^() {
        [task pause];
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    state_->pending_pause = true;
  } else {
    return false;
  }
  return true;
}

bool ControllerInternal::Resume() {
  if (!state_) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
  if (task) {
    if ([task respondsToSelector:@selector(resume)]) {
      util::DispatchAsyncSafeMainQueue(^() {
        [task resume];
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    state_->pending_pause = false;
  } else {
    return false;
  }
  return true;
}

bool ControllerInternal::Cancel() {
  if (!state_) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
  if (task) {
    if ([task respondsToSelector:@selector(cancel)]) {
      util::DispatchAsyncSafeMainQueue(^() {
        [task cancel];
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    state_->pending_cancel = true;
    state_->pending_pause = false;
  } else {
    return false;
  }
  return true;
}

bool ControllerInternal::is_paused() const {
  if (!state_) {
    return false;
  }
  // task.snapshot and its properties are autoreleased; drain them here in
  // case the caller is a C++ thread without an autorelease pool.
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
    FIRStorageTaskSnapshot* snapshot = task ? task.snapshot : nil;
    return (snapshot != nil ? snapshot.status == FIRStorageTaskStatusPause
                            : false) ||
           state_->pending_pause;
  }
}

int64_t ControllerInternal::bytes_transferred() const {
  if (!state_) {
    return 0;
  }
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
    FIRStorageTaskSnapshot* snapshot = task ? task.snapshot : nil;
    NSProgress* progress = snapshot ? snapshot.progress : nil;
    if (!progress) {
      return 0;
    }
    return progress.completedUnitCount;
  }
}

int64_t ControllerInternal::total_byte_count() const {
  if (!state_) {
    return -1;
  }
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
    FIRStorageTaskSnapshot* snapshot = task ? task.snapshot : nil;
    NSProgress* progress = snapshot ? snapshot.progress : nil;
    if (!progress) {
      return -1;
    }
    return progress.totalUnitCount;
  }
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  if (!state_) {
    return nullptr;
  }
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
    if (!state_->storage || !task) {
      return nullptr;
    }
    FIRStorageTaskSnapshot* snapshot = task.snapshot;
    if (snapshot != nil && snapshot.reference != nil) {
      return new StorageReferenceInternal(
          state_->storage,
          std::make_unique<FIRStorageReferencePointer>(snapshot.reference));
    }
    return nullptr;
  }
}

void ControllerInternal::AssignTask(
    StorageInternal* storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* task_impl) {
  LifetimeHandle::AssignTask(lifetime_handle(), storage, task_impl);
}

bool ControllerInternal::is_valid() const {
  if (!state_) {
    return false;
  }
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  return (state_->storage != nullptr && state_->get_task() != nil) ||
         state_->pending_valid;
}

void ControllerInternal::set_pending_valid(bool pending_valid) {
  if (pending_valid) {
    bool needs_fresh_state = !state_;
    if (state_) {
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      if (state_->storage != nullptr || state_->get_task() != nil) {
        needs_fresh_state = true;
      } else {
        state_->pending_valid = true;
        state_->pending_cancel = false;
        state_->pending_pause = false;
      }
    }
    if (needs_fresh_state) {
      state_ = std::make_shared<LifetimeHandle>();
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      state_->pending_valid = true;
    }
  } else if (state_) {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    state_->pending_valid = false;
    state_->pending_cancel = false;
    state_->pending_pause = false;
  }
}

std::shared_ptr<ControllerInternal::LifetimeHandle>
ControllerInternal::lifetime_handle() const {
  if (!state_) {
    state_ = std::make_shared<LifetimeHandle>();
  }
  return state_;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) {
    return true;
  }
  const bool this_valid = is_valid();
  const bool other_valid = other.is_valid();
  if (!this_valid && !other_valid) {
    return true;
  }
  if (this_valid != other_valid) {
    return false;
  }
  if (state_ == other.state_) {
    return true;
  }
  StorageInternal* this_storage = nullptr;
  FIRStorageObservableTask<FIRStorageTaskManagement>* this_task = nil;
  if (state_) {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    this_storage = state_->storage;
    this_task = state_->get_task();
  }
  StorageInternal* other_storage = nullptr;
  FIRStorageObservableTask<FIRStorageTaskManagement>* other_task = nil;
  if (other.state_) {
    std::lock_guard<std::recursive_mutex> lock(other.state_->mutex);
    other_storage = other.state_->storage;
    other_task = other.state_->get_task();
  }
  return this_storage == other_storage && this_task != nil &&
         this_task == other_task;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
