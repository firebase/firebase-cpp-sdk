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

#include "database/src/android/database_reference_android.h"

#include <jni.h>

#include "app/src/reference_counted_future_impl.h"
#include "app/src/app_android.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "database/src/android/database_android.h"
#include "database/src/android/disconnection_android.h"
#include "app/src/app_android.h"
#include "app/src/jni/env.h"
#include "database/src/common/database_reference.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define DATABASE_REFERENCE_METHODS(X)                                          \
  X(Child, "child",                                                            \
    "(Ljava/lang/String;)Lcom/google/firebase/database/DatabaseReference;"),   \
  X(Push, "push", "()Lcom/google/firebase/database/DatabaseReference;"),       \
  X(SetValue, "setValue",                                                      \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;"),                \
  X(SetValueAndPriority, "setValue",                                           \
    "(Ljava/lang/Object;Ljava/lang/Object;)"                                   \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SetPriority, "setPriority",                                                \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;"),                \
  X(UpdateChildren, "updateChildren",                                          \
    "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;"),                   \
  X(RemoveValue, "removeValue", "()Lcom/google/android/gms/tasks/Task;"),      \
  X(OnDisconnect, "onDisconnect",                                              \
    "()Lcom/google/firebase/database/OnDisconnect;"),                          \
  X(RunTransaction, "runTransaction",                                          \
    "(Lcom/google/firebase/database/Transaction$Handler;Z)V"),                 \
  X(GoOffline, "goOffline", "()V", jni::kMethodTypeStatic),                   \
  X(GoOnline, "goOnline", "()V", jni::kMethodTypeStatic),                     \
  X(GetDatabase, "getDatabase",                                                \
    "()Lcom/google/firebase/database/FirebaseDatabase;"),                      \
  X(ToString, "toString", "()Ljava/lang/String;"),                             \
  X(GetParent, "getParent",                                                    \
    "()Lcom/google/firebase/database/DatabaseReference;"),                     \
  X(GetRoot, "getRoot",                                                        \
    "()Lcom/google/firebase/database/DatabaseReference;"),                     \
  X(GetKey, "getKey", "()Ljava/lang/String;")
// clang-format on

