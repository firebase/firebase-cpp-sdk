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

#include "storage/src/android/metadata_android.h"

#include <jni.h>

#include <cstdint>
#include <cstdlib>
#include <utility>

#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

namespace {

// Deep-copies the string cache. Each cached string is heap-allocated and
// owned by a std::unique_ptr, so moving the cache (or retiring an entry) moves
// only the pointer and never relocates the characters: previously returned
// c_str() pointers stay valid by construction, with no reliance on
// std::string move/SSO behavior.
template <size_t N>
std::array<std::unique_ptr<std::string>, N> CopyStringCache(
    const std::array<std::unique_ptr<std::string>, N>& src) {
  std::array<std::unique_ptr<std::string>, N> dest{};
  for (size_t i = 0; i < src.size(); ++i) {
    if (src[i]) {
      dest[i] = std::make_unique<std::string>(*src[i]);
    }
  }
  return dest;
}

}  // namespace

// Java methods declared in the header file.
METHOD_LOOKUP_DEFINITION(storage_metadata,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageMetadata",
                         STORAGE_METADATA_METHODS)

METHOD_LOOKUP_DEFINITION(storage_metadata_builder,
                         PROGUARD_KEEP_CLASS
                         "com/google/firebase/storage/StorageMetadata$Builder",
                         STORAGE_METADATA_BUILDER_METHODS)

MetadataInternal::MetadataInternal(StorageInternal* storage, jobject obj)
    : storage_(storage),
      java_vm_((storage && storage->app()) ? storage->app()->java_vm()
                                           : nullptr),
      obj_(nullptr) {
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  if (obj == nullptr) {
    jobject builder =
        env->NewObject(storage_metadata_builder::GetClass(),
                       storage_metadata_builder::GetMethodId(
                           storage_metadata_builder::kConstructor));
    if (!util::CheckAndClearJniExceptions(env) && builder) {
      CommitBuilder(env, builder);
    } else if (builder) {
      env->DeleteLocalRef(builder);
    }
  } else {
    obj_ = env->NewGlobalRef(obj);
    // Cache read-only properties that are lost when constructing a
    // StorageMetadata object from a Builder.
    bucket();
    name();
    path();
    md5_hash();
    size_bytes();
    updated_time();
    creation_time();
    generation();
    metadata_generation();
  }
}

MetadataInternal::MetadataInternal(StorageInternal* storage)
    : MetadataInternal(storage, nullptr) {}

