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
#include "storage/src/android/storage_android.h"

#include <jni.h>
#include "app/src/jni/declaration.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "app/src/assert.h"
#include "app/src/embedded_file.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util.h"
#include "app/src/jni/collection.h"
#include "storage/src/android/controller_android.h"
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

namespace {

bool SecondsToMillisClamped(double seconds, jlong* millis_out) {
  if (!std::isfinite(seconds) || seconds < 0.0 || millis_out == nullptr) {
    return false;
  }
  constexpr jlong kMaxSafeSeconds = std::numeric_limits<jlong>::max() / 1000;
  const double kMaxSafeSecondsDouble = static_cast<double>(kMaxSafeSeconds);
  if (seconds >= kMaxSafeSecondsDouble) {
    *millis_out = kMaxSafeSeconds * 1000;
  } else {
    *millis_out = static_cast<jlong>(seconds * 1000.0);
  }
  return true;
}

}  // namespace

// clang-format off
#define FIREBASE_STORAGE_METHODS(X)                                     \
  X(GetInstance, "getInstance",                                         \
    "(Lcom/google/firebase/FirebaseApp;)"                               \
    "Lcom/google/firebase/storage/FirebaseStorage;",                    \
    firebase::jni::kMethodTypeStatic) \
  X(GetInstanceWithUrl, "getInstance",                                  \
    "(Lcom/google/firebase/FirebaseApp;Ljava/lang/String;)"             \
    "Lcom/google/firebase/storage/FirebaseStorage;",                    \
    firebase::jni::kMethodTypeStatic) \
  X(GetMaxDownloadRetryTimeMillis, "getMaxDownloadRetryTimeMillis",     \
    "()J") \
  X(SetMaxDownloadRetryTimeMillis, "setMaxDownloadRetryTimeMillis",     \
    "(J)V") \
  X(GetMaxUploadRetryTimeMillis, "getMaxUploadRetryTimeMillis",         \
    "()J") \
  X(SetMaxUploadRetryTimeMillis, "setMaxUploadRetryTimeMillis",         \
    "(J)V") \
  X(GetMaxOperationRetryTimeMillis, "getMaxOperationRetryTimeMillis",   \
    "()J") \
  X(SetMaxOperationRetryTimeMillis, "setMaxOperationRetryTimeMillis",   \
    "(J)V") \
  X(GetRootReference, "getReference",                                   \
    "()Lcom/google/firebase/storage/StorageReference;") \
  X(GetReferenceFromUrl, "getReferenceFromUrl",                         \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/storage/StorageReference;") \
  X(GetReferenceFromPath, "getReference",                               \
    "(Ljava/lang/String;)"                                              \
    "Lcom/google/firebase/storage/StorageReference;") \
  X(UseEmulator, "useEmulator",                                         \
    "(Ljava/lang/String;I)V")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(firebase_storage, FIREBASE_STORAGE_METHODS, FIELD_LOOKUP_NONE, "com/google/firebase/storage/FirebaseStorage")


// clang-format off
#define STORAGE_EXCEPTION_FIELDS(X)                                           \
  X(Unknown, "ERROR_UNKNOWN", "I", jni::kFieldTypeStatic)                   \
  X(ObjectNotFound, "ERROR_OBJECT_NOT_FOUND", "I", jni::kFieldTypeStatic)   \
  X(BucketNotFound, "ERROR_BUCKET_NOT_FOUND", "I", jni::kFieldTypeStatic)   \
  X(ProjectNotFound, "ERROR_PROJECT_NOT_FOUND", "I", jni::kFieldTypeStatic) \
  X(QuotaExceeded, "ERROR_QUOTA_EXCEEDED", "I", jni::kFieldTypeStatic)      \
  X(NotAuthenticated, "ERROR_NOT_AUTHENTICATED", "I",                         \
    jni::kFieldTypeStatic)                                                  \
  X(NotAuthorized, "ERROR_NOT_AUTHORIZED", "I", jni::kFieldTypeStatic)      \
  X(InvalidChecksum, "ERROR_INVALID_CHECKSUM", "I", jni::kFieldTypeStatic)  \
  X(Canceled, "ERROR_CANCELED", "I", jni::kFieldTypeStatic)                 \
  X(RetryLimitExceeded, "ERROR_RETRY_LIMIT_EXCEEDED", "I",                    \
      jni::kFieldTypeStatic)
// clang-format on

// clang-format off
#define STORAGE_EXCEPTION_METHODS(X)                                    \
  X(GetCause, "getCause", "()Ljava/lang/Throwable;") \
  X(GetMessage, "getMessage", "()Ljava/lang/String;") \
  X(GetErrorCode, "getErrorCode", "()I")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(storage_exception, STORAGE_EXCEPTION_METHODS, STORAGE_EXCEPTION_FIELDS, "com/google/firebase/storage/StorageException")


JNI_METHOD_LOOKUP_DECLARATION(index_out_of_bounds_exception, METHOD_LOOKUP_NONE, FIELD_LOOKUP_NONE, "java/lang/IndexOutOfBoundsException")


Mutex StorageInternal::init_mutex_;  // NOLINT
int StorageInternal::initialize_count_ = 0;
std::map<jint, Error>* StorageInternal::java_error_to_cpp_ = nullptr;

StorageInternal::StorageInternal(App* app, const char* url)
    : app_(nullptr), obj_(nullptr) {
  if (!app || !Initialize(app)) return;
  app_ = app;
  url_ = url ? url : "";
  const char kApiIdentifier[] = "Storage";
  jni_task_id_ = CreateApiIdentifier(kApiIdentifier, this);

  JNIEnv* env = app_->GetJNIEnv();
  jobject platform_app = app_->GetPlatformApp();
  jobject storage_obj = nullptr;
  if (url_.empty()) {
    storage_obj = env->CallStaticObjectMethod(
        firebase_storage::GetClass(),
        firebase_storage::GetMethodId(firebase_storage::kGetInstance),
        platform_app);
  } else {
    jobject url_jstring = env->NewStringUTF(url_.c_str());
    storage_obj = env->CallStaticObjectMethod(
        firebase_storage::GetClass(),
        firebase_storage::GetMethodId(firebase_storage::kGetInstanceWithUrl),
        platform_app, url_jstring);
    if (url_jstring != nullptr) {
      env->DeleteLocalRef(url_jstring);
    }
  }
  std::string exception = jni::GetAndClearExceptionMessage(env);
  if (platform_app != nullptr) {
    env->DeleteLocalRef(platform_app);
  }
  if (storage_obj == nullptr || !exception.empty()) {
    if (storage_obj != nullptr) {
      env->DeleteLocalRef(storage_obj);
    }
    LogError("firebase::Storage creation failed: %s", exception.c_str());
    Terminate(app_);
    app_ = nullptr;
    return;
  }
  obj_ = env->NewGlobalRef(storage_obj);
  env->DeleteLocalRef(storage_obj);
  if (obj_ == nullptr) {
    LogError("firebase::Storage creation failed: NewGlobalRef returned null.");
    Terminate(app_);
    app_ = nullptr;
    return;
  }
}

StorageInternal::~StorageInternal() {
  // Mark dead first: blocks until any holder of the token mutex is done with
  // this object, and prevents any further use through the token.
  {
    std::lock_guard<std::mutex> lock(lifetime_token_->mutex);
    lifetime_token_->alive = false;
  }
  // If initialization failed, there is nothing to clean up.
  if (app_ == nullptr) return;

  cleanup_.CleanupAll();
  JNIEnv* env = app_->GetJNIEnv();
  firebase::util::CancelCallbacks(env, jni_task_id_.c_str());
  if (obj_ != nullptr) {
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  Terminate(app_);
  app_ = nullptr;

  jni::CheckAndClearException(env);
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
    const bool own_classes_ok =
        firebase_storage::CacheMethodIds(env, activity) &&
        storage_exception::CacheMethodIds(env, activity) &&
        storage_exception::CacheFieldIds(env, activity) &&
        index_out_of_bounds_exception::CacheMethodIds(env, activity) && ::firebase::jni::throwable::CacheMethodIds(env, activity) && ::firebase::jni::j_string::CacheMethodIds(env, activity);
    const bool ref_ok =
        own_classes_ok && StorageReferenceInternal::Initialize(app);
    const bool meta_ok = ref_ok && InitializeMetadataConverter(app);
    const bool controller_ok = meta_ok && ControllerInternal::Initialize(app);
    const bool embedded_ok = controller_ok && InitializeEmbeddedClasses(app);
    if (!embedded_ok) {
      // Roll back every sub-module whose Initialize() was attempted, including
      // the one that failed (Terminate() / ReleaseClass() are safe to call on
      // partially-initialized or uninitialized state: they only release what
      // was actually cached).
      const bool controller_attempted = meta_ok;
      const bool meta_attempted = ref_ok;
      const bool ref_attempted = own_classes_ok;
      if (controller_attempted) ControllerInternal::Terminate(app);
      if (meta_attempted) TerminateMetadataConverter(app);
      if (ref_attempted) StorageReferenceInternal::Terminate(app);
      index_out_of_bounds_exception::ReleaseClass(env);
      storage_exception::ReleaseClass(env);
      firebase_storage::ReleaseClass(env);
      jni::CheckAndClearException(env);
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
    jni::CheckAndClearException(env);
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
       jni::CacheEmbeddedFiles(
          env, app->activity(),
          firebase::internal::EmbeddedFile::ToVector(
              firebase_storage_resources::storage_resources_filename,
              firebase_storage_resources::storage_resources_data,
              firebase_storage_resources::storage_resources_size));
  if (!(cpp_storage_listener::CacheClassFromFiles(env, app->activity(),
                                                  &embedded_files) &&
        cpp_storage_listener::CacheMethodIds(env, app->activity()) &&
        cpp_storage_listener::RegisterNatives(
            env, kCppStorageListener,
            FIREBASE_ARRAYSIZE(kCppStorageListener)) &&
        cpp_byte_downloader::CacheClassFromFiles(env, app->activity(),
                                                 &embedded_files) &&
        cpp_byte_downloader::CacheMethodIds(env, app->activity()) &&
        cpp_byte_downloader::RegisterNatives(
            env, kCppByteDownloader, FIREBASE_ARRAYSIZE(kCppByteDownloader)) &&
        cpp_byte_uploader::CacheClassFromFiles(env, app->activity(),
                                               &embedded_files) &&
        cpp_byte_uploader::CacheMethodIds(env, app->activity()) &&
        cpp_byte_uploader::RegisterNatives(
            env, kCppByteUploader, FIREBASE_ARRAYSIZE(kCppByteUploader)))) {
    cpp_storage_listener::ReleaseClass(env);
    cpp_byte_downloader::ReleaseClass(env);
    cpp_byte_uploader::ReleaseClass(env);
    jni::CheckAndClearException(env);
    return false;
  }
  jni::CheckAndClearException(env);
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
    TerminateMetadataConverter(app);
    StorageReferenceInternal::Terminate(app);

    // Release embedded classes.
    cpp_storage_listener::ReleaseClass(env);
    cpp_byte_downloader::ReleaseClass(env);
    cpp_byte_uploader::ReleaseClass(env);

    jni::CheckAndClearException(env);

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
    if (!env->IsInstanceOf(java_exception, storage_exception::GetClass())) {
      if (error_message != nullptr &&
          env->IsInstanceOf(java_exception, ::firebase::jni::throwable::GetClass())) {
        jobject msg_obj = env->CallObjectMethod(
            java_exception,
            ::firebase::jni::throwable::GetMethodId(::firebase::jni::throwable::kGetMessage));
        if (jni::CheckAndClearException(env)) {
          if (msg_obj != nullptr) env->DeleteLocalRef(msg_obj);
        } else if (msg_obj != nullptr) {
          *error_message = ::firebase::jni::JStringToString(env, msg_obj);
        }
      }
      jni::CheckAndClearException(env);
      return kErrorUnknown;
    }

    jint raw_code = env->CallIntMethod(
        java_exception,
        storage_exception::GetMethodId(storage_exception::kGetErrorCode));
    if (jni::CheckAndClearException(env)) {
      return kErrorUnknown;
    }
    code = ErrorFromJavaErrorCode(raw_code);
    if (error_message != nullptr) {
      jobject msg_obj = env->CallObjectMethod(
          java_exception,
          storage_exception::GetMethodId(storage_exception::kGetMessage));
      if (jni::CheckAndClearException(env)) {
        if (msg_obj != nullptr) env->DeleteLocalRef(msg_obj);
      } else if (msg_obj != nullptr) {
        *error_message = ::firebase::jni::JStringToString(env, msg_obj);
      }
    }
    if (code == kErrorUnknown) {
      jobject cause = env->CallObjectMethod(
          java_exception,
          storage_exception::GetMethodId(storage_exception::kGetCause));
      if (jni::CheckAndClearException(env)) {
        if (cause != nullptr) env->DeleteLocalRef(cause);
      } else if (cause != nullptr) {
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
            jobject cause_msg = env->CallObjectMethod(
                cause,
                ::firebase::jni::throwable::GetMethodId(::firebase::jni::throwable::kGetMessage));
            if (jni::CheckAndClearException(env)) {
              if (cause_msg != nullptr) env->DeleteLocalRef(cause_msg);
            } else if (cause_msg != nullptr) {
              *error_message = ::firebase::jni::JStringToString(env, cause_msg);
            }
          }
        }
        env->DeleteLocalRef(cause);
      }
    }
    jni::CheckAndClearException(env);
  }
  return code;
}

