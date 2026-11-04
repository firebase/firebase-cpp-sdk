// Copyright 2016 Google LLC
#include "storage/src/android/storage_reference_android.h"

#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "app/src/app_android.h"
#include "app/src/include/firebase/app.h"
#include "app/src/jni/collection.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/util_android.h"
#include "storage/src/android/controller_android.h"
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/stub/listener_stub.h"
#include "storage/storage_resources.h"

namespace firebase::storage::internal {

#define STORAGE_REFERENCE_METHODS(X)                                           \
  X(Child, "child",                                                            \
    "(Ljava/lang/String;)Lcom/google/firebase/storage/StorageReference;")      \
  X(GetParent, "getParent",                                                    \
    "()Lcom/google/firebase/storage/StorageReference;")                        \
  X(GetRoot, "getRoot", "()Lcom/google/firebase/storage/StorageReference;")    \
  X(GetName, "getName", "()Ljava/lang/String;")                                \
  X(GetPath, "getPath", "()Ljava/lang/String;")                                \
  X(GetBucket, "getBucket", "()Ljava/lang/String;")                            \
  X(GetStorage, "getStorage",                                                  \
    "()Lcom/google/firebase/storage/FirebaseStorage;")                         \
  X(PutStream, "putStream",                                                    \
    "(Ljava/io/InputStream;)Lcom/google/firebase/storage/UploadTask;")         \
  X(PutStreamWithMetadata, "putStream",                                        \
    "(Ljava/io/InputStream;Lcom/google/firebase/storage/StorageMetadata;)"     \
    "Lcom/google/firebase/storage/UploadTask;")                                \
  X(PutFile, "putFile",                                                        \
    "(Landroid/net/Uri;)Lcom/google/firebase/storage/UploadTask;")             \
  X(PutFileWithMetadata, "putFile",                                            \
    "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;)"         \
    "Lcom/google/firebase/storage/UploadTask;")                                \
  X(PutFileWithMetadataAndUri, "putFile",                                      \
    "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;"          \
    "Landroid/net/Uri;)Lcom/google/firebase/storage/UploadTask;")              \
  X(GetActiveUploadTasks, "getActiveUploadTasks", "()Ljava/util/List;")        \
  X(GetActiveDownloadTasks, "getActiveDownloadTasks", "()Ljava/util/List;")    \
  X(GetMetadata, "getMetadata", "()Lcom/google/android/gms/tasks/Task;")       \
  X(GetDownloadUrl, "getDownloadUrl", "()Lcom/google/android/gms/tasks/Task;") \
  X(UpdateMetadata, "updateMetadata",                                          \
    "(Lcom/google/firebase/storage/StorageMetadata;)"                          \
    "Lcom/google/android/gms/tasks/Task;")                                     \
  X(GetStream, "getStream",                                                    \
    "(Lcom/google/firebase/storage/StreamDownloadTask$StreamProcessor;)"       \
    "Lcom/google/firebase/storage/StreamDownloadTask;")                        \
  X(GetFileUri, "getFile",                                                     \
    "(Landroid/net/Uri;)Lcom/google/firebase/storage/FileDownloadTask;")       \
  X(GetFile, "getFile",                                                        \
    "(Ljava/io/File;)Lcom/google/firebase/storage/FileDownloadTask;")          \
  X(Delete, "delete", "()Lcom/google/android/gms/tasks/Task;")                 \
  X(List, "list", "(I)Lcom/google/android/gms/tasks/Task;")                    \
  X(ListWithToken, "list",                                                     \
    "(ILjava/lang/String;)Lcom/google/android/gms/tasks/Task;")                \
  X(ListAll, "listAll", "()Lcom/google/android/gms/tasks/Task;")               \
  X(ToString, "toString", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(
    storage_reference, STORAGE_REFERENCE_METHODS, FIELD_LOOKUP_NONE,
    "com/google/firebase/storage/StorageReference")

#define LIST_RESULT_METHODS(X)                   \
  X(GetItems, "getItems", "()Ljava/util/List;")  \
  X(GetPrefixes, "getPrefixes", "()Ljava/util/List;") \
  X(GetPageToken, "getPageToken", "()Ljava/lang/String;")
JNI_METHOD_LOOKUP_DECLARATION(list_result, LIST_RESULT_METHODS,
                              FIELD_LOOKUP_NONE,
                              "com/google/firebase/storage/ListResult")

static jobject ParseUriString(JNIEnv* env, const char* uri_string) {
  if (!uri_string) return nullptr;
  if (std::strncmp(uri_string, "file://", 7) == 0) uri_string += 7;
  jstring path_str = env->NewStringUTF(uri_string);
  if (jni::CheckAndClearException(env) || !path_str) return nullptr;
  jclass file_cls = env->FindClass("java/io/File");
  jmethodID file_ctor =
      file_cls ? env->GetMethodID(file_cls, "<init>", "(Ljava/lang/String;)V")
               : nullptr;
  jobject file = (!jni::CheckAndClearException(env) && file_ctor)
                     ? env->NewObject(file_cls, file_ctor, path_str)
                     : nullptr;
  if (file_cls) env->DeleteLocalRef(file_cls);
  env->DeleteLocalRef(path_str);
  if (jni::CheckAndClearException(env) || !file) {
    if (file) env->DeleteLocalRef(file);
    return nullptr;
  }
  jclass uri_cls = env->FindClass("android/net/Uri");
  jmethodID from_file =
      uri_cls ? env->GetStaticMethodID(
                    uri_cls, "fromFile", "(Ljava/io/File;)Landroid/net/Uri;")
              : nullptr;
  jobject uri = (!jni::CheckAndClearException(env) && from_file)
                    ? env->CallStaticObjectMethod(uri_cls, from_file, file)
                    : nullptr;
  if (uri_cls) env->DeleteLocalRef(uri_cls);
  env->DeleteLocalRef(file);
  if (jni::CheckAndClearException(env) || !uri) {
    if (uri) env->DeleteLocalRef(uri);
    return nullptr;
  }
  return uri;
}

struct JniListenerToken {
  std::recursive_mutex mutex;
  StorageInternal* storage = nullptr;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
  Listener* listener = nullptr;
  ListenerInternal* listener_internal = nullptr;
  jobject java_listener = nullptr;
  bool active_registered = false;
  bool listener_detach_registered = false;

  JniListenerToken(StorageInternal* s, Listener* l, ListenerInternal* li,
                   jobject jl)
      : storage(s),
        storage_token(s ? s->lifetime_token() : nullptr),
        listener(l),
        listener_internal(li),
        java_listener(jl),
        active_registered(s && l) {}

  JniListenerToken(const JniListenerToken&) = delete;
  JniListenerToken& operator=(const JniListenerToken&) = delete;
  ~JniListenerToken() { Complete(nullptr); }

  void InitRegistrations(const std::shared_ptr<JniListenerToken>& self) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (listener_internal) {
      std::weak_ptr<JniListenerToken> weak_self = self;
      listener_internal->RegisterDetachCallback(this, [weak_self]() {
        if (auto locked = weak_self.lock()) locked->Complete(nullptr, false);
      });
      listener_detach_registered = true;
    }
  }

  void Complete(JNIEnv* env, bool unregister_listener = true) {
    ListenerInternal* old_li = nullptr;
    StorageInternal* old_storage = nullptr;
    Listener* old_listener = nullptr;
    bool was_active = false;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      if (java_listener) {
        JNIEnv* jni_env =
            env ? env
                : ((storage && storage->app()) ? storage->app()->GetJNIEnv()
                                               : util::GetJNIEnvFromApp());
        if (jni_env) {
          jni_env->CallVoidMethod(
              java_listener,
              cpp_storage_listener::GetMethodId(
                  cpp_storage_listener::kDiscardPointers));
          jni::CheckAndClearException(jni_env);
          jni_env->DeleteGlobalRef(java_listener);
        }
        java_listener = nullptr;
      }
      if (listener_detach_registered && unregister_listener) {
        old_li = listener_internal;
      }
      listener_detach_registered = false;
      listener_internal = nullptr;
      old_storage = storage;
      old_listener = listener;
      was_active = active_registered;
      storage = nullptr;
      listener = nullptr;
      active_registered = false;
    }
    if (old_li) old_li->UnregisterDetachCallback(this);
    if (was_active) {
      ControllerInternal::UnregisterActiveListener(old_storage, old_listener);
    }
  }
};