namespace database_reference {

enum Method {
  kChild,
  kPush,
  kSetValue,
  kSetValueAndPriority,
  kSetPriority,
  kUpdateChildren,
  kRemoveValue,
  kOnDisconnect,
  kRunTransaction,
  kGoOffline,
  kGoOnline,
  kGetDatabase,
  kToString,
  kGetParent,
  kGetRoot,
  kGetKey,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/DatabaseReference"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kChild] = env->GetMethodID(g_class.get(), "child", "(Ljava/lang/String;)Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kChild]) return false;
  g_method_ids[kPush] = env->GetMethodID(g_class.get(), "push", "()Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kPush]) return false;
  g_method_ids[kSetValue] = env->GetMethodID(g_class.get(), "setValue", "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");
  if (!g_method_ids[kSetValue]) return false;
  g_method_ids[kSetValueAndPriority] = env->GetMethodID(g_class.get(), "setValue", "(Ljava/lang/Object;Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");
  if (!g_method_ids[kSetValueAndPriority]) return false;
  g_method_ids[kSetPriority] = env->GetMethodID(g_class.get(), "setPriority", "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;");
  if (!g_method_ids[kSetPriority]) return false;
  g_method_ids[kUpdateChildren] = env->GetMethodID(g_class.get(), "updateChildren", "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;");
  if (!g_method_ids[kUpdateChildren]) return false;
  g_method_ids[kRemoveValue] = env->GetMethodID(g_class.get(), "removeValue", "()Lcom/google/android/gms/tasks/Task;");
  if (!g_method_ids[kRemoveValue]) return false;
  g_method_ids[kOnDisconnect] = env->GetMethodID(g_class.get(), "onDisconnect", "()Lcom/google/firebase/database/OnDisconnect;");
  if (!g_method_ids[kOnDisconnect]) return false;
  g_method_ids[kRunTransaction] = env->GetMethodID(g_class.get(), "runTransaction", "(Lcom/google/firebase/database/Transaction$Handler;Z)V");
  if (!g_method_ids[kRunTransaction]) return false;
  g_method_ids[kGoOffline] = env->GetStaticMethodID(g_class.get(), "goOffline", "()V");
  if (!g_method_ids[kGoOffline]) return false;
  g_method_ids[kGoOnline] = env->GetStaticMethodID(g_class.get(), "goOnline", "()V");
  if (!g_method_ids[kGoOnline]) return false;
  g_method_ids[kGetDatabase] = env->GetMethodID(g_class.get(), "getDatabase", "()Lcom/google/firebase/database/FirebaseDatabase;");
  if (!g_method_ids[kGetDatabase]) return false;
  g_method_ids[kToString] = env->GetMethodID(g_class.get(), "toString", "()Ljava/lang/String;");
  if (!g_method_ids[kToString]) return false;
  g_method_ids[kGetParent] = env->GetMethodID(g_class.get(), "getParent", "()Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kGetParent]) return false;
  g_method_ids[kGetRoot] = env->GetMethodID(g_class.get(), "getRoot", "()Lcom/google/firebase/database/DatabaseReference;");
  if (!g_method_ids[kGetRoot]) return false;
  g_method_ids[kGetKey] = env->GetMethodID(g_class.get(), "getKey", "()Ljava/lang/String;");
  if (!g_method_ids[kGetKey]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace database_reference



DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* db,
                                                     jobject obj)
    : QueryInternal(db, obj), cached_disconnection_handler_(nullptr) {
  db_->future_manager().AllocFutureApi(&future_api_id_,
                                       kDatabaseReferenceFnCount);
  // Now we need to read the path this db reference refers to, so that it can be
  // put into the QuerySpec.
  // The URL includes the https://... on it, but that doesn't matter on Android
  // where the QuerySpec path is only used for sorting checking equality.
  query_spec_.path = Path(GetUrl());
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& src)
    : QueryInternal(src), cached_disconnection_handler_(nullptr) {
  {
    MutexLock src_lock(src.member_mutex_);
    cached_key_ = src.cached_key_;
  }
  db_->future_manager().AllocFutureApi(&future_api_id_,
                                       kDatabaseReferenceFnCount);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& src) {
  if (&src == this) return *this;
  Variant src_key;
  {
    MutexLock src_lock(src.member_mutex_);
    src_key = src.cached_key_;
  }
  {
    MutexLock lock(member_mutex_);
    if (cached_disconnection_handler_ != nullptr) {
      delete cached_disconnection_handler_;
      cached_disconnection_handler_ = nullptr;
    }
    cached_key_ = std::move(src_key);
  }
  db_->future_manager().ReleaseFutureApi(&future_api_id_);
  QueryInternal::operator=(src);
  db_->future_manager().AllocFutureApi(&future_api_id_,
                                       kDatabaseReferenceFnCount);
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& src) noexcept
    : QueryInternal(std::move(src)), cached_disconnection_handler_(nullptr) {
  {
    MutexLock src_lock(src.member_mutex_);
    cached_disconnection_handler_ = src.cached_disconnection_handler_;
    src.cached_disconnection_handler_ = nullptr;
    cached_key_ = std::move(src.cached_key_);
  }
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& src) noexcept {
  if (&src == this) return *this;
  DisconnectionHandler* src_handler = nullptr;
  Variant src_key;
  {
    MutexLock src_lock(src.member_mutex_);
    src_handler = src.cached_disconnection_handler_;
    src.cached_disconnection_handler_ = nullptr;
    src_key = std::move(src.cached_key_);
  }
  {
    MutexLock lock(member_mutex_);
    if (cached_disconnection_handler_ != nullptr) {
      delete cached_disconnection_handler_;
      cached_disconnection_handler_ = nullptr;
    }
    cached_disconnection_handler_ = src_handler;
    cached_key_ = std::move(src_key);
  }
  db_->future_manager().ReleaseFutureApi(&future_api_id_);
  QueryInternal::operator=(std::move(src));
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
  return *this;
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  {
    MutexLock lock(member_mutex_);
    if (cached_disconnection_handler_ != nullptr) {
      delete cached_disconnection_handler_;
      cached_disconnection_handler_ = nullptr;
    }
  }
  db_->future_manager().ReleaseFutureApi(&future_api_id_);
}

bool DatabaseReferenceInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return database_reference::CacheMethodIds(env, activity);
}

void DatabaseReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  database_reference::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  return db_->constructor_url().empty()
             ? Database::GetInstance(db_->GetApp())
             : Database::GetInstance(db_->GetApp(),
                                     db_->constructor_url().c_str());
}

