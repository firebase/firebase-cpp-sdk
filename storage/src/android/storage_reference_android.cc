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
#include "storage/src/common/list_result_internal.h"
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
  X(List, "list",                                                              \
    "(I)Lcom/google/android/gms/tasks/Task;"),                                 \
  X(ListWithToken, "list",                                                     \
    "(ILjava/lang/String;)Lcom/google/android/gms/tasks/Task;"),                \
  X(ToString, "toString",                                                      \
    "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(storage_reference, STORAGE_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(storage_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageReference",
                         STORAGE_REFERENCE_METHODS)

// clang-format off
#define LIST_RESULT_METHODS(X)                                                 \
  X(GetItems, "getItems", "()Ljava/util/List;"),                               \
  X(GetPrefixes, "getPrefixes", "()Ljava/util/List;"),                         \
  X(GetPageToken, "getPageToken", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(list_result, LIST_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(list_result,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/ListResult",
                         LIST_RESULT_METHODS)

enum StorageReferenceFn {
  kStorageReferenceFnDelete = 0,
  kStorageReferenceFnGetBytes,
  kStorageReferenceFnGetFile,
  kStorageReferenceFnGetDownloadUrl,
  kStorageReferenceFnGetMetadata,
  kStorageReferenceFnUpdateMetadata,
  kStorageReferenceFnPutBytes,
  kStorageReferenceFnPutFile,
  kStorageReferenceFnList,
  kStorageReferenceFnCount,
};

bool StorageReferenceInternal::Initialize(const App& app) {
  JNIEnv* env = app.GetJNIEnv();
  jobject activity = app.activity();
  return storage_reference::CacheMethodIds(env, activity) &&
         list_result::CacheMethodIds(env, activity);
}

void StorageReferenceInternal::Terminate(JNIEnv* env) {
  storage_reference::ReleaseClass(env);
  list_result::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

StorageReferenceInternal::StorageReferenceInternal(
    std::shared_ptr<StorageInternal> storage, jobject obj)
    : strong_storage_(std::move(storage)), weak_storage_(strong_storage_) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    obj_ = strong_storage_->GetJNIEnv()->NewGlobalRef(obj);
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    WeakTag, std::weak_ptr<StorageInternal> storage, jobject obj)
    : weak_storage_(std::move(storage)) {
  if (auto locked = weak_storage_.lock()) {
    locked->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
    obj_ = locked->GetJNIEnv()->NewGlobalRef(obj);
  }
}

// static
StorageReferenceInternal* StorageReferenceInternal::CreateWeak(
    std::weak_ptr<StorageInternal> storage, jobject ref_obj) {
  return new StorageReferenceInternal(WeakTag{}, std::move(storage), ref_obj);
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& src)
    : strong_storage_(src.LockStorage()), weak_storage_(src.weak_storage_) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    if (src.obj_ != nullptr) {
      obj_ = strong_storage_->GetJNIEnv()->NewGlobalRef(src.obj_);
    }
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& src) {
  if (this == &src) return *this;
  if (obj_ != nullptr) {
    if (JNIEnv* env = util::GetJNIEnvFromApp()) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  if (auto old_storage = LockStorage()) {
    old_storage->future_manager().ReleaseFutureApi(this);
  }
  strong_storage_ = src.LockStorage();
  weak_storage_ = src.weak_storage_;
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    if (src.obj_ != nullptr) {
      obj_ = strong_storage_->GetJNIEnv()->NewGlobalRef(src.obj_);
    }
  }
  return *this;
}

StorageReferenceInternal* StorageReferenceInternal::CloneWeak() const {
  return CreateWeak(weak_storage_, obj_);
}

// static
StorageReference StorageReferenceInternal::CloneWeakReference(
    const StorageReference& ref) {
  return ref.internal_ ? StorageReference(ref.internal_->CloneWeak())
                       : StorageReference();
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& src)
    : strong_storage_(std::move(src.strong_storage_)),
      weak_storage_(std::move(src.weak_storage_)),
      obj_(src.obj_) {
  src.obj_ = nullptr;
  if (auto locked = LockStorage()) {
    locked->future_manager().MoveFutureApi(&src, this);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& src) {
  if (this == &src) return *this;
  if (obj_ != nullptr) {
    if (JNIEnv* env = util::GetJNIEnvFromApp()) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  if (auto old_storage = LockStorage()) {
    old_storage->future_manager().ReleaseFutureApi(this);
  }
  strong_storage_ = std::move(src.strong_storage_);
  weak_storage_ = std::move(src.weak_storage_);
  obj_ = src.obj_;
  src.obj_ = nullptr;
  if (auto locked = LockStorage()) {
    locked->future_manager().MoveFutureApi(&src, this);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

StorageReferenceInternal::~StorageReferenceInternal() {
  if (obj_ != nullptr) {
    if (JNIEnv* env = util::GetJNIEnvFromApp()) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  if (auto locked = LockStorage()) {
    locked->future_manager().ReleaseFutureApi(this);
  }
}

Storage StorageReferenceInternal::storage() const {
  return Storage(LockStorage());
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  auto locked = LockStorage();
  if (!locked || obj_ == nullptr) return nullptr;
  JNIEnv* env = locked->GetJNIEnv();
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
      new StorageReferenceInternal(std::move(locked), child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

namespace {

struct FutureCallbackData {
  FutureCallbackData(FutureHandle handle_,
                     std::shared_ptr<detail::FutureImplCore> future_core_,
                     std::weak_ptr<StorageInternal> storage_,
                     StorageReferenceFn func_, jobject listener_ = nullptr,
                     void* dest_ = nullptr, size_t size_ = 0,
                     jobject cpp_byte_downloader_ = nullptr,
                     jobject cpp_byte_uploader_ = nullptr)
      : handle(handle_),
        future_core(std::move(future_core_)),
        storage(std::move(storage_)),
        func(func_),
        listener(listener_),
        dest(dest_),
        size(size_),
        cpp_byte_downloader(cpp_byte_downloader_),
        cpp_byte_uploader(cpp_byte_uploader_) {}
  FutureHandle handle;
  std::shared_ptr<detail::FutureImplCore> future_core;
  std::weak_ptr<StorageInternal> storage;
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
  std::unique_ptr<FutureCallbackData> data(
      reinterpret_cast<FutureCallbackData*>(callback_data));
  if (!data) {
    util::CheckAndClearJniExceptions(env);
    return;
  }

  auto locked_storage = data->storage.lock();
  if (!locked_storage) {
    // ~StorageInternal() already detached and deleted all registered bridge
    // global refs before releasing JNI class references.
    util::CheckAndClearJniExceptions(env);
    return;
  }

  auto cleanup_refs = [&]() {
    if (data->listener != nullptr &&
        locked_storage->TakeNativeBridge(data->listener)) {
      env->CallVoidMethod(data->listener,
                          cpp_storage_listener::GetMethodId(
                              cpp_storage_listener::kDiscardPointers));
      env->DeleteGlobalRef(data->listener);
    }
    if (data->cpp_byte_downloader != nullptr &&
        locked_storage->TakeNativeBridge(data->cpp_byte_downloader)) {
      env->CallVoidMethod(data->cpp_byte_downloader,
                          cpp_byte_downloader::GetMethodId(
                              cpp_byte_downloader::kDiscardPointers));
      env->DeleteGlobalRef(data->cpp_byte_downloader);
    }
    if (data->cpp_byte_uploader != nullptr &&
        locked_storage->TakeNativeBridge(data->cpp_byte_uploader)) {
      env->CallVoidMethod(
          data->cpp_byte_uploader,
          cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kDiscardPointers));
      env->DeleteGlobalRef(data->cpp_byte_uploader);
    }
    util::CheckAndClearJniExceptions(env);
  };

  auto future_core = std::move(data->future_core);
  FutureHandle handle = data->handle;
  std::string status_msg = status_message ? status_message : "";

  if (result_code != util::kFutureResultSuccess) {
    // Failed, so the result is a StorageException.
    std::string message;
    Error code =
        (result_code == util::kFutureResultCancelled)
            ? kErrorCancelled
            : locked_storage->ErrorFromJavaStorageException(result, &message);
    bool metadata_fn = (data->func == kStorageReferenceFnPutFile ||
                        data->func == kStorageReferenceFnPutBytes ||
                        data->func == kStorageReferenceFnGetMetadata ||
                        data->func == kStorageReferenceFnUpdateMetadata);
    LogDebug("FutureCallback: Completing a Future with an error (%d).", code);
    cleanup_refs();
    locked_storage.reset();
    if (metadata_fn) {
      // If it's a Future<Metadata> that failed, ensure that an invalid metadata
      // is returned.
      ReferenceCountedFutureImpl::CompleteCoreWithResult(
          future_core, SafeFutureHandle<Metadata>(handle), code,
          message.c_str(), Metadata(nullptr));
    } else {
      ReferenceCountedFutureImpl::CompleteCore(future_core, handle, code,
                                               message.c_str());
    }
    return;
  }

  if (result && env->IsInstanceOf(result, util::string::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a String.");
    std::string value = util::JStringToString(env, result);
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<std::string>(
        future_core, SafeFutureHandle<std::string>(handle), kErrorNone,
        status_msg.c_str(), value);
  } else if (result && env->IsInstanceOf(result, util::uri::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a URI.");
    std::string value = util::JniUriToString(env, env->NewLocalRef(result));
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<std::string>(
        future_core, SafeFutureHandle<std::string>(handle), kErrorNone,
        status_msg.c_str(), value);
  } else if (result &&
             env->IsInstanceOf(
                 result, stream_download_task_task_snapshot::GetClass()) &&
             data->dest != nullptr) {
    LogDebug("FutureCallback: Completing a Future from a byte array.");
    size_t num_bytes = static_cast<size_t>(env->CallLongMethod(
        result, stream_download_task_task_snapshot::GetMethodId(
                    stream_download_task_task_snapshot::kGetBytesTransferred)));
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<size_t>(
        future_core, SafeFutureHandle<size_t>(handle), kErrorNone,
        status_msg.c_str(), num_bytes);
  } else if (result &&
             env->IsInstanceOf(result, storage_metadata::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a StorageMetadata.");
    Metadata metadata(new MetadataInternal(data->storage, result));
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<Metadata>(
        future_core, SafeFutureHandle<Metadata>(handle), kErrorNone,
        status_msg.c_str(), metadata);
  } else if (result &&
             env->IsInstanceOf(result, upload_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from an UploadTask.");
    jobject metadata_obj = env->CallObjectMethod(
        result, upload_task_task_snapshot::GetMethodId(
                    upload_task_task_snapshot::kGetMetadata));
    Metadata metadata(new MetadataInternal(data->storage, metadata_obj));
    env->DeleteLocalRef(metadata_obj);
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<Metadata>(
        future_core, SafeFutureHandle<Metadata>(handle), kErrorNone,
        status_msg.c_str(), metadata);
  } else if (result &&
             env->IsInstanceOf(result,
                               file_download_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a FileDownloadTask.");
    size_t bytes = static_cast<size_t>(env->CallLongMethod(
        result, file_download_task_task_snapshot::GetMethodId(
                    file_download_task_task_snapshot::kGetBytesTransferred)));
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<size_t>(
        future_core, SafeFutureHandle<size_t>(handle), kErrorNone,
        status_msg.c_str(), bytes);
  } else if (result && env->IsInstanceOf(result, list_result::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a ListResult.");
    jobject prefixes_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPrefixes));
    jobject items_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetItems));
    jstring page_token_jstr = static_cast<jstring>(env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPageToken)));

    std::vector<StorageReference> prefixes;
    std::vector<StorageReference> items;
    std::string page_token = util::JStringToString(env, page_token_jstr);

    auto process_list = [&](jobject list_obj,
                            std::vector<StorageReference>& out_vec) {
      if (list_obj) {
        int size = env->CallIntMethod(
            list_obj, util::list::GetMethodId(util::list::kSize));
        for (int i = 0; i < size; ++i) {
          jobject ref_obj = env->CallObjectMethod(
              list_obj, util::list::GetMethodId(util::list::kGet), i);
          out_vec.push_back(StorageReference(
              StorageReferenceInternal::CreateWeak(data->storage, ref_obj)));
          env->DeleteLocalRef(ref_obj);
        }
        env->DeleteLocalRef(list_obj);
      }
    };

    process_list(prefixes_list, prefixes);
    process_list(items_list, items);

    if (page_token_jstr) env->DeleteLocalRef(page_token_jstr);

    internal::StorageListResultInternal* list_result_internal =
        new internal::StorageListResultInternal(
            std::move(prefixes), std::move(items), std::move(page_token));
    StorageListResult list_res =
        internal::StorageListResultInternal::AsStorageListResult(
            list_result_internal);
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCoreWithResult<StorageListResult>(
        future_core, SafeFutureHandle<StorageListResult>(handle), kErrorNone,
        status_msg.c_str(), list_res);
  } else {
    LogDebug("FutureCallback: Completing a Future from a default result.");
    cleanup_refs();
    locked_storage.reset();
    ReferenceCountedFutureImpl::CompleteCore(future_core, handle, kErrorNone,
                                             status_msg.c_str());
  }
}

Future<void> StorageReferenceInternal::Delete() {
  auto locked = LockStorage();
  if (!locked) return Future<void>();
  JNIEnv* env = locked->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<void>(kStorageReferenceFnDelete);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kDelete));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnDelete)),
      locked->jni_task_id());
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<void>&>(
                          future_api->LastResult(kStorageReferenceFnDelete))
                    : Future<void>();
}

std::string StorageReferenceInternal::bucket() const {
  auto locked = LockStorage();
  if (!locked || obj_ == nullptr) return std::string();
  JNIEnv* env = locked->GetJNIEnv();
  jstring bucket_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetBucket)));
  return util::JniStringToString(env, bucket_jstring);
}

