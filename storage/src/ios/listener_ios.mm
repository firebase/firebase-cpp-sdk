// Copyright 2018 Google LLC

#include "storage/src/ios/listener_ios.h"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/ios/controller_ios.h"
#include "storage/src/ios/storage_ios.h"

namespace {

class ActiveCallbackScope {
 public:
  ActiveCallbackScope(std::mutex& mutex, std::condition_variable& cv,
                      std::vector<std::thread::id>& active_threads)
      : mutex_(mutex),
        cv_(cv),
        active_threads_(active_threads),
        thread_id_(std::this_thread::get_id()) {}

  ~ActiveCallbackScope() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      auto it = std::find(active_threads_.begin(), active_threads_.end(),
                          thread_id_);
      if (it != active_threads_.end()) active_threads_.erase(it);
    }
    cv_.notify_all();
  }

  ActiveCallbackScope(const ActiveCallbackScope&) = delete;
  ActiveCallbackScope& operator=(const ActiveCallbackScope&) = delete;

 private:
  std::mutex& mutex_;
  std::condition_variable& cv_;
  std::vector<std::thread::id>& active_threads_;
  const std::thread::id thread_id_;
};

}  // namespace

@implementation FIRCPPStorageListenerHandle {
  std::recursive_mutex _lock;
  firebase::storage::Listener* _Nullable _listener;
  firebase::storage::internal::StorageInternal* _Nullable _storage;
  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable _task;
  int64_t _previousProgressCount;
  int64_t _previousTotalByteCount;
  BOOL _detached;
  std::mutex _callbackMutex;
  std::condition_variable _callbackCv;
  std::vector<std::thread::id> _activeCallbackThreads;
}

- (nonnull instancetype)
    initWithListener:(firebase::storage::Listener* _Nonnull)listener
             storage:
                 (firebase::storage::internal::StorageInternal* _Nonnull)storage
                task:(FIRStorageObservableTask<FIRStorageTaskManagement>*
                          _Nonnull)task {
  self = [super init];
  if (self) {
    _listener = listener;
    _storage = storage;
    _task = task;
    _previousProgressCount = -1;
    _previousTotalByteCount = -1;
    _detached = NO;
  }
  return self;
}

- (void)markDetached {
  std::lock_guard<std::recursive_mutex> lock(_lock);
  _detached = YES;
  _listener = nullptr;
  _storage = nullptr;
  _task = nil;
}

- (void)waitForCallbacks {
  const auto self_thread = std::this_thread::get_id();
  std::unique_lock<std::mutex> cb_lock(_callbackMutex);
  _callbackCv.wait(cb_lock, [self, self_thread]() {
    return std::all_of(_activeCallbackThreads.begin(),
                       _activeCallbackThreads.end(),
                       [self_thread](const auto& tid) { return tid == self_thread; });
  });
}

- (BOOL)hasActiveCallbacks {
  std::lock_guard<std::mutex> cb_lock(_callbackMutex);
  return !_activeCallbackThreads.empty();
}

- (void)notifyPaused {
  __attribute__((objc_precise_lifetime)) FIRCPPStorageListenerHandle*
      strong_self = self;
  (void)strong_self;

  firebase::storage::Listener* cur_listener = nullptr;
  firebase::storage::Controller controller;
  {
    std::lock_guard<std::recursive_mutex> lock(_lock);
    if (!_detached && _listener && _storage && _task) {
      cur_listener = _listener;
      controller.internal_->AssignTask(_storage, _task);
      std::lock_guard<std::mutex> cb_lock(_callbackMutex);
      _activeCallbackThreads.push_back(std::this_thread::get_id());
    }
  }
  if (!cur_listener) return;
  ActiveCallbackScope scope(_callbackMutex, _callbackCv, _activeCallbackThreads);
  cur_listener->OnPaused(&controller);
}