const char* DatabaseReferenceInternal::GetKey() {
  MutexLock lock(member_mutex_);
  // Check for a cached key.
  if (cached_key_.is_null()) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, database_reference::GetMethodId(database_reference::kGetKey)));
    if (::firebase::jni::CheckAndClearException(env)) {
      if (key_string != nullptr) env->DeleteLocalRef(key_string);
      return nullptr;
    }
    if (key_string == nullptr) {
      cached_key_ = Variant::EmptyMutableString();
    } else {
      const char* key = env->GetStringUTFChars(key_string, nullptr);
      if (::firebase::jni::CheckAndClearException(env) || key == nullptr) {
        if (key != nullptr) {
          env->ReleaseStringUTFChars(key_string, key);
        }
        env->DeleteLocalRef(key_string);
        return nullptr;
      }
      cached_key_ = Variant::MutableStringFromStaticString(key);
      env->ReleaseStringUTFChars(key_string, key);
      env->DeleteLocalRef(key_string);
    }
  }
  return cached_key_.string_value();
}

const std::string& DatabaseReferenceInternal::GetKeyString() {
  MutexLock lock(member_mutex_);
  (void)GetKey();  // populates cached_key_.
  if (!cached_key_.is_string()) {
    static const std::string* kEmptyString = new std::string();
    return *kEmptyString;
  }
  return cached_key_.mutable_string();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetParent));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (parent_obj != nullptr) env->DeleteLocalRef(parent_obj);
    return nullptr;
  }
  if (parent_obj == nullptr) {
    // This is already the root node, so return a copy of us.
    return new DatabaseReferenceInternal(*this);
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, parent_obj);
  env->DeleteLocalRef(parent_obj);
  return internal;
}

bool DatabaseReferenceInternal::IsRoot() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetParent));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (parent_obj != nullptr) env->DeleteLocalRef(parent_obj);
    return false;
  }
  if (parent_obj == nullptr) {
    // getParent() returns null if this is the root node.
    return true;
  } else {
    // getParent() returns an object if this is NOT the root node.
    env->DeleteLocalRef(parent_obj);
    return false;
  }
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject root_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetRoot));
  if (::firebase::jni::CheckAndClearException(env) || root_obj == nullptr) {
    if (root_obj != nullptr) env->DeleteLocalRef(root_obj);
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, root_obj);
  env->DeleteLocalRef(root_obj);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(
    const char* path) const {
  if (path == nullptr) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return nullptr;
  }
  jobject child_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kChild),
      path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::util::LogException(env, kLogLevelWarning,
                         "DatabaseReference::Child: (URL = %s) Couldn't create "
                         "child reference %s",
                         query_spec_.path.c_str(), path) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  ::firebase::jni::CheckAndClearException(env);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject child_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kPush));
  if (::firebase::util::LogException(
          env, kLogLevelWarning,
          "DatabaseReference::PushChild: (URL = %s) Couldn't push "
          "new child reference",
          query_spec_.path.c_str()) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

std::string DatabaseReferenceInternal::GetUrl() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring url_string = static_cast<jstring>(env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kToString)));
  if (::firebase::jni::CheckAndClearException(env) || url_string == nullptr) {
    if (url_string != nullptr) env->DeleteLocalRef(url_string);
    return std::string();
  }
  return ::firebase::jni::JStringToString(env, url_string);
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  MutexLock lock(member_mutex_);
  if (cached_disconnection_handler_ == nullptr) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject on_disconnect = env->CallObjectMethod(
        obj_,
        database_reference::GetMethodId(database_reference::kOnDisconnect));
    if (::firebase::jni::CheckAndClearException(env) || on_disconnect == nullptr) {
      if (on_disconnect != nullptr) env->DeleteLocalRef(on_disconnect);
      return nullptr;
    }

    cached_disconnection_handler_ = new DisconnectionHandler(
        new DisconnectionHandlerInternal(db_, on_disconnect));
    env->DeleteLocalRef(on_disconnect);
  }
  return cached_disconnection_handler_;
}

