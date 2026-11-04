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

#include "app/src/app_android.h"
#include "app/src/jni/string.h"
#include "app/src/jni/exception.h"
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

#include "app/src/include/firebase/app.h"
#include "storage/src/android/controller_android.h"
#include "storage/src/android/metadata_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/stub/listener_stub.h"
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
  X(ListAll, "listAll",                                                        \
    "()Lcom/google/android/gms/tasks/Task;"),                                  \
  X(ToString, "toString",                                                      \
    "()Ljava/lang/String;")
// clang-format on

namespace storage_reference {
  enum Method {
    kChild,
    kGetParent,
    kGetRoot,
    kGetName,
    kGetPath,
    kGetBucket,
    kGetStorage,
    kPutStream,
    kPutStreamWithMetadata,
    kPutFile,
    kPutFileWithMetadata,
    kPutFileWithMetadataAndUri,
    kGetActiveUploadTasks,
    kGetActiveDownloadTasks,
    kGetMetadata,
    kGetDownloadUrl,
    kUpdateMetadata,
    kGetStream,
    kGetFileUri,
    kGetFile,
    kDelete,
    kList,
    kListWithToken,
    kListAll,
    kToString,
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

namespace storage_reference {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/StorageReference"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kChild] = env->GetMethodID(g_class.get(), "child", "(Ljava/lang/String;)Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kChild]) return false;
    g_method_ids[kGetParent] = env->GetMethodID(g_class.get(), "getParent", "()Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kGetParent]) return false;
    g_method_ids[kGetRoot] = env->GetMethodID(g_class.get(), "getRoot", "()Lcom/google/firebase/storage/StorageReference;");
    if (!g_method_ids[kGetRoot]) return false;
    g_method_ids[kGetName] = env->GetMethodID(g_class.get(), "getName", "()Ljava/lang/String;");
    if (!g_method_ids[kGetName]) return false;
    g_method_ids[kGetPath] = env->GetMethodID(g_class.get(), "getPath", "()Ljava/lang/String;");
    if (!g_method_ids[kGetPath]) return false;
    g_method_ids[kGetBucket] = env->GetMethodID(g_class.get(), "getBucket", "()Ljava/lang/String;");
    if (!g_method_ids[kGetBucket]) return false;
    g_method_ids[kGetStorage] = env->GetMethodID(g_class.get(), "getStorage", "()Lcom/google/firebase/storage/FirebaseStorage;");
    if (!g_method_ids[kGetStorage]) return false;
    g_method_ids[kPutStream] = env->GetMethodID(g_class.get(), "putStream", "(Ljava/io/InputStream;)Lcom/google/firebase/storage/UploadTask;");
    if (!g_method_ids[kPutStream]) return false;
    g_method_ids[kPutStreamWithMetadata] = env->GetMethodID(g_class.get(), "putStream", "(Ljava/io/InputStream;Lcom/google/firebase/storage/StorageMetadata;)Lcom/google/firebase/storage/UploadTask;");
    if (!g_method_ids[kPutStreamWithMetadata]) return false;
    g_method_ids[kPutFile] = env->GetMethodID(g_class.get(), "putFile", "(Landroid/net/Uri;)Lcom/google/firebase/storage/UploadTask;");
    if (!g_method_ids[kPutFile]) return false;
    g_method_ids[kPutFileWithMetadata] = env->GetMethodID(g_class.get(), "putFile", "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;)Lcom/google/firebase/storage/UploadTask;");
    if (!g_method_ids[kPutFileWithMetadata]) return false;
    g_method_ids[kPutFileWithMetadataAndUri] = env->GetMethodID(g_class.get(), "putFile", "(Landroid/net/Uri;Lcom/google/firebase/storage/StorageMetadata;Landroid/net/Uri;)Lcom/google/firebase/storage/UploadTask;");
    if (!g_method_ids[kPutFileWithMetadataAndUri]) return false;
    g_method_ids[kGetActiveUploadTasks] = env->GetMethodID(g_class.get(), "getActiveUploadTasks", "()Ljava/util/List;");
    if (!g_method_ids[kGetActiveUploadTasks]) return false;
    g_method_ids[kGetActiveDownloadTasks] = env->GetMethodID(g_class.get(), "getActiveDownloadTasks", "()Ljava/util/List;");
    if (!g_method_ids[kGetActiveDownloadTasks]) return false;
    g_method_ids[kGetMetadata] = env->GetMethodID(g_class.get(), "getMetadata", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kGetMetadata]) return false;
    g_method_ids[kGetDownloadUrl] = env->GetMethodID(g_class.get(), "getDownloadUrl", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kGetDownloadUrl]) return false;
    g_method_ids[kUpdateMetadata] = env->GetMethodID(g_class.get(), "updateMetadata", "(Lcom/google/firebase/storage/StorageMetadata;)Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kUpdateMetadata]) return false;
    g_method_ids[kGetStream] = env->GetMethodID(g_class.get(), "getStream", "(Lcom/google/firebase/storage/StreamDownloadTask$StreamProcessor;)Lcom/google/firebase/storage/StreamDownloadTask;");
    if (!g_method_ids[kGetStream]) return false;
    g_method_ids[kGetFileUri] = env->GetMethodID(g_class.get(), "getFile", "(Landroid/net/Uri;)Lcom/google/firebase/storage/FileDownloadTask;");
    if (!g_method_ids[kGetFileUri]) return false;
    g_method_ids[kGetFile] = env->GetMethodID(g_class.get(), "getFile", "(Ljava/io/File;)Lcom/google/firebase/storage/FileDownloadTask;");
    if (!g_method_ids[kGetFile]) return false;
    g_method_ids[kDelete] = env->GetMethodID(g_class.get(), "delete", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kDelete]) return false;
    g_method_ids[kList] = env->GetMethodID(g_class.get(), "list", "(I)Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kList]) return false;
    g_method_ids[kListWithToken] = env->GetMethodID(g_class.get(), "list", "(ILjava/lang/String;)Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kListWithToken]) return false;
    g_method_ids[kListAll] = env->GetMethodID(g_class.get(), "listAll", "()Lcom/google/android/gms/tasks/Task;");
    if (!g_method_ids[kListAll]) return false;
    g_method_ids[kToString] = env->GetMethodID(g_class.get(), "toString", "()Ljava/lang/String;");
    if (!g_method_ids[kToString]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


// clang-format off
#define LIST_RESULT_METHODS(X)                                                 \
  X(GetItems, "getItems", "()Ljava/util/List;"),                               \
  X(GetPrefixes, "getPrefixes", "()Ljava/util/List;"),                         \
  X(GetPageToken, "getPageToken", "()Ljava/lang/String;")
// clang-format on

namespace list_result {
  enum Method {
    kGetItems,
    kGetPrefixes,
    kGetPageToken,
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

namespace list_result {
  ::firebase::jni::Global<jclass> g_class;
  jmethodID g_method_ids[kMethodCount];
  jfieldID g_field_ids[kFieldCount];
  jclass GetClass() { return g_class.get(); }
  jmethodID GetMethodId(Method method) { return g_method_ids[method]; }
  jfieldID GetFieldId(Field field) { return g_field_ids[field]; }
  bool CacheMethodIds(JNIEnv* env, jobject activity) {
    if (g_class) return true;
    ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/storage/ListResult"));
    if (!local_clazz) {
      ::firebase::jni::CheckAndClearException(env);
      return false;
    }
    g_class = local_clazz;
    g_method_ids[kGetItems] = env->GetMethodID(g_class.get(), "getItems", "()Ljava/util/List;");
    if (!g_method_ids[kGetItems]) return false;
    g_method_ids[kGetPrefixes] = env->GetMethodID(g_class.get(), "getPrefixes", "()Ljava/util/List;");
    if (!g_method_ids[kGetPrefixes]) return false;
    g_method_ids[kGetPageToken] = env->GetMethodID(g_class.get(), "getPageToken", "()Ljava/lang/String;");
    if (!g_method_ids[kGetPageToken]) return false;
    return CacheFieldIds(env, activity);
  }
  bool CacheFieldIds(JNIEnv* env, jobject activity) {
    if (!g_class) return false;
    return true;
  }
  void ReleaseClass(JNIEnv* env) {
    g_class = ::firebase::jni::Global<jclass>();
  }
}


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
  kStorageReferenceFnListAll,
  kStorageReferenceFnCount,
};

// Lifetime/locking design (see also ControllerInternal):
// * The StorageInternal cleanup callback captures a weak_ptr to the token, so
//   it is a harmless no-op once the token is gone and the token never needs
//   to be unregistered from a dying StorageInternal.
// * Unregistering happens only while holding the StorageInternal's lifetime
//   token mutex and seeing `alive == true`. ~StorageInternal() clears `alive`
//   under that mutex *before* it runs cleanup_.CleanupAll(), so while we hold
//   it no cleanup callback can be in flight: UnregisterObject() never blocks
//   and the StorageInternal cannot be destroyed underneath us.
struct JniListenerToken {
  std::recursive_mutex mutex;
  StorageInternal* storage = nullptr;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
  Listener* listener = nullptr;
  ListenerInternal* listener_internal = nullptr;
  jobject java_listener = nullptr;
  bool active_registered = false;
  bool cleanup_registered = false;
  bool listener_detach_registered = false;
  std::weak_ptr<JniListenerToken>* cleanup_ctx = nullptr;

  JniListenerToken(StorageInternal* s, Listener* l, ListenerInternal* li,
                   jobject jl)
      : storage(s),
        storage_token(s != nullptr ? s->lifetime_token() : nullptr),
        listener(l),
        listener_internal(li),
        java_listener(jl),
        active_registered(s != nullptr && l != nullptr) {}

  JniListenerToken(const JniListenerToken&) = delete;
  JniListenerToken& operator=(const JniListenerToken&) = delete;

  ~JniListenerToken() { Complete(nullptr); }

  void InitRegistrations(const std::shared_ptr<JniListenerToken>& self) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (storage != nullptr) {
      cleanup_ctx = new std::weak_ptr<JniListenerToken>(self);
      storage->cleanup().RegisterObject(cleanup_ctx, [](void* obj) {
        std::unique_ptr<std::weak_ptr<JniListenerToken>> weak_token(
            static_cast<std::weak_ptr<JniListenerToken>*>(obj));
        if (auto locked = weak_token->lock()) {
          locked->OnStorageDestroyed();
        }
      });
      cleanup_registered = true;
    }
    if (listener_internal != nullptr) {
      std::weak_ptr<JniListenerToken> weak_self = self;
      listener_internal->RegisterDetachCallback(this, [weak_self]() {
        if (auto locked = weak_self.lock()) {
          locked->DetachFromListener();
        }
      });
      listener_detach_registered = true;
    }
  }

  // Cleanup callback, run by StorageInternal's CleanupNotifier while the
  // StorageInternal is being destroyed (after its lifetime token was marked
  // dead, so no owner can be blocked in UnregisterObject() while holding
  // `mutex`).
  void OnStorageDestroyed() {
    ListenerInternal* old_li = nullptr;
    StorageInternal* old_storage = nullptr;
    Listener* old_listener = nullptr;
    bool was_active = false;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      cleanup_registered = false;
      cleanup_ctx = nullptr;
      if (java_listener != nullptr && storage != nullptr &&
          storage->app() != nullptr) {
        JNIEnv* env = storage->app()->GetJNIEnv();
        if (env != nullptr) {
          env->CallVoidMethod(
              java_listener,
              cpp_storage_listener::GetMethodId(
                  cpp_storage_listener::kDiscardPointers));
          jni::CheckAndClearException(env);
          env->DeleteGlobalRef(java_listener);
        }
        java_listener = nullptr;
      }
      if (listener_detach_registered) {
        old_li = listener_internal;
        listener_detach_registered = false;
      }
      listener_internal = nullptr;
      old_storage = storage;
      old_listener = listener;
      was_active = active_registered;
      storage = nullptr;
      listener = nullptr;
      active_registered = false;
    }
    if (old_li != nullptr) {
      old_li->UnregisterDetachCallback(this);
    }
    if (was_active) {
      ControllerInternal::UnregisterActiveListener(old_storage, old_listener);
    }
  }

  void DetachFromListener() {
    StorageInternal* old_storage = nullptr;
    Listener* old_listener = nullptr;
    bool was_active = false;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      listener_detach_registered = false;
      listener_internal = nullptr;
      if (java_listener != nullptr) {
        JNIEnv* env = (storage != nullptr && storage->app() != nullptr)
                          ? storage->app()->GetJNIEnv()
                          : util::GetJNIEnvFromApp();
        if (env != nullptr) {
          env->CallVoidMethod(
              java_listener,
              cpp_storage_listener::GetMethodId(
                  cpp_storage_listener::kDiscardPointers));
          jni::CheckAndClearException(env);
          env->DeleteGlobalRef(java_listener);
        }
        java_listener = nullptr;
      }
      if (cleanup_registered) {
        UnregisterFromStorageLocked();
      }
      old_storage = storage;
      old_listener = listener;
      was_active = active_registered;
      storage = nullptr;
      listener = nullptr;
      active_registered = false;
    }
    if (was_active) {
      ControllerInternal::UnregisterActiveListener(old_storage, old_listener);
    }
  }

  // Unregisters from storage->cleanup() if the StorageInternal is still
  // alive (see the design note above). If it is already dying, the cleanup
  // callback owns and deletes `cleanup_ctx`. Must be called with `mutex`
  // held.
  void UnregisterFromStorageLocked() {
    cleanup_registered = false;
    std::weak_ptr<JniListenerToken>* old_ctx = cleanup_ctx;
    cleanup_ctx = nullptr;
    if (storage == nullptr || !storage_token || old_ctx == nullptr) return;
    std::lock_guard<std::mutex> token_lock(storage_token->mutex);
    if (storage_token->alive) {
      storage->cleanup().UnregisterObject(old_ctx);
      delete old_ctx;
    }
  }

  void Complete(JNIEnv* env) {
    ListenerInternal* old_li = nullptr;
    StorageInternal* old_storage = nullptr;
    Listener* old_listener = nullptr;
    bool was_active = false;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      if (java_listener != nullptr) {
        JNIEnv* jni_env =
            env ? env
                : ((storage != nullptr && storage->app() != nullptr)
                       ? storage->app()->GetJNIEnv()
                       : util::GetJNIEnvFromApp());
        if (jni_env != nullptr) {
          jni_env->CallVoidMethod(
              java_listener,
              cpp_storage_listener::GetMethodId(
                  cpp_storage_listener::kDiscardPointers));
          jni::CheckAndClearException(jni_env);
          jni_env->DeleteGlobalRef(java_listener);
        }
        java_listener = nullptr;
      }
      if (cleanup_registered) {
        UnregisterFromStorageLocked();
      }
      if (listener_detach_registered) {
        old_li = listener_internal;
        listener_detach_registered = false;
      }
      listener_internal = nullptr;
      old_storage = storage;
      old_listener = listener;
      was_active = active_registered;
      storage = nullptr;
      listener = nullptr;
      active_registered = false;
    }
    if (old_li != nullptr) {
      old_li->UnregisterDetachCallback(this);
    }
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
    : storage_(storage), obj_(nullptr) {
  if (storage_ != nullptr) {
    storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
    if (obj != nullptr && storage_->app() != nullptr) {
      obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(obj);
    }
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& src)
    : storage_(src.storage_), obj_(nullptr) {
  if (storage_ != nullptr) {
    storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
    if (src.obj_ != nullptr && storage_->app() != nullptr) {
      obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
    }
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& src) {
  if (this == &src) {
    return *this;
  }
  if (obj_ != nullptr && storage_ != nullptr && storage_->app() != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (storage_ != src.storage_) {
    if (storage_ != nullptr) {
      storage_->future_manager().ReleaseFutureApi(this);
    }
    storage_ = src.storage_;
    if (storage_ != nullptr) {
      storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
    }
  }
  if (src.obj_ != nullptr && storage_ != nullptr &&
      storage_->app() != nullptr) {
    obj_ = storage_->app()->GetJNIEnv()->NewGlobalRef(src.obj_);
  }
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& src) noexcept
    : storage_(src.storage_), obj_(src.obj_) {
  src.obj_ = nullptr;
  if (storage_ != nullptr) {
    storage_->future_manager().MoveFutureApi(&src, this);
  }
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& src) noexcept {
  if (this == &src) {
    return *this;
  }
  if (obj_ != nullptr && storage_ != nullptr && storage_->app() != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (storage_ != nullptr && storage_ != src.storage_) {
    storage_->future_manager().ReleaseFutureApi(this);
  }
  storage_ = src.storage_;
  obj_ = src.obj_;
  src.obj_ = nullptr;
  if (storage_ != nullptr) {
    storage_->future_manager().MoveFutureApi(&src, this);
  }
  return *this;
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (obj_ != nullptr && storage_ != nullptr && storage_->app() != nullptr) {
    storage_->app()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  if (storage_ != nullptr) {
    storage_->future_manager().ReleaseFutureApi(this);
  }
}

Storage* StorageReferenceInternal::storage() const {
  const std::string& storage_url = storage_->url();
  return Storage::GetInstance(
      storage_->app(), storage_url.empty() ? nullptr : storage_url.c_str());
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  if (!path) return nullptr;
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kChild),
      path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::Child(): Couldn't create child reference %s",
          path) ||
      child_obj == nullptr) {
    if (child_obj) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  StorageReferenceInternal* internal =
      new StorageReferenceInternal(storage_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

namespace {

struct FutureCallbackData {
  FutureCallbackData(
      FutureHandle handle_, ReferenceCountedFutureImpl* impl_,
      StorageInternal* storage_, StorageReferenceFn func_,
      std::shared_ptr<JniListenerToken> listener_token_ = nullptr,
      void* dest_ = nullptr, size_t size_ = 0,
      jobject cpp_byte_downloader_ = nullptr,
      jobject cpp_byte_uploader_ = nullptr)
      : handle(handle_),
        impl(impl_),
        storage(storage_),
        func(func_),
        listener_token(std::move(listener_token_)),
        dest(dest_),
        size(size_),
        cpp_byte_downloader(cpp_byte_downloader_),
        cpp_byte_uploader(cpp_byte_uploader_) {}
  FutureHandle handle;
  ReferenceCountedFutureImpl* impl;
  StorageInternal* storage;
  StorageReferenceFn func;
  std::shared_ptr<JniListenerToken> listener_token;
  void* dest;
  size_t size;
  jobject cpp_byte_downloader;
  jobject cpp_byte_uploader;
};

}  // namespace

// Universal callback handler. This callback checks the Java type of `result`
// and completes a different typed Future depending on that type.
void StorageReferenceInternal::FutureCallback(JNIEnv* env, jobject result,
                                              firebase::util::FutureResult result_code,
                                              const char* status_message,
                                              void* callback_data) {
  FutureCallbackData* data =
      reinterpret_cast<FutureCallbackData*>(callback_data);
  if (data == nullptr) {
    jni::CheckAndClearException(env);
    return;
  }

  // Disarm listener and byte streaming helpers before completing the Future so
  // that waiting threads or OnCompletion callbacks can safely free user buffers
  // and listeners immediately after Future completion.
  if (data->listener_token != nullptr) {
    data->listener_token->Complete(env);
  }
  if (data->cpp_byte_downloader != nullptr) {
    env->CallVoidMethod(data->cpp_byte_downloader,
                        cpp_byte_downloader::GetMethodId(
                            cpp_byte_downloader::kDiscardPointers));
    jni::CheckAndClearException(env);
    env->DeleteGlobalRef(data->cpp_byte_downloader);
    data->cpp_byte_downloader = nullptr;
  }
  if (data->cpp_byte_uploader != nullptr) {
    env->CallVoidMethod(
        data->cpp_byte_uploader,
        cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kDiscardPointers));
    jni::CheckAndClearException(env);
    env->DeleteGlobalRef(data->cpp_byte_uploader);
    data->cpp_byte_uploader = nullptr;
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
        jni::JStringToString(env, result));
  } else if (result && env->IsInstanceOf(result, util::uri::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a URI.");
    // Complete a Future<std::string> from a Java URI object.
    data->impl->CompleteWithResult<std::string>(
        data->handle, kErrorNone, status_message,
        util::JniUriToString(env, env->NewLocalRef(result)));
  } else if (result &&
             env->IsInstanceOf(
                 result, stream_download_task_task_snapshot::GetClass())) {
    // Complete a Future<size_t>. We have previously also copied
    // bytes from a Java byte[] array via CppByteDownloader.
    LogDebug("FutureCallback: Completing a Future from a byte array.");
    jlong num_bytes = env->CallLongMethod(
        result, stream_download_task_task_snapshot::GetMethodId(
                    stream_download_task_task_snapshot::kGetBytesTransferred));
    jni::CheckAndClearException(env);
    data->impl->Complete<size_t>(
        data->handle, kErrorNone, status_message,
        [num_bytes](size_t* size) { *size = static_cast<size_t>(num_bytes); });
  } else if (result &&
             env->IsInstanceOf(result, storage_metadata::GetClass())) {
    // Complete a Future<Metadata> from a Java StorageMetadata object.
    LogDebug("FutureCallback: Completing a Future from a StorageMetadata.");
    data->impl->Complete<Metadata>(
        data->handle, kErrorNone, status_message,
        [data, result](Metadata* metadata) {
          *metadata = Metadata(StorageMetadataToMetadata(env, result, data->storage));
        });
  } else if (result &&
             env->IsInstanceOf(result, upload_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from an UploadTask.");
    // Complete a Future<Metadata> from a Java UploadTask.TaskSnapshot.
    jobject metadata_obj = env->CallObjectMethod(
        result, upload_task_task_snapshot::GetMethodId(
                    upload_task_task_snapshot::kGetMetadata));
    if (jni::CheckAndClearException(env)) {
      if (metadata_obj) env->DeleteLocalRef(metadata_obj);
      metadata_obj = nullptr;
    }
    data->impl->Complete<Metadata>(
        data->handle, kErrorNone, status_message,
        [data, metadata_obj](Metadata* metadata) {
          *metadata = metadata_obj
                          ? Metadata(StorageMetadataToMetadata(env, metadata_obj, data->storage))
                          : Metadata(nullptr);
        });
    if (metadata_obj) env->DeleteLocalRef(metadata_obj);
  } else if (result &&
             env->IsInstanceOf(result,
                               file_download_task_task_snapshot::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a FileDownloadTask.");
    // Complete a Future<size_t> from a Java FileDownloadTask.TaskSnapshot.
    jlong bytes = env->CallLongMethod(
        result, file_download_task_task_snapshot::GetMethodId(
                    file_download_task_task_snapshot::kGetBytesTransferred));
    jni::CheckAndClearException(env);
    data->impl->Complete<size_t>(
        data->handle, kErrorNone, status_message,
        [bytes](size_t* size) { *size = static_cast<size_t>(bytes); });
  } else if (result && env->IsInstanceOf(result, list_result::GetClass())) {
    LogDebug("FutureCallback: Completing a Future from a ListResult.");
    // Complete a Future<StorageListResult> from a Java ListResult object.
    jobject prefixes_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPrefixes));
    jni::CheckAndClearException(env);
    jobject items_list = env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetItems));
    jni::CheckAndClearException(env);
    jstring page_token_jstr = static_cast<jstring>(env->CallObjectMethod(
        result, list_result::GetMethodId(list_result::kGetPageToken)));
    jni::CheckAndClearException(env);

    std::vector<StorageReference> prefixes;
    std::vector<StorageReference> items;
    std::string page_token = jni::JStringToString(env, page_token_jstr);

    auto process_list = [&](jobject list_obj,
                            std::vector<StorageReference>& out_vec) {
      if (list_obj) {
        int size = env->CallIntMethod(
            list_obj, env->GetMethodID(env->GetObjectClass(list), "size", "()I"));
        if (!jni::CheckAndClearException(env)) {
          for (int i = 0; i < size; ++i) {
            jobject ref_obj = env->CallObjectMethod(
                list_obj, env->GetMethodID(env->GetObjectClass(list), "get", "(I)Ljava/lang/Object;"), i);
            if (!jni::CheckAndClearException(env) && ref_obj) {
              out_vec.push_back(StorageReference(
                  new StorageReferenceInternal(data->storage, ref_obj)));
              env->DeleteLocalRef(ref_obj);
            }
          }
        }
        env->DeleteLocalRef(list_obj);
      }
    };

    process_list(prefixes_list, prefixes);
    process_list(items_list, items);

    if (page_token_jstr) env->DeleteLocalRef(page_token_jstr);

    data->impl->Complete<StorageListResult>(
        data->handle, kErrorNone, status_message,
        [prefixes, items, page_token](StorageListResult* list_result) {
          internal::StorageListResultInternal* list_result_internal =
              new internal::StorageListResultInternal(prefixes, items,
                                                      page_token);
          *list_result =
              internal::StorageListResultInternal::AsStorageListResult(
                  list_result_internal);
        });
  } else {
    LogDebug("FutureCallback: Completing a Future from a default result.");
    // Unknown or null result type, treat this as a Future<void> and just
    // return success.
    data->impl->Complete(data->handle, kErrorNone, status_message);
  }
  delete data;

  jni::CheckAndClearException(env);
}

Future<void> StorageReferenceInternal::Delete() {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<void>(kStorageReferenceFnDelete);
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kDelete));
  if (util::LogException(env, kLogLevelWarning,
                         "StorageReference::Delete(): Call to delete() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    future_impl->Complete(handle, kErrorUnknown, "Call to delete() failed.");
    return DeleteLastResult();
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl, storage_, kStorageReferenceFnDelete)),
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() const {
  return static_cast<const Future<void>&>(
      future()->LastResult(kStorageReferenceFnDelete));
}

std::string StorageReferenceInternal::bucket() const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring bucket_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetBucket)));
  if (jni::CheckAndClearException(env) || !bucket_jstring) {
    if (bucket_jstring) env->DeleteLocalRef(bucket_jstring);
    return "";
  }
  return ::firebase::jni::JStringToString(env, bucket_jstring);
}

