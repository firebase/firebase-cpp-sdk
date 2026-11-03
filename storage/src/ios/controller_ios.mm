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
      action_generation(0),
      pending_valid(false),
      pending_cancel(false),
      pending_pause(false),
      pending_resume(false) {}

ControllerInternal::LifetimeHandle::~LifetimeHandle() {
  // The last shared_ptr is gone, so no other thread can reach this handle: the
  // cleanup callback only holds a weak_ptr, whose lock() now fails. If that
  // callback is running right now, the StorageInternal is inside its
  // destructor and has already marked storage_token dead, so RunWhileAlive()
  // is a no-op and the callback deletes `old_ctx`; otherwise RunWhileAlive()
  // unregisters and deletes `old_ctx` while the token mutex keeps the
  // StorageInternal alive. Either way this never touches a freed
  // StorageInternal.
  std::shared_ptr<StorageInternal::LifetimeToken> token;
  CleanupContext* old_ctx = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    token = std::move(storage_token);
    old_ctx = cleanup_ctx;
    cleanup_ctx = nullptr;
    storage = nullptr;
  }
  if (token && old_ctx != nullptr) {
    token->RunWhileAlive([old_ctx](StorageInternal& alive_storage) {
      alive_storage.cleanup().UnregisterObject(old_ctx);
      delete old_ctx;
    });
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
  // Registration with a previous StorageInternal that must be dropped. It is
  // unregistered only after self->mutex is released: the token mutex must not
  // be acquired under self->mutex (see the lock order in controller_ios.h).
  std::shared_ptr<StorageInternal::LifetimeToken> stale_token;
  CleanupContext* stale_ctx = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(self->mutex);
    // Register only when the binding actually changes: re-assigning a task
    // from the same, still-registered StorageInternal keeps the existing
    // registration instead of adding a duplicate. (If that StorageInternal's
    // cleanup callback already ran, it reset `storage`, so we re-register.)
    if (self->storage != new_storage) {
      stale_token = std::move(self->storage_token);
      stale_ctx = self->cleanup_ctx;
      self->cleanup_ctx = nullptr;
      self->storage = new_storage;
      if (new_storage != nullptr) {
        self->storage_token = new_storage->lifetime_token();
        const uint64_t generation = ++self->cleanup_generation;
        CleanupContext* ctx =
            new CleanupContext{std::weak_ptr<LifetimeHandle>(self), generation};
        self->cleanup_ctx = ctx;
        // Cleanup callbacks only run from ~StorageInternal(), after the
        // lifetime token was marked dead; UnregisterObject() is only called
        // under a live token, so it never waits for this callback. Hence the
        // callback may simply block on `mutex`.
        new_storage->cleanup().RegisterObject(ctx, [](void* obj) {
          std::unique_ptr<CleanupContext> ctx_holder(
              static_cast<CleanupContext*>(obj));
          std::shared_ptr<LifetimeHandle> locked = ctx_holder->handle.lock();
          if (!locked) {
            return;  // The handle is gone or being destroyed.
          }
          std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
          if (locked->cleanup_generation != ctx_holder->generation) {
            return;  // Superseded by a later AssignTask().
          }
          locked->cleanup_ctx = nullptr;
          locked->storage = nullptr;
          locked->storage_token.reset();
          locked->task_impl.reset();
          ++locked->action_generation;
          locked->pending_valid = false;
          locked->pending_cancel = false;
          locked->pending_pause = false;
          locked->pending_resume = false;
        });
      }
    }
    self->task_impl =
        std::make_unique<FIRStorageObservableTaskPointer>(new_task);
    self->pending_valid = false;
    self->pending_resume = false;
    const uint64_t gen = ++self->action_generation;
    // Native pause/resume/cancel calls are dispatched while holding
    // self->mutex (as in Pause(), Resume() and Cancel()) so their order on the
    // main queue matches the order of the C++ calls. self->mutex is
    // recursive, so the block may re-acquire it when
    // DispatchAsyncSafeMainQueue runs it synchronously on the main thread.
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
            if (locked->get_task() == new_task &&
                locked->action_generation == gen) {
              locked->pending_pause = false;
            }
          }
        });
      } else {
        self->pending_pause = false;
      }
    }
  }
  if (stale_token && stale_ctx != nullptr) {
    stale_token->RunWhileAlive([stale_ctx](StorageInternal& alive_storage) {
      alive_storage.cleanup().UnregisterObject(stale_ctx);
      delete stale_ctx;
    });
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
      // Keep pending_pause set until the main-queue block has run [task pause]
      // so is_paused() never transiently returns false right after Pause().
      state_->pending_pause = true;
      state_->pending_resume = false;
      const uint64_t gen = ++state_->action_generation;
      std::weak_ptr<LifetimeHandle> weak_state(state_);
      util::DispatchAsyncSafeMainQueue(^() {
        [task pause];
        if (std::shared_ptr<LifetimeHandle> locked = weak_state.lock()) {
          std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
          if (locked->get_task() == task && locked->action_generation == gen) {
            locked->pending_pause = false;
          }
        }
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    ++state_->action_generation;
    state_->pending_pause = true;
    state_->pending_resume = false;
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
      // Keep pending_resume set until the main-queue block has run
      // [task resume] so is_paused() never transiently returns true right
      // after Resume().
      state_->pending_pause = false;
      state_->pending_resume = true;
      const uint64_t gen = ++state_->action_generation;
      std::weak_ptr<LifetimeHandle> weak_state(state_);
      util::DispatchAsyncSafeMainQueue(^() {
        [task resume];
        if (std::shared_ptr<LifetimeHandle> locked = weak_state.lock()) {
          std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
          if (locked->get_task() == task && locked->action_generation == gen) {
            locked->pending_resume = false;
          }
        }
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    ++state_->action_generation;
    state_->pending_pause = false;
    state_->pending_resume = false;
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
      ++state_->action_generation;
      state_->pending_pause = false;
      state_->pending_resume = false;
      util::DispatchAsyncSafeMainQueue(^() {
        [task cancel];
      });
    } else {
      return false;
    }
  } else if (state_->pending_valid) {
    ++state_->action_generation;
    state_->pending_cancel = true;
    state_->pending_pause = false;
    state_->pending_resume = false;
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
    if (state_->pending_pause) {
      return true;
    }
    if (state_->pending_resume) {
      return false;
    }
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = state_->get_task();
    FIRStorageTaskSnapshot* snapshot = task ? task.snapshot : nil;
    return snapshot != nil ? snapshot.status == FIRStorageTaskStatusPause
                           : false;
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
  // Per firebase/storage/controller.h, total_byte_count() returns -1 when the
  // Controller is invalid or the total transfer size could not be determined
  // (matching the Android and desktop implementations).
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
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = nil;
    std::shared_ptr<StorageInternal::LifetimeToken> token;
    {
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      task = state_->get_task();
      token = state_->storage_token;
    }
    if (!task || !token) {
      return nullptr;
    }
    // Acquired after releasing state_->mutex (lock order: token mutex first).
    // Holding it keeps the StorageInternal alive while the new reference is
    // constructed.
    MutexLock token_lock(token->mutex);
    StorageInternal* storage = token->storage;
    if (!storage) {
      return nullptr;
    }
    FIRStorageTaskSnapshot* snapshot = task.snapshot;
    if (snapshot != nil && snapshot.reference != nil) {
      return new StorageReferenceInternal(
          storage,
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
        ++state_->action_generation;
        state_->pending_valid = true;
        state_->pending_cancel = false;
        state_->pending_pause = false;
        state_->pending_resume = false;
      }
    }
    if (needs_fresh_state) {
      state_ = std::make_shared<LifetimeHandle>();
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      state_->pending_valid = true;
    }
  } else if (state_) {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    ++state_->action_generation;
    state_->pending_valid = false;
    state_->pending_cancel = false;
    state_->pending_pause = false;
    state_->pending_resume = false;
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