bool StorageReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  if (storage_reference::CacheMethodIds(env, activity) &&
      list_result::CacheMethodIds(env, activity)) {
    return true;
  }
  Terminate(app);
  return false;
}

void StorageReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_reference::ReleaseClass(env);
  list_result::ReleaseClass(env);
  jni::CheckAndClearException(env);
}

StorageReferenceInternal::StorageReferenceInternal(StorageInternal* storage,
                                                   jobject obj)
    : storage_(storage),
      obj_((storage && storage->app() && obj)
               ? storage->app()->GetJNIEnv()->NewGlobalRef(obj)
               : nullptr) {}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& src)
    : StorageReferenceInternal(src.storage_, src.obj_) {}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& src) {
  if (this == &src) return *this;
  if (obj_ && storage_ && storage_->app()) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
  }
  storage_ = src.storage_;
  obj_ = (storage_ && storage_->app() && src.obj_)
             ? storage_->app()->GetJNIEnv()->NewGlobalRef(src.obj_)
             : nullptr;
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& src) noexcept
    : storage_(src.storage_), obj_(src.obj_) {
  src.storage_ = nullptr;
  src.obj_ = nullptr;
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& src) noexcept {
  if (this == &src) return *this;
  if (obj_ && storage_ && storage_->app()) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
  }
  storage_ = src.storage_;
  obj_ = src.obj_;
  src.storage_ = nullptr;
  src.obj_ = nullptr;
  return *this;
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (obj_ && storage_ && storage_->app()) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

