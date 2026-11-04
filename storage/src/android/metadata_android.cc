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
#include <string>
#include <utility>

#include "app/src/include/firebase/app.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase {
namespace storage {
namespace internal {

// clang-format off
#define STORAGE_METADATA_METHODS(X)                                            \
  X(Constructor, "<init>", "()V")                                              \
  X(GetContentType, "getContentType", "()Ljava/lang/String;")                  \
  X(GetCustomMetadata, "getCustomMetadata",                                    \
    "(Ljava/lang/String;)Ljava/lang/String;")                                  \
  X(GetCustomMetadataKeys, "getCustomMetadataKeys",                            \
    "()Ljava/util/Set;")                                                       \
  X(GetPath, "getPath", "()Ljava/lang/String;")                                \
  X(GetName, "getName", "()Ljava/lang/String;")                                \
  X(GetBucket, "getBucket", "()Ljava/lang/String;")                            \
  X(GetGeneration, "getGeneration", "()Ljava/lang/String;")                    \
  X(GetMetadataGeneration, "getMetadataGeneration", "()Ljava/lang/String;")    \
  X(GetCreationTimeMillis, "getCreationTimeMillis", "()J")                     \
  X(GetUpdatedTimeMillis, "getUpdatedTimeMillis", "()J")                       \
  X(GetSizeBytes, "getSizeBytes", "()J")                                       \
  X(GetMd5Hash, "getMd5Hash", "()Ljava/lang/String;")                          \
  X(GetCacheControl, "getCacheControl", "()Ljava/lang/String;")                \
  X(GetContentDisposition, "getContentDisposition", "()Ljava/lang/String;")    \
  X(GetContentEncoding, "getContentEncoding", "()Ljava/lang/String;")          \
  X(GetContentLanguage, "getContentLanguage", "()Ljava/lang/String;")          \
  X(GetReference, "getReference",                                              \
    "()Lcom/google/firebase/storage/StorageReference;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    storage_metadata, STORAGE_METADATA_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/storage/StorageMetadata")

// clang-format off
#define STORAGE_METADATA_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V")                                              \
  X(ConstructorFromMetadata, "<init>",                                         \
    "(Lcom/google/firebase/storage/StorageMetadata;)V")                        \
  X(Build, "build", "()Lcom/google/firebase/storage/StorageMetadata;")         \
  X(SetContentLanguage, "setContentLanguage",                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                   \
  X(SetContentDisposition, "setContentDisposition",                            \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                   \
  X(SetContentEncoding, "setContentEncoding",                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                   \
  X(SetCacheControl, "setCacheControl",                                        \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                   \
  X(SetContentType, "setContentType",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                   \
  X(SetCustomMetadata, "setCustomMetadata",                                    \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")
// clang-format on

JNI_METHOD_LOOKUP_DECLARATION(
    storage_metadata_builder, STORAGE_METADATA_BUILDER_METHODS,
    FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/storage/StorageMetadata$Builder")

bool InitializeMetadataConverter(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  jobject activity = app->activity();
  if (storage_metadata::CacheMethodIds(env, activity) &&
      storage_metadata_builder::CacheMethodIds(env, activity)) {
    return true;
  }
  TerminateMetadataConverter(app);
  return false;
}

jclass GetStorageMetadataClass() { return storage_metadata::GetClass(); }

void TerminateMetadataConverter(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_metadata_builder::ReleaseClass(env);
  storage_metadata::ReleaseClass(env);
  jni::CheckAndClearException(env);
}

jobject MetadataToStorageMetadata(JNIEnv* env, const MetadataInternal& metadata) {
  jobject builder = env->NewObject(storage_metadata_builder::GetClass(),
                                   storage_metadata_builder::GetMethodId(storage_metadata_builder::kConstructor));
  if (jni::CheckAndClearException(env) || !builder) return nullptr;

  auto set_str = [&](const char* val, storage_metadata_builder::Method m) {
    if (val && val[0] != '\0') {
      jobject jstr = env->NewStringUTF(val);
      jobject new_builder = env->CallObjectMethod(builder, storage_metadata_builder::GetMethodId(m), jstr);
      env->DeleteLocalRef(jstr);
      if (new_builder) {
        env->DeleteLocalRef(builder);
        builder = new_builder;
      }
    }
  };

  set_str(metadata.cache_control(), storage_metadata_builder::kSetCacheControl);
  set_str(metadata.content_disposition(), storage_metadata_builder::kSetContentDisposition);
  set_str(metadata.content_encoding(), storage_metadata_builder::kSetContentEncoding);
  set_str(metadata.content_language(), storage_metadata_builder::kSetContentLanguage);
  set_str(metadata.content_type(), storage_metadata_builder::kSetContentType);

  if (metadata.custom_metadata() && !metadata.custom_metadata()->empty()) {
    for (const auto& kv : *metadata.custom_metadata()) {
      jobject k_str = env->NewStringUTF(kv.first.c_str());
      jobject v_str = env->NewStringUTF(kv.second.c_str());
      jobject new_builder = env->CallObjectMethod(builder, storage_metadata_builder::GetMethodId(storage_metadata_builder::kSetCustomMetadata), k_str, v_str);
      env->DeleteLocalRef(k_str);
      env->DeleteLocalRef(v_str);
      if (new_builder) {
        env->DeleteLocalRef(builder);
        builder = new_builder;
      }
    }
  }

  jobject meta_obj = env->CallObjectMethod(builder, storage_metadata_builder::GetMethodId(storage_metadata_builder::kBuild));
  env->DeleteLocalRef(builder);
  return meta_obj;
}

MetadataInternal* StorageMetadataToMetadata(JNIEnv* env, jobject obj, StorageInternal* storage) {
  if (!obj) return nullptr;
  jobject jpath = env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetPath));
  std::string path_str = jpath ? ::firebase::jni::JStringToString(env, jpath) : "";
  if (jpath) env->DeleteLocalRef(jpath);
  StorageReference ref(new StorageReferenceInternal(storage, path_str.c_str()));
  if (!obj) return nullptr;
  MetadataInternal* meta = new MetadataInternal(ref);

  auto get_str = [&](storage_metadata::Method m, auto setter) {
    jobject jstr = env->CallObjectMethod(obj, storage_metadata::GetMethodId(m));
    if (jstr) {
      std::string val = ::firebase::jni::JStringToString(env, jstr);
      (meta->*setter)(val.c_str());
      env->DeleteLocalRef(jstr);
    }
  };

  get_str(storage_metadata::kGetBucket, &MetadataInternal::set_bucket);
  get_str(storage_metadata::kGetCacheControl, &MetadataInternal::set_cache_control);
  get_str(storage_metadata::kGetContentDisposition, &MetadataInternal::set_content_disposition);
  get_str(storage_metadata::kGetContentEncoding, &MetadataInternal::set_content_encoding);
  get_str(storage_metadata::kGetContentLanguage, &MetadataInternal::set_content_language);
  get_str(storage_metadata::kGetContentType, &MetadataInternal::set_content_type);
  get_str(storage_metadata::kGetName, &MetadataInternal::set_name);
  get_str(storage_metadata::kGetPath, &MetadataInternal::set_path);
  get_str(storage_metadata::kGetMd5Hash, &MetadataInternal::set_md5_hash);

  meta->set_size_bytes(env->CallLongMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetSizeBytes)));
  meta->set_creation_time(env->CallLongMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetCreationTimeMillis)));
  meta->set_updated_time(env->CallLongMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetUpdatedTimeMillis)));

