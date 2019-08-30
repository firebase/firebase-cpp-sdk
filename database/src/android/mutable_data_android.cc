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

#include "database/src/android/mutable_data_android.h"

#include <jni.h>

#include "app/src/util_android.h"
#include "database/src/android/database_android.h"
#include "database/src/android/util_android.h"
#include "database/src/common/database_reference.h"
#include "database/src/include/firebase/database/mutable_data.h"

namespace firebase {
namespace database {
namespace internal {

// clang-format off
#define MUTABLE_DATA_METHODS(X)                                                \
  X(HasChildren, "hasChildren", "()Z"),                                        \
  X(HasChild, "hasChild", "(Ljava/lang/String;)Z"),                            \
  X(Child, "child",                                                            \
    "(Ljava/lang/String;)Lcom/google/firebase/database/MutableData;"),         \
  X(GetChildrenCount, "getChildrenCount", "()J"),                              \
  X(GetChildren, "getChildren", "()Ljava/lang/Iterable;"),                     \
  X(GetKey, "getKey", "()Ljava/lang/String;"),                                 \
  X(GetValue, "getValue", "()Ljava/lang/Object;"),                             \
  X(SetValue, "setValue", "(Ljava/lang/Object;)V"),                            \
  X(SetPriority, "setPriority", "(Ljava/lang/Object;)V"),                      \
  X(GetPriority, "getPriority", "()Ljava/lang/Object;"),                       \
  X(Equals, "equals", "(Ljava/lang/Object;)Z"),                                \
  X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(mutable_data, MUTABLE_DATA_METHODS)
METHOD_LOOKUP_DEFINITION(mutable_data,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/database/MutableData",
                         MUTABLE_DATA_METHODS)

MutableDataInternal::MutableDataInternal(DatabaseInternal* db, jobject obj)
    : db_(db) {
  obj_ = db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj);
}

MutableDataInternal::~MutableDataInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

MutableDataInternal* MutableDataInternal::Clone() {
  return new MutableDataInternal(db_, obj_);
}

bool MutableDataInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return mutable_data::CacheMethodIds(env, activity);
}

void MutableDataInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  mutable_data::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

MutableDataInternal* MutableDataInternal::Child(const char* path) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jobject child_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kChild), path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(
          env, kLogLevelWarning,
          "MutableData::Child(): Couldn't create child reference %s", path)) {
    return nullptr;
  }
  MutableDataInternal* internal = new MutableDataInternal(db_, child_obj);
  env->DeleteLocalRef(child_obj);
  return internal;
}

std::vector<MutableData> MutableDataInternal::GetChildren() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  std::vector<MutableData> result;
  result.reserve(GetChildrenCount());
  // iterable = mutable_data.getChildren()
  jobject iterable = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetChildren));
  // iterator = iterable.iterator()
  jobject iterator = env->CallObjectMethod(
      iterable, util::iterable::GetMethodId(util::iterable::kIterator));
  // while (iterator.hasNext())
  while (env->CallBooleanMethod(
      iterator, util::iterator::GetMethodId(util::iterator::kHasNext))) {
    // MutableData java_mutable_data = iterator.next();
    jobject java_mutable_data = env->CallObjectMethod(
        iterator, util::iterator::GetMethodId(util::iterator::kNext));
    result.push_back(
        MutableData(new MutableDataInternal(db_, java_mutable_data)));
    env->DeleteLocalRef(java_mutable_data);
  }
  env->DeleteLocalRef(iterable);
  env->DeleteLocalRef(iterator);
  return result;
}

size_t MutableDataInternal::GetChildrenCount() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  return env->CallLongMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetChildrenCount));
}

const char* MutableDataInternal::GetKey() {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  if (cached_key_.is_null()) {
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, mutable_data::GetMethodId(mutable_data::kGetKey)));
    if (util::LogException(env, kLogLevelError,
                           "MutableData::GetKey() failed")) {
      return nullptr;
    }
    if (key_string == nullptr) {
      // For the root MutableData, the Key will be null
      return nullptr;
    }
    const char* key = env->GetStringUTFChars(key_string, nullptr);
    cached_key_ = Variant::MutableStringFromStaticString(key);
    env->ReleaseStringUTFChars(key_string, key);
    env->DeleteLocalRef(key_string);
  }
  return cached_key_.string_value();
}

std::string MutableDataInternal::GetKeyString() {
  (void)GetKey();
  return cached_key_.is_string() ? cached_key_.mutable_string() : "";
}

Variant MutableDataInternal::GetValue() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetValue));
  Variant v = internal::JavaObjectToVariant(env, value_obj);
  env->DeleteLocalRef(value_obj);
  return v;
}

Variant MutableDataInternal::GetPriority() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetPriority));
  Variant v = internal::JavaObjectToVariant(env, value_obj);
  env->DeleteLocalRef(value_obj);
  return v;
}

bool MutableDataInternal::HasChild(const char* path) const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  jboolean has_child = env->CallBooleanMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kHasChild), path_string);
  env->DeleteLocalRef(path_string);
  if (util::LogException(env, kLogLevelWarning,
                         "MutableData::HasChild() failed")) {
    return false;
  }
  return has_child;
}

void MutableDataInternal::SetValue(Variant value) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_java = internal::VariantToJavaObject(env, value);
  env->CallVoidMethod(obj_, mutable_data::GetMethodId(mutable_data::kSetValue),
                      value_java);
  util::LogException(env, kLogLevelError, "MutableData::SetValue() failed");
  env->DeleteLocalRef(value_java);
}

void MutableDataInternal::SetPriority(Variant priority) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  if (!IsValidPriority(priority)) {
    db_->logger()->LogError(
        "MutableData::SetPriority(): Invalid Variant type given for priority. "
        "Container types (Vector/Map) are not allowed.");
  } else {
    jobject priority_java = internal::VariantToJavaObject(env, priority);
    env->CallVoidMethod(obj_,
                        mutable_data::GetMethodId(mutable_data::kSetPriority),
                        priority_java);
    util::LogException(env, kLogLevelError,
                       "MutableData::SetPriority() failed");
    env->DeleteLocalRef(priority_java);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