std::string StorageReferenceInternal::full_path() const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring path_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetPath)));
  if (jni::CheckAndClearException(env) || !path_jstring) {
    if (path_jstring) env->DeleteLocalRef(path_jstring);
    return "";
  }
  return ::firebase::jni::JStringToString(env, path_jstring);
}

Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle = future_impl->Alloc<size_t>(kStorageReferenceFnGetFile);
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorInvalidArgument,
                          "File path cannot be null or empty.");
    return GetFileLastResult();
  }
  jobject uri = util::ParseUriString(env, path);
  if (jni::CheckAndClearException(env) || !uri) {
    if (uri) env->DeleteLocalRef(uri);
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorInvalidArgument,
                          "Invalid file path URI.");
    return GetFileLastResult();
  }
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetFileUri),
      uri);
  env->DeleteLocalRef(uri);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetFile(): Call to getFile() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorUnknown, "Call to getFile() failed.");
    return GetFileLastResult();
  }
  std::shared_ptr<JniListenerToken> listener_token =
      AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl, storage_,
                                 kStorageReferenceFnGetFile, listener_token)),
      storage_->jni_task_id());
  if (controller_out) {
    // A moved-from Controller has no internal state; allocate one so the
    // caller's Controller still binds to this task.
    if (controller_out->internal_ == nullptr) {
      controller_out->internal_ = new ControllerInternal();
    }
    controller_out->internal_->AssignTask(storage_, task);
  }
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
  return GetFileLastResult();
}