Storage* StorageReferenceInternal::storage() const {
  const std::string& u = storage_->url();
  return Storage::GetInstance(storage_->app(), u.empty() ? nullptr : u.c_str());
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  if (!path) return nullptr;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring path_str = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kChild),
      path_str);
  env->DeleteLocalRef(path_str);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::Child(): Couldn't create child reference %s",
          path) ||
      !child_obj) {
    if (child_obj) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  auto* internal = new StorageReferenceInternal(storage_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

namespace {

struct FutureCallbackData {
  FutureHandle handle;
  ReferenceCountedFutureImpl* impl;
  StorageInternal* storage;
  StorageReferenceFn func;
  std::shared_ptr<JniListenerToken> listener_token;
  void* dest = nullptr;
  size_t size = 0;
  jobject cpp_byte_downloader = nullptr;
  jobject cpp_byte_uploader = nullptr;
};

}  // namespace

void StorageReferenceInternal::BindController(Controller* controller_out,
                                              jobject task) const {
  if (!controller_out) return;
  if (!controller_out->internal_) {
    controller_out->internal_ = new ControllerInternal();
  }
  controller_out->internal_->AssignTask(storage_, task);
}

void StorageReferenceInternal::FutureCallback(
    JNIEnv* env, jobject result, firebase::util::FutureResult result_code,
    const char* status_message, void* callback_data) {
  auto* data = reinterpret_cast<FutureCallbackData*>(callback_data);
  if (!data) {
    jni::CheckAndClearException(env);
    return;
  }

  if (data->listener_token) data->listener_token->Complete(env);
  if (data->cpp_byte_downloader) {
    env->CallVoidMethod(data->cpp_byte_downloader,
                        cpp_byte_downloader::GetMethodId(
                            cpp_byte_downloader::kDiscardPointers));
    jni::CheckAndClearException(env);
    env->DeleteGlobalRef(data->cpp_byte_downloader);
  }
  if (data->cpp_byte_uploader) {
    env->CallVoidMethod(
        data->cpp_byte_uploader,
        cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kDiscardPointers));
    jni::CheckAndClearException(env);
    env->DeleteGlobalRef(data->cpp_byte_uploader);
  }

  if (result_code != util::kFutureResultSuccess) {
    std::string message;
    Error code =
        (result_code == util::kFutureResultCancelled)
            ? kErrorCancelled
            : data->storage->ErrorFromJavaStorageException(result, &message);
    if (data->func == kStorageReferenceFnPutFile ||
        data->func == kStorageReferenceFnPutBytes ||
        data->func == kStorageReferenceFnGetMetadata ||
        data->func == kStorageReferenceFnUpdateMetadata) {
      data->impl->CompleteWithResult(data->handle, code, message.c_str(),
                                     Metadata(nullptr));
    } else {
      data->impl->Complete(data->handle, code, message.c_str());
    }
  } else if (result &&
             env->IsInstanceOf(result, ::firebase::jni::j_string::GetClass())) {
    data->impl->CompleteWithResult<std::string>(
        data->handle, kErrorNone, status_message,
        jni::JStringToString(env, result));
  } else if (result &&
             env->IsInstanceOf(result, ::firebase::jni::uri::GetClass())) {
    data->impl->CompleteWithResult<std::string>(
        data->handle, kErrorNone, status_message,
        ::firebase::jni::JniUriToString(env, env->NewLocalRef(result)));
  } else if (result &&
             env->IsInstanceOf(
                 result, stream_download_task_task_snapshot::GetClass())) {
    jlong n = env->CallLongMethod(
        result, stream_download_task_task_snapshot::GetMethodId(
                    stream_download_task_task_snapshot::kGetBytesTransferred));
    jni::CheckAndClearException(env);
    data->impl->CompleteWithResult<size_t>(data->handle, kErrorNone,
                                           status_message,
                                           static_cast<size_t>(n));
  } else if (result && env->IsInstanceOf(result, GetStorageMetadataClass())) {
    data->impl->CompleteWithResult<Metadata>(
        data->handle, kErrorNone, status_message,
        Metadata(StorageMetadataToMetadata(env, result, data->storage)));
  } else if (result &&
             env->IsInstanceOf(result, upload_task_task_snapshot::GetClass())) {
    jobject meta_obj = env->CallObjectMethod(
        result, upload_task_task_snapshot::GetMethodId(
                    upload_task_task_snapshot::kGetMetadata));
    if (jni::CheckAndClearException(env)) {
      if (meta_obj) env->DeleteLocalRef(meta_obj);
      meta_obj = nullptr;
    }
    data->impl->CompleteWithResult<Metadata>(
        data->handle, kErrorNone, status_message,
        meta_obj
            ? Metadata(StorageMetadataToMetadata(env, meta_obj, data->storage))
            : Metadata(nullptr));
    if (meta_obj) env->DeleteLocalRef(meta_obj);
  } else if (result &&
             env->IsInstanceOf(result,
                               file_download_task_task_snapshot::GetClass())) {
    jlong n = env->CallLongMethod(
        result, file_download_task_task_snapshot::GetMethodId(
                    file_download_task_task_snapshot::kGetBytesTransferred));
    jni::CheckAndClearException(env);
    data->impl->CompleteWithResult<size_t>(data->handle, kErrorNone,
                                           status_message,
                                           static_cast<size_t>(n));
  } else if (result && env->IsInstanceOf(result, list_result::GetClass())) {
    jobject prefixes_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPrefixes));
    jni::CheckAndClearException(env);
    jobject items_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetItems));
    jni::CheckAndClearException(env);
    auto page_token_jstr = static_cast<jstring>(env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPageToken)));
    jni::CheckAndClearException(env);

    std::vector<StorageReference> prefixes;
    std::vector<StorageReference> items;
    std::string page_token = jni::JStringToString(env, page_token_jstr);
    if (page_token_jstr) env->DeleteLocalRef(page_token_jstr);

    auto process_list = [&](jobject list_obj,
                            std::vector<StorageReference>& out) {
      if (!list_obj) return;
      jclass cls = env->GetObjectClass(list_obj);
      jmethodID size_m = env->GetMethodID(cls, "size", "()I");
      jmethodID get_m = env->GetMethodID(cls, "get", "(I)Ljava/lang/Object;");
      int sz = env->CallIntMethod(list_obj, size_m);
      if (!jni::CheckAndClearException(env)) {
        for (int i = 0; i < sz; ++i) {
          jobject ref_obj = env->CallObjectMethod(list_obj, get_m, i);
          if (!jni::CheckAndClearException(env) && ref_obj) {
            out.push_back(StorageReference(
                new StorageReferenceInternal(data->storage, ref_obj)));
            env->DeleteLocalRef(ref_obj);
          }
        }
      }
      env->DeleteLocalRef(cls);
      env->DeleteLocalRef(list_obj);
    };
    process_list(prefixes_list, prefixes);
    process_list(items_list, items);

    data->impl->CompleteWithResult<StorageListResult>(
        data->handle, kErrorNone, status_message,
        StorageListResultInternal::AsStorageListResult(
            new StorageListResultInternal(prefixes, items, page_token)));
  } else {
    data->impl->Complete(data->handle, kErrorNone, status_message);
  }
  delete data;
  jni::CheckAndClearException(env);
}

