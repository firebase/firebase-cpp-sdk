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

#include "storage/src/desktop/storage_reference_desktop.h"

#include <chrono>  // NOLINT
#include <limits>
#include <memory>
#include <thread>  // NOLINT

#include "app/memory/unique_ptr.h"
#include "app/rest/request.h"
#include "app/rest/request_binary.h"
#include "app/rest/request_file.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/thread.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"

namespace firebase {
namespace storage {
namespace internal {

/**
 * Represents a reference to a Google Cloud Storage object. Developers can
 * upload and download objects, get/set object metadata, and delete an object
 * at a specified path.
 * (see <a href="https://cloud.google.com/storage/">Google Cloud Storage</a>)
 */

StorageReferenceInternal::StorageReferenceInternal(
    const std::string& storageUri, StorageInternal* storage)
    : storage_(storage), storageUri_(storageUri) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
}

StorageReferenceInternal::StorageReferenceInternal(
    const StoragePath& storageUri, StorageInternal* storage)
    : storage_(storage), storageUri_(storageUri) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : storage_(other.storage_), storageUri_(other.storageUri_) {
  storage_->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
}

StorageReferenceInternal::~StorageReferenceInternal() {
  storage_->future_manager().ReleaseFutureApi(this);
}

// Gets the storage to which we refer.
Storage* StorageReferenceInternal::storage() const {
  return Storage::GetInstance(storage_->app());
}

// Return the Google Cloud Storage bucket that holds this object.
std::string StorageReferenceInternal::bucket() const {
  return storageUri_.GetBucket();
}

// Return the full path of the object.
std::string StorageReferenceInternal::full_path() const {
  return "/" + storageUri_.GetPath().str();
}

// Gets a reference to a location relative to this one.
StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  if (path == nullptr) return nullptr;
  return new StorageReferenceInternal(storageUri_.GetChild(path), storage_);
}

StorageReference StorageReferenceInternal::AsStorageReference() const {
  return StorageReference(new StorageReferenceInternal(*this));
}

// Handy utility function.  Takes ownership of request/response controllers
// passed in, and will delete them when the request is complete.
// (listener and controller_out are not deleted, since they are owned by the
// calling function, if they exist.)
void StorageReferenceInternal::RestCall(rest::Request* request,
                                        Notifier* request_notifier,
                                        BlockingResponse* response,
                                        FutureHandle handle, Listener* listener,
                                        Controller* controller_out) {
  RestOperation::Start(storage_, AsStorageReference(), request,
                       request_notifier, response, listener, handle,
                       controller_out);
}

const char kFileProtocol[] = "file://";
const int kFileProtocolLength = 7;

// Remove the "file://" header from any file paths we are given.  (Desktop
// targets don't need them when opening files through stdio.)
std::string StripProtocol(std::string s) {
  if (s.compare(0, kFileProtocolLength, kFileProtocol) == 0) {
    return s.substr(kFileProtocolLength);
  }
  return s;
}

// Data structure used by SetupMetadataChain.  (See below.)  Basically all the
// data that needs to be preserved throughout the chain of OnCompletion calls.
// Is deleted by the final call.
struct MetadataChainData {
  MetadataChainData(SafeFutureHandle<Metadata> handle_,
                    const Metadata* metadata_,
                    const StorageReference& storage_ref_,
                    ReferenceCountedFutureImpl* original_future_)
      : handle(handle_),
        storage_ref(storage_ref_),
        original_future(original_future_) {
    if (metadata_) metadata = *metadata_;
    MetadataSetDefaults(&metadata);
  }
  // Reference to the future that is performing an operation before updating
  // metadata on the storage object.  We hold a reference to the future here
  // to stop the future API from being cleaned up by the future manager.
  Future<Metadata> inner_future;
  SafeFutureHandle<Metadata> handle;
  Metadata metadata;
  // A temporary, internal copy of the storage ref that started the chain.
  // Used to hide internal futures from the user.
  StorageReference storage_ref;
  // The future implementation of the original caller.  Needed to complete
  // the future returned to the user.
  ReferenceCountedFutureImpl* original_future;
};

// Convenience function for handling operations that need to update a file
// on storage, and then update the metadata as well, as part of the same
// operation.  Both PutFile and PutBytes have a version that needs this.
// Basically just chains futures together via OnCompletion callbacks, but
// has a few tricky bits, where it uses a copy of the original reference
// to hide the internal futures from the user, so the user can just deal with
// one external future.  (Which is completed by the OnCompletion chain when
// all of the operations have concluded.)
void StorageReferenceInternal::SetupMetadataChain(
    Future<Metadata> starting_future, MetadataChainData* data) {
  data->inner_future = starting_future;
  starting_future.OnCompletion(
      [](const Future<Metadata>& result, void* data) {
        MetadataChainData* on_completion_data =
            static_cast<MetadataChainData*>(data);

        if (result.error() != 0 ||
            !on_completion_data->storage_ref.is_valid()) {
          // The putfile failed.  Return with the error.
          on_completion_data->original_future->Complete(
              on_completion_data->handle, result.error(),
              result.error_message());
          delete on_completion_data;
        } else {
          // The putfile succeeded.  Now try setting the metadata of the object
          // we just created.
          Future<Metadata> metadata_future =
              on_completion_data->storage_ref.internal_->UpdateMetadata(
                  &on_completion_data->metadata);

          metadata_future.OnCompletion(
              [](const Future<Metadata>& result, void* data) {
                auto on_completion_data = static_cast<MetadataChainData*>(data);
                if (result.error() != 0) {
                  // Setting metadata failed.  Return the error.
                  on_completion_data->original_future->Complete(
                      on_completion_data->handle, result.error(),
                      result.error_message());
                } else {
                  // Metadata update succeedeed.  Return in triumph.
                  on_completion_data->original_future->CompleteWithResult(
                      on_completion_data->handle, kErrorNone,
                      *(result.result()));
                }
                delete on_completion_data;
              },
              data);
        }
      },
      data);
}

// Deletes the object at the current path.
Future<void> StorageReferenceInternal::Delete() {
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<void>(kStorageReferenceFnDelete);

  auto send_request_funct{[&]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle =
        future_api->SafeAlloc<void>(kStorageReferenceFnDeleteInternal);
    EmptyResponse* response = new EmptyResponse(handle, future_api);

    storage::internal::Request* request = new storage::internal::Request();
    PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(), "DELETE");
    RestCall(request, request->notifier(), response, handle.get(), nullptr,
             nullptr);
    return response;
  }};
  SendRequestWithRetry(kStorageReferenceFnDeleteInternal, send_request_funct,
                       handle, storage_->max_operation_retry_time());
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  return static_cast<const Future<void>&>(
      future()->LastResult(kStorageReferenceFnDelete));
}