- (void)notifyProgress:(FIRStorageTaskSnapshot* _Nullable)snapshot {
  __attribute__((objc_precise_lifetime)) FIRCPPStorageListenerHandle*
      strong_self = self;
  (void)strong_self;

  firebase::storage::Listener* cur_listener = nullptr;
  firebase::storage::Controller controller;
  {
    std::lock_guard<std::recursive_mutex> lock(_lock);
    if (!_detached && _listener && _storage && _task) {
      NSProgress* progress = snapshot ? snapshot.progress : nil;
      if (progress &&
          (_previousProgressCount != progress.completedUnitCount ||
           _previousTotalByteCount != progress.totalUnitCount)) {
        _previousProgressCount = progress.completedUnitCount;
        _previousTotalByteCount = progress.totalUnitCount;
        cur_listener = _listener;
        controller.internal_->AssignTask(_storage, _task);
        std::lock_guard<std::mutex> cb_lock(_callbackMutex);
        _activeCallbackThreads.push_back(std::this_thread::get_id());
      }
    }
  }
  if (!cur_listener) return;
  ActiveCallbackScope scope(_callbackMutex, _callbackCv, _activeCallbackThreads);
  cur_listener->OnProgress(&controller);
}

@end

namespace firebase::storage::internal {

struct ListenerInternal::SafeReferenceHolder {
  struct CleanupContext {
    firebase::util::SafeReference<ListenerInternal> weak_self;
    std::shared_ptr<FIRCPPStorageListenerHandlePointer> handle;
    uint64_t generation;
  };
  explicit SafeReferenceHolder(ListenerInternal* _Nonnull owner) : ref(owner) {}
  firebase::util::SafeReference<ListenerInternal> ref;
  CleanupContext* _Nullable cleanup_ctx = nullptr;
  std::vector<std::shared_ptr<FIRCPPStorageListenerHandlePointer>>
      detaching_handles;
};

ListenerInternal::ListenerInternal(Listener* _Nonnull listener)
    : listener_(listener),
      safe_reference_holder_(std::make_unique<SafeReferenceHolder>(this)) {}

const firebase::util::SafeReference<ListenerInternal>&
ListenerInternal::safe_reference() const {
  return safe_reference_holder_->ref;
}

ListenerInternal::~ListenerInternal() {
  safe_reference_holder_->ref.Invalidate();
  DetachedTask detached;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    destroying_ = true;
    detached = DetachTaskLocked();
  }
  FinishDetach(detached);
}

void ListenerInternal::Detach() { DetachTask(); }

void ListenerInternal::AttachTask(
    const std::shared_ptr<StorageInternal::LifetimeToken>& storage_token,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  DetachTask();
  if (!storage_token || !task) return;

  DetachedTask raced;
  storage_token->RunWhileAlive([&](StorageInternal& storage) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (destroying_) return;
    raced = DetachTaskLocked();
    task_ = std::make_unique<FIRStorageObservableTaskPointer>(task);
    FIRCPPStorageListenerHandle* handle =
        [[FIRCPPStorageListenerHandle alloc] initWithListener:listener_
                                                      storage:&storage
                                                         task:task];
    listener_handle_ =
        std::make_unique<FIRCPPStorageListenerHandlePointer>(handle);

    pause_observer_handle_ = std::make_unique<FIRStorageHandlePointer>(
        [task observeStatus:FIRStorageTaskStatusPause
                    handler:^(FIRStorageTaskSnapshot*) {
                      [handle notifyPaused];
                    }]);
    progress_observer_handle_ = std::make_unique<FIRStorageHandlePointer>(
        [task observeStatus:FIRStorageTaskStatusProgress
                    handler:^(FIRStorageTaskSnapshot* s) {
                      [handle notifyProgress:s];
                    }]);

    storage_token_ = storage_token;
    const uint64_t generation = ++cleanup_generation_;
    auto* ctx = new SafeReferenceHolder::CleanupContext{
        safe_reference_holder_->ref,
        std::make_shared<FIRCPPStorageListenerHandlePointer>(handle),
        generation};
    safe_reference_holder_->cleanup_ctx = ctx;
    storage.cleanup().RegisterObject(ctx, [](void* obj) {
      std::unique_ptr<SafeReferenceHolder::CleanupContext> ctx_holder(
          static_cast<SafeReferenceHolder::CleanupContext*>(obj));
      if (ctx_holder->handle && ctx_holder->handle->get()) {
        [ctx_holder->handle->get() markDetached];
      }
      std::vector<std::shared_ptr<FIRCPPStorageListenerHandlePointer>>
          handles_to_wait;
      const uint64_t gen = ctx_holder->generation;
      ctx_holder->weak_self.Run([&handles_to_wait, gen](ListenerInternal* self) {
        std::lock_guard<std::recursive_mutex> self_lock(self->mutex_);
        if (self->cleanup_generation_ == gen && self->storage_token_) {
          self->storage_token_.reset();
          self->safe_reference_holder_->cleanup_ctx = nullptr;
          (void)self->DetachTaskLocked();
        }
        handles_to_wait = self->safe_reference_holder_->detaching_handles;
      });
      if (ctx_holder->handle && ctx_holder->handle->get()) {
        [ctx_holder->handle->get() waitForCallbacks];
      }
      for (const auto& h : handles_to_wait) {
        if (h && h->get()) [h->get() waitForCallbacks];
      }
      if (!handles_to_wait.empty()) {
        ctx_holder->weak_self.Run([](ListenerInternal* self) {
          std::lock_guard<std::recursive_mutex> self_lock(self->mutex_);
          auto& pending = self->safe_reference_holder_->detaching_handles;
          pending.erase(
              std::remove_if(pending.begin(), pending.end(),
                             [](const auto& h) {
                               return !h || !h->get() ||
                                      ![h->get() hasActiveCallbacks];
                             }),
              pending.end());
        });
      }
    });
  });
  FinishDetach(raced);
}

