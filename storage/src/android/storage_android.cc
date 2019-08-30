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

#include "storage/src/android/storage_android.h"

#include <jni.h>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "storage/src/android/controller_android.h"
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

const char kApiIdentifier[] = "Storage";

// clang-format off
#define FIREBASE_STORAGE_METHODS(X)                                     \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/storage/FirebaseStorage;",                    \
    util::kMethodTypeStatic),                                           \
  X(GetInstanceWithUrl, "getInstance",                                  \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/storage/FirebaseStorage;",                    \
    util::kMethodTypeStatic),                                           \
  X(GetMaxDownloadRetryTimeMillis, "getMaxDownloadRetryTimeMillis",     \
    "()J"),                                                             \
  X(SetMaxDownloadRetryTimeMillis, "setMaxDownloadRetryTimeMillis",     \
    "(J)V"),                                                            \
  X(GetMaxUploadRetryTimeMillis, "getMaxUploadRetryTimeMillis",         \
    "()J"),                                                             \
  X(SetMaxUploadRetryTimeMillis, "setMaxUploadRetryTimeMillis",         \
    "(J)V"),                                                            \
  X(GetMaxOperationRetryTimeMillis, "getMaxOperationRetryTimeMillis",   \
    "()J"),                                                             \
  X(SetMaxOperationRetryTimeMillis, "setMaxOperationRetryTimeMillis",   \
    "(J)V"),                                                            \
  X(GetRootReference, "getReference",                                   \
    "()Lcom/google/firebase/storage/StorageReference;"),                \
  X(GetReferenceFromUrl, "getReferenceFromUrl",                         \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/storage/StorageReference;"),                  \
  X(GetReferenceFromPath, "getReference",                               \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/storage/StorageReference;"),                  \
  X(GetApp, "getApp",                                                   \
    "()Lcom/google/firebase/FirebaseApp;")
// clang-format on

METHOD_LOOKUP_DECLARATION(firebase_storage, FIREBASE_STORAGE_METHODS)
METHOD_LOOKUP_DEFINITION(firebase_storage,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/FirebaseStorage",
                         FIREBASE_STORAGE_METHODS)

// clang-format off
#define STORAGE_EXCEPTION_FIELDS(X)                                           \
  X(Unknown, "ERROR_UNKNOWN", "I", util::kFieldTypeStatic),                   \
  X(ObjectNotFound, "ERROR_OBJECT_NOT_FOUND", "I", util::kFieldTypeStatic),   \
  X(BucketNotFound, "ERROR_BUCKET_NOT_FOUND", "I", util::kFieldTypeStatic),   \
  X(ProjectNotFound, "ERROR_PROJECT_NOT_FOUND", "I", util::kFieldTypeStatic), \
  X(QuotaExceeded, "ERROR_QUOTA_EXCEEDED", "I", util::kFieldTypeStatic),      \
  X(NotAuthenticated, "ERROR_NOT_AUTHENTICATED", "I",                         \
    util::kFieldTypeStatic),                                                  \
  X(NotAuthorized, "ERROR_NOT_AUTHORIZED", "I", util::kFieldTypeStatic),      \
  X(InvalidChecksum, "ERROR_INVALID_CHECKSUM", "I", util::kFieldTypeStatic),  \
  X(Canceled, "ERROR_CANCELED", "I", util::kFieldTypeStatic),                 \
  X(RetryLimitExceeded, "ERROR_RETRY_LIMIT_EXCEEDED", "I",                    \
      util::kFieldTypeStatic)
// clang-format on

// clang-format off
#define STORAGE_EXCEPTION_METHODS(X)                                    \
  X(FromErrorStatus, "fromErrorStatus",                                 \
    "(Lcom/google/android/gms/common/api/Status;)"                      \
    "Lcom/google/firebase/storage/StorageException;",                   \
    util::kMethodTypeStatic),                                           \
  X(FromException, "fromException",                                     \
    "(Ljava/lang/Throwable;)"                                           \
    "Lcom/google/firebase/storage/StorageException;",                   \
    util::kMethodTypeStatic),                                           \
  X(GetIsRecoverableException, "getIsRecoverableException", "()Z"),     \
  X(GetCause, "getCause", "()Ljava/lang/Throwable;"),                   \
  X(GetMessage, "getMessage", "()Ljava/lang/String;"),                  \
  X(GetHttpResultCode, "getHttpResultCode", "()I"),                     \
  X(GetErrorCode, "getErrorCode", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(storage_exception, STORAGE_EXCEPTION_METHODS,
                          STORAGE_EXCEPTION_FIELDS)
METHOD_LOOKUP_DEFINITION(storage_exception,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageException",
                         STORAGE_EXCEPTION_METHODS, STORAGE_EXCEPTION_FIELDS)

METHOD_LOOKUP_DECLARATION(index_out_of_bounds_exception, METHOD_LOOKUP_NONE)
METHOD_LOOKUP_DEFINITION(index_out_of_bounds_exception,
                         PROGUARD_KEEP_CLASS
                         "java/lang/IndexOutOfBoundsException",
                         METHOD_LOOKUP_NONE)

METHOD_LOOKUP_DEFINITION(
    cpp_storage_listener,
    "com/google/firebase/storage/internal/cpp/CppStorageListener",
    CPP_STORAGE_LISTENER_METHODS)

METHOD_LOOKUP_DEFINITION(
    cpp_byte_downloader,
    "com/google/firebase/storage/internal/cpp/CppByteDownloader",
    CPP_BYTE_DOWNLOADER_METHODS)

METHOD_LOOKUP_DEFINITION(
    cpp_byte_uploader,
    "com/google/firebase/storage/internal/cpp/CppByteUploader",
    CPP_BYTE_UPLOADER_METHODS)

Mutex StorageInternal::init_mutex_;  // NOLINT
int StorageInternal::initialize_count_ = 0;
std::map<jint, Error>* StorageInternal::java_error_to_cpp_ = nullptr;

StorageInternal::StorageInternal(App* app, const char* url) {
  app_ = nullptr;
  if (!Initialize(app)) return;
  app_ = app;
  url_ = url ? url : "";

  JNIEnv* env = app_->GetJNIEnv();
  jobject url_jstring = env->NewStringUTF(url_.c_str());
  jobject platform_app = app_->GetPlatformApp();
  jobject storage_obj =
      url_.empty()
          ? env->CallStaticObjectMethod(
                firebase_storage::GetClass(),
                firebase_storage::GetMethodId(firebase_storage::kGetInstance),
                platform_app)
          : env->CallStaticObjectMethod(
                firebase_storage::GetClass(),
                firebase_storage::GetMethodId(
                    firebase_storage::kGetInstanceWithUrl),
                platform_app, url_jstring);
  std::string exception = util::GetAndClearExceptionMessage(env);
  env->DeleteLocalRef(platform_app);
  env->DeleteLocalRef(url_jstring);
  obj_ = nullptr;
  FIREBASE_ASSERT_MESSAGE_RETURN_VOID(
      storage_obj != nullptr && exception.empty(),
      "firebase::Storage creation failed %s", exception.c_str());
  obj_ = env->NewGlobalRef(storage_obj);
  env->DeleteLocalRef(storage_obj);
}

StorageInternal::~StorageInternal() {
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  JNIEnv* env = app_->GetJNIEnv();
  env->DeleteGlobalRef(obj_);
  obj_ = nullptr;
  Terminate(app_);
  app_ = nullptr;

  util::CheckAndClearJniExceptions(env);
}

// Which StorageException Java fields correspond to which C++ Error enum values.
static const struct {
  storage_exception::Field java_error_field;
  Error error_code;
} g_error_codes[] = {
    {storage_exception::kUnknown, kErrorUnknown},
    {storage_exception::kObjectNotFound, kErrorObjectNotFound},
    {storage_exception::kBucketNotFound, kErrorBucketNotFound},
    {storage_exception::kProjectNotFound, kErrorProjectNotFound},
    {storage_exception::kQuotaExceeded, kErrorQuotaExceeded},
    {storage_exception::kNotAuthenticated, kErrorUnauthenticated},
    {storage_exception::kNotAuthorized, kErrorUnauthorized},
    {storage_exception::kRetryLimitExceeded, kErrorRetryLimitExceeded},
    {storage_exception::kInvalidChecksum, kErrorNonMatchingChecksum},
    {storage_exception::kCanceled, kErrorCancelled},
    {storage_exception::kFieldCount, kErrorNone},  // sentinel value for end
};

bool StorageInternal::Initialize(App* app) {
  MutexLock init_lock(init_mutex_);
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    jobject activity = app->activity();
    if (!(firebase_storage::CacheMethodIds(env, activity) &&
          storage_exception::CacheMethodIds(env, activity) &&
          storage_exception::CacheFieldIds(env, activity) &&
          index_out_of_bounds_exception::CacheClass(env, activity) &&
          // Call Initialize on all other Storage internal classes.
          StorageReferenceInternal::Initialize(app) &&
          MetadataInternal::Initialize(app) &&
          ControllerInternal::Initialize(app) &&
          InitializeEmbeddedClasses(app))) {
      return false;
    }

    // Cache error codes
    java_error_to_cpp_ = new std::map<jint, Error>();
    for (int i = 0;
         g_error_codes[i].java_error_field != storage_exception::kFieldCount;
         i++) {
      jint java_error = env->GetStaticIntField(
          storage_exception::GetClass(),
          storage_exception::GetFieldId(g_error_codes[i].java_error_field));
      java_error_to_cpp_->insert(
          std::make_pair(java_error, g_error_codes[i].error_code));
    }
    util::CheckAndClearJniExceptions(env);
  }
  initialize_count_++;
  return true;
}

bool StorageInternal::InitializeEmbeddedClasses(App* app) {
  static JNINativeMethod kCppStorageListener[] = {
      {"nativeCallback", "(JJLjava/lang/Object;Z)V",
       reinterpret_cast<void*>(
           &ControllerInternal::CppStorageListenerCallback)}};
  static JNINativeMethod kCppByteDownloader[] = {
      {"writeBytes", "(JJJ[BJ)V",
       reinterpret_cast<void*>(
           &StorageReferenceInternal::CppByteDownloaderWriteBytes)}};
  static JNINativeMethod kCppByteUploader[] = {
      {"readBytes", "(JJJ[BII)I",
       reinterpret_cast<void*>(
           &StorageReferenceInternal::CppByteUploaderReadBytes)}};
  JNIEnv* env = app->GetJNIEnv();
  // Terminate() handles tearing this down.
  // Load embedded classes.
  const std::vector<firebase::internal::EmbeddedFile> embedded_files =
      util::CacheEmbeddedFiles(
          env, app->activity(),
          firebase::internal::EmbeddedFile::ToVector(
              firebase_storage_resources::storage_resources_filename,
              firebase_storage_resources::storage_resources_data,
              firebase_storage_resources::storage_resources_size));
  if (!(cpp_byte_downloader::CacheClassFromFiles(env, app->activity(),
                                                 &embedded_files) &&
        cpp_storage_listener::CacheClassFromFiles(env, app->activity(),
                                                  &embedded_files) &&
        cpp_storage_listener::RegisterNatives(
            env, kCppStorageListener,
            FIREBASE_ARRAYSIZE(kCppStorageListener)) &&
        cpp_byte_downloader::CacheMethodIds(env, app->activity()) &&
        cpp_byte_downloader::RegisterNatives(
            env, kCppByteDownloader, FIREBASE_ARRAYSIZE(kCppByteDownloader)) &&
        cpp_storage_listener::CacheMethodIds(env, app->activity()) &&
        cpp_byte_uploader::CacheMethodIds(env, app->activity()) &&
        cpp_byte_uploader::RegisterNatives(
            env, kCppByteUploader, FIREBASE_ARRAYSIZE(kCppByteUploader)))) {
    return false;
  }
  util::CheckAndClearJniExceptions(env);
  return true;
}

void StorageInternal::Terminate(App* app) {
  MutexLock init_lock(init_mutex_);
  FIREBASE_ASSERT_RETURN_VOID(initialize_count_ > 0);
  initialize_count_--;
  if (initialize_count_ == 0) {
    JNIEnv* env = app->GetJNIEnv();
    firebase_storage::ReleaseClass(env);
    storage_exception::ReleaseClass(env);
    index_out_of_bounds_exception::ReleaseClass(env);

    // Call Terminate on all other Storage internal classes.
    ControllerInternal::Terminate(app);
    MetadataInternal::Terminate(app);
    StorageReferenceInternal::Terminate(app);

    // Release embedded classes.
    cpp_storage_listener::ReleaseClass(env);
    cpp_byte_downloader::ReleaseClass(env);
    cpp_byte_uploader::ReleaseClass(env);

    util::CheckAndClearJniExceptions(env);

    delete java_error_to_cpp_;
    java_error_to_cpp_ = nullptr;
  }
}

App* StorageInternal::app() const { return app_; }

Error StorageInternal::ErrorFromJavaErrorCode(jint error_code) const {
  auto found = java_error_to_cpp_->find(error_code);
  if (found != java_error_to_cpp_->end()) {
    return found->second;
  } else {
    // Couldn't find the error, return kUnknownError.
    return kErrorUnknown;
  }
}

Error StorageInternal::ErrorFromJavaStorageException(
    jobject java_exception, std::string* error_message) const {
  JNIEnv* env = app_->GetJNIEnv();
  Error code = kErrorNone;
  if (java_exception != nullptr) {
    code = ErrorFromJavaErrorCode(env->CallIntMethod(
        java_exception,
        storage_exception::GetMethodId(storage_exception::kGetErrorCode)));
    if (error_message != nullptr) {
      *error_message = util::JniStringToString(
          env, env->CallObjectMethod(java_exception,
                                     storage_exception::GetMethodId(
                                         storage_exception::kGetMessage)));
    }
    if (code == kErrorUnknown) {
      jobject cause = env->CallObjectMethod(
          java_exception,
          storage_exception::GetMethodId(storage_exception::kGetCause));
      if (cause != nullptr) {
        if (env->IsInstanceOf(cause,
                              index_out_of_bounds_exception::GetClass())) {
          code = kErrorDownloadSizeExceeded;
          if (error_message != nullptr) {
            *error_message = GetErrorMessage(code);
          }
        } else {
          // No special error code, but we can at least provide a more helpful
          // error message.
          if (error_message != nullptr) {
            *error_message = util::JniStringToString(
                env, env->CallObjectMethod(cause,
                                           util::throwable::GetMethodId(
                                               util::throwable::kGetMessage)));
          }
        }
        env->DeleteLocalRef(cause);
      }
    }
    util::CheckAndClearJniExceptions(env);
  }
  return code;
}

StorageReferenceInternal* StorageInternal::GetReference() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject storage_reference_obj = env->CallObjectMethod(
      obj_, firebase_storage::GetMethodId(firebase_storage::kGetRootReference));
  FIREBASE_ASSERT(storage_reference_obj != nullptr);
  StorageReferenceInternal* internal = new StorageReferenceInternal(
      const_cast<StorageInternal*>(this), storage_reference_obj);
  env->DeleteLocalRef(storage_reference_obj);
  util::CheckAndClearJniExceptions(env);
  return internal;
}