StorageReferenceInternal* StorageInternal::GetReference() const {
  JNIEnv* env = app_->GetJNIEnv();
  jobject storage_reference_obj = env->CallObjectMethod(
      obj_, firebase_storage::GetMethodId(firebase_storage::kGetRootReference));
  if (jni::CheckAndClearException(env) ||
      storage_reference_obj == nullptr) {
    if (storage_reference_obj) env->DeleteLocalRef(storage_reference_obj);
    return nullptr;
  }
  StorageReferenceInternal* internal = new StorageReferenceInternal(
      const_cast<StorageInternal*>(this), storage_reference_obj);
  env->DeleteLocalRef(storage_reference_obj);
  jni::CheckAndClearException(env);
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
  if (jni::CheckAndClearException(env) ||
      storage_reference_obj == nullptr) {
    if (storage_reference_obj) env->DeleteLocalRef(storage_reference_obj);
    LogWarning("Storage::GetReference(): Invalid path specified: %s", path);
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
  if (jni::CheckAndClearException(env) ||
      storage_reference_obj == nullptr) {
    if (storage_reference_obj) env->DeleteLocalRef(storage_reference_obj);
    LogWarning(
        "Storage::GetReferenceFromUrl(): URL '%s' does not match the "
        "Storage URL.",
        url);
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
  jni::CheckAndClearException(env);
  return millis / 1000.0;
}

void StorageInternal::set_max_download_retry_time(
    double maxTransferRetrySeconds) {
  jlong millis = 0;
  if (!SecondsToMillisClamped(maxTransferRetrySeconds, &millis)) {
    return;
  }
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxDownloadRetryTimeMillis),
                      millis);
  jni::CheckAndClearException(env);
}

