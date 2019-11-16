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

#include "database/src/android/disconnection_android.h"

#include <assert.h>
#include <jni.h>

#include "app/src/future_manager.h"
#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/util_android.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/disconnection.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define ON_DISCONNECT_METHODS(X)                                               \
  X(SetValue, "setValue",                                                      \
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;"),                \
  X(SetValueAndStringPriority, "setValue",                                     \
    "(Ljava/lang/Object;Ljava/lang/String;)"                                   \
    "Lcom/google/android/gms/tasks/Task;"),                                    \
  X(SetValueAndDoublePriority, "setValue",                                     \
    "(Ljava/lang/Object;D)Lcom/google/android/gms/tasks/Task;"),               \
  X(UpdateChildren, "updateChildren",                                          \
    "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;"),                   \
  X(RemoveValue, "removeValue", "()Lcom/google/android/gms/tasks/Task;"),      \
  X(Cancel, "cancel", "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

METHOD_LOOKUP_DECLARATION(on_disconnect, ON_DISCONNECT_METHODS)
METHOD_LOOKUP_DEFINITION(on_disconnect,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/OnDisconnect",
                         ON_DISCONNECT_METHODS)

enum DisconnectionHandlerFn {
  kDisconnectionHandlerFnCancel = 0,
  kDisconnectionHandlerFnRemoveValue,
  kDisconnectionHandlerFnSetValue,
  kDisconnectionHandlerFnSetValueAndPriority,
  kDisconnectionHandlerFnUpdateChildren,
  kDisconnectionHandlerFnCount,
};

DisconnectionHandlerInternal::DisconnectionHandlerInternal(DatabaseInternal* db,
                                                           jobject obj)
    : db_(db) {
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
  db_->future_manager().AllocFutureApi(this, kDisconnectionHandlerFnCount);
}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  db_->future_manager().ReleaseFutureApi(this);
}

bool DisconnectionHandlerInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return on_disconnect::CacheMethodIds(env, activity);
}

void DisconnectionHandlerInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  on_disconnect::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
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

Future<void> DisconnectionHandlerInternal::Cancel() {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnCancel);
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kCancel));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(handle, future(), db_)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(task);
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::CancelLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnCancel));
}

Future<void> DisconnectionHandlerInternal::RemoveValue() {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnRemoveValue);
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kRemoveValue));
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      // FutureCallback will delete the newed FutureCallbackData.
      reinterpret_cast<void*>(new FutureCallbackData(handle, future(), db_)),
      kApiIdentifier);
  util::CheckAndClearJniExceptions(env);
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::RemoveValueLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnRemoveValue));
}

Future<void> DisconnectionHandlerInternal::SetValue(Variant value) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValue);
  if (SetValueAndPriorityLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = internal::VariantToJavaObject(env, value);
    jobject task = env->CallObjectMethod(
        obj_, on_disconnect::GetMethodId(on_disconnect::kSetValue), value_obj);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(new FutureCallbackData(handle, future(), db_)),
        kApiIdentifier);
    util::CheckAndClearJniExceptions(env);
    env->DeleteLocalRef(task);
    if (value_obj) env->DeleteLocalRef(value_obj);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnSetValue));
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriority(
    Variant value, Variant priority) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnSetValueAndPriority);
  if (SetValueLastResult().status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else if (!IsValidPriority(priority)) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForPriority);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = internal::VariantToJavaObject(env, value);
    jobject task;
    if (priority.is_string()) {
      jobject priority_obj = internal::VariantToJavaObject(env, priority);
      task = env->CallObjectMethod(
          obj_,
          on_disconnect::GetMethodId(on_disconnect::kSetValueAndStringPriority),
          value_obj, priority_obj);
      env->DeleteLocalRef(priority_obj);
    } else {
      task = env->CallObjectMethod(
          obj_,
          on_disconnect::GetMethodId(on_disconnect::kSetValueAndDoublePriority),
          value_obj, priority.AsDouble().double_value());
    }
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(new FutureCallbackData(handle, future(), db_)),
        kApiIdentifier);
    env->DeleteLocalRef(task);
    if (value_obj) env->DeleteLocalRef(value_obj);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::SetValueAndPriorityLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnSetValueAndPriority));
}

Future<void> DisconnectionHandlerInternal::UpdateChildren(Variant values) {
  SafeFutureHandle<void> handle =
      future()->SafeAlloc<void>(kDisconnectionHandlerFnUpdateChildren);
  if (!values.is_map()) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject values_obj = internal::VariantToJavaObject(env, values);
    jobject task = env->CallObjectMethod(
        obj_, on_disconnect::GetMethodId(on_disconnect::kUpdateChildren),
        values_obj);
    util::CheckAndClearJniExceptions(env);
    util::RegisterCallbackOnTask(
        env, task, FutureCallback,
        // FutureCallback will delete the newed FutureCallbackData.
        reinterpret_cast<void*>(new FutureCallbackData(handle, future(), db_)),
        kApiIdentifier);
    env->DeleteLocalRef(task);
    if (values_obj) env->DeleteLocalRef(values_obj);
  }
  return MakeFuture(future(), handle);
}

Future<void> DisconnectionHandlerInternal::UpdateChildrenLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kDisconnectionHandlerFnUpdateChildren));
}

ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return db_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
