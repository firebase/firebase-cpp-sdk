// Copyright 2016 Google LLC
#include "storage/src/android/metadata_android.h"

#include <jni.h>

#include <cstdlib>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/jni/declaration.h"
#include "app/src/jni/exception.h"
#include "app/src/jni/string.h"
#include "app/src/util_android.h"
#include "storage/src/android/storage_android.h"
#include "storage/src/android/storage_reference_android.h"
#include "storage/storage_resources.h"

namespace firebase::storage::internal {

#define STORAGE_METADATA_METHODS(X)                                         \
  X(Constructor, "<init>", "()V")                                           \
  X(GetContentType, "getContentType", "()Ljava/lang/String;")               \
  X(GetCustomMetadata, "getCustomMetadata",                                 \
    "(Ljava/lang/String;)Ljava/lang/String;")                               \
  X(GetCustomMetadataKeys, "getCustomMetadataKeys", "()Ljava/util/Set;")    \
  X(GetPath, "getPath", "()Ljava/lang/String;")                             \
  X(GetName, "getName", "()Ljava/lang/String;")                             \
  X(GetBucket, "getBucket", "()Ljava/lang/String;")                         \
  X(GetGeneration, "getGeneration", "()Ljava/lang/String;")                 \
  X(GetMetadataGeneration, "getMetadataGeneration", "()Ljava/lang/String;") \
  X(GetCreationTimeMillis, "getCreationTimeMillis", "()J")                  \
  X(GetUpdatedTimeMillis, "getUpdatedTimeMillis", "()J")                    \
  X(GetSizeBytes, "getSizeBytes", "()J")                                    \
  X(GetMd5Hash, "getMd5Hash", "()Ljava/lang/String;")                       \
  X(GetCacheControl, "getCacheControl", "()Ljava/lang/String;")             \
  X(GetContentDisposition, "getContentDisposition", "()Ljava/lang/String;") \
  X(GetContentEncoding, "getContentEncoding", "()Ljava/lang/String;")       \
  X(GetContentLanguage, "getContentLanguage", "()Ljava/lang/String;")       \
  X(GetReference, "getReference",                                           \
    "()Lcom/google/firebase/storage/StorageReference;")
JNI_METHOD_LOOKUP_DECLARATION(
    storage_metadata, STORAGE_METADATA_METHODS, FIELD_LOOKUP_NONE,
    PROGUARD_KEEP_CLASS "com/google/firebase/storage/StorageMetadata")

#define STORAGE_METADATA_BUILDER_METHODS(X)                                 \
  X(Constructor, "<init>", "()V")                                           \
  X(ConstructorFromMetadata, "<init>",                                      \
    "(Lcom/google/firebase/storage/StorageMetadata;)V")                     \
  X(Build, "build", "()Lcom/google/firebase/storage/StorageMetadata;")      \
  X(SetContentLanguage, "setContentLanguage",                               \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                \
  X(SetContentDisposition, "setContentDisposition",                         \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                \
  X(SetContentEncoding, "setContentEncoding",                               \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                \
  X(SetCacheControl, "setCacheControl",                                     \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                \
  X(SetContentType, "setContentType",                                       \
    "(Ljava/lang/String;)"                                                  \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")                \
  X(SetCustomMetadata, "setCustomMetadata",                                 \
    "(Ljava/lang/String;Ljava/lang/String;)"                                \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")
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

jobject MetadataToStorageMetadata(JNIEnv* env,
                                  const MetadataInternal& metadata) {
  jobject builder = env->NewObject(
      storage_metadata_builder::GetClass(),
      storage_metadata_builder::GetMethodId(
          storage_metadata_builder::kConstructor));
  if (jni::CheckAndClearException(env) || !builder) return nullptr;

  auto update_builder = [&](jobject next) {
    if (jni::CheckAndClearException(env)) {
      if (next) env->DeleteLocalRef(next);
      return;
    }
    if (next) {
      env->DeleteLocalRef(builder);
      builder = next;
    }
  };

  auto set_str = [&](const char* val, storage_metadata_builder::Method m) {
    if (!val || val[0] == '\0') return;
    jobject jstr = env->NewStringUTF(val);
    if (jni::CheckAndClearException(env) || !jstr) return;
    jobject next = env->CallObjectMethod(
        builder, storage_metadata_builder::GetMethodId(m), jstr);
    env->DeleteLocalRef(jstr);
    update_builder(next);
  };

  set_str(metadata.cache_control(),
          storage_metadata_builder::kSetCacheControl);
  set_str(metadata.content_disposition(),
          storage_metadata_builder::kSetContentDisposition);
  set_str(metadata.content_encoding(),
          storage_metadata_builder::kSetContentEncoding);
  set_str(metadata.content_language(),
          storage_metadata_builder::kSetContentLanguage);
  set_str(metadata.content_type(), storage_metadata_builder::kSetContentType);

  if (metadata.custom_metadata()) {
    for (const auto& [k, v] : *metadata.custom_metadata()) {
      jobject k_str = env->NewStringUTF(k.c_str());
      if (jni::CheckAndClearException(env) || !k_str) continue;
      jobject v_str = env->NewStringUTF(v.c_str());
      if (jni::CheckAndClearException(env) || !v_str) {
        env->DeleteLocalRef(k_str);
        continue;
      }
      jobject next = env->CallObjectMethod(
          builder,
          storage_metadata_builder::GetMethodId(
              storage_metadata_builder::kSetCustomMetadata),
          k_str, v_str);
      env->DeleteLocalRef(k_str);
      env->DeleteLocalRef(v_str);
      update_builder(next);
    }
  }

  jobject meta_obj = env->CallObjectMethod(
      builder,
      storage_metadata_builder::GetMethodId(storage_metadata_builder::kBuild));
  env->DeleteLocalRef(builder);
  if (jni::CheckAndClearException(env)) {
    if (meta_obj) env->DeleteLocalRef(meta_obj);
    return nullptr;
  }
  return meta_obj;
}

MetadataInternal* StorageMetadataToMetadata(JNIEnv* env, jobject obj,
                                            StorageInternal* storage) {
  if (!obj || !storage) return nullptr;
  jobject ref_obj = env->CallObjectMethod(
      obj, storage_metadata::GetMethodId(storage_metadata::kGetReference));
  if (jni::CheckAndClearException(env)) {
    if (ref_obj) env->DeleteLocalRef(ref_obj);
    ref_obj = nullptr;
  }
  auto* meta = new MetadataInternal(
      ref_obj ? new StorageReferenceInternal(storage, ref_obj) : nullptr);
  if (ref_obj) env->DeleteLocalRef(ref_obj);

  auto read_opt_str = [&](storage_metadata::Method m, auto&& fn) {
    jobject jstr =
        env->CallObjectMethod(obj, storage_metadata::GetMethodId(m));
    if (jni::CheckAndClearException(env)) {
      if (jstr) env->DeleteLocalRef(jstr);
      return;
    }
    if (jstr) {
      fn(::firebase::jni::JStringToString(env, jstr));
      env->DeleteLocalRef(jstr);
    }
  };

  read_opt_str(storage_metadata::kGetBucket,
               [&](const std::string& s) { meta->set_bucket(s); });
  read_opt_str(storage_metadata::kGetCacheControl,
               [&](const std::string& s) { meta->set_cache_control(s.c_str()); });
  read_opt_str(storage_metadata::kGetContentDisposition,
               [&](const std::string& s) {
                 meta->set_content_disposition(s.c_str());
               });
  read_opt_str(storage_metadata::kGetContentEncoding,
               [&](const std::string& s) {
                 meta->set_content_encoding(s.c_str());
               });
  read_opt_str(storage_metadata::kGetContentLanguage,
               [&](const std::string& s) {
                 meta->set_content_language(s.c_str());
               });
  read_opt_str(storage_metadata::kGetContentType,
               [&](const std::string& s) { meta->set_content_type(s.c_str()); });
  read_opt_str(storage_metadata::kGetName,
               [&](const std::string& s) { meta->set_name(s); });
  read_opt_str(storage_metadata::kGetPath,
               [&](const std::string& s) { meta->set_path(s); });
  read_opt_str(storage_metadata::kGetMd5Hash,
               [&](const std::string& s) { meta->set_md5_hash(s); });
  read_opt_str(storage_metadata::kGetGeneration, [&](const std::string& s) {
    meta->set_generation(std::atoll(s.c_str()));
  });
  read_opt_str(storage_metadata::kGetMetadataGeneration,
               [&](const std::string& s) {
                 meta->set_metadata_generation(std::atoll(s.c_str()));
               });

  auto read_long = [&](storage_metadata::Method m,
                       void (MetadataInternal::*setter)(int64_t)) {
    jlong v = env->CallLongMethod(obj, storage_metadata::GetMethodId(m));
    if (!jni::CheckAndClearException(env)) (meta->*setter)(v);
  };
  read_long(storage_metadata::kGetSizeBytes, &MetadataInternal::set_size_bytes);
  read_long(storage_metadata::kGetCreationTimeMillis,
            &MetadataInternal::set_creation_time);
  read_long(storage_metadata::kGetUpdatedTimeMillis,
            &MetadataInternal::set_updated_time);

  jobject keys = env->CallObjectMethod(
      obj,
      storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadataKeys));
  if (jni::CheckAndClearException(env)) {
    if (keys) env->DeleteLocalRef(keys);
  } else if (keys) {
    auto keys_arr = static_cast<jobjectArray>(env->CallObjectMethod(
        keys, firebase::util::set::GetMethodId(firebase::util::set::kToArray)));
    if (jni::CheckAndClearException(env)) {
      if (keys_arr) env->DeleteLocalRef(keys_arr);
    } else if (keys_arr) {
      int count = env->GetArrayLength(keys_arr);
      for (int i = 0; i < count; ++i) {
        auto k = static_cast<jstring>(env->GetObjectArrayElement(keys_arr, i));
        if (jni::CheckAndClearException(env) || !k) {
          if (k) env->DeleteLocalRef(k);
          continue;
        }
        auto v = static_cast<jstring>(env->CallObjectMethod(
            obj,
            storage_metadata::GetMethodId(storage_metadata::kGetCustomMetadata),
            k));
        if (!jni::CheckAndClearException(env)) {
          (*meta->custom_metadata())[::firebase::jni::JStringToString(env, k)] =
              v ? ::firebase::jni::JStringToString(env, v) : std::string();
        }
        env->DeleteLocalRef(k);
        if (v) env->DeleteLocalRef(v);
      }
      env->DeleteLocalRef(keys_arr);
    }
    env->DeleteLocalRef(keys);
  }
  jni::CheckAndClearException(env);
  return meta;
}

}  // namespace firebase::storage::internal