std::string StorageReferenceInternal::CallStringGetter(int method_id) const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  auto jstr = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(
                static_cast<storage_reference::Method>(method_id))));
  if (jni::CheckAndClearException(env) || !jstr) {
    if (jstr) env->DeleteLocalRef(jstr);
    return "";
  }
  std::string s = ::firebase::jni::JStringToString(env, jstr);
  env->DeleteLocalRef(jstr);
  return s;
}

std::string StorageReferenceInternal::bucket() const {
  return CallStringGetter(storage_reference::kGetBucket);
}
std::string StorageReferenceInternal::full_path() const {
  return CallStringGetter(storage_reference::kGetPath);
}
std::string StorageReferenceInternal::name() const {
  return CallStringGetter(storage_reference::kGetName);
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetParent));
  if (jni::CheckAndClearException(env) || !parent_obj) {
    if (parent_obj) env->DeleteLocalRef(parent_obj);
    return nullptr;
  }
  auto* internal = new StorageReferenceInternal(storage_, parent_obj);
  env->DeleteLocalRef(parent_obj);
  return internal;
}

std::shared_ptr<JniListenerToken>
StorageReferenceInternal::AssignListenerToTask(Listener* listener,
                                               jobject task) {
  if (!listener || !task || !storage_ || !storage_->app()) return nullptr;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject local = env->NewObject(
      cpp_storage_listener::GetClass(),
      cpp_storage_listener::GetMethodId(cpp_storage_listener::kConstructor),
      reinterpret_cast<jlong>(storage_), reinterpret_cast<jlong>(listener));
  if (jni::CheckAndClearException(env) || !local) {
    if (local) env->DeleteLocalRef(local);
    return nullptr;
  }
  jobject global = env->NewGlobalRef(local);
  env->DeleteLocalRef(local);
  if (!global) return nullptr;

  ControllerInternal::RegisterActiveListener(storage_, listener);
  auto token = std::make_shared<JniListenerToken>(
      storage_, listener, ListenerInternal::GetImpl(listener), global);
  token->InitRegistrations(token);

  for (auto m : {storage_task::kAddOnPausedListener,
                 storage_task::kAddOnProgressListener}) {
    jobject res =
        env->CallObjectMethod(task, storage_task::GetMethodId(m), global);
    if (res) env->DeleteLocalRef(res);
    jni::CheckAndClearException(env);
  }
  return token;
}

