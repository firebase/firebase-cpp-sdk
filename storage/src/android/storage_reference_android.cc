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

#include "storage/src/android/storage_reference_android.h"

#include <assert.h>
#include <jni.h>
#include <string.h>

#include <algorithm>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "storage/src/android/controller_android.h"
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

// clang-format off
#define STORAGE_REFERENCE_METHODS(X)                                           \
  X(Child, "child",                                                            \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageReference;"),                         \
  X(GetParent, "getParent",                                                    \
    "()Lcom/google/firebase/storage/StorageReference;"),                       \
  X(GetRoot, "getRoot",                                                        \
    "()Lcom/google/firebase/storage/StorageReference;"),                       \
  X(GetName, "getName",                                                        \
    "()Ljava/lang/String;"),                                                   \
  X(GetPath, "getPath",                                                        \
    "()Ljava/lang/String;"),                                                   \
  X(GetBucket, "getBucket",                                                    \
    "()Ljava/lang/String;"),                                                   \
  X(GetStorage, "getStorage",                                                  \
    "()Lcom/google/firebase/storage/FirebaseStorage;"),                        \
  X(PutStream, "putStream",                                                    \
    "(Ljava/io/InputStream;)Lcom/google/firebase/storage/UploadTask;"),        \
  X(PutStreamWithMetadata, "putStream",                                        \
    "(Ljava/io/InputStream;Lcom/google/firebase/storage/StorageMetadata;)"     \
    "Lcom/google/firebase/storage/UploadTask;"),                               \
  X(PutFile, "putFile",                                                        \
    "(Landroid/net/Uri;)"                                                      \
    "Lcom/google/firebase/storage/UploadTask;"),                               \
  X(PutFileWithMetadata, "putFile",                                            \
    "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;)"         \
    "Lcom/google/firebase/storage/UploadTask;"),                               \
  X(PutFileWithMetadataAndUri, "putFile",                                      \
    "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;"          \
    "Landroid/net/Uri;)Lcom/google/firebase/storage/UploadTask;"),             \
  X(GetActiveUploadTasks, "getActiveUploadTasks",                              \
    "()Ljava/util/List;"),                                                     \
  X(GetActiveDownloadTasks, "getActiveDownloadTasks",                          \
    "()Ljava/util/List;"),                                                     \
  X(GetMetadata, "getMetadata",                                                \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
  X(GetDownloadUrl, "getDownloadUrl",                                          \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
  X(UpdateMetadata, "updateMetadata",                                          \
    "(Lcom/google/firebase/storage/StorageMetadata;)"                          \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(GetStream, "getStream",                                                    \
    "(Lcom/google/firebase/storage/StreamDownloadTask$StreamProcessor;)"       \
    "Lcom/google/firebase/storage/StreamDownloadTask;"),                       \
  X(GetFileUri, "getFile",                                                     \
    "(Landroid/net/Uri;)"                                                      \
    "Lcom/google/firebase/storage/FileDownloadTask;"),                         \
  X(GetFile, "getFile",                                                        \
    "(Ljava/io/File;)"                                                         \
    "Lcom/google/firebase/storage/FileDownloadTask;"),                         \
  X(Delete, "delete",                                                          \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
  X(ToString, "toString",                                                      \
    "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(storage_reference, STORAGE_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(storage_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageReference",
                         STORAGE_REFERENCE_METHODS)

enum StorageReferenceFn {
  kStorageReferenceFnDelete = 0,
  kStorageReferenceFnGetBytes,
  kStorageReferenceFnGetFile,
  kStorageReferenceFnGetDownloadUrl,
  kStorageReferenceFnGetMetadata,
  kStorageReferenceFnUpdateMetadata,
  kStorageReferenceFnPutBytes,
  kStorageReferenceFnPutFile,
  kStorageReferenceFnCount,
};

bool StorageReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return storage_reference::CacheMethodIds(env, activity);
}

void StorageReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_reference::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

StorageReferenceInternal::StorageReferenceInternal(StorageInternal* storage,
                                                   jobject obj)
    : storage_(storage) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(obj);
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& src)
    : storage_(src.storage_) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
  obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& src) {
  obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& src)
    : storage_(src.storage_) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  storage_->future_manager().MoveFutureApi(&src, this);
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& src) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  storage_->future_manager().MoveFutureApi(&src, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

StorageReferenceInternal::~StorageReferenceInternal() {
  if (obj_ != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  storage_->future_manager().ReleaseFutureApi(this);
}

Storage* StorageReferenceInternal::storage() {
  return Storage::GetInstance(storage_->app());
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kChild),
      path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::Child(): Couldn't create child reference %s",
          path)) {
    return nullptr;
  }
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(storage_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

namespace {

struct FutureCallbackData {
  FutureCallbackData(FutureHandle handle_, ReferenceCountedFutureImpl* impl_,
                     StorageInternal* storage_, StorageReferenceFn func_,
                     jobject listener_ = nullptr, void* dest_ = nullptr,
                     size_t size_ = 0, jobject cpp_byte_downloader_ = nullptr,
                     jobject cpp_byte_uploader_ = nullptr)
      : handle(handle_),
        impl(impl_),
        storage(storage_),
        func(func_),
        listener(listener_),
        dest(dest_),
        size(size_),
        cpp_byte_downloader(cpp_byte_downloader_),
        cpp_byte_uploader(cpp_byte_uploader_) {}
  FutureHandle handle;
  ReferenceCountedFutureImpl* impl;
  StorageInternal* storage;
  StorageReferenceFn func;
  jobject listener;
  void* dest;
  size_t size;
  jobject cpp_byte_downloader;
  jobject cpp_byte_uploader;
};

}  // namespace

// Universal callback handler. This callback checks the Java type of `result`
// and completes a different typed Future depending on that type.
void StorageReferenceInternal::FutureCallback(JNIEnv* env, jobject result,
                                              util::FutureResult result_code,
                                              const char* status_message,
                                              void* callback_data) {
  FutureCallbackData* data =
      reinterpret_cast<FutureCallbackData*>(callback_data);
  if (data == nullptr) {
    util::CheckAndClearJniExceptions(env);
    return;
  }

  if (result_code != util::kFutureResultSuccess) {
    // Failed, so the result is a StorageException.
    std::string message;
    Error code =
        (result_code == util::kFutureResultCancelled)
            ? kErrorCancelled
            : data->storage->ErrorFromJavaStorageException(result, &message);
    LogDebug("FutureCallback: Completing a Future with an error (%d).", code);
    if (data->func == kStorageReferenceFnPutFile ||
        data->func == kStorageReferenceFnPutBytes ||
        data->func == kStorageReferenceFnGetMetadata ||
        data->func == kStorageReferenceFnUpdateMetadata) {
      // If it's a Future<Metadata> that failed, ensure that an invalid metadata
      // is returned.
      data->impl->CompleteWithResult(data->handle, code, message.c_str(),
                                     Metadata(nullptr));
    } else {
      data->impl->Complete(data->handle, code, message.c_str());
    }
  } else if (result && env->IsInstanceOf(result, util::string::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a String.");
    // Complete a Future<std::string> from a Java String object.
    data->impl->CompleteWithResult<std::string>(
        data->handle, kErrorNone, status_message,
        util::JStringToString(env, result));
  } else if (result && env->IsInstanceOf(result, util::uri::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a URI.");
    // Complete a Future<std::string> from a Java URI object.
    data->impl->CompleteWithResult<std::string>(
        data->handle, kErrorNone, status_message,
        util::JniUriToString(env, env->NewLocalRef(result)));
  } else if (result &&
             env->IsInstanceOf(
                 result, stream_download_task_task_snapshot::GetClass()) &&
             data->dest != nullptr) {
    // Complete a Future<size_t>. We have previously also copied
    // bytes from a Java byte[] array via CppByteDownloader.
    LogDebug("FutureCallback: Completing a Future from a byte array.");
    jlong num_bytes = env->CallLongMethod(
        result, stream_download_task_task_snapshot::GetMethodId(
                    stream_download_task_task_snapshot::kGetBytesTransferred));
    data->impl->Complete<size_t>(
        data->handle, kErrorNone, status_message,
        [num_bytes](size_t* size) { *size = num_bytes; });
  } else if (result &&
             env->IsInstanceOf(result, storage_metadata::GetClass())) {
    // Complete a Future<Metadata> from a Java StorageMetadata object.
    LogDebug("FutureCallback: Completing a Future from a StorageMetadata.");
    data->impl->Complete<Metadata>(
        data->handle, kErrorNone, status_message,
        [data, result](Metadata* metadata) {
          *metadata = Metadata(new MetadataInternal(data->storage, result));
        });
  } else if (result &&
             env->IsInstanceOf(result, upload_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from an UploadTask.");
    // Complete a Future<Metadata> from a Java UploadTask.TaskSnapshot.
    jobject metadata_obj = env->CallObjectMethod(
        result, upload_task_task_snapshot::GetMethodId(
                    upload_task_task_snapshot::kGetMetadata));
    data->impl->Complete<Metadata>(data->handle, kErrorNone, status_message,
                                   [data, metadata_obj](Metadata* metadata) {
                                     *metadata = Metadata(new MetadataInternal(
                                         data->storage, metadata_obj));
                                   });
    env->DeleteLocalRef(metadata_obj);
  } else if (result &&
             env->IsInstanceOf(result,
                               file_download_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a FileDownloadTask.");
    // Complete a Future<size_t> from a Java FileDownloadTask.TaskSnapshot.
    jlong bytes = env->CallLongMethod(
        result, file_download_task_task_snapshot::GetMethodId(
                    file_download_task_task_snapshot::kGetBytesTransferred));
    data->impl->Complete<size_t>(data->handle, kErrorNone, status_message,
                                 [bytes](size_t* size) { *size = bytes; });
  } else {
    LogDebug("FutureCallback: Completing a Future from a default result.");
    // Unknown or null result type, treat this as a Future<void> and just
    // return success.
    data->impl->Complete(data->handle, kErrorNone, status_message);
  }
  if (data->listener != nullptr) {
    env->CallVoidMethod(data->listener,
                        cpp_storage_listener::GetMethodId(
                            cpp_storage_listener::kDiscardPointers));
    env->DeleteGlobalRef(data->listener);
  }
  if (data->cpp_byte_downloader != nullptr) {
    env->CallVoidMethod(data->cpp_byte_downloader,
                        cpp_byte_downloader::GetMethodId(
                            cpp_byte_downloader::kDiscardPointers));
    env->DeleteGlobalRef(data->cpp_byte_downloader);
  }
  if (data->cpp_byte_uploader != nullptr) {
    env->CallVoidMethod(
        data->cpp_byte_uploader,
        cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kDiscardPointers));
    env->DeleteGlobalRef(data->cpp_byte_uploader);
  }
  delete data;

  util::CheckAndClearJniExceptions(env);
}

Future<void> StorageReferenceInternal::Delete() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<void>(kStorageReferenceFnDelete);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kDelete));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), storage_, kStorageReferenceFnDelete)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kStorageReferenceFnDelete));
}

