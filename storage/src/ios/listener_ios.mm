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
  NSRecursiveLock* _lock;
  firebase::storage::Listener* _Nullable _listener;
  firebase::storage::internal::StorageInternal* _Nullable _storage;
  FIRStorageObservableTask<FIRStorageTaskManagement>* _Nullable _task;
  int64_t _previousProgressCount;
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
    _lock = [[NSRecursiveLock alloc] init];
    _listener = listener;
    _storage = storage;
    _task = task;
    _previousProgressCount = -1;
    _detached = NO;
  }
  return self;
}

- (void)markDetached {
  [_lock lock];
  _detached = YES;
  _listener = nullptr;
  _storage = nullptr;
  _task = nil;
  [_lock unlock];
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

- (void)notifyPaused {
  // Keep this handle (and therefore the C++ ivars used by ActiveCallbackScope)
  // alive until the callback scope has unwound, even if the observer block
  // that invoked us is released by a concurrent detach.
  __attribute__((objc_precise_lifetime)) FIRCPPStorageListenerHandle*
      strong_self = self;
  (void)strong_self;

  firebase::storage::Listener* cur_listener = nullptr;
  firebase::storage::Controller controller;

  [_lock lock];
  if (!_detached && _listener != nullptr && _storage != nullptr &&
      _task != nil) {
    cur_listener = _listener;
    controller.internal_->AssignTask(_storage, _task);
    std::lock_guard<std::mutex> cb_lock(_callbackMutex);
    _activeCallbackThreads.push_back(std::this_thread::get_id());
  }
  [_lock unlock];

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

  [_lock lock];
  if (!_detached && _listener != nullptr && _storage != nullptr &&
      _task != nil) {
    // FIRStorageTaskSnapshot.progress can be nil when the task is starting so
    // ignore progress callbacks until progress can be reported. Any non-nil
    // progress is reported, including 0-byte transfers (totalUnitCount == 0)
    // and indeterminate transfers (totalUnitCount == -1, i.e.
    // NSURLSessionTransferSizeUnknown); duplicate completedUnitCount values
    // are suppressed.
    NSProgress* progress = snapshot ? snapshot.progress : nil;
    if (progress != nil &&
        _previousProgressCount != progress.completedUnitCount) {
      _previousProgressCount = progress.completedUnitCount;
      cur_listener = _listener;
      controller.internal_->AssignTask(_storage, _task);
      std::lock_guard<std::mutex> cb_lock(_callbackMutex);
      _activeCallbackThreads.push_back(std::this_thread::get_id());
    }
  }
  [_lock unlock];

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
};

ListenerInternal::ListenerInternal(Listener* _Nonnull listener)
    : listener_(listener),
      storage_(nullptr),
      destroying_(false),
      safe_reference_holder_(std::make_unique<SafeReferenceHolder>(this)) {}

const firebase::util::SafeReference<ListenerInternal>&
ListenerInternal::safe_reference() const {
  return safe_reference_holder_->ref;
}

ListenerInternal::~ListenerInternal() {
  safe_reference_holder_->ref.Invalidate();
  FIRCPPStorageListenerHandle* old_handle = nil;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    destroying_ = true;
    old_handle = DetachTaskLocked();
  }
  if (old_handle) {
    [old_handle waitForCallbacks];
  }
}

void ListenerInternal::Detach() { DetachTask(); }

void ListenerInternal::AttachTask(
    StorageInternal* _Nonnull storage,
    FIRStorageObservableTask<FIRStorageTaskManagement>* _Nonnull task) {
  DetachTask();
  if (!storage || !task) {
    return;
  }

  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (destroying_) {
    return;
  }
  // Ensure any task attached concurrently while unlocked is cleaned up under
  // the lock.
  //
  // The returned handle is intentionally NOT passed to -waitForCallbacks here:
  // waiting requires releasing mutex_ first (as DetachTask() does), because an
  // in-flight callback on another thread may call back into this listener
  // (e.g. Controller operations or attaching to a new task), which needs
  // mutex_ and would deadlock against a wait performed under the lock. The
  // handle is already marked detached, so it will not start any new Listener
  // callbacks; an already-running callback finishes on its own.
  FIRCPPStorageListenerHandle* raced_handle = DetachTaskLocked();
  (void)raced_handle;

  storage_ = storage;
  task_ = std::make_unique<FIRStorageObservableTaskPointer>(task);

  FIRCPPStorageListenerHandle* handle =
      [[FIRCPPStorageListenerHandle alloc] initWithListener:listener_
                                                    storage:storage
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

  firebase::util::SafeReference<ListenerInternal> weak_self =
      safe_reference_holder_->ref;
  storage->cleanup().RegisterObject(this, [weak_self](void* /*obj*/) {
    weak_self.Run([](ListenerInternal* self) {
      FIRCPPStorageListenerHandle* old_handle = nil;
      {
        // DetachTaskLocked() calls UnregisterObject() with mutex_ held, and
        // UnregisterObject() waits for this callback; so never block on
        // mutex_ here. If the owner is unregistering, it is detaching the
        // task itself, so there is nothing left for us to do.
        std::unique_lock<std::recursive_mutex> self_lock(self->mutex_,
                                                         std::defer_lock);
        while (!self_lock.try_lock()) {
          if (self->unregistering_.load() > 0) {
            return;
          }
          std::this_thread::yield();
        }
        self->storage_ = nullptr;
        old_handle = self->DetachTaskLocked();
      }
      if (old_handle) {
        [old_handle waitForCallbacks];
      }
    });
  });
}

FIRCPPStorageListenerHandle* _Nullable ListenerInternal::DetachTaskLocked() {
  FIRCPPStorageListenerHandle* old_handle = listener_handle();
  if (old_handle) {
    [old_handle markDetached];
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

  if (storage_ != nullptr) {
    // May block until an in-flight cleanup callback for `this` returns; the
    // callback bails out instead of waiting on mutex_ while unregistering_ > 0.
    unregistering_.fetch_add(1);
    storage_->cleanup().UnregisterObject(this);
    unregistering_.fetch_sub(1);
    storage_ = nullptr;
  }

  return old_handle;
}

void ListenerInternal::DetachTask() {
  FIRCPPStorageListenerHandle* old_handle = nil;
  {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    old_handle = DetachTaskLocked();
  }
  if (old_handle) {
    [old_handle waitForCallbacks];
  }
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
