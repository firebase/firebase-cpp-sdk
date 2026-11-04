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

#include "database/src/android/data_snapshot_android.h"

#include <jni.h>

#include "app/src/app_android.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/database_reference_android.h"
#include "database/src/android/util_android.h"
#include "database/src/include/firebase/database/data_snapshot.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define DATA_SNAPSHOT_METHODS(X)                                           \
  X(Child, "child",                                                        \
    "(Ljava/lang/String;)Lcom/google/firebase/database/DataSnapshot;")     \
  X(HasChild, "hasChild", "(Ljava/lang/String;)Z")                         \
  X(HasChildren, "hasChildren", "()Z")                                     \
  X(Exists, "exists", "()Z")                                               \
  X(GetValue, "getValue", "()Ljava/lang/Object;")                          \
  X(GetChildrenCount, "getChildrenCount", "()J")                           \
  X(GetRef, "getRef",                                                      \
    "()Lcom/google/firebase/database/DatabaseReference;")                  \
  X(GetKey, "getKey", "()Ljava/lang/String;")                              \
  X(GetChildren, "getChildren", "()Ljava/lang/Iterable;")                  \
  X(GetPriority, "getPriority", "()Ljava/lang/Object;")                    \
  X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    data_snapshot, DATA_SNAPSHOT_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/database/DataSnapshot")



DataSnapshotInternal::DataSnapshotInternal(DatabaseInternal* db, jobject obj)
    : db_(db) {
  obj_ = obj ? db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj) : nullptr;
}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& src)
    : db_(src.db_) {
  {
    MutexLock lock(src.key_mutex_);
    cached_key_ = src.cached_key_;
  }
  obj_ = src.obj_ ? db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_)
                  : nullptr;
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    const DataSnapshotInternal& src) {
  if (&src == this) return *this;
  Variant src_key;
  {
    MutexLock src_lock(src.key_mutex_);
    src_key = src.cached_key_;
  }
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
  }
  db_ = src.db_;
  {
    MutexLock lock(key_mutex_);
    cached_key_ = std::move(src_key);
  }
  obj_ = src.obj_ ? db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_)
                  : nullptr;
  return *this;
}

DataSnapshotInternal::DataSnapshotInternal(
    DataSnapshotInternal&& src) noexcept
    : db_(src.db_) {
  {
    MutexLock src_lock(src.key_mutex_);
    cached_key_ = std::move(src.cached_key_);
  }
  obj_ = src.obj_;
  src.obj_ = nullptr;
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    DataSnapshotInternal&& src) noexcept {
  if (&src == this) return *this;
  Variant src_key;
  {
    MutexLock src_lock(src.key_mutex_);
    src_key = std::move(src.cached_key_);
  }
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
  }
  db_ = src.db_;
  {
    MutexLock lock(key_mutex_);
    cached_key_ = std::move(src_key);
  }
  obj_ = src.obj_;
  src.obj_ = nullptr;
  return *this;
}

DataSnapshotInternal::~DataSnapshotInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

bool DataSnapshotInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return data_snapshot::CacheMethodIds(env, activity);
}

void DataSnapshotInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  data_snapshot::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

bool DataSnapshotInternal::Exists() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kExists));
  ::firebase::jni::CheckAndClearException(env);
  return result;
}

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  if (path == nullptr) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return nullptr;
  }
  jobject child_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kChild), path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::util::LogException(
          env, kLogLevelWarning,
          "DataSnapshot::Child(): Couldn't create child snapshot %s", path) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
    return nullptr;
  }
  DataSnapshotInternal* internal = new DataSnapshotInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

