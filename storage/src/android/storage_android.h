// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_

#include <jni.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "app/src/app_android.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

#define CPP_STORAGE_LISTENER_METHODS(X) \
  X(Constructor, "<init>", "(JJ)V")     \
  X(DiscardPointers, "discardPointers", "()V")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_storage_listener, CPP_STORAGE_LISTENER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/internal/cpp/CppStorageListener")

#define CPP_BYTE_DOWNLOADER_METHODS(X) \
  X(Constructor, "<init>", "(JJ)V")    \
  X(DiscardPointers, "discardPointers", "()V")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_byte_downloader, CPP_BYTE_DOWNLOADER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/internal/cpp/CppByteDownloader")

#define CPP_BYTE_UPLOADER_METHODS(X) \
  X(Constructor, "<init>", "(JJJ)V") \
  X(DiscardPointers, "discardPointers", "()V")
JNI_METHOD_LOOKUP_DECLARATION(
    cpp_byte_uploader, CPP_BYTE_UPLOADER_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/internal/cpp/CppByteUploader")

class StorageInternal {
 public:
  StorageInternal(App* app, const char* url);
  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  App* app() const { return app_; }
  const std::string& url() const { return url_; }

  StorageReferenceInternal* GetReference() const;
  StorageReferenceInternal* GetReference(const char* path) const;
  StorageReferenceInternal* GetReferenceFromUrl(const char* url) const;

  double max_download_retry_time() const;
  void set_max_download_retry_time(double max_transfer_retry_seconds);
  double max_upload_retry_time() const;
  void set_max_upload_retry_time(double max_transfer_retry_seconds);
  double max_operation_retry_time() const;
  void set_max_operation_retry_time(double max_transfer_retry_seconds);

  void UseEmulator(const char* host, int port);

  Error ErrorFromJavaErrorCode(jint java_error_code) const;
  Error ErrorFromJavaStorageException(jobject java_error,
                                      std::string* error_message) const;

  bool initialized() const { return app_ != nullptr && obj_ != nullptr; }
  const char* jni_task_id() const { return jni_task_id_.c_str(); }

  CleanupNotifier& cleanup() { return cleanup_; }

  struct LifetimeToken {
    std::mutex mutex;
    bool alive = true;
  };
  std::shared_ptr<LifetimeToken> lifetime_token() const {
    return lifetime_token_;
  }

 private:
  static bool Initialize(App* app);
  static void Terminate(App* app);
  static bool InitializeEmbeddedClasses(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jint, Error>* java_error_to_cpp_;

  App* app_ = nullptr;
  jobject obj_ = nullptr;
  std::string url_;
  std::string jni_task_id_;
  std::shared_ptr<LifetimeToken> lifetime_token_ =
      std::make_shared<LifetimeToken>();
  CleanupNotifier cleanup_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_