void StorageReferenceInternal::DeleteImpl(SafeFutureHandle<void> handle,
                                          ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kDelete));
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::Delete(): Call to delete() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknown, "Call to delete() failed.");
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnDelete, nullptr},
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::GetFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<size_t> handle,
                                           ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "File path cannot be null or empty.");
    return;
  }
  jobject uri = ParseUriString(env, path);
  if (jni::CheckAndClearException(env) || !uri) {
    if (uri) env->DeleteLocalRef(uri);
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument, "Invalid file path URI.");
    return;
  }
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetFileUri),
      uri);
  env->DeleteLocalRef(uri);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetFile(): Call to getFile() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorUnknown, "Call to getFile() failed.");
    return;
  }
  auto token = AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnGetFile, token},
      storage_->jni_task_id());
  BindController(controller_out, task);
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
}

void StorageReferenceInternal::GetBytesImpl(void* buffer, size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out,
                                            SafeFutureHandle<size_t> handle,
                                            ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (!buffer && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "Buffer cannot be null when buffer_size > 0.");
    return;
  }
  jobject dl_local = env->NewObject(
      cpp_byte_downloader::GetClass(),
      cpp_byte_downloader::GetMethodId(cpp_byte_downloader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size));
  if (jni::CheckAndClearException(env) || !dl_local) {
    if (dl_local) env->DeleteLocalRef(dl_local);
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorUnknown, "Failed to create CppByteDownloader.");
    return;
  }
  jobject dl = env->NewGlobalRef(dl_local);
  env->DeleteLocalRef(dl_local);

  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetStream), dl);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetBytes(): Call to getStream() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    env->DeleteGlobalRef(dl);
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorUnknown, "Call to getStream() failed.");
    return;
  }
  auto token = AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnGetBytes, token, buffer,
                             buffer_size, dl, nullptr},
      storage_->jni_task_id());
  BindController(controller_out, task);
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
}

