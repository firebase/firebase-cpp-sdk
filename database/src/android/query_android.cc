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
    "Lcom/google/firebase/database/ValueEventListener;"),                      \
  X(AddChildEventListener, "addChildEventListener",                            \
    "(Lcom/google/firebase/database/ChildEventListener;)"                      \
    "Lcom/google/firebase/database/ChildEventListener;"),                      \
  X(AddListenerForSingleValueEvent, "addListenerForSingleValueEvent",          \
    "(Lcom/google/firebase/database/ValueEventListener;)V"),                   \
  X(RemoveValueEventListener, "removeEventListener",                           \
    "(Lcom/google/firebase/database/ValueEventListener;)V"),                   \
  X(RemoveChildEventListener, "removeEventListener",                           \
    "(Lcom/google/firebase/database/ChildEventListener;)V"),                   \
  X(KeepSynced, "keepSynced", "(Z)V"),                                         \
  X(StartAtString, "startAt",                                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;"),               \
  X(StartAtDouble, "startAt", "(D)Lcom/google/firebase/database/Query;"),      \
  X(StartAtBool, "startAt", "(Z)Lcom/google/firebase/database/Query;"),        \
  X(StartAtStringString, "startAt",                                            \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;"),                                   \
  X(StartAtDoubleString, "startAt",                                            \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(StartAtBoolString, "startAt",                                              \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(EndAtString, "endAt",                                                      \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;"),               \
  X(EndAtDouble, "endAt", "(D)Lcom/google/firebase/database/Query;"),          \
  X(EndAtBool, "endAt", "(Z)Lcom/google/firebase/database/Query;"),            \
  X(EndAtStringString, "endAt",                                                \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;"),                                   \
  X(EndAtDoubleString, "endAt",                                                \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(EndAtBoolString, "endAt",                                                  \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(EqualToString, "equalTo",                                                  \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;"),               \
  X(EqualToDouble, "equalTo", "(D)Lcom/google/firebase/database/Query;"),      \
  X(EqualToBool, "equalTo", "(Z)Lcom/google/firebase/database/Query;"),        \
  X(EqualToStringString, "equalTo",                                            \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/database/Query;"),                                   \
  X(EqualToDoubleString, "equalTo",                                            \
    "(DLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(EqualToBoolString, "equalTo",                                              \
    "(ZLjava/lang/String;)Lcom/google/firebase/database/Query;"),              \
  X(LimitToFirst, "limitToFirst",                                              \
    "(I)Lcom/google/firebase/database/Query;"),                                \
  X(LimitToLast, "limitToLast",                                                \
    "(I)Lcom/google/firebase/database/Query;"),                                \
  X(OrderByChild, "orderByChild",                                              \
    "(Ljava/lang/String;)Lcom/google/firebase/database/Query;"),               \
  X(OrderByPriority, "orderByPriority",                                        \
    "()Lcom/google/firebase/database/Query;"),                                 \
  X(OrderByKey, "orderByKey", "()Lcom/google/firebase/database/Query;"),       \
  X(OrderByValue, "orderByValue",                                              \
    "()Lcom/google/firebase/database/Query;"),                                 \
  X(GetRef, "getRef",                                                          \
    "()Lcom/google/firebase/database/DatabaseReference;")
// clang-format on

METHOD_LOOKUP_DECLARATION(query, QUERY_METHODS)
METHOD_LOOKUP_DEFINITION(query,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/Query",
                         QUERY_METHODS)

QueryInternal::QueryInternal(DatabaseInternal* db, jobject obj) : db_(db) {
  db_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
}

QueryInternal::QueryInternal(DatabaseInternal* db, jobject obj,
                             const internal::QuerySpec& query_spec)
    : db_(db), query_spec_(query_spec) {
  db_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
}

QueryInternal::QueryInternal(const QueryInternal& src)
    : db_(src.db_), query_spec_(src.query_spec_) {
  db_->future_manager().AllocFutureApi(&future_api_id_, kQueryFnCount);
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_);
}

QueryInternal& QueryInternal::operator=(const QueryInternal& src) {
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_);
  query_spec_ = src.query_spec_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
QueryInternal::QueryInternal(QueryInternal&& src) : db_(src.db_) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
  query_spec_ = src.query_spec_;
}

QueryInternal& QueryInternal::operator=(QueryInternal&& src) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  db_->future_manager().MoveFutureApi(&src.future_api_id_, &future_api_id_);
  query_spec_ = src.query_spec_;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

QueryInternal::~QueryInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  db_->future_manager().ReleaseFutureApi(&future_api_id_);
}

bool QueryInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return query::CacheMethodIds(env, activity);
}

void QueryInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  query::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

DatabaseReferenceInternal* QueryInternal::GetReference() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject database_reference_obj =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kGetRef));
  if (util::LogException(env, kLogLevelWarning,
                         "Query::GetReference() failed")) {
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  return internal;
}

