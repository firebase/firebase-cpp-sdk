// Copyright 2016 Google LLC
#ifndef FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_
#define FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_

#include <cstddef>
#include <string>
#include <vector>

#include "firebase/future.h"
#include "firebase/internal/common.h"
#include "firebase/storage/list_result.h"
#include "firebase/storage/metadata.h"

namespace firebase {
namespace storage {

class Controller;
class Listener;
class Storage;

namespace internal {
class ControllerInternal;
class MetadataInternal;
class StorageInternal;
class StorageReferenceInternalCommon;
class StorageReferenceInternal;
}  // namespace internal

class StorageReference {
 public:
  StorageReference() : internal_(nullptr) {}
  ~StorageReference();
  StorageReference(const StorageReference& reference);
  StorageReference& operator=(const StorageReference& reference);
  StorageReference(StorageReference&& other) noexcept;
  StorageReference& operator=(StorageReference&& other) noexcept;

  Storage* storage() const;
  StorageReference Child(const char* path) const;
  StorageReference Child(const std::string& path) const {
    return Child(path.c_str());
  }

  Future<void> Delete();
  Future<void> DeleteLastResult() const;

  std::string bucket() const;
  std::string full_path() const;

  Future<size_t> GetFile(const char* path, Listener* listener = nullptr,
                         Controller* controller_out = nullptr);
#ifndef SWIG
  Future<size_t> GetFile(const std::string& path, Listener* listener = nullptr,
                         Controller* controller_out = nullptr) {
    return GetFile(path.c_str(), listener, controller_out);
  }
#endif  // SWIG
  Future<size_t> GetFileLastResult() const;

  Future<size_t> GetBytes(void* buffer, size_t buffer_size,
                          Listener* listener = nullptr,
                          Controller* controller_out = nullptr);
  Future<size_t> GetBytesLastResult() const;

  Future<std::string> GetDownloadUrl();
  Future<std::string> GetDownloadUrlLastResult() const;

  Future<Metadata> GetMetadata();
  Future<Metadata> GetMetadataLastResult() const;

  Future<Metadata> UpdateMetadata(const Metadata& metadata);
  Future<Metadata> UpdateMetadataLastResult() const;

  Future<StorageListResult> List(int max_results_per_page = 1000,
                                 const char* page_token = nullptr);
#ifndef SWIG
  Future<StorageListResult> List(int max_results_per_page,
                                 const std::string& page_token) {
    return List(max_results_per_page,
                page_token.empty() ? nullptr : page_token.c_str());
  }
#endif  // SWIG
  Future<StorageListResult> ListLastResult() const;

  Future<StorageListResult> ListAll();
  Future<StorageListResult> ListAllLastResult() const;

  std::string name() const;
  StorageReference GetParent() const;
  StorageReference GetRoot() const;
  bool is_root() const;

  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            Listener* listener = nullptr,
                            Controller* controller_out = nullptr);
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            const Metadata& metadata,
                            Listener* listener = nullptr,
                            Controller* controller_out = nullptr);
  Future<Metadata> PutBytesLastResult() const;

  Future<Metadata> PutFile(const char* path, Listener* listener = nullptr,
                           Controller* controller_out = nullptr);
#ifndef SWIG
  Future<Metadata> PutFile(const std::string& path,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr) {
    return PutFile(path.c_str(), listener, controller_out);
  }
#endif  // SWIG
  Future<Metadata> PutFile(const char* path, const Metadata& metadata,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr);
#ifndef SWIG
  Future<Metadata> PutFile(const std::string& path, const Metadata& metadata,
                           Listener* listener = nullptr,
                           Controller* controller_out = nullptr) {
    return PutFile(path.c_str(), metadata, listener, controller_out);
  }
#endif  // SWIG
  Future<Metadata> PutFileLastResult() const;

  bool is_valid() const;

#ifndef SWIG
  std::string ToString() const;
  bool operator==(const StorageReference& other) const;
  bool operator!=(const StorageReference& other) const {
    return !(*this == other);
  }
#endif  // SWIG

#if defined(FIREBASE_STORAGE_BUILDING_SDK) && !defined(SWIG) && \
    !defined(DOXYGEN)
  Storage* storage();
  Future<void> DeleteLastResult();
  std::string bucket();
  std::string full_path();
  Future<size_t> GetFileLastResult();
  Future<size_t> GetBytesLastResult();
  Future<std::string> GetDownloadUrlLastResult();
  Future<Metadata> GetMetadataLastResult();
  Future<Metadata> UpdateMetadataLastResult();
  Future<StorageListResult> ListLastResult();
  std::string name();
  StorageReference GetParent();
  Future<Metadata> PutBytesLastResult();
  Future<Metadata> PutFileLastResult();
#endif

 private:
  friend class Controller;
  friend class internal::ControllerInternal;
  friend class Metadata;
  friend class internal::MetadataInternal;
  friend class Storage;
  friend class internal::StorageReferenceInternal;
  friend class internal::StorageReferenceInternalCommon;

  StorageReference(internal::StorageReferenceInternal* internal);  // NOLINT
  internal::StorageReferenceInternal* internal_;
};

}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_INCLUDE_FIREBASE_STORAGE_STORAGE_REFERENCE_H_