  // generation and metadata_generation return strings in Java API, wait! 
  // Let me check if they were GetString in the old code.
  jobject jgen = env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetGeneration));
  if (jgen) {
    meta->set_generation(std::atoll(::firebase::jni::JStringToString(env, jgen).c_str()));
    env->DeleteLocalRef(jgen);
  }
  jobject jmgen = env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetMetadataGeneration));
  if (jmgen) {
    meta->set_metadata_generation(std::atoll(::firebase::jni::JStringToString(env, jmgen).c_str()));
    env->DeleteLocalRef(jmgen);
  }

  jobject keys = env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadataKeys));
  if (keys) {
    jclass set_class = env->FindClass("java/util/Set");
    jmethodID to_array = env->GetMethodID(set_class, "toArray", "()[Ljava/lang/Object;");
    jobjectArray keys_array = (jobjectArray)env->CallObjectMethod(keys, to_array);
    if (keys_array) {
      int count = env->GetArrayLength(keys_array);
      for (int i = 0; i < count; ++i) {
        jstring k = (jstring)env->GetObjectArrayElement(keys_array, i);
        jstring v = (jstring)env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadata), k);
        (*meta->custom_metadata())[::firebase::jni::JStringToString(env, k)] = ::firebase::jni::JStringToString(env, v);
        env->DeleteLocalRef(k);
        if (v) env->DeleteLocalRef(v);
      }
      env->DeleteLocalRef(keys_array);
    }
    env->DeleteLocalRef(set_class);
    env->DeleteLocalRef(keys);
  }

  jni::CheckAndClearException(env);
  return meta;
}

}
}
}