MetadataInternal::~MetadataInternal() {
  MutexLock lock(mutex_);
  if (obj_ != nullptr) {
    JNIEnv* env = GetJNIEnv();
    if (env) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  custom_metadata_.reset();
  for (auto& entry : cache_) {
    entry.reset();
  }
  retired_strings_.clear();
}

// Lifetime note: every MetadataInternal owned by a public Metadata is
// registered with its StorageInternal's CleanupNotifier (common/metadata.cc)
// and deleted when that StorageInternal is destroyed, so storage_ never
// dangles while this object exists.
JNIEnv* MetadataInternal::GetJNIEnv() const {
  // Use the cached JavaVM: GetThreadsafeJNIEnv() attaches the calling thread
  // if needed (so global refs are not leaked when destroyed on a native
  // thread) and does not dereference storage_.
  if (java_vm_ != nullptr) return util::GetThreadsafeJNIEnv(java_vm_);
  return util::GetJNIEnvFromApp();
}

jobject MetadataInternal::NewLocalObj(JNIEnv* env) const {
  MutexLock lock(mutex_);
  if (!env || !obj_) return nullptr;
  return env->NewLocalRef(obj_);
}

MetadataInternal::MetadataInternal(const MetadataInternal& src)
    : storage_(nullptr), obj_(nullptr) {
  MutexLock src_lock(src.mutex_);
  storage_ = src.storage_;
  java_vm_ = src.java_vm_;
  JNIEnv* env = GetJNIEnv();
  if (env && src.obj_) {
    CopyJavaMetadataObject(env, src.obj_);
  }

  if (src.custom_metadata_) {
    custom_metadata_ = std::make_unique<CustomMetadata>(*src.custom_metadata_);
  }
  cache_ = CopyStringCache(src.cache_);
  constants_ = src.constants_;
}

MetadataInternal& MetadataInternal::operator=(const MetadataInternal& src) {
  if (this == &src) {
    return *this;
  }
  StorageInternal* src_storage = nullptr;
  jobject src_obj_local = nullptr;
  std::unique_ptr<CustomMetadata> src_custom_metadata;
  std::array<std::unique_ptr<std::string>, kCacheStringCount> src_cache{};
  Constants src_constants;
  JNIEnv* src_env = nullptr;
  {
    MutexLock src_lock(src.mutex_);
    src_storage = src.storage_;
    src_env = src.GetJNIEnv();
    if (src_env && src.obj_) {
      src_obj_local = src_env->NewLocalRef(src.obj_);
    }
    if (src.custom_metadata_) {
      src_custom_metadata =
          std::make_unique<CustomMetadata>(*src.custom_metadata_);
    }
    src_cache = CopyStringCache(src.cache_);
    src_constants = src.constants_;
  }

  MutexLock lock(mutex_);
  JNIEnv* env = GetJNIEnv();
  if (obj_ != nullptr && env != nullptr) {
    // If there's already a Java object in the dest, delete it.
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  storage_ = src_storage;
  java_vm_ = (storage_ && storage_->app()) ? storage_->app()->java_vm()
                                          : nullptr;
  env = GetJNIEnv();
  if (env && src_obj_local) {
    CopyJavaMetadataObject(env, src_obj_local);
  }
  if (src_obj_local && src_env) {
    src_env->DeleteLocalRef(src_obj_local);
  }

  custom_metadata_ = std::move(src_custom_metadata);
  for (auto& entry : cache_) {
    if (entry) {
      retired_strings_.push_back(std::move(entry));
    }
  }
  cache_ = std::move(src_cache);
  constants_ = src_constants;
  return *this;
}

MetadataInternal::MetadataInternal(MetadataInternal&& src) noexcept
    : storage_(nullptr), obj_(nullptr) {
  MutexLock src_lock(src.mutex_);
  storage_ = src.storage_;
  java_vm_ = src.java_vm_;
  obj_ = src.obj_;
  custom_metadata_ = std::move(src.custom_metadata_);
  src.obj_ = nullptr;
  // Move the cache to its new owner, so the const char*'s are still valid.
  cache_ = std::move(src.cache_);
  for (auto& entry : src.cache_) {
    entry.reset();
  }
  retired_strings_ = std::move(src.retired_strings_);
  constants_ = src.constants_;
}

MetadataInternal& MetadataInternal::operator=(MetadataInternal&& src) {
  if (this == &src) {
    return *this;
  }
  StorageInternal* src_storage = nullptr;
  jobject src_obj = nullptr;
  std::unique_ptr<CustomMetadata> src_custom_metadata;
  std::array<std::unique_ptr<std::string>, kCacheStringCount> src_cache{};
  std::vector<std::unique_ptr<std::string>> src_retired_strings;
  Constants src_constants;
  {
    MutexLock src_lock(src.mutex_);
    src_storage = src.storage_;
    src_obj = src.obj_;
    src.obj_ = nullptr;
    src_custom_metadata = std::move(src.custom_metadata_);
    src_cache = std::move(src.cache_);
    for (auto& entry : src.cache_) {
      entry.reset();
    }
    src_retired_strings = std::move(src.retired_strings_);
    src_constants = src.constants_;
  }

  MutexLock lock(mutex_);
  if (obj_ != nullptr) {
    JNIEnv* env = GetJNIEnv();
    if (env) {
      env->DeleteGlobalRef(obj_);
    }
    obj_ = nullptr;
  }
  storage_ = src_storage;
  java_vm_ = (storage_ && storage_->app()) ? storage_->app()->java_vm()
                                          : nullptr;
  obj_ = src_obj;
  custom_metadata_ = std::move(src_custom_metadata);
  for (auto& entry : cache_) {
    if (entry) {
      retired_strings_.push_back(std::move(entry));
    }
  }
  for (auto& retired : src_retired_strings) {
    retired_strings_.push_back(std::move(retired));
  }
  // Move the cache to its new owner, us.
  cache_ = std::move(src_cache);
  constants_ = src_constants;
  return *this;
}

void MetadataInternal::CopyJavaMetadataObject(JNIEnv* env, jobject src_obj) {
  if (!env || !src_obj) return;
  // Use StorageMetadata.Builder to create a copy of the existing object.
  jobject builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     src_obj);
  if (util::CheckAndClearJniExceptions(env) || !builder) {
    if (builder) env->DeleteLocalRef(builder);
    return;
  }
  CommitBuilder(env, builder);
}

bool MetadataInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  if (storage_metadata::CacheMethodIds(env, activity) &&
      storage_metadata_builder::CacheMethodIds(env, activity)) {
    return true;
  }
  Terminate(app);
  return false;
}

void MetadataInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_metadata_builder::ReleaseClass(env);
  storage_metadata::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

const char* MetadataInternal::bucket() const {
  return GetStringProperty(storage_metadata::kGetBucket, kCacheStringBucket);
}

void MetadataInternal::CommitBuilder(JNIEnv* env, jobject builder) {
  if (!builder) return;
  if (!env) {
    env = GetJNIEnv();
  }
  if (!env) return;
  jobject new_metadata = env->CallObjectMethod(
      builder,
      storage_metadata_builder::GetMethodId(storage_metadata_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (util::CheckAndClearJniExceptions(env) || !new_metadata) {
    if (new_metadata) env->DeleteLocalRef(new_metadata);
    return;
  }
  if (obj_ != nullptr) env->DeleteGlobalRef(obj_);
  obj_ = env->NewGlobalRef(new_metadata);
  env->DeleteLocalRef(new_metadata);
}

void MetadataInternal::set_cache_control(const char* cache_control) {
  SetStringProperty(cache_control, storage_metadata_builder::kSetCacheControl,
                    kCacheStringCacheControl);
}

const char* MetadataInternal::cache_control() const {
  return GetStringProperty(storage_metadata::kGetCacheControl,
                           kCacheStringCacheControl);
}

void MetadataInternal::set_content_disposition(const char* disposition) {
  SetStringProperty(disposition,
                    storage_metadata_builder::kSetContentDisposition,
                    kCacheStringContentDisposition);
}

const char* MetadataInternal::content_disposition() const {
  return GetStringProperty(storage_metadata::kGetContentDisposition,
                           kCacheStringContentDisposition);
}

void MetadataInternal::set_content_encoding(const char* encoding) {
  SetStringProperty(encoding, storage_metadata_builder::kSetContentEncoding,
                    kCacheStringContentEncoding);
}

const char* MetadataInternal::content_encoding() const {
  return GetStringProperty(storage_metadata::kGetContentEncoding,
                           kCacheStringContentEncoding);
}

void MetadataInternal::set_content_language(const char* language) {
  SetStringProperty(language, storage_metadata_builder::kSetContentLanguage,
                    kCacheStringContentLanguage);
}

const char* MetadataInternal::content_language() const {
  return GetStringProperty(storage_metadata::kGetContentLanguage,
                           kCacheStringContentLanguage);
}

void MetadataInternal::set_content_type(const char* type) {
  SetStringProperty(type, storage_metadata_builder::kSetContentType,
                    kCacheStringContentType);
}

const char* MetadataInternal::content_type() const {
  return GetStringProperty(storage_metadata::kGetContentType,
                           kCacheStringContentType);
}

int64_t MetadataInternal::creation_time() const {
  return GetInt64Property(storage_metadata::kGetCreationTimeMillis,
                          &constants_.creation_time);
}

std::map<std::string, std::string>* MetadataInternal::custom_metadata() {
  MutexLock lock(mutex_);
  if (!custom_metadata_) {
    custom_metadata_ = std::make_unique<CustomMetadata>();
    ReadCustomMetadata(custom_metadata_.get());
  }
  return custom_metadata_.get();
}

void MetadataInternal::CommitCustomMetadata() {
  MutexLock lock(mutex_);
  if (!obj_ || !custom_metadata_) return;
  std::map<std::string, std::string> old_metadata;
  ReadCustomMetadata(&old_metadata);
  // Set all new values, and if any old values are not present in new, then
  // clear them.
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  jobject builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     obj_);
  if (util::CheckAndClearJniExceptions(env) || !builder) {
    if (builder) env->DeleteLocalRef(builder);
    return;
  }
  for (auto i = custom_metadata_->begin(); i != custom_metadata_->end(); ++i) {
    old_metadata.erase(i->first);  // Erase any key we see in the new metadata.
    // Any left over have been erased.
    jobject key_string = env->NewStringUTF(i->first.c_str());
    jobject value_string = env->NewStringUTF(i->second.c_str());
    jobject new_builder =
        env->CallObjectMethod(builder,
                              storage_metadata_builder::GetMethodId(
                                  storage_metadata_builder::kSetCustomMetadata),
                              key_string, value_string);
    env->DeleteLocalRef(value_string);
    env->DeleteLocalRef(key_string);
    if (util::CheckAndClearJniExceptions(env) || !new_builder) {
      if (new_builder) env->DeleteLocalRef(new_builder);
      env->DeleteLocalRef(builder);
      return;
    }
    env->DeleteLocalRef(builder);
    builder = new_builder;
  }
  // If any keys are not present in the new data, override with blank values.
  jobject empty_string = env->NewStringUTF("");
  for (auto i = old_metadata.begin(); i != old_metadata.end(); ++i) {
    jobject key_string = env->NewStringUTF(i->first.c_str());
    jobject new_builder =
        env->CallObjectMethod(builder,
                              storage_metadata_builder::GetMethodId(
                                  storage_metadata_builder::kSetCustomMetadata),
                              key_string, empty_string);
    env->DeleteLocalRef(key_string);
    if (util::CheckAndClearJniExceptions(env) || !new_builder) {
      if (new_builder) env->DeleteLocalRef(new_builder);
      env->DeleteLocalRef(empty_string);
      env->DeleteLocalRef(builder);
      return;
    }
    env->DeleteLocalRef(builder);
    builder = new_builder;
  }
  env->DeleteLocalRef(empty_string);
  CommitBuilder(env, builder);
}

void MetadataInternal::ReadCustomMetadata(
    std::map<std::string, std::string>* output_map) const {
  if (!obj_ || !output_map) return;
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  jobject key_set = env->CallObjectMethod(
      obj_,
      storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadataKeys));
  if (util::CheckAndClearJniExceptions(env) || !key_set) {
    if (key_set) env->DeleteLocalRef(key_set);
    return;
  }
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set,
      firebase::util::set::GetMethodId(firebase::util::set::kIterator));
  if (util::CheckAndClearJniExceptions(env) || !iter) {
    if (iter) env->DeleteLocalRef(iter);
    env->DeleteLocalRef(key_set);
    return;
  }
  // while (iter.hasNext())
  while (
      env->CallBooleanMethod(iter, firebase::util::iterator::GetMethodId(
                                       firebase::util::iterator::kHasNext))) {
    if (util::CheckAndClearJniExceptions(env)) break;
    // String key = iter.next();
    jobject key_object = env->CallObjectMethod(
        iter,
        firebase::util::iterator::GetMethodId(firebase::util::iterator::kNext));
    if (util::CheckAndClearJniExceptions(env) || !key_object) {
      if (key_object) env->DeleteLocalRef(key_object);
      break;
    }
    // String value = obj_.GetCustomMetadata(key);
    jobject value_object = env->CallObjectMethod(
        obj_,
        storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadata),
        key_object);
    if (util::CheckAndClearJniExceptions(env)) {
      if (value_object) env->DeleteLocalRef(value_object);
      env->DeleteLocalRef(key_object);
      break;
    }
    std::string key = util::JniStringToString(env, key_object);
    std::string value =
        value_object ? util::JniStringToString(env, value_object) : "";
    output_map->insert(std::pair<std::string, std::string>(key, value));
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