void QueryInternal::SetKeepSynchronized(bool keep_sync) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  env->CallVoidMethod(obj_, query::GetMethodId(query::kKeepSynced), keep_sync);
  util::CheckAndClearJniExceptions(env);
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
  if (util::LogException(env, kLogLevelError, "Query::OrderByChild (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::OrderByKey() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByKey;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kOrderByKey));
  if (util::LogException(env, kLogLevelError, "Query::OrderByKey (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::OrderByPriority() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByPriority;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kOrderByPriority));
  if (util::LogException(env, kLogLevelError,
                         "Query::OrderByPriority (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::OrderByValue() {
  internal::QuerySpec spec = query_spec_;
  spec.params.order_by = internal::QueryParams::kOrderByValue;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj =
      env->CallObjectMethod(obj_, query::GetMethodId(query::kOrderByValue));
  if (util::LogException(env, kLogLevelError, "Query::OrderByValue (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::LimitToFirst(size_t limit) {
  internal::QuerySpec spec = query_spec_;
  spec.params.limit_first = limit;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kLimitToFirst), limit);
  if (util::LogException(env, kLogLevelError, "Query::LimitToFirst (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::LimitToLast(size_t limit) {
  internal::QuerySpec spec = query_spec_;
  spec.params.limit_last = limit;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = env->CallObjectMethod(
      obj_, query::GetMethodId(query::kLimitToLast), limit);
  if (util::LogException(env, kLogLevelError, "Query::LimitToLast (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::StartAt(Variant value) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::StartAt(): Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  internal::QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kStartAtBool), value.bool_value());
  } else if (value.is_numeric()) {
    query_obj =
        env->CallObjectMethod(obj_, query::GetMethodId(query::kStartAtDouble),
                              value.AsDouble().double_value());
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kStartAtString), value_string);
    env->DeleteLocalRef(value_string);
  }
  if (util::LogException(env, kLogLevelError, "Query::StartAt (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::StartAt(Variant value, const char* key) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::StartAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.start_at_value = value;
  spec.params.start_at_child_key = key;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring key_string = env->NewStringUTF(key);
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kStartAtBoolString), value.bool_value(),
        key_string);
  } else if (value.is_numeric()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kStartAtDoubleString),
        value.AsDouble().double_value(), key_string);
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kStartAtStringString), value_string,
        key_string);
    env->DeleteLocalRef(value_string);
  }
  env->DeleteLocalRef(key_string);
  if (util::LogException(env, kLogLevelError, "Query::StartAt (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::EndAt(Variant value) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EndAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  internal::QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEndAtBool), value.bool_value());
  } else if (value.is_numeric()) {
    query_obj =
        env->CallObjectMethod(obj_, query::GetMethodId(query::kEndAtDouble),
                              value.AsDouble().double_value());
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEndAtString), value_string);
    env->DeleteLocalRef(value_string);
  }
  if (util::LogException(env, kLogLevelError, "Query::EndAt (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::EndAt(Variant value, const char* key) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EndAt: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.end_at_value = value;
  spec.params.end_at_child_key = key;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring key_string = env->NewStringUTF(key);
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj =
        env->CallObjectMethod(obj_, query::GetMethodId(query::kEndAtBoolString),
                              value.bool_value(), key_string);
  } else if (value.is_numeric()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEndAtDoubleString),
        value.AsDouble().double_value(), key_string);
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEndAtStringString), value_string,
        key_string);
    env->DeleteLocalRef(value_string);
  }
  env->DeleteLocalRef(key_string);
  if (util::LogException(env, kLogLevelError, "Query::EndAt (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::EqualTo(Variant value) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EqualTo: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  internal::QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEqualToBool), value.bool_value());
  } else if (value.is_numeric()) {
    query_obj =
        env->CallObjectMethod(obj_, query::GetMethodId(query::kEqualToDouble),
                              value.AsDouble().double_value());
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEqualToString), value_string);
    env->DeleteLocalRef(value_string);
  }
  if (util::LogException(env, kLogLevelError, "Query::EqualTo (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

QueryInternal* QueryInternal::EqualTo(Variant value, const char* key) {
  Logger* logger = db_->logger();
  if (!value.is_numeric() && !value.is_string() && !value.is_bool()) {
    logger->LogWarning(
        "Query::EqualTo: Only strings, numbers, and boolean values are "
        "allowed. (URL = %s)",
        query_spec_.path.c_str());
    return nullptr;
  }
  FIREBASE_ASSERT_RETURN(nullptr, key != nullptr);
  internal::QuerySpec spec = query_spec_;
  spec.params.equal_to_value = value;
  spec.params.equal_to_child_key = key;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring key_string = env->NewStringUTF(key);
  jobject query_obj = nullptr;
  if (value.is_bool()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEqualToBoolString), value.bool_value(),
        key_string);
  } else if (value.is_numeric()) {
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEqualToDoubleString),
        value.AsDouble().double_value(), key_string);
  } else if (value.is_string()) {
    jstring value_string = env->NewStringUTF(value.string_value());
    query_obj = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kEqualToStringString), value_string,
        key_string);
    env->DeleteLocalRef(value_string);
  }
  env->DeleteLocalRef(key_string);
  if (util::LogException(env, kLogLevelError, "Query::EqualTo (URL = %s)",
                         query_spec_.path.c_str())) {
    return nullptr;
  }
  QueryInternal* internal = new QueryInternal(db_, query_obj, spec);
  env->DeleteLocalRef(query_obj);
  return internal;
}