// The timeout time to wait for the App Check token.
const int kAppCheckTokenTimeoutMs = 10000;

// Handy utility function, since REST calls have similar setup and teardown.
// Can potentially block getting Future results for the Request.
void StorageReferenceInternal::PrepareRequestBlocking(
    rest::Request* request, const char* url, const char* method,
    const char* content_type) {
  request->set_url(url);
  request->set_method(method);

  // Fetch auth token and apply it, if there is one:
  std::string token = storage_->GetAuthToken();
  if (token.length() > 0) {
    std::string auth_header = "Bearer " + token;
    request->add_header("Authorization", auth_header.c_str());
  }
  // if content_type was specified, add a header.
  if (content_type != nullptr && *content_type != '\0') {
    request->add_header("Content-Type", content_type);
  }
  // Unfortunately the storage backend rejects requests with the complete
  // user agent specified by the x-goog-api-client header so we only use
  // the X-Firebase-Storage-Version header to attribute the client.
  // b/74440917 tracks the issue.
  // Add the Firebase library version to the request.
  request->add_header("X-Firebase-Storage-Version",
                      storage_->user_agent().c_str());

  // Use the function registry to get the App Check token.
  Future<std::string> app_check_future;
  bool succeeded = storage_->app()->function_registry()->CallFunction(
      ::firebase::internal::FnAppCheckGetTokenAsync, storage_->app(), nullptr,
      &app_check_future);
  if (succeeded && app_check_future.status() != kFutureStatusInvalid) {
    const std::string* token = app_check_future.Await(kAppCheckTokenTimeoutMs);
    if (token) {
      request->add_header("X-Firebase-AppCheck", token->c_str());
    }
  }
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  auto handle = future()->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
  std::string final_path = StripProtocol(path);
  auto send_request_funct{
      [&, final_path, listener, controller_out]() -> BlockingResponse* {
        auto* future_api = future();
        auto handle =
            future_api->SafeAlloc<size_t>(kStorageReferenceFnGetFileInternal);
        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(),
                               rest::util::kGet);
        GetFileResponse* response =
            new GetFileResponse(final_path.c_str(), handle, future_api);
        RestCall(request, request->notifier(), response, handle.get(), listener,
                 controller_out);
        return response;
      }};
  SendRequestWithRetry(kStorageReferenceFnGetFileInternal, send_request_funct,
                       handle, storage_->max_download_retry_time());

  return GetFileLastResult();
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() {
  return static_cast<const Future<size_t>&>(
      future()->LastResult(kStorageReferenceFnGetFile));
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  auto handle = future()->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
  auto send_request_funct{[&, buffer, buffer_size, listener,
                           controller_out]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle =
        future_api->SafeAlloc<size_t>(kStorageReferenceFnGetBytesInternal);
    storage::internal::Request* request = new storage::internal::Request();
    PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(),
                           rest::util::kGet);
    GetBytesResponse* response =
        new GetBytesResponse(buffer, buffer_size, handle, future_api);
    RestCall(request, request->notifier(), response, handle.get(), listener,
             controller_out);
    return response;
  }};
  SendRequestWithRetry(kStorageReferenceFnGetBytesInternal, send_request_funct,
                       handle, storage_->max_download_retry_time());
  return GetBytesLastResult();
}

