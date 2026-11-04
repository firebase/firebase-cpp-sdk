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

#include "app/src/app_android.h"
#include "app/src/jni/env.h"
#include "app/src/jni/ownership.h"
#include "database/src/android/database_android.h"
#include "app/src/app_android.h"
#include "app/src/jni/env.h"
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

namespace mutable_data {

enum Method {
  kHasChildren,
  kHasChild,
  kChild,
  kGetChildrenCount,
  kGetChildren,
  kGetKey,
  kGetValue,
  kSetValue,
  kSetPriority,
  kGetPriority,
  kEquals,
  kToString,
  kMethodCount
};

enum Field { kFieldCount = 0 };

::firebase::jni::Global<jclass> g_class;
jmethodID g_method_ids[kMethodCount];

jclass GetClass() { return g_class.get(); }
jmethodID GetMethodId(Method method) {
  return g_method_ids[method];
}

bool CacheMethodIds(JNIEnv* env, jobject activity) {
  if (g_class) return true;
  ::firebase::jni::Local<::firebase::jni::Class> local_clazz(env, env->FindClass("com/google/firebase/database/MutableData"));
  if (!local_clazz) {
    ::firebase::jni::CheckAndClearException(env);
    return false;
  }
  g_class = local_clazz;
  g_method_ids[kHasChildren] = env->GetMethodID(g_class.get(), "hasChildren", "()Z");
  if (!g_method_ids[kHasChildren]) return false;
  g_method_ids[kHasChild] = env->GetMethodID(g_class.get(), "hasChild", "(Ljava/lang/String;)Z");
  if (!g_method_ids[kHasChild]) return false;
  g_method_ids[kChild] = env->GetMethodID(g_class.get(), "child", "(Ljava/lang/String;)Lcom/google/firebase/database/MutableData;");
  if (!g_method_ids[kChild]) return false;
  g_method_ids[kGetChildrenCount] = env->GetMethodID(g_class.get(), "getChildrenCount", "()J");
  if (!g_method_ids[kGetChildrenCount]) return false;
  g_method_ids[kGetChildren] = env->GetMethodID(g_class.get(), "getChildren", "()Ljava/lang/Iterable;");
  if (!g_method_ids[kGetChildren]) return false;
  g_method_ids[kGetKey] = env->GetMethodID(g_class.get(), "getKey", "()Ljava/lang/String;");
  if (!g_method_ids[kGetKey]) return false;
  g_method_ids[kGetValue] = env->GetMethodID(g_class.get(), "getValue", "()Ljava/lang/Object;");
  if (!g_method_ids[kGetValue]) return false;
  g_method_ids[kSetValue] = env->GetMethodID(g_class.get(), "setValue", "(Ljava/lang/Object;)V");
  if (!g_method_ids[kSetValue]) return false;
  g_method_ids[kSetPriority] = env->GetMethodID(g_class.get(), "setPriority", "(Ljava/lang/Object;)V");
  if (!g_method_ids[kSetPriority]) return false;
  g_method_ids[kGetPriority] = env->GetMethodID(g_class.get(), "getPriority", "()Ljava/lang/Object;");
  if (!g_method_ids[kGetPriority]) return false;
  g_method_ids[kEquals] = env->GetMethodID(g_class.get(), "equals", "(Ljava/lang/Object;)Z");
  if (!g_method_ids[kEquals]) return false;
  g_method_ids[kToString] = env->GetMethodID(g_class.get(), "toString", "()Ljava/lang/String;");
  if (!g_method_ids[kToString]) return false;
  return true;
}

void ReleaseClass(JNIEnv* env) {
  g_class.reset();
}

}  // namespace mutable_data



MutableDataInternal::MutableDataInternal(DatabaseInternal* db, jobject obj)
    : db_(db) {
  obj_ = obj ? db_->GetApp()->GetJNIEnv()->NewGlobalRef(obj) : nullptr;
}

MutableDataInternal::~MutableDataInternal() {
  if (obj_ != nullptr) {
    db_->GetApp()->GetJNIEnv()->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
}

bool MutableDataInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return mutable_data::CacheMethodIds(env, activity);
}

void MutableDataInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  mutable_data::ReleaseClass(env);
  ::firebase::jni::CheckAndClearException(env);
}

