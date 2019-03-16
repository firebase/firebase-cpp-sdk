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
#include <stdlib.h>
#include "app/src/include/firebase/app.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

const int64_t kMillisToSeconds = 1000L;

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
    : storage_(storage), custom_metadata_(nullptr) {
  cache_.resize(kCacheStringCount, nullptr);
  JNIEnv* env = GetJNIEnv();
  if (obj == nullptr) {
    obj_ = nullptr;
    jobject builder =
        env->NewObject(storage_metadata_builder::GetClass(),
                       storage_metadata_builder::GetMethodId(
                           storage_metadata_builder::kConstructor));
    CommitBuilder(builder);
  } else {
    obj_ = env->NewGlobalRef(obj);
    // Cache read-only all properties that are lost when constructing a
    // StorageMetadata object from a Builder.
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

// Free any assigned string pointers from the given vector, assigning them to
// nullptr.
static void FreeVectorOfStringPointers(std::vector<std::string*>* vector) {
  for (int i = 0; i < vector->size(); i++) {
    if ((*vector)[i] != nullptr) {
      delete (*vector)[i];
      (*vector)[i] = nullptr;
    }
  }
}

// Copy any assigned string pointers from the given source vector.
static std::vector<std::string*> CopyVectorOfStringPointers(
    const std::vector<std::string*>& src) {
  std::vector<std::string*> dest;
  dest.resize(src.size(), nullptr);
  for (int i = 0; i < src.size(); i++) {
    if (src[i]) dest[i] = new std::string(*src[i]);
  }
  return dest;
}

MetadataInternal::~MetadataInternal() {
  if (obj_ != nullptr) {
    JNIEnv* env = GetJNIEnv();
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  // Clear cached strings.
  FreeVectorOfStringPointers(&cache_);
  if (custom_metadata_ != nullptr) {
    delete custom_metadata_;
  }
}

JNIEnv* MetadataInternal::GetJNIEnv() {
  return storage_ ? storage_->app()->GetJNIEnv() : util::GetJNIEnvFromApp();
}

// Creates a new copy of the map passed in (unless the map passed in is nullptr,
// in which case this function will return nullptr).
static std::map<std::string, std::string>* CreateMapCopy(
    const std::map<std::string, std::string>* src) {
  std::map<std::string, std::string>* dest = nullptr;
  if (src != nullptr) {
    dest = new std::map<std::string, std::string>;
#if defined(_STLPORT_VERSION)
    for (auto i = src->begin(); i != src->end(); ++i) {
      dest->insert(*i);
    }
#else
    *dest = *src;
#endif  // defined(_STLPORT_VERSION)
  }
  return dest;
}

MetadataInternal::MetadataInternal(const MetadataInternal& src)
    : storage_(src.storage_), obj_(nullptr), custom_metadata_(nullptr) {
  JNIEnv* env = GetJNIEnv();
  CopyJavaMetadataObject(env, src.obj_);

  custom_metadata_ = CreateMapCopy(src.custom_metadata_);
  cache_ = CopyVectorOfStringPointers(src.cache_);
  constants_ = src.constants_;
}

MetadataInternal& MetadataInternal::operator=(const MetadataInternal& src) {
  storage_ = src.storage_;
  JNIEnv* env = GetJNIEnv();
  if (obj_ != nullptr) {
    // If there's already a Java object in the dest, delete it.
    env->DeleteGlobalRef(obj_);
    obj_ = nullptr;
  }
  CopyJavaMetadataObject(env, src.obj_);

  if (custom_metadata_ != nullptr) {
    // If there's already a custom_metadata_ in the dest, delete it.
    delete custom_metadata_;
    custom_metadata_ = nullptr;
  }
  custom_metadata_ = CreateMapCopy(src.custom_metadata_);
  FreeVectorOfStringPointers(&cache_);
  cache_ = CopyVectorOfStringPointers(src.cache_);
  constants_ = src.constants_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
MetadataInternal::MetadataInternal(MetadataInternal&& src)
    : storage_(src.storage_) {
  obj_ = src.obj_;
  custom_metadata_ = std::move(src.custom_metadata_);
  src.custom_metadata_ = nullptr;
  src.obj_ = nullptr;
  // Move the cache to its new owner, so the const char*'s are still valid.
  cache_ = src.cache_;
  // Clear the source cache.
  src.cache_.clear();
  src.cache_.resize(kCacheStringCount, nullptr);
  constants_ = src.constants_;
}

MetadataInternal& MetadataInternal::operator=(MetadataInternal&& src) {
  obj_ = src.obj_;
  src.obj_ = nullptr;
  if (custom_metadata_ != nullptr) {
    delete custom_metadata_;
    custom_metadata_ = nullptr;
  }
  custom_metadata_ = std::move(src.custom_metadata_);
  src.custom_metadata_ = nullptr;
  // Clean up old cached data the destination has.
  FreeVectorOfStringPointers(&cache_);
  // Move the cache to its new owner, us.
  cache_ = src.cache_;
  // Clear the source's cache.
  src.cache_.clear();
  src.cache_.resize(kCacheStringCount, nullptr);
  constants_ = src.constants_;
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

void MetadataInternal::CopyJavaMetadataObject(JNIEnv* env, jobject src_obj) {
  // Use StorageMetadata.Builder to create a copy of the existing object.
  jobject builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     src_obj);
  CommitBuilder(builder);
}

bool MetadataInternal::Initialize(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  return storage_metadata::CacheMethodIds(env, activity) &&
         storage_metadata_builder::CacheMethodIds(env, activity);
}

void MetadataInternal::Terminate(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_metadata_builder::ReleaseClass(env);
  storage_metadata::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

const char* MetadataInternal::bucket() {
  return GetStringProperty(storage_metadata::kGetBucket, kCacheStringBucket);
}

void MetadataInternal::CommitBuilder(jobject builder) {
  JNIEnv* env = GetJNIEnv();
  jobject new_metadata = env->CallObjectMethod(
      builder,
      storage_metadata_builder::GetMethodId(storage_metadata_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (obj_ != nullptr) env->DeleteGlobalRef(obj_);
  obj_ = env->NewGlobalRef(new_metadata);
  env->DeleteLocalRef(new_metadata);
}

void MetadataInternal::set_cache_control(const char* cache_control) {
  SetStringProperty(cache_control, storage_metadata_builder::kSetCacheControl,
                    kCacheStringCacheControl);
}

const char* MetadataInternal::cache_control() {
  return GetStringProperty(storage_metadata::kGetCacheControl,
                           kCacheStringCacheControl);
}

void MetadataInternal::set_content_disposition(const char* disposition) {
  SetStringProperty(disposition,
                    storage_metadata_builder::kSetContentDisposition,
                    kCacheStringContentDisposition);
}

const char* MetadataInternal::content_disposition() {
  return GetStringProperty(storage_metadata::kGetContentDisposition,
                           kCacheStringContentDisposition);
}

void MetadataInternal::set_content_encoding(const char* encoding) {
  SetStringProperty(encoding, storage_metadata_builder::kSetContentEncoding,
                    kCacheStringContentEncoding);
}

const char* MetadataInternal::content_encoding() {
  return GetStringProperty(storage_metadata::kGetContentEncoding,
                           kCacheStringContentEncoding);
}

void MetadataInternal::set_content_language(const char* language) {
  SetStringProperty(language, storage_metadata_builder::kSetContentLanguage,
                    kCacheStringContentLanguage);
}

const char* MetadataInternal::content_language() {
  return GetStringProperty(storage_metadata::kGetContentLanguage,
                           kCacheStringContentLanguage);
}

void MetadataInternal::set_content_type(const char* type) {
  SetStringProperty(type, storage_metadata_builder::kSetContentType,
                    kCacheStringContentType);
}

const char* MetadataInternal::content_type() {
  return GetStringProperty(storage_metadata::kGetContentType,
                           kCacheStringContentType);
}

int64_t MetadataInternal::creation_time() {
  return GetInt64Property(storage_metadata::kGetCreationTimeMillis,
                          &constants_.creation_time);
}

std::map<std::string, std::string>* MetadataInternal::custom_metadata() {
  if (custom_metadata_ == nullptr) {
    custom_metadata_ = new std::map<std::string, std::string>();
    ReadCustomMetadata(custom_metadata_);
  }
  return custom_metadata_;
}

void MetadataInternal::CommitCustomMetadata() {
  std::map<std::string, std::string> old_metadata;
  ReadCustomMetadata(&old_metadata);
  // Set all new values, and if any old values are not present in new, then
  // clear them.
  JNIEnv* env = GetJNIEnv();
  jobject builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     obj_);
  if (custom_metadata_ != nullptr) {
    for (auto i = custom_metadata_->begin(); i != custom_metadata_->end();
         ++i) {
      old_metadata.erase(
          i->first);  // Erase any key we see in the new metadata.
      // Any left over have been erased.
      jobject key_string = env->NewStringUTF(i->first.c_str());
      jobject value_string = env->NewStringUTF(i->second.c_str());
      jobject new_builder = env->CallObjectMethod(
          builder,
          storage_metadata_builder::GetMethodId(
              storage_metadata_builder::kSetCustomMetadata),
          key_string, value_string);
      env->DeleteLocalRef(value_string);
      env->DeleteLocalRef(key_string);
      env->DeleteLocalRef(builder);
      builder = new_builder;
    }
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
    env->DeleteLocalRef(builder);
    builder = new_builder;
  }
  env->DeleteLocalRef(empty_string);
  CommitBuilder(builder);
}

void MetadataInternal::ReadCustomMetadata(
    std::map<std::string, std::string>* output_map) {
  JNIEnv* env = GetJNIEnv();
  jobject key_set = env->CallObjectMethod(
      obj_,
      storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadataKeys));
  // Iterator iter = key_set.iterator();
  jobject iter = env->CallObjectMethod(
      key_set,
      firebase::util::set::GetMethodId(firebase::util::set::kIterator));
  // while (iter.hasNext())
  while (
      env->CallBooleanMethod(iter, firebase::util::iterator::GetMethodId(
                                       firebase::util::iterator::kHasNext))) {
    // String key = iter.next();
    jobject key_object = env->CallObjectMethod(
        iter,
        firebase::util::iterator::GetMethodId(firebase::util::iterator::kNext));
    // String value = obj_.GetCustomMetadata(key);
    jobject value_object = env->CallObjectMethod(
        obj_,
        storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadata),
        key_object);
    std::string key = util::JniStringToString(env, key_object);
    std::string value = util::JniStringToString(env, value_object);
    output_map->insert(std::pair<std::string, std::string>(key, value));
  }
  env->DeleteLocalRef(iter);
  env->DeleteLocalRef(key_set);
}

int64_t MetadataInternal::generation() {
  int64_t output = 0;
  const char* str = GetStringProperty(storage_metadata::kGetGeneration,
                                      kCacheStringGeneration);
  if (str) output = strtoll(str, nullptr, 0);  // NOLINT
  return output;
}

int64_t MetadataInternal::metadata_generation() {
  int64_t output = 0;
  const char* str = GetStringProperty(storage_metadata::kGetMetadataGeneration,
                                      kCacheStringMetadataGeneration);
  if (str) output = strtoll(str, nullptr, 0);  // NOLINT
  return output;
}

const char* MetadataInternal::name() {
  return GetStringProperty(storage_metadata::kGetName, kCacheStringName);
}

const char* MetadataInternal::path() {
  return GetStringProperty(storage_metadata::kGetPath, kCacheStringPath);
}

StorageReferenceInternal* MetadataInternal::GetReference() {
  // If we don't have an associated Storage, we are not assigned to a reference.
  if (storage_ == nullptr) return nullptr;
  JNIEnv* env = GetJNIEnv();
  jobject ref_obj = env->CallObjectMethod(
      obj_, storage_metadata::GetMethodId(storage_metadata::kGetReference));
  if (util::CheckAndClearJniExceptions(env)) {
    // Failed to get the StorageReference Java object, thus the StorageReference
    // C++ object we are creating is invalid.
    return nullptr;
  }
  StorageReferenceInternal* new_ref =
      new StorageReferenceInternal(storage_, ref_obj);
  env->DeleteLocalRef(ref_obj);
  return new_ref;
}

int64_t MetadataInternal::size_bytes() {
  return GetInt64Property(storage_metadata::kGetSizeBytes,
                          &constants_.size_bytes);
}

int64_t MetadataInternal::updated_time() {
  return GetInt64Property(storage_metadata::kGetUpdatedTimeMillis,
                          &constants_.updated_time);
}

// Get the MD5 hash of the blob referenced by StorageReference.
const char* MetadataInternal::md5_hash() {
  return GetStringProperty(storage_metadata::kGetMd5Hash, kCacheStringMd5Hash);
}

const char* MetadataInternal::GetStringProperty(
    storage_metadata::Method string_method, CacheString cache_string) {
  std::string** cached_string = &cache_[cache_string];
  if (*cached_string == nullptr) {
    JNIEnv* env = GetJNIEnv();
    jobject str = env->CallObjectMethod(
        obj_, storage_metadata::GetMethodId(string_method));
    if (util::CheckAndClearJniExceptions(env) || !str) {
      if (str) env->DeleteLocalRef(str);
      return nullptr;
    }
    *cached_string = new std::string(util::JniStringToString(env, str));
  }
  return (*cached_string)->c_str();
}

void MetadataInternal::SetStringProperty(
    const char* string_value, storage_metadata_builder::Method builder_method,
    CacheString cache_string) {
  std::string** cached_string = &cache_[cache_string];
  if (*cached_string != nullptr) {
    delete *cached_string;
    *cached_string = nullptr;
  }
  JNIEnv* env = GetJNIEnv();
  jobject base_builder =
      env->NewObject(storage_metadata_builder::GetClass(),
                     storage_metadata_builder::GetMethodId(
                         storage_metadata_builder::kConstructorFromMetadata),
                     obj_);
  if (util::CheckAndClearJniExceptions(env)) return;
  jobject java_string = env->NewStringUTF(string_value);
  jobject updated_builder = env->CallObjectMethod(
      base_builder, storage_metadata_builder::GetMethodId(builder_method),
      java_string);
  bool commitBuilder = !util::CheckAndClearJniExceptions(env);
  env->DeleteLocalRef(base_builder);
  env->DeleteLocalRef(java_string);
  if (commitBuilder) {
    CommitBuilder(updated_builder);
  } else if (updated_builder) {
    env->DeleteLocalRef(updated_builder);
  }
}

const char* MetadataInternal::GetUriPropertyAsString(
    storage_metadata::Method uri_method, CacheString cache_string) {
  std::string** cached_string = &cache_[cache_string];
  if (*cached_string == nullptr) {
    JNIEnv* env = GetJNIEnv();
    jobject uri =
        env->CallObjectMethod(obj_, storage_metadata::GetMethodId(uri_method));
    if (util::CheckAndClearJniExceptions(env) || !uri) {
      if (uri) env->DeleteLocalRef(uri);
      return nullptr;
    }
    *cached_string = new std::string(util::JniUriToString(env, uri));
  }
  return (*cached_string)->c_str();
}

int64_t MetadataInternal::GetInt64Property(storage_metadata::Method long_method,
                                           int64_t* cached_value) {
  if (!(*cached_value)) {
    JNIEnv* env = GetJNIEnv();
    *cached_value = static_cast<int64_t>(
        env->CallLongMethod(obj_, storage_metadata::GetMethodId(long_method)));
    util::CheckAndClearJniExceptions(env);
  }
  return *cached_value;
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
