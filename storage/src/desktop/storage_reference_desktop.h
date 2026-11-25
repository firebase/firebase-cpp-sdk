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

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

enum StorageReferenceFn {
  kStorageReferenceFnDelete = 0,
  kStorageReferenceFnDeleteInternal,
  kStorageReferenceFnGetBytes,
  kStorageReferenceFnGetBytesInternal,
  kStorageReferenceFnGetFile,
  kStorageReferenceFnGetFileInternal,
  kStorageReferenceFnGetDownloadUrl,
  kStorageReferenceFnGetMetadata,
  kStorageReferenceFnGetMetadataInternal,
  kStorageReferenceFnUpdateMetadata,
  kStorageReferenceFnUpdateMetadataInternal,
  kStorageReferenceFnPutBytes,
  kStorageReferenceFnPutBytesInternal,
  kStorageReferenceFnPutFile,
  kStorageReferenceFnPutFileInternal,
  kStorageReferenceFnList,
  kStorageReferenceFnListInternal,
  kStorageReferenceFnCount,
};

class BlockingResponse;
class MetadataChainData;
class Notifier;

class StorageReferenceInternal {
 public:
  StorageReferenceInternal(const std::string& storageUri,
                           std::shared_ptr<StorageInternal> storage);
  StorageReferenceInternal(const StoragePath& storageUri,
                           std::shared_ptr<StorageInternal> storage);
  StorageReferenceInternal(const StorageReferenceInternal& other);

  ~StorageReferenceInternal();

  // Creates a non-owning reference holding weak_ptr<StorageInternal> so
  // internal objects owned by StorageInternal (RestOperation/Controller,
  // StorageListResult stored in FutureManager) do not create reference cycles.
  static StorageReferenceInternal* CreateWeak(
      const StoragePath& storageUri, std::weak_ptr<StorageInternal> storage);

  bool is_valid() const {
    return strong_storage_ != nullptr || !weak_storage_.expired();
  }

  // Gets the storage to which we refer.
  Storage storage() const;

  // Gets a reference to a location relative to this one.
  StorageReferenceInternal* Child(const char* path) const;

  // Return the Google Cloud Storage bucket that holds this object.
  std::string bucket() const;

  // Return the full path of the object.
  std::string full_path() const;

  // Returns the short name of this object.
  std::string name();

  // Returns a new instance of StorageReference pointing to the parent location
  // or null if this instance references the root location.
  StorageReferenceInternal* GetParent();

  // Deletes the object at the current path.
  Future<void> Delete();

  // Returns the result of the most recent call to Delete();
  Future<void> DeleteLastResult();

  // Asynchronously downloads the object from this StorageReference.
  Future<size_t> GetFile(const char* path, Listener* listener,
                         Controller* controller_out);

  // Returns the result of the most recent call to GetFile();
  Future<size_t> GetFileLastResult();

  // Asynchronously downloads the object from this StorageReference.
  Future<size_t> GetBytes(void* buffer, size_t buffer_size, Listener* listener,
                          Controller* controller_out);

  // Returns the result of the most recent call to GetBytes();
  Future<size_t> GetBytesLastResult();

  // Asynchronously retrieves a long lived download URL with a revokable token.
  Future<std::string> GetDownloadUrl();

  // Returns the result of the most recent call to GetDownloadUrl();
  Future<std::string> GetDownloadUrlLastResult();

  // Retrieves metadata associated with an object at this StorageReference.
  Future<Metadata> GetMetadata();

  // Returns the result of the most recent call to GetMetadata();
  Future<Metadata> GetMetadataLastResult();

  // Updates the metadata associated with this StorageReference.
  Future<Metadata> UpdateMetadata(const Metadata* metadata);