std::string StorageReferenceInternal::bucket() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring bucket_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetBucket)));
  return util::JniStringToString(env, bucket_jstring);
}

std::string StorageReferenceInternal::full_path() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring bucket_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetPath)));
  return util::JniStringToString(env, bucket_jstring);
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<size_t>(kStorageReferenceFnGetFile);
  jobject uri = util::ParseUriString(env, path);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetFileUri),
      uri);
  jobject java_listener = AssignListenerToTask(listener, task);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(handle, future(), storage_,
                                                     kStorageReferenceFnGetFile,
                                                     java_listener)),
      kApiIdentifier);
  if (controller_out) {
    controller_out->internal_->AssignTask(storage_, task);
  }
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(uri);
  util::CheckAndClearJniExceptions(env);
  return GetFileLastResult();
}

jobject StorageReferenceInternal::AssignListenerToTask(Listener* listener,
                                                       jobject task) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (listener != nullptr) {
    jobject java_listener_local = env->NewObject(
        cpp_storage_listener::GetClass(),
        cpp_storage_listener::GetMethodId(cpp_storage_listener::kConstructor),
        reinterpret_cast<jlong>(storage_), reinterpret_cast<jlong>(listener));
    jobject java_listener = env->NewGlobalRef(java_listener_local);
    env->DeleteLocalRef(java_listener_local);

    env->DeleteLocalRef(env->CallObjectMethod(
        task, storage_task::GetMethodId(storage_task::kAddOnPausedListener),
        java_listener));
    env->DeleteLocalRef(env->CallObjectMethod(
        task, storage_task::GetMethodId(storage_task::kAddOnProgressListener),
        java_listener));
    return java_listener;
  } else {
    return nullptr;
  }
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() {
  return static_cast<const Future<size_t>&>(
      future()->LastResult(kStorageReferenceFnGetFile));
}

Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<size_t>(kStorageReferenceFnGetBytes);
  jobject byte_downloader_local = env->NewObject(
      cpp_byte_downloader::GetClass(),
      cpp_byte_downloader::GetMethodId(cpp_byte_downloader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size));
  jobject byte_downloader = env->NewGlobalRef(byte_downloader_local);
  env->DeleteLocalRef(byte_downloader_local);

  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetStream),
      byte_downloader);
  jobject java_listener = AssignListenerToTask(listener, task);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData, and will
      // handle copying the data into the buffer.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), storage_, kStorageReferenceFnGetBytes,
          java_listener, buffer, buffer_size, byte_downloader)),
      kApiIdentifier);
  if (controller_out) {
    controller_out->internal_->AssignTask(storage_, task);
  }
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);
  return GetBytesLastResult();
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() {
  return static_cast<const Future<size_t>&>(
      future()->LastResult(kStorageReferenceFnGetBytes));
}

Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<std::string>(kStorageReferenceFnGetDownloadUrl);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetDownloadUrl));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), storage_, kStorageReferenceFnGetDownloadUrl)),
      kApiIdentifier);
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return GetDownloadUrlLastResult();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  return static_cast<const Future<std::string>&>(
      future()->LastResult(kStorageReferenceFnGetDownloadUrl));
}