std::shared_ptr<JniListenerToken>
StorageReferenceInternal::AssignListenerToTask(Listener* listener,
                                               jobject task) {
  if (listener == nullptr || task == nullptr || storage_ == nullptr ||
      storage_->app() == nullptr) {
    return nullptr;
  }
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject java_listener_local = env->NewObject(
      cpp_storage_listener::GetClass(),
      cpp_storage_listener::GetMethodId(cpp_storage_listener::kConstructor),
      reinterpret_cast<jlong>(storage_), reinterpret_cast<jlong>(listener));
  if (jni::CheckAndClearException(env) || !java_listener_local) {
    if (java_listener_local) env->DeleteLocalRef(java_listener_local);
    return nullptr;
  }
  jobject java_listener = env->NewGlobalRef(java_listener_local);
  env->DeleteLocalRef(java_listener_local);
  if (!java_listener) {
    return nullptr;
  }

  ControllerInternal::RegisterActiveListener(storage_, listener);
  auto token = std::make_shared<JniListenerToken>(
      storage_, listener, ListenerInternal::GetImpl(listener), java_listener);
  token->InitRegistrations(token);

  jobject paused_res = env->CallObjectMethod(
      task, storage_task::GetMethodId(storage_task::kAddOnPausedListener),
      java_listener);
  if (paused_res) env->DeleteLocalRef(paused_res);
  jni::CheckAndClearException(env);

  jobject progress_res = env->CallObjectMethod(
      task, storage_task::GetMethodId(storage_task::kAddOnProgressListener),
      java_listener);
  if (progress_res) env->DeleteLocalRef(progress_res);
  jni::CheckAndClearException(env);

  return token;
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() const {
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
  if (buffer == nullptr && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorInvalidArgument,
                          "Buffer cannot be null when buffer_size > 0.");
    return GetBytesLastResult();
  }
  jobject byte_downloader_local = env->NewObject(
      cpp_byte_downloader::GetClass(),
      cpp_byte_downloader::GetMethodId(cpp_byte_downloader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size));
  if (jni::CheckAndClearException(env) || !byte_downloader_local) {
    if (byte_downloader_local) env->DeleteLocalRef(byte_downloader_local);
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorUnknown,
                          "Failed to create CppByteDownloader.");
    return GetBytesLastResult();
  }
  jobject byte_downloader = env->NewGlobalRef(byte_downloader_local);
  env->DeleteLocalRef(byte_downloader_local);

  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetStream),
      byte_downloader);
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetBytes(): Call to getStream() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    env->DeleteGlobalRef(byte_downloader);
    if (controller_out) *controller_out = Controller();
    future_impl->Complete(handle, kErrorUnknown, "Call to getStream() failed.");
    return GetBytesLastResult();
  }
  std::shared_ptr<JniListenerToken> listener_token =
      AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData, and will
      // handle copying the data into the buffer.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl, storage_, kStorageReferenceFnGetBytes,
          listener_token, buffer, buffer_size, byte_downloader)),
      storage_->jni_task_id());
  if (controller_out) {
    // A moved-from Controller has no internal state; allocate one so the
    // caller's Controller still binds to this task.
    if (controller_out->internal_ == nullptr) {
      controller_out->internal_ = new ControllerInternal();
    }
    controller_out->internal_->AssignTask(storage_, task);
  }
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);
  return GetBytesLastResult();
}