StorageReferenceInternal* StorageInternal::GetReference(
    const char* path) const {
  FIREBASE_ASSERT_RETURN(nullptr, path != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject path_string = env->NewStringUTF(path);
  jobject storage_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_storage::GetMethodId(firebase_storage::kGetReferenceFromPath),
      path_string);
  env->DeleteLocalRef(path_string);
  if (storage_reference_obj == nullptr) {
    LogWarning("Storage::GetReference(): Invalid path specified: %s", path);
    util::CheckAndClearJniExceptions(env);
    return nullptr;
  }
  StorageReferenceInternal* internal = new StorageReferenceInternal(
      const_cast<StorageInternal*>(this), storage_reference_obj);
  env->DeleteLocalRef(storage_reference_obj);
  return internal;
}

StorageReferenceInternal* StorageInternal::GetReferenceFromUrl(
    const char* url) const {
  FIREBASE_ASSERT_RETURN(nullptr, url != nullptr);
  JNIEnv* env = app_->GetJNIEnv();
  jobject url_string = env->NewStringUTF(url);
  jobject storage_reference_obj = env->CallObjectMethod(
      obj_,
      firebase_storage::GetMethodId(firebase_storage::kGetReferenceFromUrl),
      url_string);
  env->DeleteLocalRef(url_string);
  if (storage_reference_obj == nullptr) {
    LogWarning(
        "Storage::GetReferenceFromUrl(): URL '%s' does not match the "
        "Storage URL.",
        url);
    util::CheckAndClearJniExceptions(env);
    return nullptr;
  }
  StorageReferenceInternal* internal = new StorageReferenceInternal(
      const_cast<StorageInternal*>(this), storage_reference_obj);
  env->DeleteLocalRef(storage_reference_obj);
  return internal;
}