Future<Metadata> StorageReferenceInternal::GetMetadata() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnGetMetadata);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetMetadata));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), storage_, kStorageReferenceFnGetMetadata)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return GetMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnGetMetadata));
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  if (metadata->is_valid()) metadata->internal_->CommitCustomMetadata();
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kUpdateMetadata),
      metadata->internal_->obj());
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future(), storage_, kStorageReferenceFnUpdateMetadata)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return UpdateMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnUpdateMetadata));
}

std::string StorageReferenceInternal::name() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring name_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetName)));
  return util::JniStringToString(env, name_jstring);
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetParent));
  if (parent_obj == nullptr) {
    // This is already the root node, so return a copy of us.
    env->ExceptionClear();
    return new StorageReferenceInternal(*this);
  }
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(storage_, parent_obj);
  env->DeleteLocalRef(parent_obj);
  return internal;
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out) {
  return PutBytes(buffer, buffer_size, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out) {
  if (metadata && metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
  }

  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnPutBytes);
  jobject uploader = env->NewObject(
      cpp_byte_uploader::GetClass(),
      cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size),
      static_cast<jlong>(0));
  std::string exception_message = util::GetAndClearExceptionMessage(env);
  if (exception_message.empty()) {
    jobject task =
        metadata
            ? env->CallObjectMethod(
                  obj_,
                  storage_reference::GetMethodId(
                      storage_reference::kPutStreamWithMetadata),
                  uploader, metadata->internal_->obj())
            : env->CallObjectMethod(
                  obj_,
                  storage_reference::GetMethodId(storage_reference::kPutStream),
                  uploader);
    exception_message = util::GetAndClearExceptionMessage(env);
    if (exception_message.empty()) {
      jobject java_listener = AssignListenerToTask(listener, task);
      util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          // FutureCallback will delete the newed FutureCallbackData.
          reinterpret_cast<void*>(new FutureCallbackData(
              handle, future_impl, storage_, kStorageReferenceFnPutBytes,
              java_listener, nullptr, 0, nullptr, env->NewGlobalRef(uploader))),
          kApiIdentifier);
      if (controller_out) controller_out->internal_->AssignTask(storage_, task);
      env->DeleteLocalRef(task);
    }
    env->DeleteLocalRef(uploader);
  }
  if (!exception_message.empty()) {
    future_impl->Complete(handle, kErrorUnknown, exception_message.c_str());
  }
  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutBytes));
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnPutFile);
  jobject uri = util::ParseUriString(env, path);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kPutFile), uri);
  jobject java_listener = AssignListenerToTask(listener, task);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(handle, future(), storage_,
                                                     kStorageReferenceFnPutFile,
                                                     java_listener)),
      kApiIdentifier);
  if (controller_out) {
    controller_out->internal_->AssignTask(storage_, task);
  }
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(uri);
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  if (metadata->is_valid()) metadata->internal_->CommitCustomMetadata();
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnPutFile);
  jobject uri = util::ParseUriString(env, path);
  jobject task = env->CallObjectMethod(
      obj_,
      storage_reference::GetMethodId(storage_reference::kPutFileWithMetadata),
      uri, metadata->internal_->obj());
  jobject java_listener = AssignListenerToTask(listener, task);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(handle, future(), storage_,
                                                     kStorageReferenceFnPutFile,
                                                     java_listener)),
      kApiIdentifier);
  if (controller_out) {
    controller_out->internal_->AssignTask(storage_, task);
  }
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(uri);
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutFile));
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  return storage_->future_manager().GetFutureApi(this);
}