Future<size_t> StorageReferenceInternal::GetBytesLastResult() const {
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
  if (util::LogException(env, kLogLevelWarning,
                         "StorageReference::GetDownloadUrl(): Call to "
                         "getDownloadUrl() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    future_impl->Complete(handle, kErrorUnknown,
                          "Call to getDownloadUrl() failed.");
    return GetDownloadUrlLastResult();
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl, storage_, kStorageReferenceFnGetDownloadUrl)),
      storage_->jni_task_id());
  env->DeleteLocalRef(task);
  jni::CheckAndClearException(env);

  return GetDownloadUrlLastResult();
}

Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() const {
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
  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::GetMetadata(): Call to getMetadata() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    future_impl->CompleteWithResult(
        handle, kErrorUnknown, "Call to getMetadata() failed.", Metadata(nullptr));
    return GetMetadataLastResult();
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl, storage_, kStorageReferenceFnGetMetadata)),
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
  return GetMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnGetMetadata));
}

Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  if (!metadata || !metadata->is_valid()) {
    future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                    "Metadata cannot be null or invalid.",
                                    Metadata(nullptr));
    return UpdateMetadataLastResult();
  }
  metadata->internal_->CommitCustomMetadata();
  jobject meta_obj = MetadataToStorageMetadata(env, *metadata->internal_);
  jobject task =
      meta_obj
          ? env->CallObjectMethod(obj_,
                                  storage_reference::GetMethodId(
                                      storage_reference::kUpdateMetadata),
                                  meta_obj)
          : nullptr;
  if (meta_obj) env->DeleteLocalRef(meta_obj);
  if (util::LogException(env, kLogLevelWarning,
                         "StorageReference::UpdateMetadata(): Call to "
                         "updateMetadata() failed.") ||
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    future_impl->CompleteWithResult(handle, kErrorUnknown,
                                    "Call to updateMetadata() failed.",
                                    Metadata(nullptr));
    return UpdateMetadataLastResult();
  }
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(
          handle, future_impl, storage_, kStorageReferenceFnUpdateMetadata)),
      storage_->jni_task_id());
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
  return UpdateMetadataLastResult();
}

Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnUpdateMetadata));
}

