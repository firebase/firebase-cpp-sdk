// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_ANDROID_CONTROLLER_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_CONTROLLER_ANDROID_H_

#include <jni.h>

#include <cstdint>
#include <memory>
#include <mutex>

#include "app/src/include/firebase/app.h"
#include "app/src/jni/declaration.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"

namespace firebase::storage {
class Listener;
namespace internal {

#define STORAGE_TASK_METHODS(X)                                  \
  X(Pause, "pause", "()Z")                                       \
  X(Resume, "resume", "()Z")                                     \
  X(Cancel, "cancel", "()Z")                                     \
  X(IsPaused, "isPaused", "()Z")                                 \
  X(AddOnPausedListener, "addOnPausedListener",                  \
    "(Lcom/google/firebase/storage/OnPausedListener;)"           \
    "Lcom/google/firebase/storage/StorageTask;")                 \
  X(AddOnProgressListener, "addOnProgressListener",              \
    "(Lcom/google/firebase/storage/OnProgressListener;)"         \
    "Lcom/google/firebase/storage/StorageTask;")                 \
  X(GetSnapshot, "getSnapshot",                                  \
    "()Lcom/google/firebase/storage/StorageTask$ProvideError;")
JNI_METHOD_LOOKUP_DECLARATION(storage_task, STORAGE_TASK_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/storage/StorageTask")

#define UPLOAD_TASK_TASK_SNAPSHOT_METHODS(X)            \
  X(GetStorage, "getStorage",                           \
    "()Lcom/google/firebase/storage/StorageReference;") \
  X(GetTask, "getTask",                                 \
    "()Lcom/google/firebase/storage/StorageTask;")      \
  X(GetTotalByteCount, "getTotalByteCount", "()J")      \
  X(GetBytesTransferred, "getBytesTransferred", "()J")  \
  X(GetMetadata, "getMetadata",                         \
    "()Lcom/google/firebase/storage/StorageMetadata;")
JNI_METHOD_LOOKUP_DECLARATION(
    upload_task_task_snapshot, UPLOAD_TASK_TASK_SNAPSHOT_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/UploadTask$TaskSnapshot")

#define DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS(X)          \
  X(GetStorage, "getStorage",                           \
    "()Lcom/google/firebase/storage/StorageReference;") \
  X(GetTask, "getTask",                                 \
    "()Lcom/google/firebase/storage/StorageTask;")      \
  X(GetTotalByteCount, "getTotalByteCount", "()J")      \
  X(GetBytesTransferred, "getBytesTransferred", "()J")
JNI_METHOD_LOOKUP_DECLARATION(
    file_download_task_task_snapshot, DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/FileDownloadTask$TaskSnapshot")
JNI_METHOD_LOOKUP_DECLARATION(
    stream_download_task_task_snapshot, DOWNLOAD_TASK_TASK_SNAPSHOT_METHODS,
    FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/StreamDownloadTask$TaskSnapshot")

class ControllerInternal {
 public:
  ControllerInternal() = default;
  ~ControllerInternal();
  ControllerInternal(const ControllerInternal& other);
  ControllerInternal& operator=(const ControllerInternal& other);
  ControllerInternal(ControllerInternal&& other) noexcept;
  ControllerInternal& operator=(ControllerInternal&& other) noexcept;

  bool operator==(const ControllerInternal& other) const;
  bool operator!=(const ControllerInternal& other) const {
    return !(*this == other);
  }

  bool Pause();
  bool Resume();
  bool Cancel();
  bool is_paused() const;
  std::int64_t bytes_transferred() const;
  std::int64_t total_byte_count() const;
  StorageReferenceInternal* GetReference() const;

  void AssignTask(StorageInternal* storage, jobject task_obj);

  static bool Initialize(App* app);
  static void Terminate(App* app);

  static void RegisterActiveListener(StorageInternal* storage,
                                     Listener* listener);
  static void UnregisterActiveListener(StorageInternal* storage,
                                       Listener* listener);

  static void CppStorageListenerCallback(JNIEnv* env, jclass clazz,
                                         jlong storage_ptr, jlong listener_ptr,
                                         jobject snapshot,
                                         jboolean is_on_paused);

  bool is_valid() const;

 private:
  using ControllerLock = std::lock_guard<std::recursive_mutex>;

  StorageInternal* LockLiveStorageLocked(
      std::unique_lock<std::mutex>* token_lock) const;
  JNIEnv* GetEnvLocked() const;
  void ReleaseTaskObjLocked();
  bool CallTaskMethod(storage_task::Method method,
                      const char* error_message) const;
  std::int64_t QuerySnapshotLong(
      upload_task_task_snapshot::Method upload_method,
      file_download_task_task_snapshot::Method file_method,
      stream_download_task_task_snapshot::Method stream_method,
      std::int64_t fallback) const;

  mutable std::recursive_mutex mutex_;
  StorageInternal* storage_ = nullptr;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token_;
  jobject task_obj_ = nullptr;
  JavaVM* java_vm_ = nullptr;
};

}  // namespace internal
}  // namespace firebase::storage

#endif  // FIREBASE_STORAGE_SRC_ANDROID_CONTROLLER_ANDROID_H_
