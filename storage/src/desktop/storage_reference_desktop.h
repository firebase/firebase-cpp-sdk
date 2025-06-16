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
  kStorageReferenceFnCount,
};

class BlockingResponse;
class MetadataChainData;
class Notifier;

class StorageReferenceInternal {
 public:
  StorageReferenceInternal(const std::string& storageUri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StoragePath& storageUri,
                           StorageInternal* storage);
  StorageReferenceInternal(const StorageReferenceInternal& other);

  ~StorageReferenceInternal();

  // Gets the storage to which we refer.
  Storage* storage() const;

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

  // Pointer to the StorageInternal instance we are a part of.
  StorageInternal* storage_internal() const { return storage_; }

  // Wrap this in a StorageReference.
  // Exposed for testing.
  StorageReference AsStorageReference() const;

 private:
  // Function type that sends a Rest Request and returns the BlockingResponse.
  typedef std::function<BlockingResponse*()> SendRequestFunct;

  template <typename FutureType>
  void SendRequestWithRetry(StorageReferenceFn internal_function_reference,
                            SendRequestFunct send_request_funct,
                            SafeFutureHandle<FutureType> final_handle,
                            double max_retry_time_seconds);

  template <typename FutureType>
  void AsyncSendRequestWithRetry(StorageReferenceFn internal_function_reference,
                                 SendRequestFunct send_request_funct,
                                 SafeFutureHandle<FutureType> final_handle,
                                 BlockingResponse* response,
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

  void RestCall(rest::Request* request, internal::Notifier* request_notifier,
                BlockingResponse* response, FutureHandle handle,
                Listener* listener, Controller* controller_out);

  void PrepareRequestBlocking(rest::Request* request, const char* url,
                              const char* method,
                              const char* content_type = nullptr);

  void SetupMetadataChain(Future<Metadata> starting_future,
                          MetadataChainData* data);

  ReferenceCountedFutureImpl* future();

  // Storage references are frequently duplicated.  Please avoid storing any
  // more state in them than is absolutely necessary, as it will be multiplied
  // over many allocations/copies.

  StorageInternal* storage_;
  StoragePath storageUri_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_STORAGE_REFERENCE_DESKTOP_H_
