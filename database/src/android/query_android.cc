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

#include "database/src/android/query_android.h"

#include <jni.h>
#include <utility>

#include "app/src/app_android.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/common/query.h"
#include "database/src/common/query_spec.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define QUERY_METHODS(X)                                                       \
  X(AddValueEventListener, "addValueEventListener",                            \
    "(Lcom/google/firebase/database/ValueEventListener;)"                      \
    "Lcom/google/firebase/database/ValueEventListener;")                       \
  X(AddChildEventListener, "addChildEventListener",                            \
    "(Lcom/google/firebase/database/ChildEventListener;)"                      \
    "Lcom/google/firebase/database/ChildEventListener;")                       \
  X(AddListenerForSingleValueEvent, "addListenerForSingleValueEvent",          \
    "(Lcom/google/firebase/database/ValueEventListener;)V")                    \
  X(RemoveValueEventListener, "removeEventListener",                           \
    "(Lcom/google/firebase/database/ValueEventListener;)V")                    \
  X(RemoveChildEventListener, "removeEventListener",                           \
    "(Lcom/google/firebase/database/ChildEventListener;)V")                    \
  X(KeepSynced, "keepSynced", "(Z)V")                                          \
  X(StartAtString, "startAt",                                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;")                \
  X(StartAtDouble, "startAt", "(D)Lcom/google/firebase/database/Query;")       \
  X(StartAtBool, "startAt", "(Z)Lcom/google/firebase/database/Query;")         \
  X(StartAtStringString, "startAt",                                            \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;")                                    \
  X(StartAtDoubleString, "startAt",                                            \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(StartAtBoolString, "startAt",                                              \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(EndAtString, "endAt",                                                      \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;")                \
  X(EndAtDouble, "endAt", "(D)Lcom/google/firebase/database/Query;")           \
  X(EndAtBool, "endAt", "(Z)Lcom/google/firebase/database/Query;")             \
  X(EndAtStringString, "endAt",                                                \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;")                                    \
  X(EndAtDoubleString, "endAt",                                                \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(EndAtBoolString, "endAt",                                                  \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(EqualToString, "equalTo",                                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;")                \
  X(EqualToDouble, "equalTo", "(D)Lcom/google/firebase/database/Query;")       \
  X(EqualToBool, "equalTo", "(Z)Lcom/google/firebase/database/Query;")         \
  X(EqualToStringString, "equalTo",                                            \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;")                                    \
  X(EqualToDoubleString, "equalTo",                                            \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(EqualToBoolString, "equalTo",                                              \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;")               \
  X(LimitToFirst, "limitToFirst",                                              \
    "(I)Lcom/google/firebase/database/Query;")                                 \
  X(LimitToLast, "limitToLast",                                                \
    "(I)Lcom/google/firebase/database/Query;")                                 \
  X(OrderByChild, "orderByChild",                                              \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;")                \
  X(OrderByPriority, "orderByPriority",                                        \
    "()Lcom/google/firebase/database/Query;")                                  \
  X(OrderByKey, "orderByKey", "()Lcom/google/firebase/database/Query;")        \
  X(OrderByValue, "orderByValue",                                              \
    "()Lcom/google/firebase/database/Query;")                                  \
  X(GetRef, "getRef",                                                          \
    "()Lcom/google/firebase/database/DatabaseReference;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(query, QUERY_METHODS, FIELD_LOOKUP_NONE,
                              PROGUARD_KEEP_CLASS
                              "com/google/firebase/database/Query")

QueryInternal::QueryInternal(DatabaseInternal* db, jobject obj)
    : db_(db),
      obj_(db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj)),
      future_api_(db) {}

QueryInternal::QueryInternal(DatabaseInternal* db, jobject obj,
                             const internal::QuerySpec& query_spec)
    : db_(db),
      obj_(db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj)),
      query_spec_(query_spec),
      future_api_(db) {}

QueryInternal::QueryInternal(const QueryInternal& src)
    : db_(src.db_),
      obj_(db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_)),
      query_spec_(src.query_spec_),
      future_api_(src.future_api_) {}

QueryInternal& QueryInternal::operator=(const QueryInternal& src) {
  if (&src == this) return *this;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  if (obj_ != nullptr) env->DeleteGlobalRef(obj_);
  db_ = src.db_;
  future_api_ = src.future_api_;
  obj_ = src.obj_ ? env->NewGlobalRef(src.obj_) : nullptr;
  query_spec_ = src.query_spec_;
  return *this;
}

QueryInternal::QueryInternal(QueryInternal&& src)
    : db_(src.db_),
      obj_(src.obj_),
      query_spec_(std::move(src.query_spec_)),
      future_api_(std::move(src.future_api_)) {
  src.obj_ = nullptr;
}

QueryInternal& QueryInternal::operator=(QueryInternal&& src) {
  if (&src == this) return *this;
  if (obj_ != nullptr) db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
  db_ = src.db_;
  obj_ = src.obj_;
  src.obj_ = nullptr;
  future_api_ = std::move(src.future_api_);
  query_spec_ = std::move(src.query_spec_);
  return *this;
}

QueryInternal::~QueryInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

bool QueryInternal::Initialize(App* app) {
  return query::CacheMethodIds(app->GetJNIEnv(), app->activity());
}

void QueryInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  query::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject ref_obj =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kGetRef));
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogWarning("Query::GetReference() failed");
    return nullptr;
  }
  auto* internal = new DatabaseReferenceInternal(db_, ref_obj);
  env->DeleteLocalRef(ref_obj);
  return internal;
}

