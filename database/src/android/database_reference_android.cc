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

#include <assert.h>
#include <jni.h>

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
  X(GoOffline, "goOffline", "()V", util::kMethodTypeStatic),                   \
  X(GoOnline, "goOnline", "()V", util::kMethodTypeStatic),                     \
  X(GetDatabase, "getDatabase",                                                \
    "()Lcom/google/firebase/database/FirebaseDatabase;"),                      \
  X(ToString, "toString", "()Ljava/lang/String;"),                             \
  X(GetParent, "getParent",                                                    \
    "()Lcom/google/firebase/database/DatabaseReference;"),                     \
  X(GetRoot, "getRoot",                                                        \
    "()Lcom/google/firebase/database/DatabaseReference;"),                     \
  X(GetKey, "getKey", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(database_reference, DATABASE_REFERENCE_METHODS)
METHOD_LOOKUP_DEFINITION(database_reference,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/DatabaseReference",
                         DATABASE_REFERENCE_METHODS)

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
  db_->future_manager().AllocFutureApi(&future_api_id_,
                                       kDatabaseReferenceFnCount);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    const DatabaseReferenceInternal& src) {
  QueryInternal::operator=(src);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DatabaseReferenceInternal::DatabaseReferenceInternal(
    DatabaseReferenceInternal&& src)
    : QueryInternal(std::move(src)), cached_disconnection_handler_(nullptr) {
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
}

DatabaseReferenceInternal& DatabaseReferenceInternal::operator=(
    DatabaseReferenceInternal&& src) {
  QueryInternal::operator=(std::move(src));
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DatabaseReferenceInternal::~DatabaseReferenceInternal() {
  if (cached_disconnection_handler_ != nullptr) {
    delete cached_disconnection_handler_;
    cached_disconnection_handler_ = nullptr;
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
  util::CheckAndClearJniExceptions(env);
}

Database* DatabaseReferenceInternal::GetDatabase() const {
  return Database::GetInstance(db_->GetApp());
}

const char* DatabaseReferenceInternal::GetKey() {
  // Check for a cached key.
  if (cached_key_.is_null()) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, database_reference::GetMethodId(database_reference::kGetKey)));
    util::CheckAndClearJniExceptions(env);
    FIREBASE_ASSERT_RETURN(nullptr, key_string != nullptr);
    const char* key = env->GetStringUTFChars(key_string, nullptr);
    cached_key_ = Variant::MutableStringFromStaticString(key);
    env->ReleaseStringUTFChars(key_string, key);
    env->DeleteLocalRef(key_string);
  }
  return cached_key_.string_value();
}

const std::string& DatabaseReferenceInternal::GetKeyString() {
  (void)GetKey();  // populates cached_key_.
  return cached_key_.mutable_string();
}

DatabaseReferenceInternal* DatabaseReferenceInternal::GetParent() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject parent_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kGetParent));
  if (parent_obj == nullptr) {
    // This is already the root node, so return a copy of us.
    env->ExceptionClear();
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
  if (parent_obj == nullptr) {
    env->ExceptionClear();
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
  assert(root_obj != nullptr);
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, root_obj);
  env->DeleteLocalRef(root_obj);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::Child(
    const char* path) const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kChild),
      path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(env, kLogLevelWarning,
                         "DatabaseReference::Child: (URL = %s) Couldn't create "
                         "child reference %s",
                         query_spec_.path.c_str(), path)) {
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  util::CheckAndClearJniExceptions(env);
  return internal;
}

DatabaseReferenceInternal* DatabaseReferenceInternal::PushChild() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject child_obj = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kPush));
  if (util::LogException(
          env, kLogLevelWarning,
          "DatabaseReference::PushChild: (URL = %s) Couldn't push "
          "new child reference",
          query_spec_.path.c_str())) {
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
  return util::JniStringToString(env, url_string);
}

DisconnectionHandler* DatabaseReferenceInternal::OnDisconnect() {
  if (cached_disconnection_handler_ == nullptr) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject on_disconnect = env->CallObjectMethod(
        obj_,
        database_reference::GetMethodId(database_reference::kOnDisconnect));
    util::CheckAndClearJniExceptions(env);
    if (on_disconnect == nullptr) return nullptr;

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
}

void DatabaseReferenceInternal::GoOnline() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallStaticVoidMethod(
      database_reference::GetClass(),
      database_reference::GetMethodId(database_reference::kGoOnline));
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
  env->CallVoidMethod(
      obj_,
      database_reference::GetMethodId(database_reference::kRunTransaction),
      transaction_handler, trigger_local_events);
  return MakeFuture(ref_future(), handle);
}