Future<StorageListResult> StorageReferenceInternal::List(
    int max_results_per_page, const char* page_token) {
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<StorageListResult>(kStorageReferenceFnList);
  if (max_results_per_page <= 0 || max_results_per_page > 1000) {
    future_impl->Complete(
        handle, kErrorInvalidArgument,
        "max_results_per_page must be between 1 and 1000 inclusive.");
    return ListLastResult();
  }

  JNIEnv* env = storage_->app()->GetJNIEnv();
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
                         "StorageReference::List(): Call to list() failed.") ||
      task == nullptr) {
    future_impl->Complete(handle, kErrorUnknown, "Call to list() failed.");
  } else {
    firebase::util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        reinterpret_cast<void*>(new FutureCallbackData(
            handle, future_impl, storage_, kStorageReferenceFnList)),
        storage_->jni_task_id());
  }
  jni::CheckAndClearException(env);
  if (task) env->DeleteLocalRef(task);

  return ListLastResult();
}

Future<StorageListResult> StorageReferenceInternal::ListLastResult() const {
  return static_cast<const Future<StorageListResult>&>(
      future()->LastResult(kStorageReferenceFnList));
}

Future<StorageListResult> StorageReferenceInternal::ListAll() {
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<StorageListResult>(kStorageReferenceFnListAll);

  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kListAll));

  if (util::LogException(
          env, kLogLevelWarning,
          "StorageReference::ListAll(): Call to listAll() failed.") ||
      task == nullptr) {
    future_impl->Complete(handle, kErrorUnknown, "Call to listAll() failed.");
  } else {
    firebase::util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        reinterpret_cast<void*>(new FutureCallbackData(
            handle, future_impl, storage_, kStorageReferenceFnListAll)),
        storage_->jni_task_id());
  }
  jni::CheckAndClearException(env);
  if (task) env->DeleteLocalRef(task);

  return ListAllLastResult();
}