int64_t MetadataInternal::generation() const {
  MutexLock lock(mutex_);
  if (!obj_ && !cache_[kCacheStringGeneration]) return -1;
  const char* str = GetStringProperty(storage_metadata::kGetGeneration,
                                      kCacheStringGeneration);
  if (!str || str[0] == '\0') return 0;
  char* end = nullptr;
  int64_t output = std::strtoll(str, &end, 10);  // NOLINT
  if (end == str) return 0;
  return output;
}

int64_t MetadataInternal::metadata_generation() const {
  MutexLock lock(mutex_);
  if (!obj_ && !cache_[kCacheStringMetadataGeneration]) return -1;
  const char* str = GetStringProperty(storage_metadata::kGetMetadataGeneration,
                                      kCacheStringMetadataGeneration);
  if (!str || str[0] == '\0') return 0;
  char* end = nullptr;
  int64_t output = std::strtoll(str, &end, 10);  // NOLINT
  if (end == str) return 0;
  return output;
}

const char* MetadataInternal::name() const {
  return GetStringProperty(storage_metadata::kGetName, kCacheStringName);
}

const char* MetadataInternal::path() const {
  return GetStringProperty(storage_metadata::kGetPath, kCacheStringPath);
}

StorageReferenceInternal* MetadataInternal::GetReference() const {
  MutexLock lock(mutex_);
  // If we don't have an associated Storage, we are not assigned to a reference.
  if (storage_ == nullptr || obj_ == nullptr) return nullptr;
  JNIEnv* env = GetJNIEnv();
  if (!env) return nullptr;
  jobject ref_obj = env->CallObjectMethod(
      obj_, storage_metadata::GetMethodId(storage_metadata::kGetReference));
  if (util::CheckAndClearJniExceptions(env) || !ref_obj) {
    if (ref_obj) env->DeleteLocalRef(ref_obj);
    return nullptr;
  }
  StorageReferenceInternal* new_ref =
      new StorageReferenceInternal(storage_, ref_obj);
  env->DeleteLocalRef(ref_obj);
  return new_ref;
}

