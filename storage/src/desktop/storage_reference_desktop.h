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
#include <cstddef>
#include <functional>
#include <memory>
#include <string>

#include "app/rest/request.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/desktop/storage_path.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

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
  kStorageReferenceFnListAll,
  kStorageReferenceFnCount,
};

class BlockingResponse;
class ControllerBinding;
struct FutureApiOwner;
struct ListAllChainData;
struct ListenerSharedState;
struct MetadataChainData;
class Notifier;
class StorageInternal;

class StorageReferenceInternal {
 public:
  StorageReferenceInternal(const std::string& storage_uri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StoragePath& storage_uri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StorageReferenceInternal& other);
  StorageReferenceInternal& operator=(const StorageReferenceInternal& other);
  StorageReferenceInternal(StorageReferenceInternal&& other) noexcept;
  StorageReferenceInternal& operator=(
      StorageReferenceInternal&& other) noexcept;

  ~StorageReferenceInternal();

  // Gets the storage to which we refer.
  Storage* storage() const;

  // Ownership convention for the factory methods below (Child(), GetParent()):
  // they return a newly allocated object (or null) and transfer ownership to
  // the caller. They return a raw pointer rather than std::unique_ptr because
  // the platform-independent layer (storage/src/common/storage_reference.cc,
  // shared with the Android and iOS implementations) adopts the result
  // directly via StorageReference(StorageReferenceInternal*), which then owns
  // and deletes it. Desktop-internal callers must adopt the result
  // immediately, e.g. with Adopt() or a std::unique_ptr.

  // Gets a reference to a location relative to this one, or null if `path`
  // is null. The caller owns the result (see above).
  StorageReferenceInternal* Child(const char* path) const;

  // Return the Google Cloud Storage bucket that holds this object.
  std::string bucket() const;

  // Return the full path of the object.
  std::string full_path() const;

  // Returns the short name of this object.
  std::string name() const;

  // Returns a new instance pointing to the parent location, or null if this
  // instance references the root location. The caller owns the result (see
  // the ownership convention above Child()).
  StorageReferenceInternal* GetParent() const;

  // Deletes the object at the current path.
  Future<void> Delete();

  // Returns the result of the most recent call to Delete();
  Future<void> DeleteLastResult() const;

  // Asynchronously downloads the object from this StorageReference.
  Future<size_t> GetFile(const char* path, Listener* listener,
                         Controller* controller_out);

  // Returns the result of the most recent call to GetFile();
  Future<size_t> GetFileLastResult() const;

  // Asynchronously downloads the object from this StorageReference.
  Future<size_t> GetBytes(void* buffer, size_t buffer_size, Listener* listener,
                          Controller* controller_out);

  // Returns the result of the most recent call to GetBytes();
  Future<size_t> GetBytesLastResult() const;

  // Asynchronously retrieves a long lived download URL with a revokable token.
  Future<std::string> GetDownloadUrl();

  // Returns the result of the most recent call to GetDownloadUrl();
  Future<std::string> GetDownloadUrlLastResult() const;

  // Retrieves metadata associated with an object at this StorageReference.
  Future<Metadata> GetMetadata();

  // Returns the result of the most recent call to GetMetadata();
  Future<Metadata> GetMetadataLastResult() const;

  // Updates the metadata associated with this StorageReference.
  Future<Metadata> UpdateMetadata(const Metadata* metadata);

