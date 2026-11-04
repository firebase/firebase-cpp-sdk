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

// RAII guard that registers the current thread as running a Listener callback
// for a FIRCPPStorageListenerHandle, and on scope exit (including when the
// user's OnProgress / OnPaused throws a C++ exception) removes it again and
// wakes any thread blocked in -waitForCallbacks. Without this, an exception
// would leave the thread ID registered forever and deadlock the next
// -waitForCallbacks (e.g. in ~ListenerInternal).
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
      if (it != active_threads_.end()) {
        active_threads_.erase(it);
      }
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
  const std::thread::id self_thread = std::this_thread::get_id();
  std::unique_lock<std::mutex> cb_lock(_callbackMutex);
  _callbackCv.wait(cb_lock, [self, self_thread]() {
    for (const auto& tid : _activeCallbackThreads) {
      if (tid != self_thread) {
        return false;
      }
    }
    return true;
  });
}

- (BOOL)hasActiveCallbacks {
  std::lock_guard<std::mutex> cb_lock(_callbackMutex);
  return !_activeCallbackThreads.empty();
}

- (void)notifyPaused {
  // Keep this handle (and therefore the C++ ivars used by ActiveCallbackScope)
  // alive until the callback scope has unwound, even if the observer block
  // that invoked us is released by a concurrent detach.
  __attribute__((objc_precise_lifetime)) FIRCPPStorageListenerHandle*
      strong_self = self;
  (void)strong_self;

  firebase::storage::Listener* cur_listener = nullptr;
  firebase::storage::Controller controller;

  {
    std::lock_guard<std::recursive_mutex> lock(_lock);
    if (!_detached && _listener != nullptr && _storage != nullptr &&
        _task != nil) {
      cur_listener = _listener;
      controller.internal_->AssignTask(_storage, _task);
      std::lock_guard<std::mutex> cb_lock(_callbackMutex);
      _activeCallbackThreads.push_back(std::this_thread::get_id());
    }
  }

  if (!cur_listener) {
    return;
  }

  ActiveCallbackScope callback_scope(_callbackMutex, _callbackCv,
                                     _activeCallbackThreads);
  cur_listener->OnPaused(&controller);
}

