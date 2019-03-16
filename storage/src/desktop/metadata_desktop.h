// Copyright 2017 Google LLC
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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_METADATA_DESKTOP_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_METADATA_DESKTOP_H_

#include <list>
#include <map>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

class StorageInternal;
class StorageReferenceInternal;

class MetadataInternal {
 public:
  static const char* kContentLanguageKey;
  static const char* kContentEncodingKey;
  static const char* kContentDispositionKey;
  static const char* kCacheControlKey;
  static const char* kMetadataKey;
  static const char* kContentTypeKey;
  static const char* kDownloadTokensKey;
  static const char* kMd5HashKey;
  static const char* kSizeKey;
  static const char* kTimeUpdatedKey;
  static const char* kTimeCreatedKey;
  static const char* kMetaGenerationKey;
  static const char* kBucketKey;
  static const char* kNameKey;
  static const char* kGenerationKey;

  MetadataInternal(const StorageReference& storage_reference);

  MetadataInternal(const MetadataInternal& metadata);

  MetadataInternal& operator=(const MetadataInternal& metadata);

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
  MetadataInternal(MetadataInternal&& other);

  MetadataInternal& operator=(MetadataInternal&& other);
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

  ~MetadataInternal() {}

  // Return the owning Google Cloud Storage bucket for the StorageReference.
  const char* bucket() { return bucket_.c_str(); }

  // Return the path of the StorageReference object.
  const char* path() { return path_.c_str(); }

  // Set the Cache Control setting of the StorageReference.
  void set_cache_control(const char* cache_control) {
    cache_control_ = cache_control;
  }

  // Return the Cache Control setting of the StorageReference.
  const char* cache_control() const { return cache_control_.c_str(); }

  // Set the content disposition of the StorageReference.
  void set_content_disposition(const char* content_disposition) {
    content_disposition_ = content_disposition;
  }

  // Return the content disposition of the StorageReference.
  const char* content_disposition() const {
    return content_disposition_.c_str();
  }

  // Set the content encoding for the StorageReference.
  void set_content_encoding(const char* content_encoding) {
    content_encoding_ = content_encoding;
  }

  // Return the content encoding for the StorageReference.
  const char* content_encoding() const { return content_encoding_.c_str(); }

  // Set the content language for the StorageReference.
  void set_content_language(const char* language) {
    content_language_ = language;
  }

  // Return the content language for the StorageReference.
  const char* content_language() const { return content_language_.c_str(); }

  // Set the content type of the StorageReference.
  void set_content_type(const char* content_type) {
    content_type_ = content_type;
  }

  // Return the content type of the StorageReference.
  const char* content_type() const { return content_type_.c_str(); }

  // Return the time the StorageReference was created.
  int64_t creation_time() const { return creation_time_; }

  // Return a map of custom metadata key value pairs.
  std::map<std::string, std::string>* custom_metadata() {
    return &custom_metadata_;
  }

  // Returns a long lived download URL with a revokable token.
  // This should be for internal use only until we re-implement
  // StorageReferenceInternal::GetDownloadUrl() (see b/78908154)
  const char* download_url() const;

  // Return a version String indicating what version of the StorageReference.
  int64_t generation() const { return generation_; }

  // Return a version String indicating the version of this StorageMetadata.
  int64_t metadata_generation() const { return metadata_generation_; }

  // Return a simple name of the StorageReference object.  (Or an empty string
  // if there isn't one.)
  const char* name() { return name_.c_str(); }

  // Return the associated StorageReference for which this metadata belongs to.
  StorageReferenceInternal* GetReference() const;

  // Return the stored Size in bytes of the StorageReference object.
  int64_t size_bytes() const { return size_bytes_; }

  // Return the time the StorageReference was last updated.
  int64_t updated_time() const { return updated_time_; }

  // Gets the StorageInternal we are a part of.
  StorageInternal* storage_internal() const { return storage_internal_; }

  const char* md5_hash() { return md5_hash_.c_str(); }

  // Special method to create an invalid Metadata, because Metadata's default
  // constructor now gives us a valid one.
  static Metadata GetInvalidMetadata() { return Metadata(nullptr); }

  // Import and export functions for json text.
  bool ImportFromJson(const char* json);
  std::string ExportAsJson();

 private:
  // Cached the StorageInternal from the wrapped StorageReference object.
  void UpdateStorageInternal();

 public:
  // Converts a metadata internal pointer into a metadata object.
  static Metadata AsMetadata(MetadataInternal* metadata_internal) {
    return Metadata(metadata_internal);
  }

 private:
  // The storage reference this metadata belongs to.
  StorageReference storage_reference_;
  // Cached storage_internal_ this metadata belongs to.
  // If storage_reference is invalidated before this object is invalidated this
  // is required to remove the Metadata object from the Storage object's cleanup
  // notifier.
  StorageInternal* storage_internal_;

  std::string path_;
  std::string name_;
  std::string bucket_;
  std::string cache_control_;
  std::string content_type_;
  int64_t generation_;
  int64_t metadata_generation_;
  int64_t creation_time_;
  int64_t updated_time_;
  int64_t size_bytes_;
  std::string md5_hash_;
  std::string content_disposition_;
  std::string content_encoding_;
  std::string content_language_;
  std::map<std::string, std::string> custom_metadata_;
  std::list<std::string> download_tokens_;
  // Holder for the first entry in the download_tokens_ list,
  // as a downloadable URL.
  std::string download_url_;

  std::string GetPathFromToken(const std::string& token) const;

  static int64_t GetTimeFromTimeString(const std::string& time_str);

  // Convenience functions for populating metadata from json.
  std::string LookUpString(Variant* root, const char* key,
                           const char* default_value = nullptr);
  int64_t LookUpInt64(Variant* root, const char* key);
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_METADATA_DESKTOP_H_
