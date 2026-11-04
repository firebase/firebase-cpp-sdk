// Copyright 2016 Google LLC
#include "storage/src/android/controller_android.h"

#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "app/src/jni/env.h"
#include "app/src/jni/exception.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase::storage::internal {
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

jobject CallSnapshotObject(
    JNIEnv* env, jobject snapshot,
    upload_task_task_snapshot::Method upload_method,
    file_download_task_task_snapshot::Method file_method,
    stream_download_task_task_snapshot::Method stream_method) {
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    return env->CallObjectMethod(
        snapshot, upload_task_task_snapshot::GetMethodId(upload_method));
  }
  if (env->IsInstanceOf(snapshot,
                        file_download_task_task_snapshot::GetClass())) {
    return env->CallObjectMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(file_method));
  }
  if (env->IsInstanceOf(snapshot,
                        stream_download_task_task_snapshot::GetClass())) {
    return env->CallObjectMethod(
        snapshot,
        stream_download_task_task_snapshot::GetMethodId(stream_method));
  }
  return nullptr;
}

}  // namespace

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
  jni::CheckAndClearException(env);
}

ControllerInternal::~ControllerInternal() {
  ControllerLock lock(mutex_);
  ReleaseTaskObjLocked();
  storage_ = nullptr;
  storage_token_.reset();
}

ControllerInternal::ControllerInternal(const ControllerInternal& other) {
  ControllerLock other_lock(other.mutex_);
  storage_ = other.storage_;
  storage_token_ = other.storage_token_;
  java_vm_ = other.java_vm_;
  if (other.task_obj_) {
    if (JNIEnv* env = other.GetEnvLocked()) {
      task_obj_ = env->NewGlobalRef(other.task_obj_);
    }
  }
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (this == &other) return *this;
  std::scoped_lock lock(mutex_, other.mutex_);
  ReleaseTaskObjLocked();
  storage_ = other.storage_;
  storage_token_ = other.storage_token_;
  java_vm_ = other.java_vm_;
  if (other.task_obj_) {
    if (JNIEnv* env = GetEnvLocked()) {
      task_obj_ = env->NewGlobalRef(other.task_obj_);
    }
  }
  return *this;
}

ControllerInternal::ControllerInternal(ControllerInternal&& other) noexcept {
  ControllerLock other_lock(other.mutex_);
  storage_ = other.storage_;
  storage_token_ = std::move(other.storage_token_);
  task_obj_ = other.task_obj_;
  java_vm_ = other.java_vm_;
  other.storage_ = nullptr;
  other.task_obj_ = nullptr;
}

ControllerInternal& ControllerInternal::operator=(
    ControllerInternal&& other) noexcept {
  if (this == &other) return *this;
  std::scoped_lock lock(mutex_, other.mutex_);
  ReleaseTaskObjLocked();
  storage_ = other.storage_;
  storage_token_ = std::move(other.storage_token_);
  task_obj_ = other.task_obj_;
  java_vm_ = other.java_vm_;
  other.storage_ = nullptr;
  other.task_obj_ = nullptr;
  return *this;
}

bool ControllerInternal::operator==(const ControllerInternal& other) const {
  if (this == &other) return true;
  std::scoped_lock lock(mutex_, other.mutex_);
  const bool this_valid = storage_ && task_obj_;
  const bool other_valid = other.storage_ && other.task_obj_;
  if (!this_valid && !other_valid) return true;
  if (this_valid != other_valid || storage_ != other.storage_) return false;
  if (task_obj_ == other.task_obj_) return true;
  JNIEnv* env = GetEnvLocked();
  return env && env->IsSameObject(task_obj_, other.task_obj_) != JNI_FALSE;
}

StorageInternal* ControllerInternal::LockLiveStorageLocked(
    std::unique_lock<std::mutex>* token_lock) const {
  if (!storage_ || !task_obj_ || !storage_token_) return nullptr;
  *token_lock = std::unique_lock<std::mutex>(storage_token_->mutex);
  if (!storage_token_->alive || !storage_->app()) {
    token_lock->unlock();
    return nullptr;
  }
  return storage_;
}