std::string StorageReferenceInternal::full_path() const {
  auto locked = LockStorage();
  if (!locked || obj_ == nullptr) return std::string();
  JNIEnv* env = locked->GetJNIEnv();
  jstring bucket_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetPath)));
  return util::JniStringToString(env, bucket_jstring);
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  auto locked = LockStorage();
  if (!locked) return Future<size_t>();
  JNIEnv* env = locked->GetJNIEnv();
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
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnGetFile, java_listener)),
      locked->jni_task_id());
  if (controller_out) {
    controller_out->internal_->AssignTask(weak_storage_, task);
  }
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(uri);
  util::CheckAndClearJniExceptions(env);
  return GetFileLastResult();
}

jobject StorageReferenceInternal::AssignListenerToTask(Listener* listener,
                                                       jobject task) {
  auto locked = LockStorage();
  if (!locked) return nullptr;
  JNIEnv* env = locked->GetJNIEnv();
  if (listener != nullptr) {
    jobject java_listener_local = env->NewObject(
        cpp_storage_listener::GetClass(),
        cpp_storage_listener::GetMethodId(cpp_storage_listener::kConstructor),
        static_cast<jlong>(locked->GetNativeHandleId()),
        reinterpret_cast<jlong>(listener));
    jobject java_listener = env->NewGlobalRef(java_listener_local);
    env->DeleteLocalRef(java_listener_local);
    locked->RegisterNativeBridge(java_listener,
                                 StorageInternal::NativeBridgeKind::kListener);

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
  auto* future_api = future();
  return future_api ? static_cast<const Future<size_t>&>(
                          future_api->LastResult(kStorageReferenceFnGetFile))
                    : Future<size_t>();
}

Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  auto locked = LockStorage();
  if (!locked) return Future<size_t>();
  JNIEnv* env = locked->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<size_t>(kStorageReferenceFnGetBytes);
  jobject byte_downloader_local = env->NewObject(
      cpp_byte_downloader::GetClass(),
      cpp_byte_downloader::GetMethodId(cpp_byte_downloader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size));
  jobject byte_downloader = env->NewGlobalRef(byte_downloader_local);
  env->DeleteLocalRef(byte_downloader_local);
  locked->RegisterNativeBridge(
      byte_downloader, StorageInternal::NativeBridgeKind::kByteDownloader);

  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetStream),
      byte_downloader);
  jobject java_listener = AssignListenerToTask(listener, task);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData, and will
      // handle copying the data into the buffer.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnGetBytes, java_listener,
                                 buffer, buffer_size, byte_downloader)),
      locked->jni_task_id());
  if (controller_out) {
    controller_out->internal_->AssignTask(weak_storage_, task);
  }
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);
  return GetBytesLastResult();
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<size_t>&>(
                          future_api->LastResult(kStorageReferenceFnGetBytes))
                    : Future<size_t>();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  auto locked = LockStorage();
  if (!locked) return Future<std::string>();
  JNIEnv* env = locked->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<std::string>(kStorageReferenceFnGetDownloadUrl);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetDownloadUrl));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnGetDownloadUrl)),
      locked->jni_task_id());
  env->DeleteLocalRef(task);
  util::CheckAndClearJniExceptions(env);

  return GetDownloadUrlLastResult();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<std::string>&>(
                   future_api->LastResult(kStorageReferenceFnGetDownloadUrl))
             : Future<std::string>();
}