void DatabaseReferenceInternal::GoOffline() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallStaticVoidMethod(
      database_reference::GetClass(),
      database_reference::GetMethodId(database_reference::kGoOffline));
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseReferenceInternal::GoOnline() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallStaticVoidMethod(
      database_reference::GetClass(),
      database_reference::GetMethodId(database_reference::kGoOnline));
  ::firebase::jni::CheckAndClearException(env);
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  SafeFutureHandle<DataSnapshot> handle = ref_future()->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  // The TransactionData* created here will be deleted in the Java OnCompleted
  // handler, right before the Future completes.  Or, if the Database is
  // destroyed while the Transaction is still pending, it will be deleted in
  // DatabaseInternal's destructor.
  TransactionData* data = new TransactionData(transaction_function,
                                              ref_future(), handle);  // NOLINT
  data->context = context;
  data->delete_context = delete_context;
  jobject transaction_handler = db_->CreateJavaTransactionHandler(data);
  if (transaction_handler == nullptr) {
    delete data;
    ref_future()->Complete(handle, kErrorUnknownError,
                           "RunTransaction failed");
    return MakeFuture(ref_future(), handle);
  }
  env->CallVoidMethod(
      obj_,
      database_reference::GetMethodId(database_reference::kRunTransaction),
      transaction_handler, trigger_local_events);
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->DeleteJavaTransactionHandler(transaction_handler);
    ref_future()->Complete(handle, kErrorUnknownError,
                           "RunTransaction failed");
  }
  return MakeFuture(ref_future(), handle);
}


namespace {
struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<void> handle_,
                     ReferenceCountedFutureImpl* impl_, DatabaseInternal* db_)
      : handle(handle_),
        impl(impl_),
        db_ref(db_->this_ref()),
        backing_future(MakeFuture(impl_, handle_)) {}
  SafeFutureHandle<void> handle;
  ReferenceCountedFutureImpl* impl;
  DatabaseInternal::ThisRef db_ref;
  Future<void> backing_future;
};

void FutureCallback(JNIEnv* env, jobject result, util::FutureResult result_code,
                    const char* status_message, void* callback_data) {
  (void)env;
  FutureCallbackData* data =
      reinterpret_cast<FutureCallbackData*>(callback_data);
  if (data != nullptr) {
    DatabaseInternal::ThisRefLock lock(&data->db_ref);
    DatabaseInternal* db = lock.GetReference();
    if (db != nullptr) {
      Error error_code = kErrorNone;
      std::string message = status_message != nullptr ? status_message : "";
      if (result_code == util::kFutureResultCancelled) {
        error_code = kErrorWriteCanceled;
      } else if (result_code == util::kFutureResultFailure) {
        error_code = db->ErrorFromJavaThrowable(result, &message);
      }
      data->impl->Complete(data->handle, error_code, message.c_str());
    }
    delete data;
  }
}

