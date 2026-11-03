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

#include "storage/src/android/controller_android.h"

#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "app/src/util_android.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase {
namespace storage {
namespace internal {

namespace {

struct InFlightListenerCallback {
  StorageInternal* storage = nullptr;
  Listener* listener = nullptr;
  std::thread::id thread_id;
};

struct ActiveListenerRegistry {
  std::mutex mutex;
  std::condition_variable cv;
  std::map<std::pair<StorageInternal*, Listener*>, int> active_counts;
  std::vector<InFlightListenerCallback> in_flight;
};

ActiveListenerRegistry& GetActiveListenerRegistry() {
  static auto* registry = new ActiveListenerRegistry();
  return *registry;
}

}  // namespace

METHOD_LOOKUP_DEFINITION(storage_task,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageTask",
                         STORAGE_TASK_METHODS)

METHOD_LOOKUP_DEFINITION(upload_task_task_snapshot,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/UploadTask$TaskSnapshot",
                         UPLOAD_TASK_TASK_SNAPSHOT_METHODS)

METHOD_LOOKUP_DEFINITION(
    file_download_task_task_snapshot,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/storage/FileDownloadTask$TaskSnapshot",
    DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)

METHOD_LOOKUP_DEFINITION(
    stream_download_task_task_snapshot,
    PROGUARD_KEEP_CLASS
    "com/google/firebase/storage/StreamDownloadTask$TaskSnapshot",
    DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)

bool ControllerInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  if (storage_task::CacheMethodIds(env, activity) &&
      upload_task_task_snapshot::CacheMethodIds(env, activity) &&
      file_download_task_task_snapshot::CacheMethodIds(env, activity) &&
      stream_download_task_task_snapshot::CacheMethodIds(env, activity)) {
    return true;
  }
  Terminate(app);
  return false;
}

void ControllerInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_task::ReleaseClass(env);
  upload_task_task_snapshot::ReleaseClass(env);
  file_download_task_task_snapshot::ReleaseClass(env);
  stream_download_task_task_snapshot::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

ControllerInternal::ControllerInternal() = default;

ControllerInternal::~ControllerInternal() {
  // Disarm the cleanup callback first. If CleanupAll() is running the callback
  // on another thread right now, this blocks until it has finished, so the
  // callback can never touch a destroyed ControllerInternal. Lock order is
  // always cleanup_guard_->mutex -> mutex_.
  {
    std::lock_guard<std::mutex> guard_lock(cleanup_guard_->mutex);
    cleanup_guard_->self = nullptr;
  }
  ControllerLock lock(mutex_);
  UnregisterCleanupLocked();
  ReleaseTaskObjLocked();
  storage_ = nullptr;
}

ControllerInternal::ControllerInternal(const ControllerInternal& other) {
  ControllerLock other_lock(other.mutex_);
  ControllerLock lock(mutex_);
  storage_ = other.storage_;
  if (storage_ != nullptr && other.task_obj_ != nullptr &&
      storage_->app() != nullptr) {
    task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(other.task_obj_);
  }
  RegisterCleanupLocked();
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (this == &other) {
    return *this;
  }
  const bool lock_this_first =
      std::less<const ControllerInternal*>()(this, &other);
  ControllerLock first_lock(lock_this_first ? mutex_ : other.mutex_);
  ControllerLock second_lock(lock_this_first ? other.mutex_ : mutex_);

  UnregisterCleanupLocked();
  ReleaseTaskObjLocked();
  storage_ = other.storage_;
  task_obj_ = nullptr;
  if (storage_ != nullptr && other.task_obj_ != nullptr &&
      storage_->app() != nullptr) {
    task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(other.task_obj_);
  }
  RegisterCleanupLocked();
  return *this;
}

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept {
  ControllerLock other_lock(other.mutex_);
  ControllerLock lock(mutex_);
  other.UnregisterCleanupLocked();
  storage_ = other.storage_;
  task_obj_ = other.task_obj_;
  other.storage_ = nullptr;
  other.task_obj_ = nullptr;
  RegisterCleanupLocked();
}

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  const bool lock_this_first =
      std::less<const ControllerInternal*>()(this, &other);
  ControllerLock first_lock(lock_this_first ? mutex_ : other.mutex_);
  ControllerLock second_lock(lock_this_first ? other.mutex_ : mutex_);