Future<DataSnapshot> DatabaseReferenceInternal::RunTransactionLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRunTransaction));
}

namespace {
struct FutureCallbackData {
  FutureCallbackData(SafeFutureHandle<void> handle_,
                     ReferenceCountedFutureImpl* impl_, DatabaseInternal* db_)
      : handle(handle_), impl(impl_), db(db_) {}
  SafeFutureHandle<void> handle;
  ReferenceCountedFutureImpl* impl;
  DatabaseInternal* db;
};

void FutureCallback(JNIEnv* env, jobject result, util::FutureResult result_code,
                    const char* status_message, void* callback_data) {
  int status = 0;  // TODO(140207379): populate with proper status code
  FutureCallbackData* data =
      reinterpret_cast<FutureCallbackData*>(callback_data);
  if (data != nullptr) {
    data->impl->Complete(
        data->handle,
        data->db->ErrorFromResultAndErrorCode(result_code, status),
        status_message);
    delete data;
  }
}
}  // namespace

Future<void> DatabaseReferenceInternal::RemoveValue() {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnRemoveValue);
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, database_reference::GetMethodId(database_reference::kRemoveValue));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(
          new FutureCallbackData(handle, ref_future(), db_)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnRemoveValue));
}

Future<void> DatabaseReferenceInternal::SetValue(Variant value) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = internal::VariantToJavaObject(env, value);
    jobject task = env->CallObjectMethod(
        obj_, database_reference::GetMethodId(database_reference::kSetValue),
        value_obj);
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(
            new FutureCallbackData(handle, ref_future(), db_)),
        kApiIdentifier);
    env->DeleteLocalRef(task);
    if (value_obj) env->DeleteLocalRef(value_obj);
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValue));
}

Future<void> DatabaseReferenceInternal::SetPriority(Variant priority) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetPriority);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject priority_obj = internal::VariantToJavaObject(env, priority);
    jobject task = env->CallObjectMethod(
        obj_, database_reference::GetMethodId(database_reference::kSetPriority),
        priority_obj);
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(
            new FutureCallbackData(handle, ref_future(), db_)),
        kApiIdentifier);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(task);
    if (priority_obj) env->DeleteLocalRef(priority_obj);
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetPriority));
}

Future<void> DatabaseReferenceInternal::SetValueAndPriority(Variant value,
                                                            Variant priority) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnSetValueAndPriority);
  if (SetValueLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetValue);
  } else if (SetPriorityLastResult().status() == kFutureStatusPending) {
    ref_future()->Complete(handle, kErrorConflictingOperationInProgress,
                           kErrorMsgConflictSetPriority);
  } else if (!IsValidPriority(priority)) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForPriority);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = internal::VariantToJavaObject(env, value);
    jobject priority_obj = internal::VariantToJavaObject(env, priority);
    jobject task =
        env->CallObjectMethod(obj_,
                              database_reference::GetMethodId(
                                  database_reference::kSetValueAndPriority),
                              value_obj, priority_obj);
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(
            new FutureCallbackData(handle, ref_future(), db_)),
        kApiIdentifier);
    env->DeleteLocalRef(task);
    if (value_obj) env->DeleteLocalRef(value_obj);
    if (priority_obj) env->DeleteLocalRef(priority_obj);
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnSetValueAndPriority));
}

Future<void> DatabaseReferenceInternal::UpdateChildren(Variant values) {
  SafeFutureHandle<void> handle =
      ref_future()->SafeAlloc<void>(kDatabaseReferenceFnUpdateChildren);
  if (!values.is_map()) {
    ref_future()->Complete(handle, kErrorInvalidVariantType,
                           kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject values_obj = internal::VariantToJavaObject(env, values);
    jobject task = env->CallObjectMethod(
        obj_,
        database_reference::GetMethodId(database_reference::kUpdateChildren),
        values_obj);
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(
            new FutureCallbackData(handle, ref_future(), db_)),
        kApiIdentifier);
    env->DeleteLocalRef(task);
    if (values_obj) env->DeleteLocalRef(values_obj);
  }
  return MakeFuture(ref_future(), handle);
}

Future<void> DatabaseReferenceInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      ref_future()->LastResult(kDatabaseReferenceFnUpdateChildren));
}

ReferenceCountedFutureImpl* DatabaseReferenceInternal::ref_future() {
  return db_->future_manager().GetFutureApi(&future_api_id_);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