Future<StorageListResult> StorageReferenceInternal::ListAllLastResult() const {
  return static_cast<const Future<StorageListResult>&>(
      future()->LastResult(kStorageReferenceFnListAll));
}

std::string StorageReferenceInternal::name() const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jstring name_jstring = static_cast<jstring>(env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetName)));
  if (jni::CheckAndClearException(env) || !name_jstring) {
    if (name_jstring) env->DeleteLocalRef(name_jstring);
    return "";
  }
  return ::firebase::jni::JStringToString(env, name_jstring);
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, storage_reference::GetMethodId(storage_reference::kGetParent));
  if (jni::CheckAndClearException(env) || parent_obj == nullptr) {
    if (parent_obj) env->DeleteLocalRef(parent_obj);
    return nullptr;
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
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnPutBytes);
  if (buffer == nullptr && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    future_impl->CompleteWithResult(
        handle, kErrorInvalidArgument,
        "Buffer cannot be null when buffer_size > 0.", Metadata(nullptr));
    return PutBytesLastResult();
  }
  jobject meta_obj = nullptr;
  if (metadata && metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
    meta_obj = MetadataToStorageMetadata(env, *metadata->internal_);
  }

  bool started = false;
  jobject uploader = env->NewObject(
      cpp_byte_uploader::GetClass(),
      cpp_byte_uploader::GetMethodId(cpp_byte_uploader::kConstructor),
      reinterpret_cast<jlong>(buffer), static_cast<jlong>(buffer_size),
      static_cast<jlong>(0));
  std::string exception_message = jni::GetAndClearExceptionMessage(env);
  if (exception_message.empty() && uploader != nullptr) {
    jobject task =
        (meta_obj != nullptr)
            ? env->CallObjectMethod(
                  obj_,
                  storage_reference::GetMethodId(
                      storage_reference::kPutStreamWithMetadata),
                  uploader, meta_obj)
            : env->CallObjectMethod(
                  obj_,
                  storage_reference::GetMethodId(storage_reference::kPutStream),
                  uploader);
    exception_message = jni::GetAndClearExceptionMessage(env);
    if (exception_message.empty() && task != nullptr) {
      std::shared_ptr<JniListenerToken> listener_token =
          AssignListenerToTask(listener, task);
      firebase::util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          // FutureCallback will delete the newed FutureCallbackData.
          reinterpret_cast<void*>(new FutureCallbackData(
              handle, future_impl, storage_, kStorageReferenceFnPutBytes,
              listener_token, nullptr, 0, nullptr,
              env->NewGlobalRef(uploader))),
          storage_->jni_task_id());
      if (controller_out) {
        if (controller_out->internal_ == nullptr) {
          controller_out->internal_ = new ControllerInternal();
        }
        controller_out->internal_->AssignTask(storage_, task);
      }
      jni::CheckAndClearException(env);
      env->DeleteLocalRef(task);
      started = true;
    } else if (task != nullptr) {
      env->DeleteLocalRef(task);
    }
    env->DeleteLocalRef(uploader);
  } else if (uploader != nullptr) {
    env->DeleteLocalRef(uploader);
  }
  if (meta_obj) {
    env->DeleteLocalRef(meta_obj);
  }
  if (!started) {
    if (controller_out) *controller_out = Controller();
    const char* error_msg = exception_message.empty()
                                ? "Call to putStream() failed."
                                : exception_message.c_str();
    future_impl->CompleteWithResult(handle, kErrorUnknown, error_msg,
                                    Metadata(nullptr));
  }
  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutBytes));
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  return PutFile(path, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  JNIEnv* env = storage_->app()->GetJNIEnv();
  ReferenceCountedFutureImpl* future_impl = future();
  FutureHandle handle =
      future_impl->Alloc<Metadata>(kStorageReferenceFnPutFile);
  if (!path || path[0] == '\0') {
    if (controller_out) *controller_out = Controller();
    future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                    "File path cannot be null or empty.",
                                    Metadata(nullptr));
    return PutFileLastResult();
  }
  jobject meta_obj = nullptr;
  if (metadata && metadata->is_valid()) {
    metadata->internal_->CommitCustomMetadata();
    meta_obj = MetadataToStorageMetadata(env, *metadata->internal_);
  }
  jobject uri = util::ParseUriString(env, path);
  if (jni::CheckAndClearException(env) || !uri) {
    if (uri) env->DeleteLocalRef(uri);
    if (meta_obj) env->DeleteLocalRef(meta_obj);
    if (controller_out) *controller_out = Controller();
    future_impl->CompleteWithResult(handle, kErrorInvalidArgument,
                                    "Invalid file path URI.",
                                    Metadata(nullptr));
    return PutFileLastResult();
  }
  jobject task =
      (meta_obj != nullptr)
          ? env->CallObjectMethod(
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
      task == nullptr) {
    if (task) env->DeleteLocalRef(task);
    if (controller_out) *controller_out = Controller();
    future_impl->CompleteWithResult(
        handle, kErrorUnknown, "Call to putFile() failed.", Metadata(nullptr));
    return PutFileLastResult();
  }
  std::shared_ptr<JniListenerToken> listener_token =
      AssignListenerToTask(listener, task);
  firebase::util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, future_impl, storage_,
                                 kStorageReferenceFnPutFile, listener_token)),
      storage_->jni_task_id());
  if (controller_out) {
    // A moved-from Controller has no internal state; allocate one so the
    // caller's Controller still binds to this task.
    if (controller_out->internal_ == nullptr) {
      controller_out->internal_ = new ControllerInternal();
    }
    controller_out->internal_->AssignTask(storage_, task);
  }
  jni::CheckAndClearException(env);
  env->DeleteLocalRef(task);
  return PutFileLastResult();
}

