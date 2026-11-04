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
#include <utility>

#include "app/src/app_android.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/disconnection_android.h"
#include "database/src/android/util_android.h"
#include "database/src/common/database_reference.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define DATABASE_REFERENCE_METHODS(X)                                          \
  X(Child, "child",                                                            \
    "(Ljava/lang/String;)Lcom/google/firebase/database/DatabaseReference;")    \
  X(Push, "push", "()Lcom/google/firebase/database/DatabaseReference;")        \
  X(SetValue, "setValue",                                                      \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;")                 \
  X(SetValueAndPriority, "setValue",                                           \
    "(Ljava/lang/Object;Ljava/lang/Object;)"                                   \
    "Lcom/google/android/gms/tasks/Task;")                                     \
  X(SetPriority, "setPriority",                                                \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;")                 \
  X(UpdateChildren, "updateChildren",                                          \
    "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;")                    \
  X(RemoveValue, "removeValue", "()Lcom/google/android/gms/tasks/Task;")       \
  X(OnDisconnect, "onDisconnect",                                              \
    "()Lcom/google/firebase/database/OnDisconnect;")                           \
  X(RunTransaction, "runTransaction",                                          \
    "(Lcom/google/firebase/database/Transaction$Handler;Z)V")                  \
  X(GoOffline, "goOffline", "()V", jni::kMethodTypeStatic)                     \
  X(GoOnline, "goOnline", "()V", jni::kMethodTypeStatic)                       \
  X(GetDatabase, "getDatabase",                                                \
    "()Lcom/google/firebase/database/FirebaseDatabase;")                       \
  X(ToString, "toString", "()Ljava/lang/String;")                              \
  X(GetParent, "getParent",                                                    \
    "()Lcom/google/firebase/database/DatabaseReference;")                      \
  X(GetRoot, "getRoot",                                                        \
    "()Lcom/google/firebase/database/DatabaseReference;")                      \
  X(GetKey, "getKey", "()Ljava/lang/String;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    database_reference, DATABASE_REFERENCE_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/DatabaseReference")

DatabaseReferenceInternal::DatabaseReferenceInternal(DatabaseInternal* db,
                                                     jobject obj)
    : QueryInternal(db, obj), ref_future_api_(db) {
  query_spec_.path = Path(GetUrl());
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    const DatabaseReferenceInternal& src)
    : QueryInternal(src), ref_future_api_(src.ref_future_api_) {
  MutexLock src_lock(src.member_mutex_);
  cached_key_ = src.cached_key_;
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
    delete cached_disconnection_handler_;
    cached_disconnection_handler_ = nullptr;
    cached_key_ = std::move(src_key);
  }
  QueryInternal::operator=(src);
  ref_future_api_ = src.ref_future_api_;
  return *this;
}

DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& src) noexcept
    : QueryInternal(std::move(src)),
      ref_future_api_(std::move(src.ref_future_api_)) {
  MutexLock src_lock(src.member_mutex_);
  cached_disconnection_handler_ = src.cached_disconnection_handler_;
  src.cached_disconnection_handler_ = nullptr;
  cached_key_ = std::move(src.cached_key_);
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
    delete cached_disconnection_handler_;
    cached_disconnection_handler_ = src_handler;
    cached_key_ = std::move(src_key);
  }
  QueryInternal::operator=(std::move(src));
  ref_future_api_ = std::move(src.ref_future_api_);
  return *this;
}

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  MutexLock lock(member_mutex_);
  delete cached_disconnection_handler_;
  cached_disconnection_handler_ = nullptr;
}

bool DatabaseReferenceInternal::Initialize(App* app) {
  return database_reference::CacheMethodIds(app->GetJNIEnv(), app->activity());
}

void DatabaseReferenceInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  database_reference::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  if (!db_) return nullptr;
  return db_->constructor_url().empty()
             ? Database::GetInstance(db_->GetApp())
             : Database::GetInstance(db_->GetApp(),
                                     db_->constructor_url().c_str());
}