void StorageReferenceInternal::GetDownloadUrlImpl(
    SafeFutureHandle<std::string> handle, ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetDownloadUrl));
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetDownloadUrl(): Call to getDownloadUrl() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknown, "Call to getDownloadUrl() failed.");
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnGetDownloadUrl, nullptr},
      storage_->jni_task_id());
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
}

void StorageReferenceInternal::GetMetadataImpl(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetMetadata));
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetMetadata(): Call to getMetadata() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->CompleteWithResult(handle, kErrorUnknown,
                            "Call to getMetadata() failed.", Metadata(nullptr));
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnGetMetadata, nullptr},
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::UpdateMetadataImpl(
    const Metadata* metadata, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (!metadata || !metadata->is_valid()) {
    api->CompleteWithResult(handle, kErrorInvalidArgument,
                            "Metadata cannot be null or invalid.",
                            Metadata(nullptr));
    return;
  }
  jobject meta_obj = MetadataToStorageMetadata(env, *metadata->internal_);
  jobject task =
      meta_obj ? env->CallObjectMethod(
                     obj_,
                     storage_reference::GetMethodId(
                         storage_reference::kUpdateMetadata),
                     meta_obj)
               : nullptr;
  if (meta_obj) env->DeleteLocalRef(meta_obj);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::UpdateMetadata(): Call to updateMetadata() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->CompleteWithResult(handle, kErrorUnknown,
                            "Call to updateMetadata() failed.",
                            Metadata(nullptr));
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnUpdateMetadata, nullptr},
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::ListImpl(
    int max_results_per_page, const char* page_token,
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* api) {
  if (max_results_per_page <= 0 || max_results_per_page > 1000) {
    api->Complete(handle, kErrorInvalidArgument,
                  "max_results_per_page must be between 1 and 1000 inclusive.");
    return;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring token_str = page_token ? env->NewStringUTF(page_token) : nullptr;
  jobject task =
      token_str
          ? env->CallObjectMethod(
                obj_,
                storage_reference::GetMethodId(storage_reference::kListWithToken),
                static_cast<jint>(max_results_per_page), token_str)
          : env->CallObjectMethod(
                obj_, storage_reference::GetMethodId(storage_reference::kList),
                static_cast<jint>(max_results_per_page));
  if (token_str) env->DeleteLocalRef(token_str);
  if (util::LogException(env, kLogLevelWarning,
                         "StorageReference::List(): Call to list() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknown, "Call to list() failed.");
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnList, nullptr},
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::ListAllImpl(
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kListAll));
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::ListAll(): Call to listAll() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknown, "Call to listAll() failed.");
    return;
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnListAll, nullptr},
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  PutBytesImpl(buffer, buffer_size, nullptr, listener, controller_out, handle,
               api);
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out,
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (!buffer && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    api->CompleteWithResult(handle, kErrorInvalidArgument,
                            "Buffer cannot be null when buffer_size > 0.",
                            Metadata(nullptr));
    return;
  }
  jobject meta_obj = (metadata && metadata->is_valid())
                         ? MetadataToStorageMetadata(env, *metadata->internal_)
                         : nullptr;
  jobject uploader = env->NewObject(
      cpp_byte_uploader::GetClass(),
      cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size),
      static_cast<jlong>(0));
  std::string err = jni::GetAndClearExceptionMessage(env);
  bool started = false;
  if (err.empty() && uploader) {
    jobject task =
        meta_obj ? env->CallObjectMethod(
                       obj_,
                       storage_reference::GetMethodId(
                           storage_reference::kPutStreamWithMetadata),
                       uploader, meta_obj)
                 : env->CallObjectMethod(
                       obj_,
                       storage_reference::GetMethodId(
                           storage_reference::kPutStream),
                       uploader);
    err = jni::GetAndClearExceptionMessage(env);
    if (err.empty() && task) {
      auto token = AssignListenerToTask(listener, task);
      firebase::util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          new FutureCallbackData{handle.get(), api, storage_,
                                 kStorageReferenceFnPutBytes, token, nullptr, 0,
                                 nullptr, env->NewGlobalRef(uploader)},
          storage_->jni_task_id());
      BindController(controller_out, task);
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(task);
      started = true;
    } else if (task) {
      env->DeleteLocalRef(task);
    }
    env->DeleteLocalRef(uploader);
  } else if (uploader) {
    env->DeleteLocalRef(uploader);
  }
  if (meta_obj) env->DeleteLocalRef(meta_obj);
  if (!started) {
    if (controller_out) *controller_out = Controller();
    api->CompleteWithResult(
        handle, kErrorUnknown,
        err.empty() ? "Call to putStream() failed." : err.c_str(),
        Metadata(nullptr));
  }
}