JNIEnv* ControllerInternal::GetEnvLocked() const {
  return java_vm_ ? util::GetThreadsafeJNIEnv(java_vm_) : nullptr;
}

void ControllerInternal::ReleaseTaskObjLocked() {
  if (!task_obj_) return;
  if (JNIEnv* env = GetEnvLocked()) env->DeleteGlobalRef(task_obj_);
  task_obj_ = nullptr;
}

void ControllerInternal::AssignTask(StorageInternal* storage,
                                    jobject task_obj) {
  ControllerLock lock(mutex_);
  ReleaseTaskObjLocked();
  storage_ = storage;
  storage_token_ = storage ? storage->lifetime_token() : nullptr;
  if (storage && storage->app()) java_vm_ = storage->app()->java_vm();
  if (storage_ && task_obj) {
    if (JNIEnv* env = GetEnvLocked()) task_obj_ = env->NewGlobalRef(task_obj);
  }
}

bool ControllerInternal::is_valid() const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  return LockLiveStorageLocked(&token_lock) != nullptr;
}

bool ControllerInternal::CallTaskMethod(storage_task::Method method,
                                        const char* error_message) const {
  JNIEnv* env = nullptr;
  jobject task_local = nullptr;
  jmethodID method_id = nullptr;
  {
    ControllerLock lock(mutex_);
    std::unique_lock<std::mutex> token_lock;
    if (!LockLiveStorageLocked(&token_lock)) return false;
    env = GetEnvLocked();
    if (!env) return false;
    method_id = storage_task::GetMethodId(method);
    task_local = env->NewLocalRef(task_obj_);
  }
  if (!task_local || !method_id) {
    if (task_local && env) env->DeleteLocalRef(task_local);
    return false;
  }
  jboolean res = env->CallBooleanMethod(task_local, method_id);
  bool failed = error_message
                    ? util::LogException(env, kLogLevelError, error_message)
                    : jni::CheckAndClearException(env);
  env->DeleteLocalRef(task_local);
  return !failed && res != JNI_FALSE;
}

bool ControllerInternal::Pause() {
  return CallTaskMethod(storage_task::kPause, "Controller::Pause() failed");
}
bool ControllerInternal::Resume() {
  return CallTaskMethod(storage_task::kResume, "Controller::Resume() failed");
}
bool ControllerInternal::Cancel() {
  return CallTaskMethod(storage_task::kCancel, "Controller::Cancel() failed");
}
bool ControllerInternal::is_paused() const {
  return CallTaskMethod(storage_task::kIsPaused, nullptr);
}

std::int64_t ControllerInternal::QuerySnapshotLong(
    upload_task_task_snapshot::Method upload_method,
    file_download_task_task_snapshot::Method file_method,
    stream_download_task_task_snapshot::Method stream_method,
    std::int64_t fallback) const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  if (!LockLiveStorageLocked(&token_lock)) return fallback;
  JNIEnv* env = GetEnvLocked();
  if (!env) return fallback;
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (jni::CheckAndClearException(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return fallback;
  }
  std::int64_t res = fallback;
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    res = env->CallLongMethod(
        snapshot, upload_task_task_snapshot::GetMethodId(upload_method));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    res = env->CallLongMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(file_method));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    res = env->CallLongMethod(
        snapshot,
        stream_download_task_task_snapshot::GetMethodId(stream_method));
  }
  if (jni::CheckAndClearException(env)) res = fallback;
  env->DeleteLocalRef(snapshot);
  return res;
}

std::int64_t ControllerInternal::total_byte_count() const {
  return QuerySnapshotLong(upload_task_task_snapshot::kGetTotalByteCount,
                           file_download_task_task_snapshot::kGetTotalByteCount,
                           stream_download_task_task_snapshot::kGetTotalByteCount,
                           -1);
}