MutableDataInternal* MutableDataInternal::Child(const char* path) {
  if (path == nullptr) return nullptr;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return nullptr;
  }
  jobject child_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kChild), path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::util::LogException(
          env, kLogLevelWarning,
          "MutableData::Child(): Couldn't create child reference %s", path) ||
      child_obj == nullptr) {
    if (child_obj != nullptr) env->DeleteLocalRef(child_obj);
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
  if (::firebase::jni::CheckAndClearException(env) || iterable == nullptr) {
    if (iterable != nullptr) env->DeleteLocalRef(iterable);
    return result;
  }
  // iterator = iterable.iterator()
  jobject iterator = env->CallObjectMethod(
      iterable, env->GetMethodID(env->GetObjectClass(iterable), "iterator", "()Ljava/util/Iterator;"));
  if (::firebase::jni::CheckAndClearException(env) || iterator == nullptr) {
    if (iterator != nullptr) env->DeleteLocalRef(iterator);
    env->DeleteLocalRef(iterable);
    return result;
  }
  // while (iterator.hasNext())
  while (true) {
    jboolean has_next = env->CallBooleanMethod(
        iterator, env->GetMethodID(env->GetObjectClass(iterator), "hasNext", "()Z"));
    if (::firebase::jni::CheckAndClearException(env) || !has_next) {
      break;
    }
    // MutableData java_mutable_data = iterator.next();
    jobject java_mutable_data = env->CallObjectMethod(
        iterator, env->GetMethodID(env->GetObjectClass(iterator), "next", "()Ljava/lang/Object;"));
    if (::firebase::jni::CheckAndClearException(env) || java_mutable_data == nullptr) {
      if (java_mutable_data != nullptr) env->DeleteLocalRef(java_mutable_data);
      break;
    }
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
  jlong count = env->CallLongMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetChildrenCount));
  if (::firebase::jni::CheckAndClearException(env) || count < 0) {
    return 0;
  }
  return static_cast<size_t>(count);
}

const char* MutableDataInternal::GetKey() {
  MutexLock lock(key_mutex_);
  if (cached_key_.is_null()) {
    JNIEnv* env = db_->GetApp()->GetJNIEnv();
    jstring key_string = static_cast<jstring>(env->CallObjectMethod(
        obj_, mutable_data::GetMethodId(mutable_data::kGetKey)));
    if (::firebase::util::LogException(env, kLogLevelError,
                           "MutableData::GetKey() failed")) {
      if (key_string != nullptr) env->DeleteLocalRef(key_string);
      return nullptr;
    }
    if (key_string == nullptr) {
      // For the root MutableData, the Key will be null in Java; return "".
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

std::string MutableDataInternal::GetKeyString() {
  MutexLock lock(key_mutex_);
  const char* key = GetKey();
  return key != nullptr ? std::string(key) : std::string();
}

Variant MutableDataInternal::GetValue() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetValue));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
    return Variant::Null();
  }
  Variant v = ::firebase::util::JavaObjectToVariant(env, value_obj);
  if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
  return v;
}

Variant MutableDataInternal::GetPriority() const {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_obj = env->CallObjectMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kGetPriority));
  if (::firebase::jni::CheckAndClearException(env)) {
    if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
    return Variant::Null();
  }
  Variant v = ::firebase::util::JavaObjectToVariant(env, value_obj);
  if (value_obj != nullptr) env->DeleteLocalRef(value_obj);
  return v;
}

bool MutableDataInternal::HasChild(const char* path) const {
  if (path == nullptr) return false;
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jstring path_string = env->NewStringUTF(path);
  if (::firebase::jni::CheckAndClearException(env) || path_string == nullptr) {
    if (path_string != nullptr) env->DeleteLocalRef(path_string);
    return false;
  }
  jboolean has_child = env->CallBooleanMethod(
      obj_, mutable_data::GetMethodId(mutable_data::kHasChild), path_string);
  env->DeleteLocalRef(path_string);
  if (::firebase::util::LogException(env, kLogLevelWarning,
                         "MutableData::HasChild() failed")) {
    return false;
  }
  return has_child;
}

void MutableDataInternal::SetValue(const Variant& value) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  jobject value_java = ::firebase::util::VariantToJavaObject(env, value);
  if (!value.is_null() && value_java == nullptr) {
    db_->logger()->LogError("MutableData::SetValue() failed: invalid Variant");
    return;
  }
  env->CallVoidMethod(obj_, mutable_data::GetMethodId(mutable_data::kSetValue),
                      value_java);
  ::firebase::util::LogException(env, kLogLevelError, "MutableData::SetValue() failed");
  if (value_java != nullptr) env->DeleteLocalRef(value_java);
}

void MutableDataInternal::SetPriority(const Variant& priority) {
  JNIEnv* env = db_->GetApp()->GetJNIEnv();
  if (!IsValidPriority(priority)) {
    db_->logger()->LogError(
        "MutableData::SetPriority(): Invalid Variant type given for priority. "
        "Container types (Vector/Map) are not allowed.");
  } else {
    jobject priority_java = ::firebase::util::VariantToJavaObject(env, priority);
    if (!priority.is_null() && priority_java == nullptr) {
      db_->logger()->LogError(
          "MutableData::SetPriority() failed: invalid Variant");
      return;
    }
    env->CallVoidMethod(obj_,
                        mutable_data::GetMethodId(mutable_data::kSetPriority),
                        priority_java);
    ::firebase::util::LogException(env, kLogLevelError,
                       "MutableData::SetPriority() failed");
    if (priority_java != nullptr) env->DeleteLocalRef(priority_java);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