Future<Metadata> StorageReferenceInternal::PutFileLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutFile));
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() const {
  return storage_->future_manager().GetFutureApi(
      const_cast<StorageReferenceInternal*>(this));
}

void StorageReferenceInternal::CppByteDownloaderWriteBytes(
    JNIEnv* env, jclass clazz, jlong buffer_ptr, jlong buffer_size,
    jlong buffer_offset, jbyteArray byte_array, jlong num_bytes_to_copy) {
  (void)clazz;
  if (!buffer_ptr || !byte_array || num_bytes_to_copy <= 0 ||
      buffer_offset < 0 || buffer_size < 0 || buffer_offset > buffer_size ||
      num_bytes_to_copy > buffer_size - buffer_offset) {
    return;
  }
  jsize array_length = env->GetArrayLength(byte_array);
  if (array_length < 0 ||
      num_bytes_to_copy > static_cast<jlong>(array_length)) {
    return;
  }

  jbyte* jbytes = env->GetByteArrayElements(byte_array, nullptr);
  if (!jbytes) {
    return;
  }
  memcpy(reinterpret_cast<void*>(buffer_ptr + buffer_offset),
         reinterpret_cast<void*>(jbytes),
         static_cast<size_t>(num_bytes_to_copy));
  env->ReleaseByteArrayElements(byte_array, jbytes, JNI_ABORT);
}