void QueryInternal::SetKeepSynchronized(bool keep_sync) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallVoidMethod(obj_, query::GetMethodId(query::kKeepSynced), keep_sync);
  ::firebase::jni::CheckAndClearException(env);
}

QueryInternal* QueryInternal::OrderByChild(const char* path) {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByChild;
  spec.params.order_by_child = path;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject path_string = env->NewStringUTF(path);
  jobject query_obj = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kOrderByChild), path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogError("Query::OrderByChild (URL = %s)",
                            query_spec_.path.c_str());
    return nullptr;
  }
  auto* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

namespace {
QueryInternal* MakeNoArgQuery(DatabaseInternal* db, jobject obj,
                              const internal::QuerySpec& spec,
                              query::Method method, const char* fn_name) {
  JNIEnv* env = db->GetApp()->GetJNIEnv();
  jobject query_obj = env->CallObjectMethod(obj, query::GetMethodId(method));
  if (::firebase::jni::CheckAndClearException(env)) {
    db->logger()->LogError("Query::%s (URL = %s)", fn_name, spec.path.c_str());
    return nullptr;
  }
  auto* internal = new QueryInternal(db, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}
}  // namespace

QueryInternal* QueryInternal::OrderByKey() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByKey;
  return MakeNoArgQuery(db_, obj_, spec, query::kOrderByKey, "OrderByKey");
}

QueryInternal* QueryInternal::OrderByPriority() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByPriority;
  return MakeNoArgQuery(db_, obj_, spec, query::kOrderByPriority,
                        "OrderByPriority");
}

QueryInternal* QueryInternal::OrderByValue() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByValue;
  return MakeNoArgQuery(db_, obj_, spec, query::kOrderByValue, "OrderByValue");
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  internal::QuerySpec spec = query_spec_;
  spec.params.limit_first = limit;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kLimitToFirst), static_cast<jint>(limit));
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogError("Query::LimitToFirst (URL = %s)",
                            query_spec_.path.c_str());
    return nullptr;
  }
  auto* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  internal::QuerySpec spec = query_spec_;
  spec.params.limit_last = limit;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kLimitToLast), static_cast<jint>(limit));
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogError("Query::LimitToLast (URL = %s)",
                            query_spec_.path.c_str());
    return nullptr;
  }
  auto* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::CallRangeMethod(
    const Variant& value, const char* key, const internal::QuerySpec& spec,
    int bool_method, int double_method, int str_method, const char* fn_name) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring key_str = key ? env->NewStringUTF(key) : nullptr;
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    jmethodID mid = query::GetMethodId(static_cast<query::Method>(bool_method));
    query_obj = key ? env->CallObjectMethod(obj_, mid, value.bool_value(),
                                            key_str)
                    : env->CallObjectMethod(obj_, mid, value.bool_value());
  } else if (value.is_numeric()) {
    jmethodID mid =
        query::GetMethodId(static_cast<query::Method>(double_method));
    double d = value.AsDouble().double_value();
    query_obj = key ? env->CallObjectMethod(obj_, mid, d, key_str)
                    : env->CallObjectMethod(obj_, mid, d);
  } else if (value.is_string()) {
    jmethodID mid = query::GetMethodId(static_cast<query::Method>(str_method));
    jstring val_str = env->NewStringUTF(value.string_value());
    query_obj = key ? env->CallObjectMethod(obj_, mid, val_str, key_str)
                    : env->CallObjectMethod(obj_, mid, val_str);
    env->DeleteLocalRef(val_str);
  }
  if (key_str) env->DeleteLocalRef(key_str);
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogError("Query::%s (URL = %s)", fn_name,
                            query_spec_.path.c_str());
    return nullptr;
  }
  auto* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