SingleValueListener::SingleValueListener(DatabaseInternal* db,
                                         ReferenceCountedFutureImpl* future,
                                         SafeFutureHandle<DataSnapshot> handle)
    : db_(db), future_(future), handle_(handle), java_listener_(nullptr) {}

SingleValueListener::~SingleValueListener() {
  if (java_listener_ != nullptr) {
    db_->RemoveSingleValueListener(java_listener_);
  }
}

void SingleValueListener::SetJavaListener(jobject obj) {
  java_listener_ = obj;
  db_->AddSingleValueListener(java_listener_);
}

void SingleValueListener::OnValueChanged(const DataSnapshot& snapshot) {
  db_->ClearJavaEventListener(java_listener_);
  db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(java_listener_);
  future_->Complete<DataSnapshot>(
      handle_, kErrorNone, "",
      [&snapshot](DataSnapshot* data) { *data = snapshot; });
  delete this;
}

void SingleValueListener::OnCancelled(const Error& error_code,
                                      const char* error_message) {
  db_->ClearJavaEventListener(java_listener_);
  db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(java_listener_);
  future_->Complete(handle_, error_code, error_message);
  delete this;
}

Future<DataSnapshot> QueryInternal::GetValue() {
  // Register a one-time ValueEventListener with the query.
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  SafeFutureHandle<DataSnapshot> handle =
      query_future()->SafeAlloc<DataSnapshot>(kQueryFnGetValue,
                                              DataSnapshot(nullptr));
  SingleValueListener* single_listener =
      new SingleValueListener(db_, query_future(), handle);
  // The SingleValueListener newed above is deleted in one of several places:
  // - In the LogException block below, if an exception was thrown adding the
  //   listener.
  // - In the listener's OnValueChanged and OnCancelled callbacks, which are
  //   guaranteed to be called once as long as the database is still valid.
  // - In DatabaseInternal's destructor, if any SingleValueListeners were
  //   not deleted (due to shutdown happening while a value is pending).
  jobject listener = db_->CreateJavaEventListener(single_listener);
  single_listener->SetJavaListener(listener);
  env->CallVoidMethod(
      obj_, query::GetMethodId(query::kAddListenerForSingleValueEvent),
      listener);
  if (util::LogException(env, kLogLevelError,
                         "Query::GetValue (URL = %s) failed")) {
    // The query failed, so it needs to clean itself up.
    db_->ClearJavaEventListener(listener);
    env->DeleteGlobalRef(listener);
    delete single_listener;
    query_future()->Complete(handle, kErrorUnknownError,
                             "addListenerForSingleValueEvent failed");
  }
  return MakeFuture(query_future(), handle);
}