int64_t MetadataInternal::size_bytes() const {
  return GetInt64Property(storage_metadata::kGetSizeBytes,
                          &constants_.size_bytes);
}

int64_t MetadataInternal::updated_time() const {
  return GetInt64Property(storage_metadata::kGetUpdatedTimeMillis,
                          &constants_.updated_time);
}

// Get the MD5 hash of the blob referenced by StorageReference.
const char* MetadataInternal::md5_hash() const {
  return GetStringProperty(storage_metadata::kGetMd5Hash, kCacheStringMd5Hash);
}

const char* MetadataInternal::GetStringProperty(
    storage_metadata::Method string_method, CacheString cache_string) const {
  MutexLock lock(mutex_);
  std::unique_ptr<std::string>& cached_string = cache_[cache_string];
  if (!cached_string) {
    if (!obj_) {
      cached_string = std::make_unique<std::string>("");
      return cached_string->c_str();
    }
    JNIEnv* env = GetJNIEnv();
    if (!env) {
      cached_string = std::make_unique<std::string>("");
      return cached_string->c_str();
    }
    jobject str = env->CallObjectMethod(
        obj_, storage_metadata::GetMethodId(string_method));
    if (util::CheckAndClearJniExceptions(env) || !str) {
      if (str) env->DeleteLocalRef(str);
      cached_string = std::make_unique<std::string>("");
      return cached_string->c_str();
    }
    cached_string = std::make_unique<std::string>(util::JniStringToString(env, str));
  }
  return cached_string->c_str();
}

void MetadataInternal::SetStringProperty(
    const char* string_value, storage_metadata_builder::Method builder_method,
    CacheString cache_string) {
  MutexLock lock(mutex_);
  if (!obj_) return;
  std::string value_copy = string_value ? string_value : "";
  std::unique_ptr<std::string>& cached_string = cache_[cache_string];
  if (cached_string) {
    // Keep the old buffer alive: callers may still hold its c_str().
    retired_strings_.push_back(std::move(cached_string));
  }
  JNIEnv* env = GetJNIEnv();
  if (!env) return;
  jobject base_builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     obj_);
  if (util::CheckAndClearJniExceptions(env) || !base_builder) {
    if (base_builder) env->DeleteLocalRef(base_builder);
    return;
  }
  jobject java_string = env->NewStringUTF(value_copy.c_str());
  jobject updated_builder = env->CallObjectMethod(
      base_builder, storage_metadata_builder::GetMethodId(builder_method),
      java_string);
  bool commit_builder =
      !util::CheckAndClearJniExceptions(env) && updated_builder;
  env->DeleteLocalRef(base_builder);
  env->DeleteLocalRef(java_string);
  if (commit_builder) {
    CommitBuilder(env, updated_builder);
  } else if (updated_builder) {
    env->DeleteLocalRef(updated_builder);
  }
}

int64_t MetadataInternal::GetInt64Property(storage_metadata::Method long_method,
                                           int64_t* cached_value) const {
  MutexLock lock(mutex_);
  if (*cached_value < 0 && obj_ != nullptr) {
    JNIEnv* env = GetJNIEnv();
    if (env) {
      int64_t val = static_cast<int64_t>(env->CallLongMethod(
          obj_, storage_metadata::GetMethodId(long_method)));
      if (!util::CheckAndClearJniExceptions(env)) {
        *cached_value = val;
      }
    }
  }
  return *cached_value;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
