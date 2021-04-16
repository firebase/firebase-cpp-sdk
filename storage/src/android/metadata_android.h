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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_METADATA_ANDROID_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_METADATA_ANDROID_H_

#include <jni.h>
#include <map>
#include <string>
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/util_android.h"
#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase {
namespace storage {
namespace internal {

// Declared here because StorageReferenceInternal needs to refer to the Java
// StorageMetadata class.

// clang-format off
#define STORAGE_METADATA_METHODS(X)                                            \
  X(Constructor, "<init>", "()V"),                                             \
  X(GetContentType, "getContentType", "()Ljava/lang/String;"),                 \
  X(GetCustomMetadata, "getCustomMetadata",                                    \
    "(Ljava/lang/String;)Ljava/lang/String;"),                                 \
  X(GetCustomMetadataKeys, "getCustomMetadataKeys",                            \
    "()Ljava/util/Set;"),                                                      \
  X(GetPath, "getPath", "()Ljava/lang/String;"),                               \
  X(GetName, "getName", "()Ljava/lang/String;"),                               \
  X(GetBucket, "getBucket", "()Ljava/lang/String;"),                           \
  X(GetGeneration, "getGeneration", "()Ljava/lang/String;"),                   \
  X(GetMetadataGeneration, "getMetadataGeneration", "()Ljava/lang/String;"),   \
  X(GetCreationTimeMillis, "getCreationTimeMillis", "()J"),                    \
  X(GetUpdatedTimeMillis, "getUpdatedTimeMillis", "()J"),                      \
  X(GetSizeBytes, "getSizeBytes", "()J"),                                      \
  X(GetMd5Hash, "getMd5Hash", "()Ljava/lang/String;"),                         \
  X(GetCacheControl, "getCacheControl", "()Ljava/lang/String;"),               \
  X(GetContentDisposition, "getContentDisposition", "()Ljava/lang/String;"),   \
  X(GetContentEncoding, "getContentEncoding", "()Ljava/lang/String;"),         \
  X(GetContentLanguage, "getContentLanguage", "()Ljava/lang/String;"),         \
  X(GetReference, "getReference",                                              \
    "()Lcom/google/firebase/storage/StorageReference;")
// clang-format on

METHOD_LOOKUP_DECLARATION(storage_metadata, STORAGE_METADATA_METHODS)

// clang-format off
#define STORAGE_METADATA_BUILDER_METHODS(X)                                    \
  X(Constructor, "<init>", "()V"),                                             \
  X(ConstructorFromMetadata, "<init>",                                         \
    "(Lcom/google/firebase/storage/StorageMetadata;)V"),                       \
  X(Build, "build", "()Lcom/google/firebase/storage/StorageMetadata;"),        \
  X(SetContentLanguage, "setContentLanguage",                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;"),                  \
  X(SetContentDisposition, "setContentDisposition",                            \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;"),                  \
  X(SetContentEncoding, "setContentEncoding",                                  \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;"),                  \
  X(SetCacheControl, "setCacheControl",                                        \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;"),                  \
  X(SetContentType, "setContentType",                                          \
    "(Ljava/lang/String;)"                                                     \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;"),                  \
  X(SetCustomMetadata, "setCustomMetadata",                                    \
    "(Ljava/lang/String;Ljava/lang/String;)"                                   \
    "Lcom/google/firebase/storage/StorageMetadata$Builder;")
// clang-format on

METHOD_LOOKUP_DECLARATION(storage_metadata_builder,
                          STORAGE_METADATA_BUILDER_METHODS)

class StorageReferenceInternal;

class MetadataInternal {
 public:
  // You may pass in nullptr for `storage`; it will only cause GetReference to
  // fail.
  MetadataInternal(StorageInternal* storage, jobject obj);

  // Construct a MetadataInternal with an empty Java StorageMetadata object.
  // You may pass in nullptr for `storage`; it will only cause GetReference to
  // fail.
  explicit MetadataInternal(StorageInternal* storage);

  MetadataInternal(const MetadataInternal& ref);

  MetadataInternal& operator=(const MetadataInternal& ref);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  MetadataInternal(MetadataInternal&& ref);

  MetadataInternal& operator=(MetadataInternal&& ref);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~MetadataInternal();

  // Return the owning Google Cloud Storage bucket for the StorageReference.
  const char* bucket();

  // Set the Cache Control setting of the StorageReference.
  void set_cache_control(const char* cache_control);

  // Return the Cache Control setting of the StorageReference.
  const char* cache_control();

  // Set the content disposition of the StorageReference.
  void set_content_disposition(const char* disposition);

  // Return the content disposition of the StorageReference.
  const char* content_disposition();

  // Set the content encoding for the StorageReference.
  void set_content_encoding(const char* encoding);

  // Return the content encoding for the StorageReference.
  const char* content_encoding();

  // Set the content language for the StorageReference.
  void set_content_language(const char* language);

  // Return the content language for the StorageReference.
  const char* content_language();

  // Set the content type of the StorageReference.
  void set_content_type(const char* type);

  // Return the content type of the StorageReference.
  const char* content_type();

  // Return the time the StorageReference was created.
  int64_t creation_time();

  // Return a map of custom metadata key value pairs.
  std::map<std::string, std::string>* custom_metadata();

  // Take the keys/values that are present in custom_metadata_ and write them
  // into the Java StorageMetadata object. This should be called before you
  // upload the metadata.
  void CommitCustomMetadata();

  // Return a version number indicating what version of the StorageReference.
  int64_t generation();

  // Return a version number indicating the version of this StorageMetadata.
  int64_t metadata_generation();

  // Return a simple name of the StorageReference object.
  const char* name();

  // Return the path of the StorageReference object.
  const char* path();

  // Return the associated StorageReference for which this metadata belongs to.
  StorageReferenceInternal* GetReference();

  // Return the stored Size in bytes of the StorageReference object.
  int64_t size_bytes();

  // Return the time the StorageReference was last updated.
  int64_t updated_time();

  // Get the MD5 hash of the blob referenced by StorageReference.
  const char* md5_hash();

  // Initialize JNI bindings for this class.
  static bool Initialize(App* app);
  static void Terminate(App* app);

  // Gets the StorageInternal we are a part of.
  StorageInternal* storage_internal() const { return storage_; }

  // Special method to create an invalid Metadata, because Metadata's default
  // constructor now gives us a valid one.
  static Metadata GetInvalidMetadata() { return Metadata(nullptr); }

 private:
  friend class StorageReferenceInternal;

  // We need to store local copies of string fields, so that we can return
  // pointers to the strings.
  enum CacheString {
    kCacheStringBucket = 0,
    kCacheStringCacheControl,
    kCacheStringContentDisposition,
    kCacheStringContentEncoding,
    kCacheStringContentLanguage,
    kCacheStringContentType,
    kCacheStringName,
    kCacheStringPath,
    kCacheStringGeneration,
    kCacheStringMetadataGeneration,
    kCacheStringMd5Hash,

    kCacheStringCount,
  };

  jobject obj() { return obj_; }

  void ReadCustomMetadata(
      std::map<std::string, std::string>* custom_metadata_out);

  // Commit a pending builder into obj_. Deletes the builder.
  void CommitBuilder(jobject builder);

  // Get the JNIEnv from the linked Storage instance, or from the default (or
  // any) App.
  JNIEnv* GetJNIEnv();

  // Copy the source Java StorageMetadata object to ourselves, as a new global
  // reference.
  void CopyJavaMetadataObject(JNIEnv* env, jobject obj);

  // Read a metadata string property from the cache or fall back to reading
  // from Java object and caching it.
  const char* GetStringProperty(storage_metadata::Method string_method,
                                CacheString cache_string);

  // Write a metadata string property to Java object and clear the currently
  // cached value so that it's read from the Java object the next time it's
  // requested by the application.
  void SetStringProperty(const char* string_value,
                         storage_metadata_builder::Method builder_method,
                         CacheString cache_string);

  // Read a metadata string property from the cache or fall back to reading
  // a Uri from the Java object and caching it.
  const char* GetUriPropertyAsString(storage_metadata::Method uri_method,
                                     CacheString cache_string);

  // Read a int64_t property from a cached value or fall back to reading
  // from the Java object and caching it.
  int64_t GetInt64Property(storage_metadata::Method long_method,
                           int64_t* cached_value);

  StorageInternal* storage_;
  jobject obj_;

  std::map<std::string, std::string>* custom_metadata_;

  // We store pointers to std::string so that nullptr can mean "we
  // don't have a cached value for this at the moment."
  std::vector<std::string*> cache_;

  // Non-string properties of the StorageMetadata Java object that are lost
  // when constructing a new instance with StorageMetadata.Builder.  These
  // properties are cached when this object is constructed and returned
  // throughout the lifetime of the object.
  struct Constants {
    Constants() : size_bytes(0), updated_time(0), creation_time(0) {}

    int64_t size_bytes;
    int64_t updated_time;
    int64_t creation_time;
  } constants_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_ANDROID_METADATA_ANDROID_H_
