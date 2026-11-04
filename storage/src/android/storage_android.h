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

#ifndef FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_
#define FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_

#include "app/src/app_android.h"
#include <jni.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "app/src/cleanup_notifier.h"
#include "app/src/future_manager.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

// clang-format off
#define CPP_STORAGE_LISTENER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V"),                                     \
    X(DiscardPointers, "discardPointers", "()V")
// clang-format on
namespace cpp_storage_listener {
  enum Method {
    kConstructor,
    kDiscardPointers,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


// clang-format off
#define CPP_BYTE_DOWNLOADER_METHODS(X)                                    \
    X(Constructor, "<init>", "(JJ)V"),                                    \
    X(DiscardPointers, "discardPointers", "()V")
// clang-format on
namespace cpp_byte_downloader {
  enum Method {
    kConstructor,
    kDiscardPointers,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


// clang-format off
#define CPP_BYTE_UPLOADER_METHODS(X)                                      \
    X(Constructor, "<init>", "(JJJ)V"),                                   \
    X(DiscardPointers, "discardPointers", "()V")
// clang-format on
namespace cpp_byte_uploader {
  enum Method {
    kConstructor,
    kDiscardPointers,
    kMethodCount
  };
  enum Field {
    kFieldCount
  };

  jclass GetClass();
  bool CacheMethodIds(JNIEnv* env, jobject activity);
  bool CacheFieldIds(JNIEnv* env, jobject activity);
  void ReleaseClass(JNIEnv* env);
  jmethodID GetMethodId(Method method);
  jfieldID GetFieldId(Field field);
}


class StorageInternal {
 public:
  // Build a Storage. A nullptr or empty url uses the default getInstance.
  StorageInternal(App* app, const char* url);
  ~StorageInternal();

  StorageInternal(const StorageInternal&) = delete;
  StorageInternal& operator=(const StorageInternal&) = delete;
  StorageInternal(StorageInternal&&) = delete;
  StorageInternal& operator=(StorageInternal&&) = delete;

  // Return the App we were created with.
  App* app() const;

  // Return the URL we were created with.
  const std::string& url() const { return url_; }

  StorageReferenceInternal* GetReference() const;

  StorageReferenceInternal* GetReference(const char* path) const;

  StorageReferenceInternal* GetReferenceFromUrl(const char* url) const;

  // Returns the maximum time in seconds to retry a download if a failure
  // occurs.
  double max_download_retry_time() const;
  // Sets the maximum time to retry a download if a failure occurs.
  void set_max_download_retry_time(double max_transfer_retry_seconds);

  // Returns the maximum time to retry an upload if a failure occurs.
  double max_upload_retry_time() const;
  // Sets the maximum time to retry an upload if a failure occurs.
  void set_max_upload_retry_time(double max_transfer_retry_seconds);

  // Returns the maximum time to retry operations other than upload and
  // download if a failure occurs.
  double max_operation_retry_time() const;
  // Sets the maximum time to retry operations other than upload and download
  // if a failure occurs.
  void set_max_operation_retry_time(double max_transfer_retry_seconds);

  // Configures the Storage SDK to use an emulated backend instead of calling
  // the default remote backend.
  void UseEmulator(const char* host, int port);

  // Convert an error code obtained from a Java StorageException into a C++
  // Error enum.
  Error ErrorFromJavaErrorCode(jint java_error_code) const;

  // Convert a Java StorageException instance into a C++ Error enum.
  // If error_message is not null, it will be set to the error message string
  // from the exception.
  Error ErrorFromJavaStorageException(jobject java_error,
                                      std::string* error_message) const;

  FutureManager& future_manager() { return future_manager_; }

  // Whether this object was successfully initialized by the constructor.
  bool initialized() const { return app_ != nullptr && obj_ != nullptr; }

  // When this is deleted, it will clean up all StorageReferences and other
  // objects.
  CleanupNotifier& cleanup() { return cleanup_; }

  // Used for registering global callbacks. See
  // firebase::util::RegisterCallbackOnTask for context.
  const char* jni_task_id() const { return jni_task_id_.c_str(); }

  // Shared liveness token. `alive` is cleared under `mutex` at the very start
  // of ~StorageInternal(), before any teardown. Objects that may outlive this
  // StorageInternal (e.g. ControllerInternal) hold the token and only touch
  // the StorageInternal while holding `mutex` and seeing `alive == true`,
  // which also blocks ~StorageInternal() until they are done.
  struct LifetimeToken {
    std::mutex mutex;
    bool alive = true;
  };
  std::shared_ptr<LifetimeToken> lifetime_token() const {
    return lifetime_token_;
  }

 private:
  // Initialize JNI for all classes.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Initialize classes loaded from embedded files.
  static bool InitializeEmbeddedClasses(App* app);

  static Mutex init_mutex_;
  static int initialize_count_;
  static std::map<jint, Error>* java_error_to_cpp_;

  App* app_ = nullptr;
  // Java FirebaseStorage global ref.
  jobject obj_ = nullptr;

  FutureManager future_manager_;

  std::string url_;

  CleanupNotifier cleanup_;

  // String to be used when registering for JNI task callbacks.
  std::string jni_task_id_;

  std::shared_ptr<LifetimeToken> lifetime_token_ =
      std::make_shared<LifetimeToken>();
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_ANDROID_STORAGE_ANDROID_H_