  UnregisterCleanupLocked();
  ReleaseTaskObjLocked();
  other.UnregisterCleanupLocked();
  storage_ = other.storage_;
  task_obj_ = other.task_obj_;
  other.storage_ = nullptr;
  other.task_obj_ = nullptr;
  RegisterCleanupLocked();
  return *this;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) {
    return true;
  }
  StorageInternal* this_storage = nullptr;
  jobject this_task = nullptr;
  StorageInternal* other_storage = nullptr;
  jobject other_task = nullptr;
  JNIEnv* env = nullptr;
  {
    const bool lock_this_first =
        std::less<const ControllerInternal*>()(this, &other);
    ControllerLock first_lock(lock_this_first ? mutex_ : other.mutex_);
    ControllerLock second_lock(lock_this_first ? other.mutex_ : mutex_);

    const bool this_valid =
        (storage_ != nullptr && task_obj_ != nullptr &&
         storage_->app() != nullptr);
    const bool other_valid =
        (other.storage_ != nullptr && other.task_obj_ != nullptr &&
         other.storage_->app() != nullptr);
    if (!this_valid && !other_valid) {
      return true;
    }
    if (this_valid != other_valid || storage_ != other.storage_) {
      return false;
    }
    this_storage = storage_;
    other_storage = other.storage_;
    env = this_storage->app()->GetJNIEnv();
    if (env == nullptr) {
      return false;
    }
    this_task = env->NewLocalRef(task_obj_);
    other_task = env->NewLocalRef(other.task_obj_);
  }
  const bool same =
      (this_storage == other_storage && this_task != nullptr &&
       other_task != nullptr &&
       env->IsSameObject(this_task, other_task) != JNI_FALSE);
  if (this_task != nullptr) env->DeleteLocalRef(this_task);
  if (other_task != nullptr) env->DeleteLocalRef(other_task);
  return same;
}

void ControllerInternal::ReleaseTaskObjLocked() {
  if (task_obj_ == nullptr) return;
  JNIEnv* env =
      java_vm_ != nullptr ? util::GetThreadsafeJNIEnv(java_vm_) : nullptr;
  if (env != nullptr) {
    env->DeleteGlobalRef(task_obj_);
  }
  task_obj_ = nullptr;
}

void ControllerInternal::UnregisterCleanupLocked() {
  if (storage_ == nullptr) return;
  cleanup_guard_->unregistering.fetch_add(1);
  // May block until an in-flight cleanup callback for `this` returns; the
  // callback never blocks on mutex_ while unregistering > 0 (see
  // CleanupGuard), so this cannot deadlock.
  storage_->cleanup().UnregisterObject(this);
  cleanup_guard_->unregistering.fetch_sub(1);
  StorageInternal* dying = cleanup_guard_->dying_storage.exchange(nullptr);
  if (dying != nullptr && dying == storage_) {
    // storage_ is being destroyed: drop the binding without touching it.
    ReleaseTaskObjLocked();
    storage_ = nullptr;
  }
}

void ControllerInternal::RegisterCleanupLocked() {
  if (storage_ == nullptr || task_obj_ == nullptr) return;
  if (storage_->app() != nullptr) {
    java_vm_ = storage_->app()->java_vm();
  }
  std::shared_ptr<CleanupGuard> guard = cleanup_guard_;
  StorageInternal* bound_storage = storage_;
  storage_->cleanup().RegisterObject(
      this, [guard, bound_storage](void* /*obj*/) {
        std::lock_guard<std::mutex> guard_lock(guard->mutex);
        ControllerInternal* self = guard->self;
        if (self == nullptr) {
          return;  // Already destroyed (or being destroyed).
        }
        while (true) {
          if (self->mutex_.try_lock()) {
            std::lock_guard<std::recursive_mutex> lock(self->mutex_,
                                                       std::adopt_lock);
            if (self->storage_ == bound_storage) {
              self->ReleaseTaskObjLocked();
              self->storage_ = nullptr;
            }
            return;
          }
          if (guard->unregistering.load() > 0) {
            // The owner holds mutex_ and is blocked in UnregisterObject()
            // waiting for us; let it release the binding instead.
            guard->dying_storage.store(bound_storage);
            return;
          }
          std::this_thread::yield();
        }
      });
}