  // Returns the result of the most recent call to UpdateMetadata();
  Future<Metadata> UpdateMetadataLastResult() const;

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            Listener* listener, Controller* controller_out);

  // Asynchronously uploads data to the currently specified StorageReference,
  // with additional metadata.
  Future<Metadata> PutBytes(const void* buffer, size_t buffer_size,
                            const Metadata* metadata, Listener* listener,
                            Controller* controller_out);

  // Returns the result of the most recent call to PutBytes();
  Future<Metadata> PutBytesLastResult() const;

  // Asynchronously uploads data to the currently specified StorageReference,
  // without additional metadata.
  Future<Metadata> PutFile(const char* path, Listener* listener,
                           Controller* controller_out);

  // Asynchronously uploads data to the currently specified StorageReference,
  // with additional metadata.
  Future<Metadata> PutFile(const char* path, const Metadata* metadata,
                           Listener* listener, Controller* controller_out);

  // Returns the result of the most recent call to PutFile();
  Future<Metadata> PutFileLastResult() const;

  // List items (files) and prefixes (folders) under this StorageReference.
  Future<StorageListResult> List(int max_results_per_page,
                                 const char* page_token);

  // Returns the result of the most recent call to List();
  Future<StorageListResult> ListLastResult() const;

  // List all items (files) and prefixes (folders) under this StorageReference.
  Future<StorageListResult> ListAll();

  // Returns the result of the most recent call to ListAll();
  Future<StorageListResult> ListAllLastResult() const;

  // Pointer to the StorageInternal instance we are a part of.
  StorageInternal* storage_internal() const { return storage_; }

  // Wrap this in a StorageReference.
  // Exposed for testing.
  StorageReference AsStorageReference() const;

  // Wraps `internal` in a StorageReference, which takes ownership of it
  // without copying. A null `internal` yields an invalid StorageReference.
  static StorageReference Adopt(
      std::unique_ptr<StorageReferenceInternal> internal);

 private:
  struct RequestAttempt {
    FutureBase future;
    std::shared_ptr<std::atomic<int>> http_status;
    bool local_failure = false;
  };

  // Function type that sends a REST request and returns its RequestAttempt
  // handle and shared HTTP status cell.
  using SendRequestFunct =
      std::function<RequestAttempt(Controller* attempt_controller_out)>;

  template <typename FutureType>
  void SendRequestWithRetry(SendRequestFunct send_request_funct,
                            SafeFutureHandle<FutureType> final_handle,
                            double max_retry_time_seconds,
                            Controller* controller_out = nullptr);

  template <typename FutureType>
  static void AsyncSendRequestWithRetry(
      StorageReference self_ref,
      std::shared_ptr<FutureApiOwner> future_api_owner,
      SendRequestFunct send_request_funct,
      SafeFutureHandle<FutureType> final_handle, RequestAttempt first_attempt,
      double max_retry_time_seconds, bool retry_all_errors,
      std::shared_ptr<ControllerBinding> binding);

  // Returns whether or not an HTTP status indicates a retryable failure.
  static bool IsRetryableFailure(int http_status, bool retry_all_errors);

  // Upload data without metadata.
  Future<Metadata> PutBytesInternal(const void* buffer, size_t buffer_size,
                                    Listener* listener,
                                    Controller* controller_out,
                                    const char* content_type = nullptr);
  // Upload file without metadata.
  Future<Metadata> PutFileInternal(const char* path, Listener* listener,
                                   Controller* controller_out,
                                   const char* content_type = nullptr);

  void RestCall(rest::Request* request, Notifier* request_notifier,
                BlockingResponse* response, FutureHandle handle,
                Listener* listener,
                const std::shared_ptr<ListenerSharedState>& listener_state,
                Controller* controller_out);

  void PrepareRequestBlocking(rest::Request* request, const char* url,
                              const char* method,
                              const char* content_type = nullptr);

  void SetupMetadataChain(Future<Metadata> starting_future,
                          const std::shared_ptr<MetadataChainData>& data);

  static void ContinueListAll(Future<StorageListResult> page_future,
                              const std::shared_ptr<ListAllChainData>& data);

  ReferenceCountedFutureImpl* future() const;

  // Storage references are frequently duplicated.  Please avoid storing any
  // more state in them than is absolutely necessary, as it will be multiplied
  // over many allocations/copies.

  StorageInternal* storage_ = nullptr;
  StoragePath storage_uri_;
  std::shared_ptr<FutureApiOwner> future_api_owner_;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