std::int64_t ControllerInternal::bytes_transferred() const {
  return QuerySnapshotLong(
      upload_task_task_snapshot::kGetBytesTransferred,
      file_download_task_task_snapshot::kGetBytesTransferred,
      stream_download_task_task_snapshot::kGetBytesTransferred, 0);
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  StorageInternal* storage = LockLiveStorageLocked(&token_lock);
  if (!storage) return nullptr;
  JNIEnv* env = GetEnvLocked();
  if (!env) return nullptr;
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (jni::CheckAndClearException(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return nullptr;
  }
  jobject obj = CallSnapshotObject(
      env, snapshot, upload_task_task_snapshot::kGetStorage,
      file_download_task_task_snapshot::kGetStorage,
      stream_download_task_task_snapshot::kGetStorage);
  env->DeleteLocalRef(snapshot);
  if (jni::CheckAndClearException(env) || !obj) {
    if (obj) env->DeleteLocalRef(obj);
    return nullptr;
  }
  auto* internal = new StorageReferenceInternal(storage, obj);
  env->DeleteLocalRef(obj);
  jni::CheckAndClearException(env);
  return internal;
}

void ControllerInternal::RegisterActiveListener(StorageInternal* storage,
                                                Listener* listener) {
  if (!storage || !listener) return;
  ActiveListenerRegistry& reg = GetActiveListenerRegistry();
  std::lock_guard<std::mutex> lock(reg.mutex);
  reg.active_counts[{storage, listener}]++;
}

void ControllerInternal::UnregisterActiveListener(StorageInternal* storage,
                                                  Listener* listener) {
  if (!storage || !listener) return;
  ActiveListenerRegistry& reg = GetActiveListenerRegistry();
  std::unique_lock<std::mutex> lock(reg.mutex);
  auto it = reg.active_counts.find({storage, listener});
  if (it != reg.active_counts.end() && --(it->second) <= 0) {
    reg.active_counts.erase(it);
  }
  const std::thread::id self_thread = std::this_thread::get_id();
  reg.cv.wait(lock, [&]() {
    for (const auto& e : reg.in_flight) {
      if (e.storage == storage && e.listener == listener &&
          e.thread_id != self_thread) {
        return false;
      }
    }
    return true;
  });
}

void ControllerInternal::CppStorageListenerCallback(JNIEnv* env, jclass,
                                                    jlong storage_ptr,
                                                    jlong listener_ptr,
                                                    jobject snapshot,
                                                    jboolean is_on_paused) {
  if (!storage_ptr || !listener_ptr || !snapshot) return;
  auto* storage = reinterpret_cast<StorageInternal*>(storage_ptr);
  auto* listener = reinterpret_cast<Listener*>(listener_ptr);

  ActiveListenerRegistry& reg = GetActiveListenerRegistry();
  const std::thread::id self_thread = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto it = reg.active_counts.find({storage, listener});
    if (it == reg.active_counts.end() || it->second <= 0) return;
    reg.in_flight.push_back({storage, listener, self_thread});
  }

  struct InFlightScope {
    ActiveListenerRegistry& reg;
    StorageInternal* storage;
    Listener* listener;
    std::thread::id thread_id;
    ~InFlightScope() {
      {
        std::lock_guard<std::mutex> lock(reg.mutex);
        for (auto it = reg.in_flight.begin(); it != reg.in_flight.end(); ++it) {
          if (it->storage == storage && it->listener == listener &&
              it->thread_id == thread_id) {
            reg.in_flight.erase(it);
            break;
          }
        }
      }
      reg.cv.notify_all();
    }
  } scope{reg, storage, listener, self_thread};

  jobject task_obj = CallSnapshotObject(
      env, snapshot, upload_task_task_snapshot::kGetTask,
      file_download_task_task_snapshot::kGetTask,
      stream_download_task_task_snapshot::kGetTask);
  if (jni::CheckAndClearException(env) || !task_obj) {
    if (task_obj) env->DeleteLocalRef(task_obj);
    return;
  }

  bool still_active = false;
  {
    std::lock_guard<std::mutex> lock(reg.mutex);
    auto it = reg.active_counts.find({storage, listener});
    still_active = (it != reg.active_counts.end() && it->second > 0);
  }
  if (!still_active) {
    env->DeleteLocalRef(task_obj);
    return;
  }

  auto* internal = new ControllerInternal();
  internal->AssignTask(storage, task_obj);
  env->DeleteLocalRef(task_obj);
  Controller controller(internal);
  if (is_on_paused) {
    listener->OnPaused(&controller);
  } else {
    listener->OnProgress(&controller);
  }
}

}  // namespace firebase::storage::internal