  // Returns the result of the most recent call to UpdateMetadata();
  Future<Metadata> UpdateMetadataLastResult();

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            Listener* listener, Controller* controller_out);

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            const Metadata* metadata, Listener* listener,
                            Controller* controller_out);

  // Returns the result of the most recent call to Write();
  Future<Metadata> PutBytesLastResult();

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutFile(const char* path, Listener* listener,
                           Controller* controller_out);

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutFile(const char* path, const Metadata* metadata,
                           Listener* listener, Controller* controller_out);

  // Returns the result of the most recent call to Write();
  Future<Metadata> PutFileLastResult();

  // List items (files) and prefixes (folders) under this StorageReference.
  Future<StorageListResult> List(int max_results_per_page,
                                 const char* page_token);

  // Returns the result of the most recent call to List();
  Future<StorageListResult> ListLastResult();

  // Returns a strong shared_ptr to the owning StorageInternal if still alive.
  std::shared_ptr<StorageInternal> LockStorage() const {
    return strong_storage_ ? strong_storage_ : weak_storage_.lock();
  }

  // Returns the non-owning weak handle to the owning StorageInternal without
  // temporarily locking a strong reference.
  std::weak_ptr<StorageInternal> weak_storage() const { return weak_storage_; }

  // Non-owning raw pointer for internal callers that already hold a lock.
  StorageInternal* storage_internal() const { return LockStorage().get(); }

  // Wrap this in a StorageReference.
  // Exposed for testing.
  StorageReference AsStorageReference() const;

  // Wrap this in a non-owning StorageReference (for RestOperation/Controller).
  StorageReference AsWeakStorageReference() const;

  // Create a non-owning StorageReference directly (e.g. for StorageListResult).
  static StorageReference CreateWeakReference(
      const StoragePath& storageUri, std::weak_ptr<StorageInternal> storage);

  // Creates a strong (owning) copy of this StorageReferenceInternal, or
  // nullptr if the underlying StorageInternal has expired.
  StorageReferenceInternal* CloneStrong() const;

  // Creates a non-owning (weak) copy of this StorageReferenceInternal so
  // internal copies (StorageListResultInternal, ControllerInternal) never
  // create a reference cycle with StorageInternal / AppState.
  StorageReferenceInternal* CloneWeak() const;

  // Copies a StorageReference weakly without promoting to a strong handle.
  static StorageReference CloneWeakReference(const StorageReference& ref);

 private:
  struct WeakTag {};
  StorageReferenceInternal(WeakTag, const StoragePath& storageUri,
                           std::weak_ptr<StorageInternal> storage);

  // Function type that sends a single REST attempt using an isolated
  // per-attempt future API and atomic HTTP status slot. Must not capture
  // `this` or any stack references.
  typedef std::function<void(const std::shared_ptr<StorageInternal>&,
                             const std::shared_ptr<ReferenceCountedFutureImpl>&,
                             const std::shared_ptr<std::atomic<int>>&,
                             Controller*)>
      SendRequestFunct;

  template <typename FutureType>
  static void SendRequestWithRetry(
      const std::shared_ptr<StorageInternal>& storage,
      std::weak_ptr<StorageInternal> weak_storage,
      SendRequestFunct send_request_funct,
      std::shared_ptr<detail::FutureImplCore> final_future_core,
      SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
      Controller* first_attempt_controller_out = nullptr);

  template <typename FutureType>
  static void AsyncSendRequestWithRetry(
      std::weak_ptr<StorageInternal> weak_storage,
      SendRequestFunct send_request_funct,
      std::shared_ptr<detail::FutureImplCore> final_future_core,
      SafeFutureHandle<FutureType> final_handle,
      std::shared_ptr<ReferenceCountedFutureImpl> attempt_api,
      std::shared_ptr<std::atomic<int>> http_status,
      double max_retry_time_seconds);

  // Returns whether or not an HTTP status or future error indicates a retryable
  // failure.
  static bool IsRetryableFailure(int httpStatus);

  // Upload data without metadata.
  Future<Metadata> PutBytesInternal(const void* buffer, size_t buffer_size,
                                    Listener* listener,
                                    Controller* controller_out,
                                    const char* content_type = nullptr);
  // Upload file without metadata.
  Future<Metadata> PutFileInternal(const char* path, Listener* listener,
                                   Controller* controller_out,
                                   const char* content_type = nullptr);

  static void RestCall(const std::shared_ptr<StorageInternal>& storage,
                       const StoragePath& storage_uri,
                       const std::weak_ptr<StorageInternal>& weak_storage,
                       rest::Request* request,
                       internal::Notifier* request_notifier,
                       BlockingResponse* response, FutureHandle handle,
                       Listener* listener, Controller* controller_out);

  static void PrepareRequestBlocking(
      const std::shared_ptr<StorageInternal>& storage, rest::Request* request,
      const char* url, const char* method, const char* content_type = nullptr);

  void SetupMetadataChain(Future<Metadata> starting_future,
                          std::shared_ptr<MetadataChainData> data);

  ReferenceCountedFutureImpl* future();

  // Storage references are frequently duplicated.  Please avoid storing any
  // more state in them than is absolutely necessary, as it will be multiplied
  // over many allocations/copies.

  std::shared_ptr<StorageInternal> strong_storage_;
  std::weak_ptr<StorageInternal> weak_storage_;
  StoragePath storageUri_;
  std::atomic<bool> has_future_api_{false};
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