ListenerInternal::DetachedTask ListenerInternal::DetachTaskLocked() {
  DetachedTask detached;
  if (listener_handle_ && listener_handle_->get()) {
    [listener_handle_->get() markDetached];
    safe_reference_holder_->detaching_handles.push_back(
        std::move(listener_handle_));
  }
  listener_handle_.reset();

  if (auto* cur_task = task()) {
    if (FIRStorageHandle h = pause_observer_handle()) {
      [cur_task removeObserverWithHandle:h];
    }
    if (FIRStorageHandle h = progress_observer_handle()) {
      [cur_task removeObserverWithHandle:h];
    }
  }
  pause_observer_handle_.reset();
  progress_observer_handle_.reset();
  task_.reset();

  detached.storage_token = std::move(storage_token_);
  detached.cleanup_ctx =
      std::exchange(safe_reference_holder_->cleanup_ctx, nullptr);
  return detached;
}

void ListenerInternal::FinishDetach(DetachedTask& detached) {
  if (detached.storage_token && detached.cleanup_ctx) {
    void* old_ctx = std::exchange(detached.cleanup_ctx, nullptr);
    detached.storage_token->RunWhileAlive(
        [old_ctx](StorageInternal& alive_storage) {
          alive_storage.cleanup().UnregisterObject(old_ctx);
          delete static_cast<SafeReferenceHolder::CleanupContext*>(old_ctx);
        });
    detached.storage_token.reset();
  }
  if (detached.handle && detached.handle->get()) {
    [detached.handle->get() waitForCallbacks];
  }
  detached.handle.reset();

  std::vector<std::shared_ptr<FIRCPPStorageListenerHandlePointer>>
      handles_to_wait;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    handles_to_wait = safe_reference_holder_->detaching_handles;
  }
  if (!handles_to_wait.empty()) {
    for (const auto& h : handles_to_wait) {
      if (h && h->get()) [h->get() waitForCallbacks];
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto& pending = safe_reference_holder_->detaching_handles;
    pending.erase(
        std::remove_if(pending.begin(), pending.end(),
                       [](const auto& h) {
                         return !h || !h->get() || ![h->get() hasActiveCallbacks];
                       }),
        pending.end());
  }
}

void ListenerInternal::DetachTask() {
  DetachedTask detached;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    detached = DetachTaskLocked();
  }
  FinishDetach(detached);
}

}  // namespace firebase::storage::internal