void StorageReferenceInternal::CppByteDownloaderWriteBytes(
    JNIEnv* env, jclass clazz, jlong buffer_ptr, jlong buffer_size,
    jlong buffer_offset, jbyteArray byte_array, jlong num_bytes_to_copy) {
  if (!buffer_ptr) return;
  FIREBASE_ASSERT(buffer_offset + num_bytes_to_copy <= buffer_size);

  jbyte* jbytes = env->GetByteArrayElements(byte_array, nullptr);
  memcpy(reinterpret_cast<void*>(buffer_ptr + buffer_offset),
         reinterpret_cast<void*>(jbytes), num_bytes_to_copy);
  env->ReleaseByteArrayElements(byte_array, jbytes, JNI_ABORT);
}

jint StorageReferenceInternal::CppByteUploaderReadBytes(
    JNIEnv* env, jclass clazz, jlong cpp_buffer_pointer, jlong cpp_buffer_size,
    jlong cpp_buffer_offset, jobject bytes, jint bytes_offset,
    jint num_bytes_to_read) {
  if (!cpp_buffer_pointer) return -1;
  // Right now we don't support unbound streaming.  Once a streaming callback is
  // plumbed in, it can be called from here.
  assert(cpp_buffer_size >= 0);
  jlong cpp_buffer_remaining = cpp_buffer_size - cpp_buffer_offset;
  if (cpp_buffer_remaining == 0) return -1;
  jbyteArray bytes_array_object = reinterpret_cast<jbyteArray>(bytes);
  jbyte* bytes_array = env->GetByteArrayElements(bytes_array_object, nullptr);
  if (!bytes_array) {
    LogError(
        "Attempt to stream data into Java buffer failed, aborting this "
        "stream.");
    return -2;
  }
  jint data_read =
      std::min(num_bytes_to_read, static_cast<jint>(cpp_buffer_remaining));
  LogDebug("Reading %d bytes from 0x%08x offset %d / %d into %d / %d",
           data_read, static_cast<int>(cpp_buffer_pointer),
           static_cast<int>(cpp_buffer_offset),
           static_cast<int>(cpp_buffer_size), bytes_offset, num_bytes_to_read);
  memcpy(bytes_array + bytes_offset,
         reinterpret_cast<char*>(cpp_buffer_pointer) + cpp_buffer_offset,
         data_read);
  env->ReleaseByteArrayElements(bytes_array_object, bytes_array, 0);
  return data_read;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
