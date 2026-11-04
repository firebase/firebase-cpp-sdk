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
    "(Ljava/lang/Object;)Lcom/google/android/gms/tasks/Task;")                 \
  X(SetValueAndStringPriority, "setValue",                                     \
    "(Ljava/lang/Object;Ljava/lang/String;)"                                   \
    "Lcom/google/android/gms/tasks/Task;")                                     \
  X(SetValueAndDoublePriority, "setValue",                                     \
    "(Ljava/lang/Object;D)Lcom/google/android/gms/tasks/Task;")                \
  X(UpdateChildren, "updateChildren",                                          \
    "(Ljava/util/Map;)Lcom/google/android/gms/tasks/Task;")                    \
  X(RemoveValue, "removeValue", "()Lcom/google/android/gms/tasks/Task;")       \
  X(Cancel, "cancel", "()Lcom/google/android/gms/tasks/Task;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    on_disconnect, ON_DISCONNECT_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/OnDisconnect")

DisconnectionHandlerInternal::DisconnectionHandlerInternal(DatabaseInternal* db,
                                                           jobject obj)
    : db_(db),
      obj_(obj ? db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj) : nullptr),
      future_api_(db) {}

DisconnectionHandlerInternal::~DisconnectionHandlerInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

bool DisconnectionHandlerInternal::Initialize(App* app) {
  return on_disconnect::CacheMethodIds(app->GetJNIEnv(), app->activity());
}

void DisconnectionHandlerInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  on_disconnect::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

void DisconnectionHandlerInternal::CancelImpl(SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kCancel));
  db_->RegisterTaskCallback(env, task, handle, future(), "Cancel failed");
}

void DisconnectionHandlerInternal::RemoveValueImpl(
    SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kRemoveValue));
  db_->RegisterTaskCallback(env, task, handle, future(), "RemoveValue failed");
}

void DisconnectionHandlerInternal::SetValueImpl(const Variant& value,
                                                SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kSetValue), value_obj);
  if (value_obj) env->DeleteLocalRef(value_obj);
  db_->RegisterTaskCallback(env, task, handle, future(), "SetValue failed");
}

void DisconnectionHandlerInternal::SetValueAndPriorityImpl(
    const Variant& value, const Variant& priority,
    SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = ::firebase::util::VariantToJavaObject(env, value);
  jobject task = nullptr;
  if (priority.is_string() || priority.is_null()) {
    jobject priority_obj =
        priority.is_null() ? nullptr
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
  db_->RegisterTaskCallback(env, task, handle, future(),
                            "SetValueAndPriority failed");
}

void DisconnectionHandlerInternal::UpdateChildrenImpl(
    const Variant& values, SafeFutureHandle<void> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject values_obj = ::firebase::util::VariantToJavaObject(env, values);
  jobject task = env->CallObjectMethod(
      obj_, on_disconnect::GetMethodId(on_disconnect::kUpdateChildren),
      values_obj);
  if (values_obj) env->DeleteLocalRef(values_obj);
  db_->RegisterTaskCallback(env, task, handle, future(),
                            "UpdateChildren failed");
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
