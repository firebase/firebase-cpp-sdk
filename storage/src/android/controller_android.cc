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
#include "app/src/util_android.h"
#include "storage/src/include/firebase/storage/listener.h"

namespace firebase {
namespace storage {
namespace internal {

METHOD_LOOKUP_DEFINITION(storage_task, PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageTask",
                         STORAGE_TASK_METHODS)

METHOD_LOOKUP_DEFINITION(upload_task_task_snapshot, PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/UploadTask$TaskSnapshot",
                         UPLOAD_TASK_TASK_SNAPSHOT_METHODS)

METHOD_LOOKUP_DEFINITION(
    file_download_task_task_snapshot, PROGUARD_KEEP_CLASS
    "com/google/firebase/storage/FileDownloadTask$TaskSnapshot",
    DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)

METHOD_LOOKUP_DEFINITION(
    stream_download_task_task_snapshot, PROGUARD_KEEP_CLASS
    "com/google/firebase/storage/StreamDownloadTask$TaskSnapshot",
    DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)

bool ControllerInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return storage_task::CacheMethodIds(env, activity) &&
         upload_task_task_snapshot::CacheMethodIds(env, activity) &&
         file_download_task_task_snapshot::CacheMethodIds(env, activity) &&
         stream_download_task_task_snapshot::CacheMethodIds(env, activity);
}

void ControllerInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_task::ReleaseClass(env);
  upload_task_task_snapshot::ReleaseClass(env);
  file_download_task_task_snapshot::ReleaseClass(env);
  stream_download_task_task_snapshot::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

ControllerInternal::ControllerInternal()
    : storage_(nullptr), task_obj_(nullptr) {}

ControllerInternal::~ControllerInternal() {
  if (task_obj_ != nullptr && storage_ != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(task_obj_);
    task_obj_ = nullptr;
  }
}

ControllerInternal::ControllerInternal(const ControllerInternal& other)
    : storage_(other.storage_), task_obj_(nullptr) {
  if (other.storage_ != nullptr && other.task_obj_ != nullptr) {
    task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(other.task_obj_);
  }
}

ControllerInternal& ControllerInternal::operator=(
    const ControllerInternal& other) {
  if (storage_ != nullptr && task_obj_ != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(task_obj_);
  }
  storage_ = other.storage_;
  task_obj_ = nullptr;
  if (other.storage_ != nullptr && other.task_obj_ != nullptr) {
    task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(other.task_obj_);
  }
  return *this;
}

void ControllerInternal::AssignTask(StorageInternal* storage,
                                    jobject task_obj) {
  if (task_obj_ != nullptr && storage_ != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(task_obj_);
    task_obj_ = nullptr;
  }
  storage_ = storage;
  task_obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(task_obj);
}

bool ControllerInternal::Pause() {
  if (storage_ == nullptr || task_obj_ == nullptr) return false;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  env->CallBooleanMethod(task_obj_,
                         storage_task::GetMethodId(storage_task::kPause));
  if (util::LogException(env, kLogLevelError, "Controller::Pause() failed")) {
    return false;
  }
  return true;
}

bool ControllerInternal::Resume() {
  if (storage_ == nullptr || task_obj_ == nullptr) return false;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  env->CallBooleanMethod(task_obj_,
                         storage_task::GetMethodId(storage_task::kResume));
  if (util::LogException(env, kLogLevelError, "Controller::Resume() failed")) {
    return false;
  }
  return true;
}

bool ControllerInternal::Cancel() {
  if (storage_ == nullptr || task_obj_ == nullptr) return false;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  env->CallBooleanMethod(task_obj_,
                         storage_task::GetMethodId(storage_task::kCancel));
  if (util::LogException(env, kLogLevelError, "Controller::Cancel() failed")) {
    return false;
  }
  return true;
}

bool ControllerInternal::is_paused() const {
  if (storage_ == nullptr || task_obj_ == nullptr) return false;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  return env->CallBooleanMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kIsPaused));
}

int64_t ControllerInternal::total_byte_count() const {
  int64_t result = 0;
  if (storage_ == nullptr || task_obj_ == nullptr) return result;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
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
  util::CheckAndClearJniExceptions(env);
  return result;
}

int64_t ControllerInternal::bytes_transferred() const {
  int64_t result = 0;
  if (storage_ == nullptr || task_obj_ == nullptr) return result;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
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
  util::CheckAndClearJniExceptions(env);
  return result;
}

StorageReferenceInternal* ControllerInternal::GetReference() const {
  if (storage_ == nullptr || task_obj_ == nullptr) return nullptr;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject snapshot = env->CallObjectMethod(
      task_obj_, storage_task::GetMethodId(storage_task::kGetSnapshot));
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
  if (!obj) return nullptr;
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(storage_, obj);
  env->DeleteLocalRef(obj);
  util::CheckAndClearJniExceptions(env);
  return internal;
}

void ControllerInternal::CppStorageListenerCallback(JNIEnv* env, jclass clazz,
                                                    jlong storage_ptr,
                                                    jlong listener_ptr,
                                                    jobject snapshot,
                                                    jboolean is_on_paused) {
  if (!storage_ptr || !listener_ptr) return;
  StorageInternal* storage = reinterpret_cast<StorageInternal*>(storage_ptr);
  Listener* listener = reinterpret_cast<Listener*>(listener_ptr);
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
  if (task_obj) {
    ControllerInternal* internal = new ControllerInternal();
    internal->AssignTask(storage, task_obj);
    Controller controller(internal);
    if (is_on_paused) {
      listener->OnPaused(&controller);
    } else {
      listener->OnProgress(&controller);
    }
  }
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