void StorageReferenceInternal::PutFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<Metadata> handle,
                                           ReferenceCountedFutureImpl* api) {
  PutFileImpl(path, nullptr, listener, controller_out, handle, api);
}

void StorageReferenceInternal::PutFileImpl(const char* path,
                                           const Metadata* metadata,
                                           Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<Metadata> handle,
                                           ReferenceCountedFutureImpl* api) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    api->CompleteWithResult(handle, kErrorInvalidArgument,
                            "File path cannot be null or empty.",
                            Metadata(nullptr));
    return;
  }
  jobject meta_obj = (metadata && metadata->is_valid())
                         ? MetadataToStorageMetadata(env, *metadata->internal_)
                         : nullptr;
  jobject uri = ParseUriString(env, path);
  if (jni::CheckAndClearException(env) || !uri) {
    if (uri) env->DeleteLocalRef(uri);
    if (meta_obj) env->DeleteLocalRef(meta_obj);
    if (controller_out) *controller_out = Controller();
    api->CompleteWithResult(handle, kErrorInvalidArgument,
                            "Invalid file path URI.", Metadata(nullptr));
    return;
  }
  jobject task =
      meta_obj ? env->CallObjectMethod(
                     obj_,
                     storage_reference::GetMethodId(
                         storage_reference::kPutFileWithMetadata),
                     uri, meta_obj)
               : env->CallObjectMethod(
                     obj_,
                     storage_reference::GetMethodId(storage_reference::kPutFile),
                     uri);
  env->DeleteLocalRef(uri);
  if (meta_obj) env->DeleteLocalRef(meta_obj);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::PutFile(): Call to putFile() failed.") ||
      !task) {
    if (task) env->DeleteLocalRef(task);
    if (controller_out) *controller_out = Controller();
    api->CompleteWithResult(handle, kErrorUnknown, "Call to putFile() failed.",
                            Metadata(nullptr));
    return;
  }
  auto token = AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      new FutureCallbackData{handle.get(), api, storage_,
                             kStorageReferenceFnPutFile, token},
      storage_->jni_task_id());
  BindController(controller_out, task);
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
}