namespace {
bool ValidateRangeValue(Logger* logger, const Variant& value, const char* name,
                        const char* url) {
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::%s: Only strings, numbers, and boolean values are allowed. "
        "(URL = %s)",
        name, url);
    return false;
  }
  return true;
}
}  // namespace

QueryInternal* QueryInternal::StartAt(Variant value) {
  if (!ValidateRangeValue(db_->logger(), value, "StartAt()",
                          query_spec_.path.c_str()))
    return nullptr;
  internal::QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  return CallRangeMethod(value, nullptr, spec, query::kStartAtBool,
                         query::kStartAtDouble, query::kStartAtString,
                         "StartAt");
}

QueryInternal* QueryInternal::StartAt(Variant value, const char* key) {
  if (!ValidateRangeValue(db_->logger(), value, "StartAt",
                          query_spec_.path.c_str()))
    return nullptr;
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  spec.params.start_at_child_key = key;
  return CallRangeMethod(value, key, spec, query::kStartAtBoolString,
                         query::kStartAtDoubleString,
                         query::kStartAtStringString, "StartAt");
}

QueryInternal* QueryInternal::EndAt(Variant value) {
  if (!ValidateRangeValue(db_->logger(), value, "EndAt",
                          query_spec_.path.c_str()))
    return nullptr;
  internal::QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  return CallRangeMethod(value, nullptr, spec, query::kEndAtBool,
                         query::kEndAtDouble, query::kEndAtString, "EndAt");
}

QueryInternal* QueryInternal::EndAt(Variant value, const char* key) {
  if (!ValidateRangeValue(db_->logger(), value, "EndAt",
                          query_spec_.path.c_str()))
    return nullptr;
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  spec.params.end_at_child_key = key;
  return CallRangeMethod(value, key, spec, query::kEndAtBoolString,
                         query::kEndAtDoubleString, query::kEndAtStringString,
                         "EndAt");
}

QueryInternal* QueryInternal::EqualTo(Variant value) {
  if (!ValidateRangeValue(db_->logger(), value, "EqualTo",
                          query_spec_.path.c_str()))
    return nullptr;
  internal::QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  return CallRangeMethod(value, nullptr, spec, query::kEqualToBool,
                         query::kEqualToDouble, query::kEqualToString,
                         "EqualTo");
}

QueryInternal* QueryInternal::EqualTo(Variant value, const char* key) {
  if (!ValidateRangeValue(db_->logger(), value, "EqualTo",
                          query_spec_.path.c_str()))
    return nullptr;
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  spec.params.equal_to_child_key = key;
  return CallRangeMethod(value, key, spec, query::kEqualToBoolString,
                         query::kEqualToDoubleString,
                         query::kEqualToStringString, "EqualTo");
}