Future<Metadata> StorageReferenceInternal::GetMetadata() {
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  JNIEnv* env = locked->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnGetMetadata);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetMetadata));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnGetMetadata)),
      locked->jni_task_id());
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return GetMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<Metadata>&>(
                   future_api->LastResult(kStorageReferenceFnGetMetadata))
             : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  if (metadata->is_valid()) metadata->internal_->CommitCustomMetadata();
  JNIEnv* env = locked->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kUpdateMetadata),
      metadata->internal_->obj());
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnUpdateMetadata)),
      locked->jni_task_id());
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return UpdateMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<Metadata>&>(
                   future_api->LastResult(kStorageReferenceFnUpdateMetadata))
             : Future<Metadata>();
}

Future<StorageListResult> StorageReferenceInternal::List(
    int max_results_per_page, const char* page_token) {
  auto locked = LockStorage();
  if (!locked) return Future<StorageListResult>();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<StorageListResult>(kStorageReferenceFnList);

  JNIEnv* env = locked->GetJNIEnv();
  jstring page_token_string =
      page_token ? env->NewStringUTF(page_token) : nullptr;
  jobject task = nullptr;

  if (page_token_string) {
    task = env->CallObjectMethod(
        obj_, storage_reference::GetMethodId(storage_reference::kListWithToken),
        (jint)max_results_per_page, page_token_string);
    env->DeleteLocalRef(page_token_string);
  } else {
    task = env->CallObjectMethod(
        obj_, storage_reference::GetMethodId(storage_reference::kList),
        (jint)max_results_per_page);
  }

  if (util::LogException(env, kLogLevelWarning,
                         "StorageReference::List(): Call to list() failed.")) {
    future_impl->Complete(handle, kErrorUnknown, "Call to list() failed.");
  } else {
    util::RegisterCallbackOnTask(env, task, FutureCallback,
                                 reinterpret_cast<void*>(new FutureCallbackData(
                                     handle, future_impl->core(), weak_storage_,
                                     kStorageReferenceFnList)),
                                 locked->jni_task_id());
  }
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);

  return ListLastResult();
}

