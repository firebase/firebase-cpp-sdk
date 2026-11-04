// Copyright 2016 Google LLC

#include "storage/src/ios/controller_ios.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

#import "FirebaseStorage-Swift.h"
#include "app/src/util_ios.h"
#include "storage/src/ios/storage_ios.h"

namespace firebase::storage::internal {

ControllerInternal::LifetimeHandle::LifetimeHandle()
    : task_impl(std::make_unique<FIRStorageObservableTaskPointer>(nil)) {}

ControllerInternal::LifetimeHandle::~LifetimeHandle() {
  std::shared_ptr<StorageInternal::LifetimeToken> token;
  CleanupContext* old_ctx = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    token = std::move(storage_token);
    old_ctx = std::exchange(cleanup_ctx, nullptr);
    storage = nullptr;
  }
  if (token && old_ctx) {
    token->RunWhileAlive([old_ctx](StorageInternal& alive_storage) {
      alive_storage.cleanup().UnregisterObject(old_ctx);
      delete old_ctx;
    });
  }
  task_impl.reset();
}

void ControllerInternal::LifetimeHandle::AssignTask(
    const std::shared_ptr<LifetimeHandle>& self,
    StorageInternal* _Nullable new_storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable new_task) {
  if (!self) return;
  std::shared_ptr<StorageInternal::LifetimeToken> stale_token;
  CleanupContext* stale_ctx = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(self->mutex);
    if (self->storage != new_storage) {
      stale_token = std::move(self->storage_token);
      stale_ctx = std::exchange(self->cleanup_ctx, nullptr);
      self->storage = new_storage;
      if (new_storage) {
        self->storage_token = new_storage->lifetime_token();
        const uint64_t generation = ++self->cleanup_generation;
        auto* ctx =
            new CleanupContext{std::weak_ptr<LifetimeHandle>(self), generation};
        self->cleanup_ctx = ctx;
        new_storage->cleanup().RegisterObject(ctx, [](void* obj) {
          std::unique_ptr<CleanupContext> ctx_holder(
              static_cast<CleanupContext*>(obj));
          auto locked = ctx_holder->handle.lock();
          if (!locked) return;
          std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
          if (locked->cleanup_generation != ctx_holder->generation) return;
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
        std::weak_ptr<LifetimeHandle> weak_self(self);
        util::DispatchAsyncSafeMainQueue(^() {
          [new_task pause];
          if (auto locked = weak_self.lock()) {
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
  if (stale_token && stale_ctx) {
    stale_token->RunWhileAlive([stale_ctx](StorageInternal& alive_storage) {
      alive_storage.cleanup().UnregisterObject(stale_ctx);
      delete stale_ctx;
    });
  }
}

ControllerInternal::ControllerInternal()
    : state_(std::make_shared<LifetimeHandle>()) {}
ControllerInternal::ControllerInternal(const ControllerInternal&) = default;
ControllerInternal& ControllerInternal::operator=(const ControllerInternal&) =
    default;
ControllerInternal::ControllerInternal(ControllerInternal&&) noexcept = default;
ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&&) noexcept = default;
ControllerInternal::~ControllerInternal() = default;

bool ControllerInternal::Pause() {
  if (!state_) return false;
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  if (auto* task = state_->get_task()) {
    if (![task respondsToSelector:@selector(pause)]) return false;
    state_->pending_pause = true;
    state_->pending_resume = false;
    const uint64_t gen = ++state_->action_generation;
    std::weak_ptr<LifetimeHandle> weak_state(state_);
    util::DispatchAsyncSafeMainQueue(^() {
      [task pause];
      if (auto locked = weak_state.lock()) {
        std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
        if (locked->get_task() == task && locked->action_generation == gen) {
          locked->pending_pause = false;
        }
      }
    });
    return true;
  }
  if (!state_->pending_valid) return false;
  ++state_->action_generation;
  state_->pending_pause = true;
  state_->pending_resume = false;
  return true;
}

bool ControllerInternal::Resume() {
  if (!state_) return false;
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  if (auto* task = state_->get_task()) {
    if (![task respondsToSelector:@selector(resume)]) return false;
    state_->pending_pause = false;
    state_->pending_resume = true;
    const uint64_t gen = ++state_->action_generation;
    std::weak_ptr<LifetimeHandle> weak_state(state_);
    util::DispatchAsyncSafeMainQueue(^() {
      [task resume];
      if (auto locked = weak_state.lock()) {
        std::lock_guard<std::recursive_mutex> state_lock(locked->mutex);
        if (locked->get_task() == task && locked->action_generation == gen) {
          locked->pending_resume = false;
        }
      }
    });
    return true;
  }
  if (!state_->pending_valid) return false;
  ++state_->action_generation;
  state_->pending_pause = false;
  state_->pending_resume = false;
  return true;
}

bool ControllerInternal::Cancel() {
  if (!state_) return false;
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  if (auto* task = state_->get_task()) {
    if (![task respondsToSelector:@selector(cancel)]) return false;
    ++state_->action_generation;
    state_->pending_pause = false;
    state_->pending_resume = false;
    util::DispatchAsyncSafeMainQueue(^() {
      [task cancel];
    });
    return true;
  }
  if (!state_->pending_valid) return false;
  ++state_->action_generation;
  state_->pending_cancel = true;
  state_->pending_pause = false;
  state_->pending_resume = false;
  return true;
}

bool ControllerInternal::is_paused() const {
  if (!state_) return false;
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    if (state_->pending_pause) return true;
    if (state_->pending_resume) return false;
    auto* task = state_->get_task();
    FIRStorageTaskSnapshot* snapshot = task ? task.snapshot : nil;
    return snapshot && snapshot.status == FIRStorageTaskStatusPause;
  }
}

int64_t ControllerInternal::bytes_transferred() const {
  if (!state_) return 0;
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    auto* task = state_->get_task();
    NSProgress* progress = (task && task.snapshot) ? task.snapshot.progress : nil;
    return progress ? progress.completedUnitCount : 0;
  }
}

int64_t ControllerInternal::total_byte_count() const {
  if (!state_) return -1;
  @autoreleasepool {
    std::lock_guard<std::recursive_mutex> lock(state_->mutex);
    auto* task = state_->get_task();
    NSProgress* progress = (task && task.snapshot) ? task.snapshot.progress : nil;
    return progress ? progress.totalUnitCount : -1;
  }
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  if (!state_) return nullptr;
  @autoreleasepool {
    FIRStorageObservableTask<FIRStorageTaskManagement>* task = nil;
    std::shared_ptr<StorageInternal::LifetimeToken> token;
    {
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      task = state_->get_task();
      token = state_->storage_token;
    }
    if (!task || !token) return nullptr;
    MutexLock token_lock(token->mutex);
    StorageInternal* storage = token->storage;
    if (!storage) return nullptr;
    FIRStorageTaskSnapshot* snapshot = task.snapshot;
    return (snapshot && snapshot.reference)
               ? new StorageReferenceInternal(
                     storage, std::make_unique<FIRStorageReferencePointer>(
                                  snapshot.reference))
               : nullptr;
  }
}

void ControllerInternal::AssignTask(
    StorageInternal* _Nullable storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable task_impl) {
  if (!state_) state_ = std::make_shared<LifetimeHandle>();
  LifetimeHandle::AssignTask(state_, storage, task_impl);
}

bool ControllerInternal::is_valid() const {
  if (!state_) return false;
  std::lock_guard<std::recursive_mutex> lock(state_->mutex);
  return (state_->storage != nullptr && state_->get_task() != nil) ||
         state_->pending_valid;
}

void ControllerInternal::set_pending_valid(bool pending_valid) {
  if (pending_valid) {
    bool needs_fresh = !state_;
    if (state_) {
      std::lock_guard<std::recursive_mutex> lock(state_->mutex);
      if (state_->storage != nullptr || state_->get_task() != nil) {
        needs_fresh = true;
      } else {
        ++state_->action_generation;
        state_->pending_valid = true;
        state_->pending_cancel = false;
        state_->pending_pause = false;
        state_->pending_resume = false;
      }
    }
    if (needs_fresh) {
      auto fresh = std::make_shared<LifetimeHandle>();
      fresh->pending_valid = true;
      state_ = std::move(fresh);
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

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) return true;
  const bool this_valid = is_valid();
  const bool other_valid = other.is_valid();
  if (!this_valid && !other_valid) return true;
  if (this_valid != other_valid) return false;
  if (state_ == other.state_) return true;
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

}  // namespace firebase::storage::internal