// Sends a rest request, and creates a separate thread to retry failures.
template <typename FutureType>
void StorageReferenceInternal::SendRequestWithRetry(
    StorageReferenceFn internal_function_reference,
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds) {
  BlockingResponse* first_response = send_request_funct();
  std::thread async_retry_thread(
      &StorageReferenceInternal::AsyncSendRequestWithRetry<FutureType>, this,
      internal_function_reference, send_request_funct, final_handle,
      first_response, max_retry_time_seconds);
  async_retry_thread.detach();
}

const int kInitialSleepTimeMillis = 1000;
const int kMaxSleepTimeMillis = 30000;

// In a separate thread, repeatedly send Rest requests until one succeeds or a
// maximum amount of time has passed.
template <typename FutureType>
void StorageReferenceInternal::AsyncSendRequestWithRetry(
    StorageReferenceFn internal_function_reference,
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, BlockingResponse* response,
    double max_retry_time_seconds) {
  auto* future_api = future();
  firebase::FutureBase internal_future;
  auto end_time = std::chrono::steady_clock::now() +
                  std::chrono::duration<double>(max_retry_time_seconds);
  auto current_sleep_time = std::chrono::milliseconds(kInitialSleepTimeMillis);
  auto max_sleep_time = std::chrono::milliseconds(kMaxSleepTimeMillis);
  while (true) {
    internal_future = future_api->LastResult(internal_function_reference);
    // Wait for completion, then check status and error.
    while (internal_future.status() == firebase::kFutureStatusPending) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    // For any request that succeeds or fails in a non-retryable way, don't
    // bother retrying. Response can be null if the request failed to create.
    int httpStatus = response == nullptr ? 400 : response->status();
    if (internal_future.status() != firebase::kFutureStatusComplete ||
        !IsRetryableFailure(httpStatus)) {
      break;
    }
    // Interrupt the loop if the retry deadline has been reached
    auto current_time = std::chrono::steady_clock::now();
    if (current_time + current_sleep_time > end_time) {
      break;
    }
    // Sleep for an exponentially increasing duration, then retry the request.
    std::this_thread::sleep_for(current_sleep_time);
    current_sleep_time *= 2;
    if (current_sleep_time > max_sleep_time) {
      current_sleep_time = max_sleep_time;
    }
    response = send_request_funct();
  }
  // Copy from the internal future to the final future.
  Future<FutureType> typed_future =
      static_cast<const Future<FutureType>&>(internal_future);
  if (typed_future.result() != nullptr) {
    if constexpr (std::is_void<FutureType>::value) {
      future_api->Complete(final_handle, internal_future.error());
    } else {
      future_api->CompleteWithResult(final_handle, internal_future.error(),
                                     *(typed_future.result()));
    }
  } else {
    future_api->Complete(final_handle, internal_future.error(),
                         internal_future.error_message());
  }
}

// Can be set in tests to retry all types of errors.
bool g_retry_all_errors_for_testing = false;