const char* DatabaseReferenceInternal::GetKey() {
  MutexLock lock(member_mutex_);
  if (cached_key_.is_null()) {
    if (!db_) return nullptr;
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    auto key_string = static_cast<jstring>(env->CallObjectMethod(
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
        if (key != nullptr) env->ReleaseStringUTFChars(key_string, key);
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
  (void)GetKey();
  MutexLock lock(member_mutex_);
  if (!cached_key_.is_string()) {
    static const std::string* kEmptyString = new std::string();
    return *kEmptyString;
  }
  return cached_key_.mutable_string();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() const {
  if (!db_) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetParent));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (parent_obj != nullptr) env->DeleteLocalRef(parent_obj);
    return nullptr;
  }
  if (parent_obj == nullptr) return new DatabaseReferenceInternal(*this);
  auto* internal = new DatabaseReferenceInternal(db_, parent_obj);
  env->DeleteLocalRef(parent_obj);
  return internal;
}

bool DatabaseReferenceInternal::IsRoot() const {
  if (!db_) return false;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetParent));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (parent_obj != nullptr) env->DeleteLocalRef(parent_obj);
    return false;
  }
  if (parent_obj == nullptr) return true;
  env->DeleteLocalRef(parent_obj);
  return false;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetRoot() const {
  if (!db_) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject root_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetRoot));
  if (::firebase::jni::CheckAndClearException(env) || root_obj == nullptr) {
    if (root_obj != nullptr) env->DeleteLocalRef(root_obj);
    return nullptr;
  }
  auto* internal = new DatabaseReferenceInternal(db_, root_obj);
  env->DeleteLocalRef(root_obj);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(
    const char* path) const {
  if (!db_ || path == nullptr) return nullptr;
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
  if (::firebase::util::LogException(
          env, kLogLevelWarning,
          "DatabaseReference::Child: (URL = %s) Couldn't create child "
          "reference %s",
          query_spec_.path.c_str(), path) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  auto* internal = new DatabaseReferenceInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  ::firebase::jni::CheckAndClearException(env);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() const {
  if (!db_) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject child_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kPush));
  if (::firebase::util::LogException(
          env, kLogLevelWarning,
          "DatabaseReference::PushChild: (URL = %s) Couldn't push new child "
          "reference",
          query_spec_.path.c_str()) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  auto* internal = new DatabaseReferenceInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

std::string DatabaseReferenceInternal::GetUrl() const {
  if (!db_) return std::string();
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  auto url_string = static_cast<jstring>(env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kToString)));
  if (::firebase::jni::CheckAndClearException(env) || url_string == nullptr) {
    if (url_string != nullptr) env->DeleteLocalRef(url_string);
    return std::string();
  }
  std::string url = ::firebase::jni::JStringToString(env, url_string);
  env->DeleteLocalRef(url_string);
  return url;
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  MutexLock lock(member_mutex_);
  if (cached_disconnection_handler_ == nullptr) {
    if (!db_) return nullptr;
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject on_disconnect = env->CallObjectMethod(
        obj_,
        database_reference::GetMethodId(database_reference::kOnDisconnect));
    if (::firebase::jni::CheckAndClearException(env) ||
        on_disconnect == nullptr) {
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
  if (!db_) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallStaticVoidMethod(
      database_reference::GetClass(),
      database_reference::GetMethodId(database_reference::kGoOffline));
  ::firebase::jni::CheckAndClearException(env);
}

void DatabaseReferenceInternal::GoOnline() const {
  if (!db_) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallStaticVoidMethod(
      database_reference::GetClass(),
      database_reference::GetMethodId(database_reference::kGoOnline));
  ::firebase::jni::CheckAndClearException(env);
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransaction(
    DoTransactionWithContext transaction_function, void* context,
    void (*delete_context)(void*), bool trigger_local_events) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) {
    if (delete_context) delete_context(context);
    return Future<DataSnapshot>();
  }
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  SafeFutureHandle<DataSnapshot> handle = api->SafeAlloc<DataSnapshot>(
      kDatabaseReferenceFnRunTransaction, DataSnapshot(nullptr));
  auto* data = new TransactionData(transaction_function, api, handle);
  data->context = context;
  data->delete_context = delete_context;
  jobject transaction_handler = db_->CreateJavaTransactionHandler(data);
  if (transaction_handler == nullptr) {
    delete data;
    api->Complete(handle, kErrorUnknownError, "RunTransaction failed");
    return MakeFuture(api, handle);
  }
  env->CallVoidMethod(
      obj_,
      database_reference::GetMethodId(database_reference::kRunTransaction),
      transaction_handler, trigger_local_events);
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->DeleteJavaTransactionHandler(transaction_handler);
    api->Complete(handle, kErrorUnknownError, "RunTransaction failed");
  }
  return MakeFuture(api, handle);
}

void DatabaseReferenceInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kRemoveValue));
  db_->RegisterTaskCallback(env, task, handle, api, "RemoveValue failed");
}

void DatabaseReferenceInternal::SetValueImpl(const Variant& value,
                                             SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
  if (!value.is_null() && value_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType,
                  "SetValue failed: invalid Variant");
    return;
  }
  jobject task = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kSetValue),
      value_obj);
  if (value_obj) env->DeleteLocalRef(value_obj);
  db_->RegisterTaskCallback(env, task, handle, api, "SetValue failed");
}

void DatabaseReferenceInternal::SetPriorityImpl(const Variant& priority,
                                                SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject priority_obj = ::firebase::util::VariantToJavaObject(env, priority);
  if (!priority.is_null() && priority_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForPriority);
    return;
  }
  jobject task = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kSetPriority),
      priority_obj);
  if (priority_obj) env->DeleteLocalRef(priority_obj);
  db_->RegisterTaskCallback(env, task, handle, api, "SetPriority failed");
}

void DatabaseReferenceInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority,
    SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
  if (!value.is_null() && value_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType,
                  "SetValueAndPriority failed: invalid Variant");
    return;
  }
  jobject priority_obj = ::firebase::util::VariantToJavaObject(env, priority);
  if (!priority.is_null() && priority_obj == nullptr) {
    if (value_obj) env->DeleteLocalRef(value_obj);
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForPriority);
    return;
  }
  jobject task = env->CallObjectMethod(
      obj_,
      database_reference::GetMethodId(database_reference::kSetValueAndPriority),
      value_obj, priority_obj);
  if (value_obj) env->DeleteLocalRef(value_obj);
  if (priority_obj) env->DeleteLocalRef(priority_obj);
  db_->RegisterTaskCallback(env, task, handle, api,
                            "SetValueAndPriority failed");
}

void DatabaseReferenceInternal::UpdateChildrenImpl(
    const Variant& values, SafeFutureHandle<void> handle) {
  ReferenceCountedFutureImpl* api = ref_future();
  if (!db_ || !api) return;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject values_obj = ::firebase::util::VariantToJavaObject(env, values);
  if (values_obj == nullptr) {
    api->Complete(handle, kErrorInvalidVariantType,
                  kErrorMsgInvalidVariantForUpdateChildren);
    return;
  }
  jobject task = env->CallObjectMethod(
      obj_,
      database_reference::GetMethodId(database_reference::kUpdateChildren),
      values_obj);
  env->DeleteLocalRef(values_obj);
  db_->RegisterTaskCallback(env, task, handle, api, "UpdateChildren failed");
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
