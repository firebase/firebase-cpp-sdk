// Copyright 2021 Google LLC
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

#include "storage/src/android/list_result_android.h"

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"  // For StorageReferenceInternal constructor
#include "storage/src/common/common_android.h"              // For METHOD_LOOKUP_DECLARATION/DEFINITION

namespace firebase {
namespace storage {
namespace internal {

// clang-format off
#define LIST_RESULT_METHODS(X)                                                 \
  X(GetItems, "getItems", "()Ljava/util/List;"),                               \
  X(GetPrefixes, "getPrefixes", "()Ljava/util/List;"),                         \
  X(GetPageToken, "getPageToken", "()Ljava/lang/String;")
// clang-format on
METHOD_LOOKUP_DECLARATION(list_result, LIST_RESULT_METHODS)
METHOD_LOOKUP_DEFINITION(list_result,
                         "com/google/firebase/storage/ListResult",
                         LIST_RESULT_METHODS)

// clang-format off
#define JAVA_LIST_METHODS(X)                                                   \
  X(Size, "size", "()I"),                                                      \
  X(Get, "get", "(I)Ljava/lang/Object;")
// clang-format on
METHOD_LOOKUP_DECLARATION(java_list, JAVA_LIST_METHODS)
METHOD_LOOKUP_DEFINITION(java_list, "java/util/List", JAVA_LIST_METHODS)

bool ListResultInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  if (!list_result::CacheMethodIds(env, app->activity())) {
    return false;
  }
  if (!java_list::CacheMethodIds(env, app->activity())) {
    // Release already cached list_result methods if java_list fails.
    list_result::ReleaseClass(env);
    return false;
  }
  return true;
}

void ListResultInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  list_result::ReleaseClass(env);
  java_list::ReleaseClass(env);
}

ListResultInternal::ListResultInternal(StorageInternal* storage_internal,
                                       jobject java_list_result)
    : storage_internal_(storage_internal), list_result_java_ref_(nullptr) {
  FIREBASE_ASSERT(storage_internal != nullptr);
  FIREBASE_ASSERT(java_list_result != nullptr);
  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  list_result_java_ref_ = env->NewGlobalRef(java_list_result);
}

ListResultInternal::ListResultInternal(const ListResultInternal& other)
    : storage_internal_(other.storage_internal_),
      list_result_java_ref_(nullptr) {
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  if (other.list_result_java_ref_ != nullptr) {
    list_result_java_ref_ = env->NewGlobalRef(other.list_result_java_ref_);
  }
}

ListResultInternal& ListResultInternal::operator=(
    const ListResultInternal& other) {
  if (&other == this) {
    return *this;
  }
  storage_internal_ = other.storage_internal_;  // This is a raw pointer, just copy.
  FIREBASE_ASSERT(storage_internal_ != nullptr);
  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  if (list_result_java_ref_ != nullptr) {
    env->DeleteGlobalRef(list_result_java_ref_);
    list_result_java_ref_ = nullptr;
  }
  if (other.list_result_java_ref_ != nullptr) {
    list_result_java_ref_ = env->NewGlobalRef(other.list_result_java_ref_);
  }
  return *this;
}

ListResultInternal::~ListResultInternal() {
  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  if (list_result_java_ref_ != nullptr) {
    env->DeleteGlobalRef(list_result_java_ref_);
    list_result_java_ref_ = nullptr;
  }
}

std::vector<StorageReference> ListResultInternal::ProcessJavaReferenceList(
    jobject java_list_ref) const {
  std::vector<StorageReference> cpp_references;
  if (java_list_ref == nullptr) {
    return cpp_references;
  }

  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  jint size = env->CallIntMethod(java_list_ref, java_list::GetMethodId(java_list::kSize));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LogError("Failed to get size of Java List in ListResultInternal");
    return cpp_references;
  }

  for (jint i = 0; i < size; ++i) {
    jobject java_storage_ref =
        env->CallObjectMethod(java_list_ref, java_list::GetMethodId(java_list::kGet), i);
    if (env->ExceptionCheck() || java_storage_ref == nullptr) {
      env->ExceptionClear();
      LogError("Failed to get StorageReference object from Java List at index %d", i);
      if (java_storage_ref) env->DeleteLocalRef(java_storage_ref);
      continue;
    }
    // Create a C++ StorageReferenceInternal from the Java StorageReference.
    // StorageReferenceInternal constructor will create a global ref for the java obj.
    StorageReferenceInternal* sfr_internal =
        new StorageReferenceInternal(storage_internal_, java_storage_ref);
    cpp_references.push_back(StorageReference(sfr_internal));
    env->DeleteLocalRef(java_storage_ref);
  }
  return cpp_references;
}

std::vector<StorageReference> ListResultInternal::items() const {
  if (!list_result_java_ref_) return {};

  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  jobject java_items_list = env->CallObjectMethod(
      list_result_java_ref_, list_result::GetMethodId(list_result::kGetItems));
  if (env->ExceptionCheck() || java_items_list == nullptr) {
    env->ExceptionClear();
    LogError("Failed to call getItems() on Java ListResult");
    if (java_items_list) env->DeleteLocalRef(java_items_list);
    return {};
  }

  std::vector<StorageReference> items_vector =
      ProcessJavaReferenceList(java_items_list);
  env->DeleteLocalRef(java_items_list);
  return items_vector;
}

std::vector<StorageReference> ListResultInternal::prefixes() const {
  if (!list_result_java_ref_) return {};

  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  jobject java_prefixes_list = env->CallObjectMethod(
      list_result_java_ref_, list_result::GetMethodId(list_result::kGetPrefixes));
  if (env->ExceptionCheck() || java_prefixes_list == nullptr) {
    env->ExceptionClear();
    LogError("Failed to call getPrefixes() on Java ListResult");
    if (java_prefixes_list) env->DeleteLocalRef(java_prefixes_list);
    return {};
  }

  std::vector<StorageReference> prefixes_vector =
      ProcessJavaReferenceList(java_prefixes_list);
  env->DeleteLocalRef(java_prefixes_list);
  return prefixes_vector;
}

std::string ListResultInternal::page_token() const {
  if (!list_result_java_ref_) return "";

  JNIEnv* env = storage_internal_->app()->GetJNIEnv();
  jstring page_token_jstring = static_cast<jstring>(env->CallObjectMethod(
      list_result_java_ref_, list_result::GetMethodId(list_result::kGetPageToken)));
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
    LogError("Failed to call getPageToken() on Java ListResult");
    if (page_token_jstring) env->DeleteLocalRef(page_token_jstring);
    return "";
  }

  std::string page_token_std_string = util::JniStringToString(env, page_token_jstring);
  if (page_token_jstring) env->DeleteLocalRef(page_token_jstring);
  return page_token_std_string;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
