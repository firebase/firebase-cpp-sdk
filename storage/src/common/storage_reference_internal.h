#ifndef FIREBASE_STORAGE_SRC_COMMON_STORAGE_REFERENCE_INTERNAL_H_
#define FIREBASE_STORAGE_SRC_COMMON_STORAGE_REFERENCE_INTERNAL_H_

#include <string>
#include <vector>

#include "firebase/future.h"
#include "firebase/storage/metadata.h"
// Listener and Controller are used in method signatures, need full declaration or forward
#include "firebase/storage/listener.h"
#include "firebase/storage/controller.h"

namespace firebase {
namespace storage {

// Forward declarations from public API
class Storage;
class ListResult;
class StorageReference;


namespace internal {

class StorageInternal; // Platform-specific internal helper for Storage.

// Defines the common internal interface for StorageReference.
// Platform-specific implementations (Desktop, Android, iOS) will derive from this.
class StorageReferenceInternal {
 public:
  virtual ~StorageReferenceInternal() = default;

  // Interface methods mirroring the public StorageReference API
  virtual Storage* storage() const = 0;
  virtual StorageReferenceInternal* Child(const char* path) const = 0;
  virtual std::string bucket() const = 0;
  virtual std::string full_path() const = 0;
  virtual std::string name() = 0; // Desktop implementation is not const
  virtual StorageReferenceInternal* GetParent() = 0; // Desktop implementation is not const

  virtual Future<void> Delete() = 0;
  virtual Future<void> DeleteLastResult() = 0;

  virtual Future<size_t> GetFile(const char* path, Listener* listener,
                                 Controller* controller_out) = 0;
  virtual Future<size_t> GetFileLastResult() = 0;

  virtual Future<size_t> GetBytes(void* buffer, size_t buffer_size,
                                  Listener* listener,
                                  Controller* controller_out) = 0;
  virtual Future<size_t> GetBytesLastResult() = 0;

  virtual Future<std::string> GetDownloadUrl() = 0;
  virtual Future<std::string> GetDownloadUrlLastResult() = 0;

  virtual Future<Metadata> GetMetadata() = 0;
  virtual Future<Metadata> GetMetadataLastResult() = 0;

  virtual Future<Metadata> UpdateMetadata(const Metadata* metadata) = 0;
  virtual Future<Metadata> UpdateMetadataLastResult() = 0;

  virtual Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                                    Listener* listener,
                                    Controller* controller_out) = 0;
  virtual Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                                    const Metadata* metadata, Listener* listener,
                                    Controller* controller_out) = 0;
  virtual Future<Metadata> PutBytesLastResult() = 0;

  virtual Future<Metadata> PutFile(const char* path, Listener* listener,
                                   Controller* controller_out) = 0;
  virtual Future<Metadata> PutFile(const char* path, const Metadata* metadata,
                                   Listener* listener,
                                   Controller* controller_out) = 0;
  virtual Future<Metadata> PutFileLastResult() = 0;

  virtual Future<ListResult> List(int32_t max_results) = 0;
  virtual Future<ListResult> List(int32_t max_results,
                                  const char* page_token) = 0;
  virtual Future<ListResult> ListAll() = 0;
  virtual Future<ListResult> ListLastResult() = 0;

  // Common utility methods needed by StorageReference and platform implementations
  virtual StorageInternal* storage_internal() const = 0;
  virtual StorageReferenceInternal* Clone() const = 0;

 protected:
  StorageReferenceInternal() = default;

 private:
  // Public class is a friend to access constructor & internal_
  friend class firebase::storage::StorageReference;

  // Disallow copy/move of the base class directly; cloning is explicit.
  StorageReferenceInternal(const StorageReferenceInternal&) = delete;
  StorageReferenceInternal& operator=(const StorageReferenceInternal&) = delete;
  StorageReferenceInternal(StorageReferenceInternal&&) = delete;
  StorageReferenceInternal& operator=(StorageReferenceInternal&&) = delete;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_COMMON_STORAGE_REFERENCE_INTERNAL_H_