SingleValueListener::SingleValueListener(DatabaseInternal* db,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : db_(db), future_(future), handle_(handle), java_listener_(nullptr) {}

SingleValueListener::~SingleValueListener() {
  if (java_listener_ != nullptr) db_->RemoveSingleValueListener(java_listener_);
}

void SingleValueListener::SetJavaListener(jobject obj) {
  java_listener_ = obj;
  db_->AddSingleValueListener(java_listener_);
}

void SingleValueListener::CleanupJavaListener() {
  if (java_listener_ != nullptr) {
    db_->RemoveSingleValueListener(java_listener_);
    db_->ClearJavaEventListener(java_listener_);
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(java_listener_);
    java_listener_ = nullptr;
  }
}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  CleanupJavaListener();
  future_->Complete<DataSnapshot>(
      handle_, kErrorNone, "",
      [&snapshot](DataSnapshot* data) { *data = snapshot; });
  delete this;
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  CleanupJavaListener();
  future_->Complete(handle_, error_code, error_message);
  delete this;
}

void QueryInternal::GetValueImpl(SafeFutureHandle<DataSnapshot> handle) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  auto* single_listener = new SingleValueListener(db_, query_future(), handle);
  jobject listener = db_->CreateJavaEventListener(single_listener);
  single_listener->SetJavaListener(listener);
  env->CallVoidMethod(
      obj_, query::GetMethodId(query::kAddListenerForSingleValueEvent),
      listener);
  if (::firebase::jni::CheckAndClearException(env)) {
    db_->logger()->LogError("Query::GetValue (URL = %s) failed",
                            query_spec_.path.c_str());
    delete single_listener;
    db_->ClearJavaEventListener(listener);
    env->DeleteGlobalRef(listener);
    query_future()->Complete(handle, kErrorUnknownError,
                             "addListenerForSingleValueEvent failed");
  }
}

void QueryInternal::AddValueListener(ValueListener* value_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->RegisterValueEventListener(query_spec_, value_listener);
  if (listener != nullptr) {
    jobject listener_ref = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kAddValueEventListener), listener);
    env->DeleteLocalRef(listener_ref);
    if (::firebase::jni::CheckAndClearException(env)) {
      db_->logger()->LogError("Query::AddValueListener (URL = %s) failed",
                              query_spec_.path.c_str());
    }
  } else {
    db_->logger()->LogWarning(
        "Query::AddValueListener (URL = %s): You may not register the same "
        "ValueListener more than once on the same Query.",
        query_spec_.path.c_str());
  }
}

void QueryInternal::RemoveValueListener(ValueListener* value_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->UnregisterValueEventListener(query_spec_, value_listener);
  if (listener != nullptr) {
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveValueEventListener), listener);
    if (::firebase::jni::CheckAndClearException(env)) {
      db_->logger()->LogError("Query::RemoveValueListener (URL = %s) failed",
                              query_spec_.path.c_str());
    }
    env->DeleteLocalRef(listener);
  }
}

void QueryInternal::RemoveAllValueListeners() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  for (jobject listener : db_->UnregisterAllValueEventListeners(query_spec_)) {
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveValueEventListener), listener);
    ::firebase::jni::CheckAndClearException(env);
    env->DeleteLocalRef(listener);
  }
}

void QueryInternal::AddChildListener(ChildListener* child_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->RegisterChildEventListener(query_spec_, child_listener);
  if (listener != nullptr) {
    jobject listener_ref = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kAddChildEventListener), listener);
    env->DeleteLocalRef(listener_ref);
    if (::firebase::jni::CheckAndClearException(env)) {
      db_->logger()->LogError("Query::AddChildListener (URL = %s) failed",
                              query_spec_.path.c_str());
    }
  } else {
    db_->logger()->LogWarning(
        "Query::AddChildListener (URL = %s): You may not register the same "
        "ChildListener more than once on the same Query.",
        query_spec_.path.c_str());
  }
}

void QueryInternal::RemoveChildListener(ChildListener* child_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->UnregisterChildEventListener(query_spec_, child_listener);
  if (listener != nullptr) {
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveChildEventListener), listener);
    if (::firebase::jni::CheckAndClearException(env)) {
      db_->logger()->LogError("Query::RemoveChildListener (URL = %s) failed",
                              query_spec_.path.c_str());
    }
    env->DeleteLocalRef(listener);
  }
}

void QueryInternal::RemoveAllChildListeners() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  for (jobject listener : db_->UnregisterAllChildEventListeners(query_spec_)) {
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveChildEventListener), listener);
    ::firebase::jni::CheckAndClearException(env);
    env->DeleteLocalRef(listener);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