void ControllerInternal::AssignTask(StorageInternal* storage,
                                    jobject task_obj) {
  ControllerLock lock(mutex_);
  UnregisterCleanupLocked();
  ReleaseTaskObjLocked();
  storage_ = storage;
  if (storage_ != nullptr && task_obj != nullptr &&
      storage_->app() != nullptr) {
    task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(task_obj);
  }
  RegisterCleanupLocked();
}

bool ControllerInternal::Pause() {
  ControllerLock lock(mutex_);
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return false;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kPause));
  if (util::LogException(env, kLogLevelError, "Controller::Pause() failed")) {
    return false;
  }
  return result != JNI_FALSE;
}

bool ControllerInternal::Resume() {
  ControllerLock lock(mutex_);
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return false;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kResume));
  if (util::LogException(env, kLogLevelError, "Controller::Resume() failed")) {
    return false;
  }
  return result != JNI_FALSE;
}

bool ControllerInternal::Cancel() {
  ControllerLock lock(mutex_);
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return false;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kCancel));
  if (util::LogException(env, kLogLevelError, "Controller::Cancel() failed")) {
    return false;
  }
  return result != JNI_FALSE;
}

bool ControllerInternal::is_paused() const {
  ControllerLock lock(mutex_);
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return false;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kIsPaused));
  if (util::CheckAndClearJniExceptions(env)) {
    return false;
  }
  return result != JNI_FALSE;
}

int64_t ControllerInternal::total_byte_count() const {
  ControllerLock lock(mutex_);
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return -1;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (util::CheckAndClearJniExceptions(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return -1;
  }
  int64_t result = -1;
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, upload_task_task_snapshot::GetMethodId(
                      upload_task_task_snapshot::kGetTotalByteCount));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(
                      file_download_task_task_snapshot::kGetTotalByteCount));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, stream_download_task_task_snapshot::GetMethodId(
                      stream_download_task_task_snapshot::kGetTotalByteCount));
  }
  env->DeleteLocalRef(snapshot);
  if (util::CheckAndClearJniExceptions(env)) {
    result = -1;
  }
  return result;
}

int64_t ControllerInternal::bytes_transferred() const {
  ControllerLock lock(mutex_);
  int64_t result = 0;
  if (storage_ == nullptr || task_obj_ == nullptr ||
      storage_->app() == nullptr) {
    return result;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (util::CheckAndClearJniExceptions(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return 0;
  }
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, upload_task_task_snapshot::GetMethodId(
                      upload_task_task_snapshot::kGetBytesTransferred));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(
                      file_download_task_task_snapshot::kGetBytesTransferred));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot,
        stream_download_task_task_snapshot::GetMethodId(
            stream_download_task_task_snapshot::kGetBytesTransferred));
  }
  env->DeleteLocalRef(snapshot);
  if (util::CheckAndClearJniExceptions(env)) {
    result = 0;
  }
  return result;
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  StorageInternal* storage = nullptr;
  JNIEnv* env = nullptr;
  jobject task = nullptr;
  {
    ControllerLock lock(mutex_);
    if (storage_ == nullptr || task_obj_ == nullptr ||
        storage_->app() == nullptr) {
      return nullptr;
    }
    storage = storage_;
    env = storage_->app()->GetJNIEnv();
    if (env == nullptr) return nullptr;
    task = env->NewLocalRef(task_obj_);
  }
  if (task == nullptr) return nullptr;
  jobject snapshot = env->CallObjectMethod(
      task, storage_task::GetMethodId(storage_task::kGetSnapshot));
  env->DeleteLocalRef(task);
  if (util::CheckAndClearJniExceptions(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return nullptr;
  }
  jobject obj = nullptr;
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    obj = env->CallObjectMethod(snapshot,
                                upload_task_task_snapshot::GetMethodId(
                                    upload_task_task_snapshot::kGetStorage));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    obj = env->CallObjectMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(
                      file_download_task_task_snapshot::kGetStorage));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    obj = env->CallObjectMethod(
        snapshot, stream_download_task_task_snapshot::GetMethodId(
                      stream_download_task_task_snapshot::kGetStorage));
  }
  env->DeleteLocalRef(snapshot);
  if (util::CheckAndClearJniExceptions(env) || !obj) {
    if (obj) env->DeleteLocalRef(obj);
    return nullptr;
  }
  {
    ControllerLock lock(mutex_);
    if (storage_ == nullptr || storage_ != storage) {
      env->DeleteLocalRef(obj);
      return nullptr;
    }
  }
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(storage, obj);
  env->DeleteLocalRef(obj);
  util::CheckAndClearJniExceptions(env);
  return internal;
}