Future<StorageListResult> StorageReferenceInternal::ListLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<StorageListResult>&>(
                          future_api->LastResult(kStorageReferenceFnList))
                    : Future<StorageListResult>();
}

std::string StorageReferenceInternal::name() {
  auto locked = LockStorage();
  if (!locked || obj_ == nullptr) return std::string();
  JNIEnv* env = locked->GetJNIEnv();
  jstring name_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetName)));
  return util::JniStringToString(env, name_jstring);
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  auto locked = LockStorage();
  if (!locked || obj_ == nullptr) return nullptr;
  JNIEnv* env = locked->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetParent));
  if (parent_obj == nullptr) {
    // This is already the root node, so return an owning copy of us.
    env->ExceptionClear();
    return new StorageReferenceInternal(std::move(locked), obj_);
  }
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(std::move(locked), parent_obj);
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
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  if (metadata && metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
  }

  JNIEnv* env = locked->GetJNIEnv();
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
      jobject uploader_global = env->NewGlobalRef(uploader);
      locked->RegisterNativeBridge(
          uploader_global, StorageInternal::NativeBridgeKind::kByteUploader);
      util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          // FutureCallback will delete the newed FutureCallbackData.
          reinterpret_cast<void*>(
              new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                     kStorageReferenceFnPutBytes, java_listener,
                                     nullptr, 0, nullptr, uploader_global)),
          locked->jni_task_id());
      if (controller_out) {
        controller_out->internal_->AssignTask(weak_storage_, task);
      }
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
  auto* future_api = future();
  return future_api ? static_cast<const Future<Metadata>&>(
                          future_api->LastResult(kStorageReferenceFnPutBytes))
                    : Future<Metadata>();
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  JNIEnv* env = locked->GetJNIEnv();
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
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnPutFile, java_listener)),
      locked->jni_task_id());
  if (controller_out) {
    controller_out->internal_->AssignTask(weak_storage_, task);
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
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  if (metadata->is_valid()) metadata->internal_->CommitCustomMetadata();
  JNIEnv* env = locked->GetJNIEnv();
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
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl->core(), weak_storage_,
                                 kStorageReferenceFnPutFile, java_listener)),
      locked->jni_task_id());
  if (controller_out) {
    controller_out->internal_->AssignTask(weak_storage_, task);
  }
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  env->DeleteLocalRef(uri);
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<Metadata>&>(
                          future_api->LastResult(kStorageReferenceFnPutFile))
                    : Future<Metadata>();
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  auto locked = LockStorage();
  return locked ? locked->future_manager().GetFutureApi(this) : nullptr;
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
