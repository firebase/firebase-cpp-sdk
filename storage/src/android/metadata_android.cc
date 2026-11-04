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

void TerminateMetadataConverter(App* app) {
  JNIEnv* env = app->GetJNIEnv();
  storage_metadata_builder::ReleaseClass(env);
  storage_metadata::ReleaseClass(env);
  util::CheckAndClearJniExceptions(env);
}

jobject MetadataToStorageMetadata(JNIEnv* env, const MetadataInternal& metadata) {
  jobject builder = env->NewObject(storage_metadata_builder::GetClass(),
                                   storage_metadata_builder::GetMethodId(storage_metadata_builder::kConstructor));
  if (util::CheckAndClearJniExceptions(env) || !builder) return nullptr;

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
  std::string path_str = jpath ? util::JniStringToString(env, jpath) : "";
  if (jpath) env->DeleteLocalRef(jpath);
  StorageReference ref(new StorageReferenceInternal(storage, path_str.c_str()));
  if (!obj) return nullptr;
  MetadataInternal* meta = new MetadataInternal(ref);

  auto get_str = [&](storage_metadata::Method m, void (MetadataInternal::*setter)(const std::string&)) {
    jobject jstr = env->CallObjectMethod(obj, storage_metadata::GetMethodId(m));
    if (jstr) {
      (meta->*setter)(util::JniStringToString(env, jstr));
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
    meta->set_generation(std::atoll(util::JniStringToString(env, jgen).c_str()));
    env->DeleteLocalRef(jgen);
  }
  jobject jmgen = env->CallObjectMethod(obj, storage_metadata::GetMethodId(storage_metadata::kGetMetadataGeneration));
  if (jmgen) {
    meta->set_metadata_generation(std::atoll(util::JniStringToString(env, jmgen).c_str()));
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
        (*meta->custom_metadata())[util::JniStringToString(env, k)] = util::JniStringToString(env, v);
        env->DeleteLocalRef(k);
        if (v) env->DeleteLocalRef(v);
      }
      env->DeleteLocalRef(keys_array);
    }
    env->DeleteLocalRef(set_class);
    env->DeleteLocalRef(keys);
  }

  util::CheckAndClearJniExceptions(env);
  return meta;
}

}
}
}
