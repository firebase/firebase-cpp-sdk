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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_METADATA_IOS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_METADATA_IOS_H_

#include <map>
#include <string>

#include "app/memory/unique_ptr.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
#import "FIRStorage.h"
#import "FIRStorageMetadata.h"
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class FIRStorageMetadataPointer, which is a C++-compatible
// wrapper around the FIRStorageMetadata Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorageMetadata);

#pragma clang assume_nonnull begin

class MetadataInternal {
 public:
  MetadataInternal();

  // Construct a MetadataInternal with an empty FIRStorageMetadata.
  explicit MetadataInternal(StorageInternal* _Nullable storage);

  MetadataInternal(StorageInternal* storage,
                   UniquePtr<FIRStorageMetadataPointer> impl);

  MetadataInternal(const MetadataInternal& metadata);

  MetadataInternal& operator=(const MetadataInternal& metadata);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  MetadataInternal(MetadataInternal&& other);

  MetadataInternal& operator=(MetadataInternal&& other);
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
  // into the NSDictionary on the FIRStorageMetadata. This should be called
  // before you upload the metadata.
  void CommitCustomMetadata() const;

  // Return a version String indicating what version of the StorageReference.
  int64_t generation();

  // Return a version String indicating the version of this StorageMetadata.
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

  // MD5 hash of the data; encoded using base64.
  const char* md5_hash();

  // Gets the StorageInternal we are a part of.
  StorageInternal* storage_internal() const { return storage_; }

  // Special method to create an invalid Metadata, because Metadata's default
  // constructor now gives us a valid one.
  static Metadata GetInvalidMetadata() { return Metadata(nullptr); }

 private:
#ifdef __OBJC__
  FIRStorageMetadata* impl() const { return impl_->get(); }
#endif  // __OBJC__

  friend class StorageReferenceInternal;

  StorageInternal* storage_;

  // Object lifetime managed by Objective C ARC.
  UniquePtr<FIRStorageMetadataPointer> impl_;

  // Backing store for string properties.
  std::string bucket_;
  std::string cache_control_;
  std::string content_disposition_;
  std::string content_encoding_;
  std::string content_language_;
  std::string content_type_;
  std::string name_;
  std::string path_;
  std::string md5_hash_;

  std::map<std::string, std::string>* custom_metadata_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_IOS_METADATA_IOS_H_