void StorageReferenceInternal::CppByteDownloaderWriteBytes(
    JNIEnv* env, jclass, jlong buffer_ptr, jlong buffer_size,
    jlong buffer_offset, jbyteArray byte_array, jlong num_bytes_to_copy) {
  if (!buffer_ptr || !byte_array || num_bytes_to_copy <= 0 ||
      buffer_offset < 0 || buffer_size < 0 || buffer_offset > buffer_size ||
      num_bytes_to_copy > buffer_size - buffer_offset) {
    return;
  }
  jsize len = env->GetArrayLength(byte_array);
  if (len < 0 || num_bytes_to_copy > static_cast<jlong>(len)) return;
  jbyte* jbytes = env->GetByteArrayElements(byte_array, nullptr);
  if (!jbytes) return;
  std::memcpy(reinterpret_cast<void*>(buffer_ptr + buffer_offset), jbytes,
              static_cast<size_t>(num_bytes_to_copy));
  env->ReleaseByteArrayElements(byte_array, jbytes, JNI_ABORT);
}

jint StorageReferenceInternal::CppByteUploaderReadBytes(
    JNIEnv* env, jclass, jlong cpp_buffer_pointer, jlong cpp_buffer_size,
    jlong cpp_buffer_offset, jobject bytes, jint bytes_offset,
    jint num_bytes_to_read) {
  if (!cpp_buffer_pointer || !bytes || num_bytes_to_read <= 0 ||
      bytes_offset < 0 || cpp_buffer_offset < 0 || cpp_buffer_size < 0 ||
      cpp_buffer_offset >= cpp_buffer_size) {
    return -1;
  }
  auto arr = reinterpret_cast<jbyteArray>(bytes);
  jsize len = env->GetArrayLength(arr);
  if (len < 0 || bytes_offset > len || num_bytes_to_read > len - bytes_offset) {
    return -1;
  }
  jbyte* elems = env->GetByteArrayElements(arr, nullptr);
  if (!elems) {
    LogError("Attempt to stream data into Java buffer failed, aborting this "
             "stream.");
    return -2;
  }
  jint n = static_cast<jint>(std::min<jlong>(
      static_cast<jlong>(num_bytes_to_read),
      cpp_buffer_size - cpp_buffer_offset));
  std::memcpy(
      elems + bytes_offset,
      reinterpret_cast<const char*>(cpp_buffer_pointer) + cpp_buffer_offset,
      static_cast<size_t>(n));
  env->ReleaseByteArrayElements(arr, elems, 0);
  return n;
}

}  // namespace firebase::storage::internal