Future<DataSnapshot> QueryInternal::GetValueLastResult() {
  return static_cast<const Future<DataSnapshot>&>(
      query_future()->LastResult(kQueryFnGetValue));
}

void QueryInternal::AddValueListener(ValueListener* value_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->RegisterValueEventListener(query_spec_, value_listener);
  if (listener != nullptr) {
    // Register a Java listener.
    jobject listener_ref = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kAddValueEventListener), listener);
    env->DeleteLocalRef(listener_ref);
    util::LogException(env, kLogLevelError,
                       "Query::AddValueListener (URL = %s) failed",
                       query_spec_.path.c_str());
  } else {
    Logger* logger = db_->logger();
    logger->LogWarning(
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
    // Unregister it. If this is the last one, it will be deleted once this
    // local ref is deleted.
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveValueEventListener), listener);
    util::LogException(env, kLogLevelError,
                       "Query::RemoveValueListener (URL = %s) failed",
                       query_spec_.path.c_str());
    env->DeleteLocalRef(listener);
  }
}

void QueryInternal::RemoveAllValueListeners() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  std::vector<jobject> listeners =
      db_->UnregisterAllValueEventListeners(query_spec_);
  for (int i = 0; i < listeners.size(); i++) {
    env->CallVoidMethod(obj_,
                        query::GetMethodId(query::kRemoveValueEventListener),
                        listeners[i]);
    env->DeleteLocalRef(listeners[i]);
  }
}

void QueryInternal::AddChildListener(ChildListener* child_listener) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject listener =
      db_->RegisterChildEventListener(query_spec_, child_listener);
  if (listener != nullptr) {
    // Register a Java listener.
    jobject listener_ref = env->CallObjectMethod(
        obj_, query::GetMethodId(query::kAddChildEventListener), listener);
    env->DeleteLocalRef(listener_ref);
    util::LogException(env, kLogLevelError,
                       "Query::AddChildListener (URL = %s) failed",
                       query_spec_.path.c_str());
  } else {
    Logger* logger = db_->logger();
    logger->LogWarning(
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
    // Unregister it. If this is the last one, it will be deleted once this
    // local ref is deleted.
    env->CallVoidMethod(
        obj_, query::GetMethodId(query::kRemoveChildEventListener), listener);
    util::LogException(env, kLogLevelError,
                       "Query::RemoveChildListener (URL = %s) failed",
                       query_spec_.path.c_str());
    env->DeleteLocalRef(listener);
  }
}

ReferenceCountedFutureImpl* QueryInternal::query_future() {
  return db_->future_manager().GetFutureApi(&future_api_id_);
}

void QueryInternal::RemoveAllChildListeners() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  std::vector<jobject> listeners =
      db_->UnregisterAllChildEventListeners(query_spec_);
  for (int i = 0; i < listeners.size(); i++) {
    env->CallVoidMethod(obj_,
                        query::GetMethodId(query::kRemoveChildEventListener),
                        listeners[i]);
    env->DeleteLocalRef(listeners[i]);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