double StorageInternal::max_download_retry_time() const {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = env->CallLongMethod(
      obj_, firebase_storage::GetMethodId(
                firebase_storage::kGetMaxDownloadRetryTimeMillis));
  return millis / 1000.0;
}

void StorageInternal::set_max_download_retry_time(
    double maxTransferRetrySeconds) {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = static_cast<jlong>(maxTransferRetrySeconds * 1000.0);
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxDownloadRetryTimeMillis),
                      millis);
}

double StorageInternal::max_upload_retry_time() const {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = env->CallLongMethod(
      obj_, firebase_storage::GetMethodId(
                firebase_storage::kGetMaxUploadRetryTimeMillis));
  return millis / 1000.0;
}

void StorageInternal::set_max_upload_retry_time(
    double maxTransferRetrySeconds) {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = static_cast<jlong>(maxTransferRetrySeconds * 1000.0);
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxUploadRetryTimeMillis),
                      millis);
}

double StorageInternal::max_operation_retry_time() const {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = env->CallLongMethod(
      obj_, firebase_storage::GetMethodId(
                firebase_storage::kGetMaxOperationRetryTimeMillis));
  return millis / 1000.0;
}

void StorageInternal::set_max_operation_retry_time(
    double maxTransferRetrySeconds) {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = static_cast<jlong>(maxTransferRetrySeconds * 1000.0);
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxOperationRetryTimeMillis),
                      millis);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