void RegisterTaskCallback(JNIEnv* env, jobject task,
                          SafeFutureHandle<void> handle,
                          ReferenceCountedFutureImpl* future_impl,
                          DatabaseInternal* db, const char* error_msg) {
  FutureCallbackData* callback_data =
      new FutureCallbackData(handle, future_impl, db);
  util::RegisterCallbackOnTask(env, task, FutureCallback,
                               reinterpret_cast<void*>(callback_data),
                               db->jni_task_id());
  if (::firebase::jni::CheckAndClearException(env)) {
    delete callback_data;
    future_impl->Complete(handle, kErrorUnknownError, error_msg);
  }
  env->DeleteLocalRef(task);
}
}  // namespace

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(obj_, database_reference::GetMethodId(database_reference::kRemoveValue));
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknownError, "RemoveValue failed");
  } else {
    RegisterTaskCallback(env, task, handle, api, db_, "RemoveValue failed");
  }
}


void DatabaseReferenceInternal::SetValueImpl(const Variant& value, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
  if (!value.is_null() && value_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType, "SetValue failed: invalid Variant");
    return;
  }
  jobject task = env->CallObjectMethod(obj_, database_reference::GetMethodId(database_reference::kSetValue), value_obj);
  if (value_obj) env->DeleteLocalRef(value_obj);
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknownError, "SetValue failed");
  } else {
    RegisterTaskCallback(env, task, handle, api, db_, "SetValue failed");
  }
}


void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject priority_obj = ::firebase::util::VariantToJavaObject(env, priority);
  if (!priority.is_null() && priority_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType, kErrorMsgInvalidVariantForPriority);
    return;
  }
  jobject task = env->CallObjectMethod(obj_, database_reference::GetMethodId(database_reference::kSetPriority), priority_obj);
  if (priority_obj) env->DeleteLocalRef(priority_obj);
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknownError, "SetPriority failed");
  } else {
    RegisterTaskCallback(env, task, handle, api, db_, "SetPriority failed");
  }
}


void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority, SafeFutureHandle<void> handle) {
  if (ref_future()->LastResult(kDatabaseReferenceFnSetValue).status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else if (ref_future()->LastResult(kDatabaseReferenceFnSetPriority).status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
    if (!value.is_null() && value_obj == nullptr) {
      ref_future()->Complete(handle, kErrorInvalidVariantType,
                             "SetValueAndPriority failed: invalid Variant");
  return;
    }
    jobject priority_obj = ::firebase::util::VariantToJavaObject(env, priority);
    if (!priority.is_null() && priority_obj == nullptr) {
      if (value_obj) env->DeleteLocalRef(value_obj);
      ref_future()->Complete(handle, kErrorInvalidVariantType,
                             kErrorMsgInvalidVariantForPriority);
  return;
    }
    jobject task =
        env->CallObjectMethod(obj_,
                              database_reference::GetMethodId(
                                  database_reference::kSetValueAndPriority),
                              value_obj, priority_obj);
    if (value_obj) env->DeleteLocalRef(value_obj);
    if (priority_obj) env->DeleteLocalRef(priority_obj);
    if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
      if (task != nullptr) env->DeleteLocalRef(task);
      ref_future()->Complete(handle, kErrorUnknownError,
                             "SetValueAndPriority failed");
    } else {
      RegisterTaskCallback(env, task, handle, ref_future(), db_,
                           "SetValueAndPriority failed");
    }
  }
  return;
}


void DatabaseReferenceInternal::UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject values_obj = ::firebase::util::VariantToJavaObject(env, values);
  jobject task = env->CallObjectMethod(obj_, database_reference::GetMethodId(database_reference::kUpdateChildren), values_obj);
  if (values_obj) env->DeleteLocalRef(values_obj);
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    api->Complete(handle, kErrorUnknownError, "UpdateChildren failed");
  } else {
    RegisterTaskCallback(env, task, handle, api, db_, "UpdateChildren failed");
  }
}


ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return db_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