// Returns whether or not an http status represents a failure that should be
// retried.
bool StorageReferenceInternal::IsRetryableFailure(int httpStatus) {
  return (httpStatus >= 500 && httpStatus < 600) || httpStatus == 429 ||
         httpStatus == 408 ||
         (g_retry_all_errors_for_testing &&
          (httpStatus < 200 || httpStatus > 299));
}

// Returns the result of the most recent call to GetBytes();
Future<size_t> StorageReferenceInternal::GetBytesLastResult() {
  return static_cast<const Future<size_t>&>(
      future()->LastResult(kStorageReferenceFnGetBytes));
}

// Asynchronously uploads data to the currently specified StorageReference,
// without additional metadata.
Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out) {
  return PutBytes(buffer, buffer_size, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutBytesInternal(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out, const char* content_type) {
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);

  std::string content_type_str = content_type ? content_type : "";
  auto send_request_funct{[&, content_type_str, buffer, buffer_size, listener,
                           controller_out]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle =
        future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytesInternal);

    storage::internal::RequestBinary* request =
        new storage::internal::RequestBinary(static_cast<const char*>(buffer),
                                             buffer_size);
    PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(),
                           rest::util::kPost, content_type_str.c_str());
    ReturnedMetadataResponse* response =
        new ReturnedMetadataResponse(handle, future_api, AsStorageReference());
    RestCall(request, request->notifier(), response, handle.get(), listener,
             controller_out);
    return response;
  }};
  SendRequestWithRetry(kStorageReferenceFnPutBytesInternal, send_request_funct,
                       handle, storage_->max_upload_retry_time());
  return PutBytesLastResult();
}

// Asynchronously uploads data to the currently specified StorageReference,
// with metadata included.
Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out) {
  // This is the handle for the actual future returned to the user.
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  MetadataChainData* data =
      new MetadataChainData(handle, metadata, AsStorageReference(), future_api);
  // This is the future to do the actual putfile.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutFileLastResult.
  Future<Metadata> putbytes_internal =
      data->storage_ref.internal_->PutBytesInternal(
          buffer, buffer_size, listener, controller_out,
          metadata ? metadata->content_type() : nullptr);

  SetupMetadataChain(putbytes_internal, data);

  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutBytes));
}

// Asynchronously uploads data to the currently specified StorageReference,
// without additional metadata.
// Currently not terribly efficient about it.
// TODO(b/69434445): Make this more efficient. b/69434445
Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  return PutFile(path, nullptr, listener, controller_out);
}

Future<Metadata> StorageReferenceInternal::PutFileInternal(
    const char* path, Listener* listener, Controller* controller_out,
    const char* content_type) {
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);

  std::string final_path = StripProtocol(path);
  std::string content_type_str = content_type ? content_type : "";
  auto send_request_funct{[&, final_path, content_type_str, listener,
                           controller_out]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle =
        future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFileInternal);

    // Open the file, calculate the length.
    storage::internal::RequestFile* request(
        new storage::internal::RequestFile(final_path.c_str(), 0));
    if (!request->IsFileOpen()) {
      delete request;
      future_api->Complete(handle, kErrorUnknown, "Could not read file.");
      return nullptr;
    } else {
      // Everything is good.  Fire off the request.
      ReturnedMetadataResponse* response = new ReturnedMetadataResponse(
          handle, future_api, AsStorageReference());

      PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(),
                             rest::util::kPost, content_type_str.c_str());
      RestCall(request, request->notifier(), response, handle.get(), listener,
               controller_out);
      return response;
    }
  }};
  SendRequestWithRetry(kStorageReferenceFnPutFileInternal, send_request_funct,
                       handle, storage_->max_upload_retry_time());
  return PutFileLastResult();
}

// Asynchronously uploads data to the currently specified StorageReference,
// without additional metadata.
Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  // This is the handle for the actual future returned to the user.
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  MetadataChainData* data =
      new MetadataChainData(handle, metadata, AsStorageReference(), future_api);
  // This is the future to do the actual putfile.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutFileLastResult.
  Future<Metadata> putfile_internal =
      data->storage_ref.internal_->PutFileInternal(
          path, listener, controller_out,
          metadata ? metadata->content_type() : nullptr);

  SetupMetadataChain(putfile_internal, data);

  return PutFileLastResult();
}

