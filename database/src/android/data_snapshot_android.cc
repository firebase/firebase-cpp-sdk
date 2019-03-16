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
    "(Ljava/lang/String;)Lcom/google/firebase/database/DataSnapshot;"),    \
  X(HasChild, "hasChild", "(Ljava/lang/String;)Z"),                        \
  X(HasChildren, "hasChildren", "()Z"),                                    \
  X(Exists, "exists", "()Z"),                                              \
  X(GetValue, "getValue", "()Ljava/lang/Object;"),                        \
  X(GetChildrenCount, "getChildrenCount", "()J"),                          \
  X(GetRef, "getRef",                                                      \
    "()Lcom/google/firebase/database/DatabaseReference;"),                 \
  X(GetKey, "getKey", "()Ljava/lang/String;"),                             \
  X(GetChildren, "getChildren", "()Ljava/lang/Iterable;"),                 \
  X(GetPriority, "getPriority", "()Ljava/lang/Object;"),                   \
  X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(data_snapshot, DATA_SNAPSHOT_METHODS)
METHOD_LOOKUP_DEFINITION(data_snapshot,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/DataSnapshot",
                         DATA_SNAPSHOT_METHODS)

DataSnapshotInternal::DataSnapshotInternal(DatabaseInternal* db, jobject obj)
    : db_(db) {
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
}

DataSnapshotInternal::DataSnapshotInternal(const DataSnapshotInternal& src)
    : db_(src.db_) {
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_);
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    const DataSnapshotInternal& src) {
  db_ = src.db_;  // In case we are assigning to another db's Query.
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(src.obj_);
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
DataSnapshotInternal::DataSnapshotInternal(DataSnapshotInternal&& src)
    : db_(src.db_) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
}

DataSnapshotInternal& DataSnapshotInternal::operator=(
    DataSnapshotInternal&& src) {
  db_ = src.db_;
  obj_ = src.obj_;
  src.obj_ = nullptr;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

DataSnapshotInternal::~DataSnapshotInternal() {
  if (obj_ != nullptr) {
    util::GetJNIEnvFromApp()->DeleteGlobalRef(obj_);
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
  util::CheckAndClearJniExceptions(env);
}

bool DataSnapshotInternal::Exists() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jboolean result = env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kExists));
  util::CheckAndClearJniExceptions(env);
  return result;
}

DataSnapshotInternal* DataSnapshotInternal::Child(const char* path) const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kChild), path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(
          env, kLogLevelWarning,
          "DataSnapshot::Child(): Couldn't create child snapshot %s", path)) {
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
  // iterator = iterable.iterator()
  jobject iterator = env->CallObjectMethod(
      iterable, util::iterable::GetMethodId(util::iterable::kIterator));
  // while (iterator.hasNext())
  while (env->CallBooleanMethod(
      iterator, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    // DataSnapshot java_snapshot = iterator.next();
    jobject java_snapshot = env->CallObjectMethod(
        iterator, util::iterator::GetMethodId(util::iterator::kNext));
    result.push_back(
        DataSnapshot(new DataSnapshotInternal(db_, java_snapshot)));
    env->DeleteLocalRef(java_snapshot);
  }
  env->DeleteLocalRef(iterable);
  env->DeleteLocalRef(iterator);
  return result;
}

size_t DataSnapshotInternal::GetChildrenCount() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  return env->CallLongMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetChildrenCount));
}

bool DataSnapshotInternal::HasChildren() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  return env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kHasChildren));
}

const char* DataSnapshotInternal::GetKey() {
  if (cached_key_.is_null()) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, data_snapshot::GetMethodId(data_snapshot::kGetKey)));
    if (util::LogException(env, kLogLevelError,
                           "DataSnapshot::GetKey() failed")) {
      return nullptr;
    }
    const char* key = env->GetStringUTFChars(key_string, nullptr);
    // key_string and key will be null if this is a snapshot of the Root.
    cached_key_ = Variant::MutableStringFromStaticString(key ? key : "");
    env->ReleaseStringUTFChars(key_string, key);
    env->DeleteLocalRef(key_string);
  }
  return cached_key_.string_value();
}

std::string DataSnapshotInternal::GetKeyString() {
  (void)GetKey();
  return cached_key_.is_string() ? cached_key_.mutable_string() : "";
}

Variant DataSnapshotInternal::GetValue() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetValue));
  Variant v = internal::JavaObjectToVariant(env, value_obj);
  env->DeleteLocalRef(value_obj);
  return v;
}

Variant DataSnapshotInternal::GetPriority() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetPriority));
  Variant v = internal::JavaObjectToVariant(env, value_obj);
  env->DeleteLocalRef(value_obj);
  return v;
}

DatabaseReferenceInternal* DataSnapshotInternal::GetReference() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject database_reference_obj = env->CallObjectMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kGetRef));
  if (util::LogException(env, kLogLevelWarning,
                         "DataSnapshot::GetReference() failed")) {
    return nullptr;
  }
  DatabaseReferenceInternal* internal =
      new DatabaseReferenceInternal(db_, database_reference_obj);
  env->DeleteLocalRef(database_reference_obj);
  return internal;
}

bool DataSnapshotInternal::HasChild(const char* path) const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jboolean has_child = env->CallBooleanMethod(
      obj_, data_snapshot::GetMethodId(data_snapshot::kHasChild), path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(env, kLogLevelWarning,
                         "DataSnapshot::HasChild() failed")) {
    return false;
  }
  return has_child;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
