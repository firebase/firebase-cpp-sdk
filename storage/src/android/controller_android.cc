#include "app/src/jni/env.h"
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

#include "app/src/jni/exception.h"
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

namespace storage_task {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/StorageTask"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kPause] = env->GetMethodID(g_class.get(), "pause", "()Z");
    if (!g_method_ids[kPause]) return false;
    g_method_ids[kResume] = env->GetMethodID(g_class.get(), "resume", "()Z");
    if (!g_method_ids[kResume]) return false;
    g_method_ids[kCancel] = env->GetMethodID(g_class.get(), "cancel", "()Z");
    if (!g_method_ids[kCancel]) return false;
    g_method_ids[kIsPaused] = env->GetMethodID(g_class.get(), "isPaused", "()Z");
    if (!g_method_ids[kIsPaused]) return false;
    g_method_ids[kAddOnPausedListener] = env->GetMethodID(g_class.get(), "addOnPausedListener", "(Lcom/google/firebase/storage/OnPausedListener;)Lcom/google/firebase/storage/StorageTask;");
    if (!g_method_ids[kAddOnPausedListener]) return false;
    g_method_ids[kAddOnProgressListener] = env->GetMethodID(g_class.get(), "addOnProgressListener", "(Lcom/google/firebase/storage/OnProgressListener;)Lcom/google/firebase/storage/StorageTask;");
    if (!g_method_ids[kAddOnProgressListener]) return false;
    g_method_ids[kGetSnapshot] = env->GetMethodID(g_class.get(), "getSnapshot", "()Lcom/google/firebase/storage/StorageTask$ProvideError;");
    if (!g_method_ids[kGetSnapshot]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


namespace upload_task_task_snapshot {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/UploadTask$TaskSnapshot"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetStorage] = env->GetMethodID(g_class.get(), "getStorage", "()Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kGetStorage]) return false;
    g_method_ids[kGetTask] = env->GetMethodID(g_class.get(), "getTask", "()Lcom/google/firebase/storage/StorageTask;");
    if (!g_method_ids[kGetTask]) return false;
    g_method_ids[kGetTotalByteCount] = env->GetMethodID(g_class.get(), "getTotalByteCount", "()J");
    if (!g_method_ids[kGetTotalByteCount]) return false;
    g_method_ids[kGetBytesTransferred] = env->GetMethodID(g_class.get(), "getBytesTransferred", "()J");
    if (!g_method_ids[kGetBytesTransferred]) return false;
    g_method_ids[kGetMetadata] = env->GetMethodID(g_class.get(), "getMetadata", "()Lcom/google/firebase/storage/StorageMetadata;");
    if (!g_method_ids[kGetMetadata]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


namespace file_download_task_task_snapshot {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/FileDownloadTask$TaskSnapshot"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetStorage] = env->GetMethodID(g_class.get(), "getStorage", "()Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kGetStorage]) return false;
    g_method_ids[kGetTask] = env->GetMethodID(g_class.get(), "getTask", "()Lcom/google/firebase/storage/StorageTask;");
    if (!g_method_ids[kGetTask]) return false;
    g_method_ids[kGetTotalByteCount] = env->GetMethodID(g_class.get(), "getTotalByteCount", "()J");
    if (!g_method_ids[kGetTotalByteCount]) return false;
    g_method_ids[kGetBytesTransferred] = env->GetMethodID(g_class.get(), "getBytesTransferred", "()J");
    if (!g_method_ids[kGetBytesTransferred]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


namespace stream_download_task_task_snapshot {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/StreamDownloadTask$TaskSnapshot"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetStorage] = env->GetMethodID(g_class.get(), "getStorage", "()Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kGetStorage]) return false;
    g_method_ids[kGetTask] = env->GetMethodID(g_class.get(), "getTask", "()Lcom/google/firebase/storage/StorageTask;");
    if (!g_method_ids[kGetTask]) return false;
    g_method_ids[kGetTotalByteCount] = env->GetMethodID(g_class.get(), "getTotalByteCount", "()J");
    if (!g_method_ids[kGetTotalByteCount]) return false;
    g_method_ids[kGetBytesTransferred] = env->GetMethodID(g_class.get(), "getBytesTransferred", "()J");
    if (!g_method_ids[kGetBytesTransferred]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


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

ControllerInternal::ControllerInternal() = default;

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
  if (other.task_obj_ != nullptr) {
    JNIEnv* env = other.GetEnvLocked();
    if (env != nullptr) task_obj_ = env->NewGlobalRef(other.task_obj_);
  }
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (this == &other) {
    return *this;
  }
  std::scoped_lock lock(mutex_, other.mutex_);
  ReleaseTaskObjLocked();
  storage_ = other.storage_;
  storage_token_ = other.storage_token_;
  java_vm_ = other.java_vm_;
  if (other.task_obj_ != nullptr) {
    JNIEnv* env = GetEnvLocked();
    if (env != nullptr) task_obj_ = env->NewGlobalRef(other.task_obj_);
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
  if (this == &other) {
    return *this;
  }
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
  if (this == &other) {
    return true;
  }
  std::scoped_lock lock(mutex_, other.mutex_);
  const bool this_valid = storage_ != nullptr && task_obj_ != nullptr;
  const bool other_valid =
      other.storage_ != nullptr && other.task_obj_ != nullptr;
  if (!this_valid && !other_valid) {
    return true;
  }
  if (this_valid != other_valid || storage_ != other.storage_) {
    return false;
  }
  if (task_obj_ == other.task_obj_) {
    return true;
  }
  JNIEnv* env = GetEnvLocked();
  return env != nullptr &&
         env->IsSameObject(task_obj_, other.task_obj_) != JNI_FALSE;
}

StorageInternal* ControllerInternal::LockLiveStorageLocked(
    std::unique_lock<std::mutex>* token_lock) const {
  if (storage_ == nullptr || task_obj_ == nullptr || !storage_token_) {
    return nullptr;
  }
  *token_lock = std::unique_lock<std::mutex>(storage_token_->mutex);
  if (!storage_token_->alive || storage_->app() == nullptr) {
    token_lock->unlock();
    return nullptr;
  }
  return storage_;
}

JNIEnv* ControllerInternal::GetEnvLocked() const {
  return java_vm_ != nullptr ? util::GetThreadsafeJNIEnv(java_vm_) : nullptr;
}

void ControllerInternal::ReleaseTaskObjLocked() {
  if (task_obj_ == nullptr) return;
  JNIEnv* env = GetEnvLocked();
  if (env != nullptr) {
    env->DeleteGlobalRef(task_obj_);
  }
  task_obj_ = nullptr;
}

void ControllerInternal::AssignTask(StorageInternal* storage,
                                    jobject task_obj) {
  ControllerLock lock(mutex_);
  ReleaseTaskObjLocked();
  storage_ = storage;
  storage_token_ = storage != nullptr ? storage->lifetime_token() : nullptr;
  if (storage != nullptr && storage->app() != nullptr) {
    java_vm_ = storage->app()->java_vm();
  }
  if (storage_ != nullptr && task_obj != nullptr) {
    JNIEnv* env = GetEnvLocked();
    if (env != nullptr) task_obj_ = env->NewGlobalRef(task_obj);
  }
}

bool ControllerInternal::is_valid() const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  return LockLiveStorageLocked(&token_lock) != nullptr;
}

namespace {

// Calls a no-arg boolean StorageTask method; returns false on any failure.
bool CallTaskBoolean(JNIEnv* env, jobject task_local, jmethodID method_id,
                     const char* error_message) {
  if (env == nullptr || task_local == nullptr || method_id == nullptr) {
    return false;
  }
  jboolean result = env->CallBooleanMethod(task_local, method_id);
  if (error_message != nullptr) {
    if (util::LogException(env, kLogLevelError, error_message)) return false;
  } else if (jni::CheckAndClearException(env)) {
    return false;
  }
  return result != JNI_FALSE;
}

// Reads a long from whichever TaskSnapshot subclass `snapshot` is.
int64_t SnapshotLong(JNIEnv* env, jobject snapshot,
                     upload_task_task_snapshot::Method upload_method,
                     file_download_task_task_snapshot::Method file_method,
                     stream_download_task_task_snapshot::Method stream_method,
                     int64_t fallback) {
  int64_t result = fallback;
  if (env->IsInstanceOf(snapshot, upload_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, upload_task_task_snapshot::GetMethodId(upload_method));
  } else if (env->IsInstanceOf(snapshot,
                               file_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot, file_download_task_task_snapshot::GetMethodId(file_method));
  } else if (env->IsInstanceOf(
                 snapshot, stream_download_task_task_snapshot::GetClass())) {
    result = env->CallLongMethod(
        snapshot,
        stream_download_task_task_snapshot::GetMethodId(stream_method));
  }
  if (jni::CheckAndClearException(env)) {
    result = fallback;
  }
  return result;
}

}  // namespace

bool ControllerInternal::Pause() {
  JNIEnv* env = nullptr;
  jobject task_local = nullptr;
  jmethodID method_id = nullptr;
  {
    ControllerLock lock(mutex_);
    std::unique_lock<std::mutex> token_lock;
    if (LockLiveStorageLocked(&token_lock) == nullptr) return false;
    env = GetEnvLocked();
    if (env == nullptr) return false;
    method_id = storage_task::GetMethodId(storage_task::kPause);
    task_local = env->NewLocalRef(task_obj_);
  }
  if (task_local == nullptr) return false;
  // Release both locks before calling into Java so synchronous listener
  // callbacks on the same thread cannot deadlock re-acquiring token_lock.
  const bool result =
      CallTaskBoolean(env, task_local, method_id, "Controller::Pause() failed");
  env->DeleteLocalRef(task_local);
  return result;
}

bool ControllerInternal::Resume() {
  JNIEnv* env = nullptr;
  jobject task_local = nullptr;
  jmethodID method_id = nullptr;
  {
    ControllerLock lock(mutex_);
    std::unique_lock<std::mutex> token_lock;
    if (LockLiveStorageLocked(&token_lock) == nullptr) return false;
    env = GetEnvLocked();
    if (env == nullptr) return false;
    method_id = storage_task::GetMethodId(storage_task::kResume);
    task_local = env->NewLocalRef(task_obj_);
  }
  if (task_local == nullptr) return false;
  const bool result = CallTaskBoolean(env, task_local, method_id,
                                      "Controller::Resume() failed");
  env->DeleteLocalRef(task_local);
  return result;
}

bool ControllerInternal::Cancel() {
  JNIEnv* env = nullptr;
  jobject task_local = nullptr;
  jmethodID method_id = nullptr;
  {
    ControllerLock lock(mutex_);
    std::unique_lock<std::mutex> token_lock;
    if (LockLiveStorageLocked(&token_lock) == nullptr) return false;
    env = GetEnvLocked();
    if (env == nullptr) return false;
    method_id = storage_task::GetMethodId(storage_task::kCancel);
    task_local = env->NewLocalRef(task_obj_);
  }
  if (task_local == nullptr) return false;
  const bool result = CallTaskBoolean(env, task_local, method_id,
                                      "Controller::Cancel() failed");
  env->DeleteLocalRef(task_local);
  return result;
}

bool ControllerInternal::is_paused() const {
  JNIEnv* env = nullptr;
  jobject task_local = nullptr;
  jmethodID method_id = nullptr;
  {
    ControllerLock lock(mutex_);
    std::unique_lock<std::mutex> token_lock;
    if (LockLiveStorageLocked(&token_lock) == nullptr) return false;
    env = GetEnvLocked();
    if (env == nullptr) return false;
    method_id = storage_task::GetMethodId(storage_task::kIsPaused);
    task_local = env->NewLocalRef(task_obj_);
  }
  if (task_local == nullptr) return false;
  const bool result = CallTaskBoolean(env, task_local, method_id, nullptr);
  env->DeleteLocalRef(task_local);
  return result;
}

std::int64_t ControllerInternal::total_byte_count() const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  if (LockLiveStorageLocked(&token_lock) == nullptr) return -1;
  JNIEnv* env = GetEnvLocked();
  if (env == nullptr) return -1;
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (jni::CheckAndClearException(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return -1;
  }
  const std::int64_t result =
      SnapshotLong(env, snapshot, upload_task_task_snapshot::kGetTotalByteCount,
                   file_download_task_task_snapshot::kGetTotalByteCount,
                   stream_download_task_task_snapshot::kGetTotalByteCount, -1);
  env->DeleteLocalRef(snapshot);
  return result;
}

std::int64_t ControllerInternal::bytes_transferred() const {
  ControllerLock lock(mutex_);
  std::unique_lock<std::mutex> token_lock;
  if (LockLiveStorageLocked(&token_lock) == nullptr) return 0;
  JNIEnv* env = GetEnvLocked();
  if (env == nullptr) return 0;
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (jni::CheckAndClearException(env) || !snapshot) {
    if (snapshot) env->DeleteLocalRef(snapshot);
    return 0;
  }
  const std::int64_t result = SnapshotLong(
      env, snapshot, upload_task_task_snapshot::kGetBytesTransferred,
      file_download_task_task_snapshot::kGetBytesTransferred,
      stream_download_task_task_snapshot::kGetBytesTransferred, 0);
  env->DeleteLocalRef(snapshot);
  return result;
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  ControllerLock lock(mutex_);
  // Hold the storage lifetime token for the whole call (including the
  // StorageReferenceInternal construction) so the StorageInternal cannot be
  // destroyed underneath us.
  std::unique_lock<std::mutex> token_lock;
  StorageInternal* storage = LockLiveStorageLocked(&token_lock);
  if (storage == nullptr) return nullptr;
  JNIEnv* env = GetEnvLocked();
  if (env == nullptr) return nullptr;
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
  if (jni::CheckAndClearException(env) || !snapshot) {
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
  if (jni::CheckAndClearException(env) || !obj) {
    if (obj) env->DeleteLocalRef(obj);
    return nullptr;
  }
  StorageReferenceInternal* internal = new StorageReferenceInternal(storage, obj);
  env->DeleteLocalRef(obj);
  jni::CheckAndClearException(env);
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
      {
        std::lock_guard<std::mutex> lock(registry.mutex);
        for (auto it = registry.in_flight.begin();
             it != registry.in_flight.end(); ++it) {
          if (it->storage == storage && it->listener == listener &&
              it->thread_id == thread_id) {
            registry.in_flight.erase(it);
            break;
          }
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
  if (jni::CheckAndClearException(env) || !task_obj) {
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
