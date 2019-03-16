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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_CONTROLLER_ANDROID_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_CONTROLLER_ANDROID_H_

#include <jni.h>
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_reference_android.h"

namespace firebase {
namespace storage {
namespace internal {

// Used for StorageTask.
// clang-format off
#define STORAGE_TASK_METHODS(X)                                         \
  X(Pause, "pause", "()Z"),                                             \
  X(Resume, "resume", "()Z"),                                           \
  X(Cancel, "cancel", "()Z"),                                           \
  X(IsPaused, "isPaused", "()Z"),                                       \
  X(AddOnPausedListener, "addOnPausedListener",                         \
    "(Lcom/google/firebase/storage/OnPausedListener;)"                  \
    "Lcom/google/firebase/storage/StorageTask;"),                       \
  X(AddOnProgressListener, "addOnProgressListener",                     \
    "(Lcom/google/firebase/storage/OnProgressListener;)"                \
    "Lcom/google/firebase/storage/StorageTask;"),                       \
  X(GetSnapshot, "getSnapshot",                                         \
    "()Lcom/google/firebase/storage/StorageTask$ProvideError;")
// clang-format on
METHOD_LOOKUP_DECLARATION(storage_task, STORAGE_TASK_METHODS)

// Used for UploadTask.TaskSnapshot.
// clang-format off
#define UPLOAD_TASK_TASK_SNAPSHOT_METHODS(X)                    \
  X(GetStorage, "getStorage",                                   \
    "()Lcom/google/firebase/storage/StorageReference;"),        \
  X(GetTask, "getTask",                                         \
    "()Lcom/google/firebase/storage/StorageTask;"),             \
  X(GetTotalByteCount, "getTotalByteCount", "()J"),             \
  X(GetBytesTransferred, "getBytesTransferred", "()J"),         \
  X(GetMetadata, "getMetadata",                                 \
    "()Lcom/google/firebase/storage/StorageMetadata;")
// clang-format on
METHOD_LOOKUP_DECLARATION(upload_task_task_snapshot,
                          UPLOAD_TASK_TASK_SNAPSHOT_METHODS)

// Used for StreamDownloadTask.TaskSnapshot and FileDownloadTask.TaskSnapshot.
// clang-format off
#define DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS(X)                  \
  X(GetStorage, "getStorage",                                   \
    "()Lcom/google/firebase/storage/StorageReference;"),        \
  X(GetTask, "getTask",                                         \
    "()Lcom/google/firebase/storage/StorageTask;"),             \
  X(GetTotalByteCount, "getTotalByteCount", "()J"),             \
  X(GetBytesTransferred, "getBytesTransferred", "()J")
// clang-format on
METHOD_LOOKUP_DECLARATION(file_download_task_task_snapshot,
                          DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)
METHOD_LOOKUP_DECLARATION(stream_download_task_task_snapshot,
                          DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS)

class ControllerInternal {
 public:
  ControllerInternal();

  ~ControllerInternal();

  ControllerInternal(const ControllerInternal& other);

  ControllerInternal& operator=(const ControllerInternal& other);

  // Pauses the operation currently in progress.
  bool Pause();

  // Resumes the operation that is paused.
  bool Resume();

  // Cancels the operation currently in progress.
  bool Cancel();

  // Returns true if the operation is paused.
  bool is_paused() const;

  // Returns the number of bytes transferred so far.
  int64_t bytes_transferred() const;

  // Returns the total bytes to be transferred.
  int64_t total_byte_count() const;

  // Returns the StorageReference associated with this Controller.
  StorageReferenceInternal* GetReference() const;

  // Assign implementation pointers.
  //
  // We don't assign these in the constructor because Controllers can be
  // constructed by the user of the library, and those controllers are not
  // associated with a specific operation until passed to a Read or Write call.
  void AssignTask(StorageInternal* storage, jobject task_obj);

  // Initialize JNI bindings for this class.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Instantiate a ControllerInternal from a task snapshot by calling getTask on
  // the snapshot. Returns nullptr if the type was unknown.
  static ControllerInternal* ControllerFromTaskSnapshot(
      StorageInternal* storage, jobject task_snapshot_obj);

  static void CppStorageListenerCallback(JNIEnv* env, jclass clazz,
                                         jlong storage_ptr, jlong listener_ptr,
                                         jobject snapshot,
                                         jboolean is_on_paused);

  bool is_valid() { return storage_ != nullptr && task_obj_ != nullptr; }

 private:
  StorageInternal* storage_;
  jobject task_obj_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_CONTROLLER_ANDROID_H_