- (void)notifyProgress:(FIRStorageTaskSnapshot* _Nullable)snapshot {
  // See -notifyPaused for why a precise-lifetime strong self is held.
  __attribute__((objc_precise_lifetime)) FIRCPPStorageListenerHandle*
      strong_self = self;
  (void)strong_self;

  firebase::storage::Listener* cur_listener = nullptr;
  firebase::storage::Controller controller;

  {
    std::lock_guard<std::recursive_mutex> lock(_lock);
    if (!_detached && _listener != nullptr && _storage != nullptr &&
        _task != nil) {
      // FIRStorageTaskSnapshot.progress can be nil when the task is starting so
      // ignore progress callbacks until progress can be reported. Any non-nil
      // progress is reported, including 0-byte transfers (totalUnitCount == 0)
      // and indeterminate transfers (totalUnitCount == -1, i.e.
      // NSURLSessionTransferSizeUnknown); duplicate (completedUnitCount,
      // totalUnitCount) pairs are suppressed.
      NSProgress* progress = snapshot ? snapshot.progress : nil;
      if (progress != nil &&
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

  if (!cur_listener) {
    return;
  }

  ActiveCallbackScope callback_scope(_callbackMutex, _callbackCv,
                                     _activeCallbackThreads);
  cur_listener->OnProgress(&controller);
}

@end

namespace firebase {
namespace storage {
namespace internal {

struct ListenerInternal::SafeReferenceHolder {

  explicit SafeReferenceHolder(ListenerInternal* _Nonnull owner)
      : ref(owner) {}
  firebase::util::SafeReference<ListenerInternal> ref;

  // Retains detached handles whose callbacks may still be in flight so any
  // concurrent DetachTask(), AttachTask(), ~ListenerInternal(), or
  // ~StorageInternal() cleanup callback also waits for them to finish.
  std::vector<std::shared_ptr<FIRCPPStorageListenerHandlePointer>>
      detaching_handles;
};

ListenerInternal::ListenerInternal(Listener* _Nonnull listener)
    : listener_(listener),
      destroying_(false),
      safe_reference_holder_(std::make_unique<SafeReferenceHolder>(this)) {}

const firebase::util::SafeReference<ListenerInternal>&
ListenerInternal::safe_reference() const {
  return safe_reference_holder_->ref;
}

ListenerInternal::~ListenerInternal() {
  // Waits for any running SafeReference::Run() scope (including the cleanup
  // callback's detach step) and turns later ones into no-ops.
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
  if (!storage_token || !task) {
    return;
  }

  DetachedTask raced;
  // The token mutex (held by RunWhileAlive) keeps the StorageInternal alive
  // while registering with its cleanup(); acquired before mutex_ per the lock
  // order documented in listener_ios.h.
  storage_token->RunWhileAlive([&](StorageInternal& storage) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (destroying_) {
      return;
    }
    // A task attached concurrently while mutex_ was released above is
    // detached here; it is finished (unregistered and waited for) below,
    // after releasing mutex_, so that no orphaned handle can still be running
    // a callback into listener_ when AttachTask() returns.
    raced = DetachTaskLocked();

    task_ = std::make_unique<FIRStorageObservableTaskPointer>(task);

    FIRCPPStorageListenerHandle* handle =
        [[FIRCPPStorageListenerHandle alloc] initWithListener:listener_
                                                      storage:&storage
                                                         task:task];
    listener_handle_ =
        std::make_unique<FIRCPPStorageListenerHandlePointer>(handle);

    FIRStorageVoidSnapshot pause_handler =
        ^(FIRStorageTaskSnapshot* /*snapshot*/) {
          [handle notifyPaused];
        };

    FIRStorageVoidSnapshot progress_handler =
        ^(FIRStorageTaskSnapshot* snapshot) {
          [handle notifyProgress:snapshot];
        };

    pause_observer_handle_ = std::make_unique<FIRStorageHandlePointer>(
        [task observeStatus:FIRStorageTaskStatusPause handler:pause_handler]);
    progress_observer_handle_ = std::make_unique<FIRStorageHandlePointer>(
        [task observeStatus:FIRStorageTaskStatusProgress
                    handler:progress_handler]);

    storage_token_ = storage_token;
      if (!handles_to_wait.empty()) {
        ctx_holder->weak_self.Run([](ListenerInternal* self) {
          std::lock_guard<std::recursive_mutex> self_lock(self->mutex_);
          auto& pending = self->safe_reference_holder_->detaching_handles;
          pending.erase(
              std::remove_if(
                  pending.begin(), pending.end(),
                  [](const std::shared_ptr<FIRCPPStorageListenerHandlePointer>&
                         h) {
                    return !h || !h->get() || ![h->get() hasActiveCallbacks];
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

  FIRStorageObservableTask<FIRStorageTaskManagement>* cur_task = task();
  FIRStorageHandle pause_handle = pause_observer_handle();
  if (pause_handle && cur_task) {
    [cur_task removeObserverWithHandle:pause_handle];
  }
  pause_observer_handle_.reset();

  FIRStorageHandle progress_handle = progress_observer_handle();
  if (progress_handle && cur_task) {
    [cur_task removeObserverWithHandle:progress_handle];
  }
  progress_observer_handle_.reset();

  task_.reset();

  detached.storage_token = std::move(storage_token_);

  return detached;
}

void ListenerInternal::FinishDetach(DetachedTask& detached) {

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
      if (h && h->get()) {
        [h->get() waitForCallbacks];
      }
    }
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto& pending = safe_reference_holder_->detaching_handles;
    pending.erase(
        std::remove_if(
            pending.begin(), pending.end(),
            [](const std::shared_ptr<FIRCPPStorageListenerHandlePointer>& h) {
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

}  // namespace internal
}  // namespace storage
}  // namespace firebase