// Returns the result of the most recent call to PutFile();
Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutFile));
}

// Retrieves metadata associated with an object at this StorageReference.
Future<Metadata> StorageReferenceInternal::GetMetadata() {
  auto* future_api = future();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);

  auto send_request_funct{[&]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle =
        future_api->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadataInternal);
    ReturnedMetadataResponse* response =
        new ReturnedMetadataResponse(handle, future_api, AsStorageReference());

    storage::internal::Request* request = new storage::internal::Request();
    PrepareRequestBlocking(request, storageUri_.AsHttpMetadataUrl().c_str(),
                           rest::util::kGet);

    RestCall(request, request->notifier(), response, handle.get(), nullptr,
             nullptr);

    return response;
  }};
  SendRequestWithRetry(kStorageReferenceFnGetMetadataInternal,
                       send_request_funct, handle,
                       storage_->max_operation_retry_time());
  return GetMetadataLastResult();
}

// Returns the result of the most recent call to GetMetadata();
Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnGetMetadata));
}

// Updates the metadata associated with this StorageReference.
Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  auto* future_api = future();
  auto handle =
      future_api->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);

  auto send_request_funct{[&, metadata]() -> BlockingResponse* {
    auto* future_api = future();
    auto handle = future_api->SafeAlloc<Metadata>(
        kStorageReferenceFnUpdateMetadataInternal);

    ReturnedMetadataResponse* response =
        new ReturnedMetadataResponse(handle, future_api, AsStorageReference());

    storage::internal::Request* request = new storage::internal::Request();
    PrepareRequestBlocking(request, storageUri_.AsHttpUrl().c_str(), "PATCH",
                           "application/json");

    std::string metadata_json = metadata->internal_->ExportAsJson();
    request->set_post_fields(metadata_json.c_str(), metadata_json.length());

    RestCall(request, request->notifier(), response, handle.get(), nullptr,
             nullptr);
    return response;
  }};

  SendRequestWithRetry(kStorageReferenceFnUpdateMetadataInternal,
                       send_request_funct, handle,
                       storage_->max_operation_retry_time());
  return UpdateMetadataLastResult();
}

// Returns the result of the most recent call to UpdateMetadata();
Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnUpdateMetadata));
}

// Asynchronously retrieves a long lived download URL with a revokable token.
Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  // TODO(b/78908154): Re-implement this function without use of GetMetadata()
  Future<Metadata> metadata_future = GetMetadata();
  auto* future_api = future();
  auto handle =
      future_api->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);

  struct GetUrlOnCompletionData {
    GetUrlOnCompletionData(ReferenceCountedFutureImpl* future_,
                           SafeFutureHandle<std::string> handle_)
        : future(future_), handle(handle_) {}
    ReferenceCountedFutureImpl* future;
    SafeFutureHandle<std::string> handle;
  };

  // Set the Metadata Callback.  The callback we return to the user is
  // separate from the metadata one, and we just mark the returned future
  // complete in the metadata onCompletion handler.
  metadata_future.OnCompletion(
      [](const Future<Metadata>& result, void* data) {
        auto on_completion_data = UniquePtr<GetUrlOnCompletionData>(
            static_cast<GetUrlOnCompletionData*>(data));
        if (result.error() != 0) {
          on_completion_data->future->Complete(on_completion_data->handle,
                                               result.error(),
                                               result.error_message());
        } else {
          // Use MetaDataInternal to retrieve download_url because we are
          // deprecating public API to get url from Metadata.
          // Note that GetMetadata() may not generate download_token soon,
          // which may break the expectation of this function.  More details
          // in b/78908154
          on_completion_data->future->CompleteWithResult(
              on_completion_data->handle, kErrorNone,
              std::string(result.result()->internal_->download_url()));
        }
      },
      new GetUrlOnCompletionData(future_api, handle));

  return GetDownloadUrlLastResult();
}

// Returns the result of the most recent call to GetDownloadUrl();
Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  return static_cast<const Future<std::string>&>(
      future()->LastResult(kStorageReferenceFnGetDownloadUrl));
}

// Returns the short name of this object.
std::string StorageReferenceInternal::name() {
  return storageUri_.GetPath().GetBaseName();
}

// Returns a new instance of StorageReference pointing to the parent location
// or null if this instance references the root location.
StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  return new StorageReferenceInternal(storageUri_.GetParent(), storage_);
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  return storage_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