std::vector<DataSnapshot> DataSnapshotInternal::GetChildren() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  std::vector<DataSnapshot> result;
  result.reserve(GetChildrenCount());
  // iterable = snapshot.getChildren()
  jobject iterable = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetChildren));
  if (::firebase::jni::CheckAndClearException(env) || iterable == nullptr) {
    if (iterable != nullptr) env->DeleteLocalRef(iterable);
    return result;
  }
  jclass iterable_class = env->GetObjectClass(iterable);
  if (::firebase::jni::CheckAndClearException(env) ||
      iterable_class == nullptr) {
    if (iterable_class != nullptr) env->DeleteLocalRef(iterable_class);
    env->DeleteLocalRef(iterable);
    return result;
  }
  jmethodID iterator_method =
      env->GetMethodID(iterable_class, "iterator", "()Ljava/util/Iterator;");
  env->DeleteLocalRef(iterable_class);
  if (::firebase::jni::CheckAndClearException(env) ||
      iterator_method == nullptr) {
    env->DeleteLocalRef(iterable);
    return result;
  }
  // iterator = iterable.iterator()
  jobject iterator = env->CallObjectMethod(iterable, iterator_method);
  env->DeleteLocalRef(iterable);
  if (::firebase::jni::CheckAndClearException(env) || iterator == nullptr) {
    if (iterator != nullptr) env->DeleteLocalRef(iterator);
    return result;
  }
  jclass iterator_class = env->GetObjectClass(iterator);
  if (::firebase::jni::CheckAndClearException(env) ||
      iterator_class == nullptr) {
    if (iterator_class != nullptr) env->DeleteLocalRef(iterator_class);
    env->DeleteLocalRef(iterator);
    return result;
  }
  jmethodID has_next_method =
      env->GetMethodID(iterator_class, "hasNext", "()Z");
  if (::firebase::jni::CheckAndClearException(env) ||
      has_next_method == nullptr) {
    env->DeleteLocalRef(iterator_class);
    env->DeleteLocalRef(iterator);
    return result;
  }
  jmethodID next_method =
      env->GetMethodID(iterator_class, "next", "()Ljava/lang/Object;");
  env->DeleteLocalRef(iterator_class);
  if (::firebase::jni::CheckAndClearException(env) || next_method == nullptr) {
    env->DeleteLocalRef(iterator);
    return result;
  }
  // while (iterator.hasNext())
  while (true) {
    jboolean has_next = env->CallBooleanMethod(iterator, has_next_method);
    if (::firebase::jni::CheckAndClearException(env) || !has_next) {
      break;
    }
    // DataSnapshot java_snapshot = iterator.next();
    jobject java_snapshot = env->CallObjectMethod(iterator, next_method);
    if (::firebase::jni::CheckAndClearException(env) ||
        java_snapshot == nullptr) {
      if (java_snapshot != nullptr) env->DeleteLocalRef(java_snapshot);
      break;
    }
    result.push_back(
        DataSnapshot(new DataSnapshotInternal(db_, java_snapshot)));
    env->DeleteLocalRef(java_snapshot);
  }
  env->DeleteLocalRef(iterator);
  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jlong count = env->CallLongMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetChildrenCount));
  if (::firebase::jni::CheckAndClearException(env) || count < 0) {
    return 0;
  }
  return static_cast<size_t>(count);
}

bool DataSnapshotInternal::HasChildren() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jboolean has_children = env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kHasChildren));
  if (::firebase::jni::CheckAndClearException(env)) {
    return false;
  }
  return has_children;
}

const char* DataSnapshotInternal::GetKey() {
  MutexLock lock(key_mutex_);
  if (cached_key_.is_null()) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, data_snapshot::GetMethodId(data_snapshot::kGetKey)));
    if (::firebase::util::LogException(env, kLogLevelError,
                           "DataSnapshot::GetKey() failed")) {
      if (key_string != nullptr) env->DeleteLocalRef(key_string);
      return nullptr;
    }
    if (key_string == nullptr) {
      // key_string will be null if this is a snapshot of the Root.
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

std::string DataSnapshotInternal::GetKeyString() {
  const char* key = GetKey();
  return key != nullptr ? std::string(key) : std::string();
}

Variant DataSnapshotInternal::GetValue() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetValue));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
    return Variant::Null();
  }
  Variant v = ::firebase::util::JavaObjectToVariant(env, value_obj);
  if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
  return v;
}

Variant DataSnapshotInternal::GetPriority() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetPriority));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
    return Variant::Null();
  }
  Variant v = ::firebase::util::JavaObjectToVariant(env, value_obj);
  if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
  return v;
}

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject database_reference_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetRef));
  if (::firebase::util::LogException(env, kLogLevelWarning,
                         "DataSnapshot::GetReference() failed") ||
      database_reference_obj == nullptr) {
    if (database_reference_obj != nullptr) {
      env->DeleteLocalRef(database_reference_obj);
    }
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  return internal;
}

bool DataSnapshotInternal::HasChild(const char* path) const {
  if (path == nullptr) return false;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return false;
  }
  jboolean has_child = env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kHasChild), path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::util::LogException(env, kLogLevelWarning,
                         "DataSnapshot::HasChild() failed")) {
    return false;
  }
  return has_child;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
