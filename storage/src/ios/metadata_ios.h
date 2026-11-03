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

#ifndef FIREBASE_STORAGE_SRC_IOS_METADATA_IOS_H_
#define FIREBASE_STORAGE_SRC_IOS_METADATA_IOS_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/ios/storage_reference_ios.h"

#ifdef __OBJC__
@class FIRStorageMetadata;
#endif  // __OBJC__

namespace firebase {
namespace storage {
namespace internal {

// This defines the class FIRStorageMetadataPointer, which is a C++-compatible
// wrapper around the FIRStorageMetadata Obj-C class.
OBJ_C_PTR_WRAPPER(FIRStorageMetadata);

using CustomMetadata = std::map<std::string, std::string>;

#pragma clang assume_nonnull begin

// Private Pimpl implementation class owned by firebase::storage::Metadata via
// a heap-allocated pointer (Metadata::internal_). Its size and member layout
// are internal to libfirebase_storage and never part of the public SDK ABI.
class MetadataInternal {
 public:
  // Construct a MetadataInternal with an empty FIRStorageMetadata.
  explicit MetadataInternal(StorageInternal* _Nullable storage);

  MetadataInternal(StorageInternal* _Nullable storage,
                   std::unique_ptr<FIRStorageMetadataPointer> impl);

  MetadataInternal(const MetadataInternal& metadata);

  MetadataInternal& operator=(const MetadataInternal& metadata);

  MetadataInternal(MetadataInternal&& other) noexcept;

  MetadataInternal& operator=(MetadataInternal&& other) noexcept;

  ~MetadataInternal();

  // Return the owning Google Cloud Storage bucket for the StorageReference.
  const char* bucket() const;

  // Set the Cache Control setting of the StorageReference.
  void set_cache_control(const char* _Nullable cache_control);

  // Return the Cache Control setting of the StorageReference.
  const char* cache_control() const;

  // Set the content disposition of the StorageReference.
  void set_content_disposition(const char* _Nullable disposition);

  // Return the content disposition of the StorageReference.
  const char* content_disposition() const;

  // Set the content encoding for the StorageReference.
  void set_content_encoding(const char* _Nullable encoding);

  // Return the content encoding for the StorageReference.
  const char* content_encoding() const;

  // Set the content language for the StorageReference.
  void set_content_language(const char* _Nullable language);

  // Return the content language for the StorageReference.
  const char* content_language() const;

  // Set the content type of the StorageReference.
  void set_content_type(const char* _Nullable type);

  // Return the content type of the StorageReference.
  const char* content_type() const;

  // Return the time the StorageReference was created.
  int64_t creation_time() const;

  // Return a map of custom metadata key value pairs.
  //
  // The map is owned by this MetadataInternal and the returned pointer stays
  // valid for its whole lifetime: copy- and move-assignment replace the map's
  // contents in place rather than destroying it (a move-constructed object
  // takes over the source's map object). Like the public
  // Metadata::custom_metadata() it backs, the map itself is not thread-safe:
  // callers must not access it concurrently with each other or with any other
  // use of this Metadata that reads or replaces it (CommitCustomMetadata(),
  // copying, assignment).
  CustomMetadata* _Nullable custom_metadata() const;

  // Take the keys/values that are present in custom_metadata_ and write them
  // into the NSDictionary on the FIRStorageMetadata. This should be called
  // before you upload the metadata. Holds mutex_ while reading the map; see
  // custom_metadata() for the caller's side of the contract.
  void CommitCustomMetadata() const;

  // Return a version String indicating what version of the StorageReference.
  int64_t generation() const;

  // Return a version String indicating the version of this StorageMetadata.
  int64_t metadata_generation() const;

  // Return a simple name of the StorageReference object.
  const char* name() const;

  // Return the path of the StorageReference object.
  const char* path() const;

  // Return the associated StorageReference for which this metadata belongs to.
  StorageReferenceInternal* _Nullable GetReference() const;

  // Return the stored Size in bytes of the StorageReference object.
  int64_t size_bytes() const;

  // Return the time the StorageReference was last updated.
  int64_t updated_time() const;

  // MD5 hash of the data; encoded using base64.
  const char* md5_hash() const;

  // Gets the StorageInternal we are a part of.
  StorageInternal* _Nullable storage_internal() const {
    MutexLock lock(mutex_);
    return storage_;
  }

 private:
#ifdef __OBJC__
  // Thread-safe accessor (mutex_ is recursive, so callers that already hold
  // mutex_ may also call this safely).
  FIRStorageMetadata* _Nullable impl() const {
    MutexLock lock(mutex_);
    return impl_ ? impl_->get() : nil;
  }
#endif  // __OBJC__

  // Makes the custom metadata reflect an assignment, after impl_ has been
  // replaced: moves `source`'s contents in (if non-null) or reloads them from
  // impl(), reusing the existing map object so pointers returned by
  // custom_metadata() stay valid. Requires mutex_.
  void ReplaceCustomMetadataLocked(CustomMetadata* _Nullable source);

  friend class StorageReferenceInternal;

  mutable Mutex mutex_;

  StorageInternal* _Nullable storage_ = nullptr;

  // Object lifetime managed by Objective C ARC.
  std::unique_ptr<FIRStorageMetadataPointer> impl_;

  // Backing store for string properties. Updated under mutex_ only when the
  // underlying Objective-C string value has changed, so concurrent const
  // getter calls never reallocate an unchanged buffer. Per
  // firebase/storage/metadata.h, returned const char* pointers remain valid
  // until the corresponding property or Metadata object is modified or
  // destroyed.
  mutable std::string bucket_;
  mutable std::string cache_control_;
  mutable std::string content_disposition_;
  mutable std::string content_encoding_;
  mutable std::string content_language_;
  mutable std::string content_type_;
  mutable std::string name_;
  mutable std::string path_;
  mutable std::string md5_hash_;

  mutable std::unique_ptr<CustomMetadata> custom_metadata_;
};

#pragma clang assume_nonnull end

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_IOS_METADATA_IOS_H_