double StorageInternal::max_upload_retry_time() const {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = env->CallLongMethod(
      obj_, firebase_storage::GetMethodId(
                firebase_storage::kGetMaxUploadRetryTimeMillis));
  jni::CheckAndClearException(env);
  return millis / 1000.0;
}

void StorageInternal::set_max_upload_retry_time(
    double maxTransferRetrySeconds) {
  jlong millis = 0;
  if (!SecondsToMillisClamped(maxTransferRetrySeconds, &millis)) {
    return;
  }
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxUploadRetryTimeMillis),
                      millis);
  jni::CheckAndClearException(env);
}

double StorageInternal::max_operation_retry_time() const {
  JNIEnv* env = app_->GetJNIEnv();
  jlong millis = env->CallLongMethod(
      obj_, firebase_storage::GetMethodId(
                firebase_storage::kGetMaxOperationRetryTimeMillis));
  jni::CheckAndClearException(env);
  return millis / 1000.0;
}

void StorageInternal::set_max_operation_retry_time(
    double maxTransferRetrySeconds) {
  jlong millis = 0;
  if (!SecondsToMillisClamped(maxTransferRetrySeconds, &millis)) {
    return;
  }
  JNIEnv* env = app_->GetJNIEnv();
  env->CallVoidMethod(obj_,
                      firebase_storage::GetMethodId(
                          firebase_storage::kSetMaxOperationRetryTimeMillis),
                      millis);
  jni::CheckAndClearException(env);
}

void StorageInternal::UseEmulator(const char* host, int port) {
  if (host == nullptr || host[0] == '\0') {
    LogError("Emulator host cannot be null or empty.");
    return;
  }
  if (port <= 0 || port > 65535) {
    LogError("Emulator port must be between 1 and 65535.");
    return;
  }
  JNIEnv* env = app_->GetJNIEnv();
  jobject host_string = env->NewStringUTF(host);
  jint port_num = static_cast<jint>(port);

  env->CallVoidMethod(
      obj_, firebase_storage::GetMethodId(firebase_storage::kUseEmulator),
      host_string, port_num);
  env->DeleteLocalRef(host_string);
  jni::CheckAndClearException(env);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
