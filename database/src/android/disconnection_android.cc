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

#include <jni.h>

#include "app/src/app_android.h"
#include "app/src/future_manager.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
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

JNI_METHOD_LOOKUP_DECLARATION(
    on_disconnect, ON_DISCONNECT_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/OnDisconnect")




DisconnectionHandlerInternal::DisconnectionHandlerInternal(DatabaseInternal* db,
                                                           jobject obj)
    : db_(db), obj_(nullptr) {
  if (obj != nullptr) obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
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
  ::firebase::jni::CheckAndClearException(env);
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
}  // namespace

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kCancel));
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    future()->Complete(handle, kErrorUnknownError, "Cancel failed");
  return;
  }
  FutureCallbackData* callback_data = new FutureCallbackData(handle, future(), db_);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      reinterpret_cast<void*>(callback_data),
      db_->jni_task_id());
  if (::firebase::jni::CheckAndClearException(env)) {
    delete callback_data;
    future()->Complete(handle, kErrorUnknownError, "Cancel failed");
  }
  env->DeleteLocalRef(task);
  return;
}


void DisconnectionHandlerInternal::RemoveValueImpl(SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kRemoveValue));
  if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
    if (task != nullptr) env->DeleteLocalRef(task);
    future()->Complete(handle, kErrorUnknownError, "RemoveValue failed");
  return;
  }
  FutureCallbackData* callback_data = new FutureCallbackData(handle, future(), db_);
  util::RegisterCallbackOnTask(
      env, task, FutureCallback,
      reinterpret_cast<void*>(callback_data),
      db_->jni_task_id());
  if (::firebase::jni::CheckAndClearException(env)) {
    delete callback_data;
    future()->Complete(handle, kErrorUnknownError, "RemoveValue failed");
  }
  env->DeleteLocalRef(task);
  return;
}


void DisconnectionHandlerInternal::SetValueImpl(const Variant& value, SafeFutureHandle<void> handle) {
  if (future()->LastResult(kDisconnectionHandlerFnSetValueAndPriority).status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
    jobject task = env->CallObjectMethod(
        obj_, on_disconnect::GetMethodId(on_disconnect::kSetValue), value_obj);
    if (value_obj) env->DeleteLocalRef(value_obj);
    if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
      if (task != nullptr) env->DeleteLocalRef(task);
      future()->Complete(handle, kErrorUnknownError, "SetValue failed");
    } else {
      FutureCallbackData* callback_data = new FutureCallbackData(handle, future(), db_);
      util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          reinterpret_cast<void*>(callback_data),
          db_->jni_task_id());
      if (::firebase::jni::CheckAndClearException(env)) {
        delete callback_data;
        future()->Complete(handle, kErrorUnknownError, "SetValue failed");
      }
      env->DeleteLocalRef(task);
    }
  }
  return;
}


void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority, SafeFutureHandle<void> handle) {
  if (future()->LastResult(kDisconnectionHandlerFnSetValue).status() == kFutureStatusPending) {
    future()->Complete(handle, kErrorConflictingOperationInProgress,
                       kErrorMsgConflictSetValue);
  } else if (!IsValidPriority(priority)) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForPriority);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
    jobject task = nullptr;
    if (priority.is_string() || priority.is_null()) {
      jobject priority_obj = priority.is_null()
                                 ? nullptr
                                 : ::firebase::util::VariantToJavaObject(env, priority);
      task = env->CallObjectMethod(
          obj_,
          on_disconnect::GetMethodId(on_disconnect::kSetValueAndStringPriority),
          value_obj, priority_obj);
      if (priority_obj) env->DeleteLocalRef(priority_obj);
    } else {
      task = env->CallObjectMethod(
          obj_,
          on_disconnect::GetMethodId(on_disconnect::kSetValueAndDoublePriority),
          value_obj, priority.AsDouble().double_value());
    }
    if (value_obj) env->DeleteLocalRef(value_obj);
    if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
      if (task != nullptr) env->DeleteLocalRef(task);
      future()->Complete(handle, kErrorUnknownError,
                         "SetValueAndPriority failed");
    } else {
      FutureCallbackData* callback_data = new FutureCallbackData(handle, future(), db_);
      util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          reinterpret_cast<void*>(callback_data),
          db_->jni_task_id());
      if (::firebase::jni::CheckAndClearException(env)) {
        delete callback_data;
        future()->Complete(handle, kErrorUnknownError, "SetValueAndPriority failed");
      }
      env->DeleteLocalRef(task);
    }
  }
  return;
}


void DisconnectionHandlerInternal::UpdateChildrenImpl(const Variant& values, SafeFutureHandle<void> handle) {
  if (!values.is_map()) {
    future()->Complete(handle, kErrorInvalidVariantType,
                       kErrorMsgInvalidVariantForUpdateChildren);
  } else {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jobject values_obj = ::firebase::util::VariantToJavaObject(env, values);
    jobject task = env->CallObjectMethod(
        obj_, on_disconnect::GetMethodId(on_disconnect::kUpdateChildren),
        values_obj);
    if (values_obj) env->DeleteLocalRef(values_obj);
    if (::firebase::jni::CheckAndClearException(env) || task == nullptr) {
      if (task != nullptr) env->DeleteLocalRef(task);
      future()->Complete(handle, kErrorUnknownError, "UpdateChildren failed");
    } else {
      FutureCallbackData* callback_data = new FutureCallbackData(handle, future(), db_);
      util::RegisterCallbackOnTask(
          env, task, FutureCallback,
          reinterpret_cast<void*>(callback_data),
          db_->jni_task_id());
      if (::firebase::jni::CheckAndClearException(env)) {
        delete callback_data;
        future()->Complete(handle, kErrorUnknownError, "UpdateChildren failed");
      }
      env->DeleteLocalRef(task);
    }
  }
  return;
}


ReferenceCountedFutureImpl* DisconnectionHandlerInternal::future() {
  return db_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