jint StorageReferenceInternal::CppByteUploaderReadBytes(
    JNIEnv* env, jclass clazz, jlong cpp_buffer_pointer, jlong cpp_buffer_size,
    jlong cpp_buffer_offset, jobject bytes, jint bytes_offset,
    jint num_bytes_to_read) {
  (void)clazz;
  if (!cpp_buffer_pointer || !bytes || num_bytes_to_read <= 0 ||
      bytes_offset < 0 || cpp_buffer_offset < 0 || cpp_buffer_size < 0 ||
      cpp_buffer_offset >= cpp_buffer_size) {
    return -1;
  }
  jlong cpp_buffer_remaining = cpp_buffer_size - cpp_buffer_offset;
  jbyteArray bytes_array_object = reinterpret_cast<jbyteArray>(bytes);
  jsize array_length = env->GetArrayLength(bytes_array_object);
  if (array_length < 0 || bytes_offset > array_length ||
      num_bytes_to_read > array_length - bytes_offset) {
    return -1;
  }
  jbyte* bytes_array = env->GetByteArrayElements(bytes_array_object, nullptr);
  if (!bytes_array) {
    LogError(
        "Attempt to stream data into Java buffer failed, aborting this "
        "stream.");
    return -2;
  }
  jint data_read = static_cast<jint>(std::min<jlong>(
      static_cast<jlong>(num_bytes_to_read), cpp_buffer_remaining));
  LogDebug("Reading %d bytes from %p offset %lld / %lld into %d / %d",
           static_cast<int>(data_read),
           reinterpret_cast<const void*>(cpp_buffer_pointer),
           static_cast<long long>(cpp_buffer_offset),
           static_cast<long long>(cpp_buffer_size),
           static_cast<int>(bytes_offset),
           static_cast<int>(num_bytes_to_read));
  memcpy(bytes_array + bytes_offset,
         reinterpret_cast<const char*>(cpp_buffer_pointer) + cpp_buffer_offset,
         static_cast<size_t>(data_read));
  env->ReleaseByteArrayElements(bytes_array_object, bytes_array, 0);
  return data_read;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
