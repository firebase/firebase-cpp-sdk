// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_METADATA_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_METADATA_H_

#include <stdint.h>

#include <map>
#include <string>

#include "firebase/internal/common.h"

namespace firebase {
namespace storage {

namespace internal {
class MetadataInternal;
class MetadataInternalCommon;
class StorageReferenceInternal;
}  // namespace internal

class StorageReference;

class Metadata {
 public:
  Metadata();
  Metadata(const Metadata& other);
  Metadata& operator=(const Metadata& other);
  Metadata(Metadata&& other) noexcept;
  Metadata& operator=(Metadata&& other) noexcept;
  ~Metadata();

  const char* bucket() const;

  void set_cache_control(const char* cache_control);
  void set_cache_control(const std::string& cache_control) {
    set_cache_control(cache_control.c_str());
  }
  const char* cache_control() const;

  void set_content_disposition(const char* disposition);
  void set_content_disposition(const std::string& disposition) {
    set_content_disposition(disposition.c_str());
  }
  const char* content_disposition() const;

  void set_content_encoding(const char* encoding);
  void set_content_encoding(const std::string& encoding) {
    set_content_encoding(encoding.c_str());
  }
  const char* content_encoding() const;

  void set_content_language(const char* language);
  void set_content_language(const std::string& language) {
    set_content_language(language.c_str());
  }
  const char* content_language() const;

  void set_content_type(const char* type);
  void set_content_type(const std::string& type) {
    set_content_type(type.c_str());
  }
  const char* content_type() const;

  int64_t creation_time() const;
  std::map<std::string, std::string>* custom_metadata() const;
  int64_t generation() const;
  int64_t metadata_generation() const;
  const char* name() const;
  const char* path() const;
  StorageReference GetReference() const;
  int64_t size_bytes() const;
  int64_t updated_time() const;
  bool is_valid() const;
  const char* md5_hash() const;

#ifndef SWIG
  bool operator==(const Metadata& other) const;
  bool operator!=(const Metadata& other) const { return !(*this == other); }
#endif  // SWIG

 private:
  friend class StorageReference;
  friend class internal::MetadataInternal;
  friend class internal::MetadataInternalCommon;
  friend class internal::StorageReferenceInternal;

  explicit Metadata(internal::MetadataInternal* internal);
  internal::MetadataInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_METADATA_H_
