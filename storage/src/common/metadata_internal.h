// Copyright 2017 Google LLC
#ifndef FIREBASE_STORAGE_SRC_COMMON_METADATA_INTERNAL_H_
#define FIREBASE_STORAGE_SRC_COMMON_METADATA_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

class MetadataInternal {
 public:
  static constexpr const char* kContentLanguageKey = "contentLanguage";
  static constexpr const char* kContentEncodingKey = "contentEncoding";
  static constexpr const char* kContentDispositionKey = "contentDisposition";
  static constexpr const char* kCacheControlKey = "cacheControl";
  static constexpr const char* kMetadataKey = "metadata";
  static constexpr const char* kContentTypeKey = "contentType";
  static constexpr const char* kDownloadTokensKey = "downloadTokens";
  static constexpr const char* kMd5HashKey = "md5Hash";
  static constexpr const char* kSizeKey = "size";
  static constexpr const char* kTimeUpdatedKey = "updated";
  static constexpr const char* kTimeCreatedKey = "timeCreated";
  static constexpr const char* kMetaGenerationKey = "metageneration";
  static constexpr const char* kBucketKey = "bucket";
  static constexpr const char* kNameKey = "name";
  static constexpr const char* kGenerationKey = "generation";

  explicit MetadataInternal(const StorageReference& storage_reference)
      : storage_reference_(storage_reference),
        path_(storage_reference.full_path()),
        name_(storage_reference.name()),
        bucket_(storage_reference.bucket()) {}

  explicit MetadataInternal(StorageReferenceInternal* storage_reference)
      : MetadataInternal(StorageReference(storage_reference)) {}

  explicit MetadataInternal(std::nullptr_t) : storage_reference_(nullptr) {}

  MetadataInternal(const MetadataInternal&) = default;
  MetadataInternal& operator=(const MetadataInternal&) = default;
  MetadataInternal(MetadataInternal&&) noexcept = default;
  MetadataInternal& operator=(MetadataInternal&&) noexcept = default;
  ~MetadataInternal() = default;

  const char* bucket() const { return bucket_.c_str(); }
  const char* path() const { return path_.c_str(); }
  const char* name() const { return name_.c_str(); }
  const char* md5_hash() const { return md5_hash_.c_str(); }
  int64_t creation_time() const { return creation_time_; }
  int64_t updated_time() const { return updated_time_; }
  int64_t generation() const { return generation_; }
  int64_t metadata_generation() const { return metadata_generation_; }
  int64_t size_bytes() const { return size_bytes_; }

  void set_cache_control(const char* v) {
    cache_control_ = v ? v : "";
    has_cache_control_ = true;
  }
  const char* cache_control() const { return cache_control_.c_str(); }

  void set_content_disposition(const char* v) {
    content_disposition_ = v ? v : "";
    has_content_disposition_ = true;
  }
  const char* content_disposition() const {
    return content_disposition_.c_str();
  }

  void set_content_encoding(const char* v) {
    content_encoding_ = v ? v : "";
    has_content_encoding_ = true;
  }
  const char* content_encoding() const { return content_encoding_.c_str(); }

  void set_content_language(const char* v) {
    content_language_ = v ? v : "";
    has_content_language_ = true;
  }
  const char* content_language() const { return content_language_.c_str(); }

  void set_content_type(const char* v) {
    content_type_ = v ? v : "";
    has_content_type_ = true;
  }
  const char* content_type() const { return content_type_.c_str(); }

  std::map<std::string, std::string>* custom_metadata() {
    return &custom_metadata_;
  }
  const std::map<std::string, std::string>* custom_metadata() const {
    return &custom_metadata_;
  }
  const std::vector<std::string>& download_tokens() const {
    return download_tokens_;
  }

  void set_bucket(const std::string& v) { bucket_ = v; }
  void set_path(const std::string& v) { path_ = v; }
  void set_name(const std::string& v) { name_ = v; }
  void set_generation(int64_t v) { generation_ = v; }
  void set_metadata_generation(int64_t v) { metadata_generation_ = v; }
  void set_creation_time(int64_t v) { creation_time_ = v; }
  void set_updated_time(int64_t v) { updated_time_ = v; }
  void set_size_bytes(int64_t v) { size_bytes_ = v; }
  void set_md5_hash(const std::string& v) { md5_hash_ = v; }

  bool ImportFromJson(const char* json);
  std::string ExportAsJson() const;

  static Metadata AsMetadata(MetadataInternal* metadata_internal) {
    return Metadata(metadata_internal);
  }

  StorageReference GetReference() const { return storage_reference_; }
  StorageReferenceInternal* reference_internal() const {
    return storage_reference_.internal_;
  }

 private:
  static int64_t GetTimeFromTimeString(const std::string& time_str);
  static std::string LookUpString(const Variant& root, const char* key,
                                  const char* default_value = nullptr);
  static bool LookUpOptionalString(const Variant& root, const char* key,
                                   std::string* value);
  static int64_t LookUpInt64(const Variant& root, const char* key);

  StorageReference storage_reference_;
  std::string path_;
  std::string name_;
  std::string bucket_;
  std::string cache_control_;
  std::string content_type_;
  int64_t generation_ = -1;
  int64_t metadata_generation_ = -1;
  int64_t creation_time_ = -1;
  int64_t updated_time_ = -1;
  int64_t size_bytes_ = -1;
  std::string md5_hash_;
  std::string content_disposition_;
  std::string content_encoding_;
  std::string content_language_;
  bool has_cache_control_ = false;
  bool has_content_disposition_ = false;
  bool has_content_encoding_ = false;
  bool has_content_language_ = false;
  bool has_content_type_ = false;
  std::map<std::string, std::string> custom_metadata_;
  std::vector<std::string> download_tokens_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_COMMON_METADATA_INTERNAL_H_