void ControllerInternal::RegisterActiveListener(StorageInternal* storage,
                                                Listener* listener) {
  if (storage == nullptr || listener == nullptr) return;
  ActiveListenerRegistry& registry = GetActiveListenerRegistry();
  std::lock_guard<std::mutex> lock(registry.mutex);
  registry.active_counts[{storage, listener}]++;
}

void ControllerInternal::UnregisterActiveListener(StorageInternal* storage,
                                                  Listener* listener) {
  if (storage == nullptr || listener == nullptr) return;
  ActiveListenerRegistry& registry = GetActiveListenerRegistry();
  std::unique_lock<std::mutex> lock(registry.mutex);
  auto it = registry.active_counts.find({storage, listener});
  if (it != registry.active_counts.end()) {
    if (--(it->second) <= 0) {
      registry.active_counts.erase(it);
    }
  }
  const std::thread::id self_thread = std::this_thread::get_id();
  registry.cv.wait(lock, [&]() {
    for (const auto& entry : registry.in_flight) {
      if (entry.storage == storage && entry.listener == listener &&
          entry.thread_id != self_thread) {
        return false;
      }
    }
    return true;
  });
}

void ControllerInternal::CppStorageListenerCallback(JNIEnv* env, jclass clazz,
                                                    jlong storage_ptr,
                                                    jlong listener_ptr,
                                                    jobject snapshot,
                                                    jboolean is_on_paused) {
  (void)clazz;
  if (!storage_ptr || !listener_ptr || !snapshot) return;
  StorageInternal* storage = reinterpret_cast<StorageInternal*>(storage_ptr);
  Listener* listener = reinterpret_cast<Listener*>(listener_ptr);

  ActiveListenerRegistry& registry = GetActiveListenerRegistry();
  const std::thread::id self_thread = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto it = registry.active_counts.find({storage, listener});
    if (it == registry.active_counts.end() || it->second <= 0) {
      return;
    }
    registry.in_flight.push_back({storage, listener, self_thread});
  }

  // RAII: always remove this thread's in-flight entry and wake waiters in
  // UnregisterActiveListener(), on every return path and even if the user's
  // OnPaused()/OnProgress() throws.
  struct InFlightScope {
    ActiveListenerRegistry& registry;
    StorageInternal* storage;
    Listener* listener;
    std::thread::id thread_id;
    ~InFlightScope() {
      std::lock_guard<std::mutex> lock(registry.mutex);
      for (auto it = registry.in_flight.begin();
           it != registry.in_flight.end(); ++it) {
        if (it->storage == storage && it->listener == listener &&
            it->thread_id == thread_id) {
          registry.in_flight.erase(it);
          break;
        }
      }
      registry.cv.notify_all();
    }
  } in_flight_scope{registry, storage, listener, self_thread};

  jobject task_obj = nullptr;
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    task_obj = env->CallObjectMethod(snapshot,
                                     upload_task_task_snapshot::GetMethodId(
                                         upload_task_task_snapshot::kGetTask));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    task_obj = env->CallObjectMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(
                      file_download_task_task_snapshot::kGetTask));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    task_obj = env->CallObjectMethod(
        snapshot, stream_download_task_task_snapshot::GetMethodId(
                      stream_download_task_task_snapshot::kGetTask));
  }
  if (util::CheckAndClearJniExceptions(env) || !task_obj) {
    if (task_obj) env->DeleteLocalRef(task_obj);
    return;
  }

  bool still_active = false;
  {
    std::lock_guard<std::mutex> lock(registry.mutex);
    auto it = registry.active_counts.find({storage, listener});
    still_active = (it != registry.active_counts.end() && it->second > 0);
  }
  if (!still_active) {
    env->DeleteLocalRef(task_obj);
    return;
  }

  ControllerInternal* internal = new ControllerInternal();
  internal->AssignTask(storage, task_obj);
  env->DeleteLocalRef(task_obj);
  Controller controller(internal);
  if (is_on_paused) {
    listener->OnPaused(&controller);
  } else {
    listener->OnProgress(&controller);
  }
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
