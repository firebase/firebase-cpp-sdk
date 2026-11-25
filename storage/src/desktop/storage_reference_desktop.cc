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

#include <algorithm>
#include <atomic>
#include <chrono>  // NOLINT
#include <limits>
#include <memory>
#include <thread>  // NOLINT
#include <type_traits>
#include <utility>

#include "app/rest/request.h"
#include "app/rest/request_binary.h"
#include "app/rest/request_file.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/include/firebase/app.h"
#include "app/src/service_interfaces.h"
#include "app/src/thread.h"
#include "app/src/variant_util.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/list_result.h"

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
    const std::string& storageUri, std::shared_ptr<StorageInternal> storage)
    : strong_storage_(std::move(storage)),
      weak_storage_(strong_storage_),
      storageUri_(strong_storage_.get(), storageUri) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    has_future_api_.store(true, std::memory_order_release);
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    const StoragePath& storageUri, std::shared_ptr<StorageInternal> storage)
    : strong_storage_(std::move(storage)),
      weak_storage_(strong_storage_),
      storageUri_(storageUri) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    has_future_api_.store(true, std::memory_order_release);
  }
}

StorageReferenceInternal::StorageReferenceInternal(
    WeakTag, const StoragePath& storageUri,
    std::weak_ptr<StorageInternal> storage)
    : weak_storage_(std::move(storage)), storageUri_(storageUri) {
  // Defer FutureManager allocation until future() is called so constructing or
  // destroying weak references (including on the CurlThread) never locks a
  // strong reference to StorageInternal.
}

// static
StorageReferenceInternal* StorageReferenceInternal::CreateWeak(
    const StoragePath& storageUri, std::weak_ptr<StorageInternal> storage) {
  return new StorageReferenceInternal(WeakTag{}, storageUri,
                                      std::move(storage));
}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : strong_storage_(other.LockStorage()),
      weak_storage_(other.weak_storage_),
      storageUri_(other.storageUri_) {
  if (strong_storage_) {
    strong_storage_->future_manager().AllocFutureApi(this,
                                                     kStorageReferenceFnCount);
    has_future_api_.store(true, std::memory_order_release);
  }
}

StorageReferenceInternal::~StorageReferenceInternal() {
  if (has_future_api_.load(std::memory_order_acquire)) {
    if (auto locked = LockStorage()) {
      locked->future_manager().ReleaseFutureApi(this);
    }
  }
}

// Gets the storage to which we refer.
Storage StorageReferenceInternal::storage() const {
  return Storage(LockStorage());
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
  auto locked = LockStorage();
  if (!locked) return nullptr;
  return new StorageReferenceInternal(storageUri_.GetChild(path),
                                      std::move(locked));
}

StorageReference StorageReferenceInternal::AsStorageReference() const {
  auto locked = LockStorage();
  if (!locked) return StorageReference();
  return StorageReference(
      new StorageReferenceInternal(storageUri_, std::move(locked)));
}

StorageReference StorageReferenceInternal::AsWeakStorageReference() const {
  return StorageReference(CreateWeak(storageUri_, weak_storage_));
}

StorageReferenceInternal* StorageReferenceInternal::CloneStrong() const {
  auto locked = LockStorage();
  if (!locked) return nullptr;
  return new StorageReferenceInternal(storageUri_, std::move(locked));
}

StorageReferenceInternal* StorageReferenceInternal::CloneWeak() const {
  return CreateWeak(storageUri_, weak_storage_);
}

// static
StorageReference StorageReferenceInternal::CloneWeakReference(
    const StorageReference& ref) {
  return ref.internal_ ? StorageReference(ref.internal_->CloneWeak())
                       : StorageReference();
}

// Handy utility function.  Takes ownership of request/response controllers
// passed in, and will delete them when the request is complete.
// (listener and controller_out are not deleted, since they are owned by the
// calling function, if they exist.)
void StorageReferenceInternal::RestCall(
    const std::shared_ptr<StorageInternal>& storage,
    const StoragePath& storage_uri,
    const std::weak_ptr<StorageInternal>& weak_storage, rest::Request* request,
    Notifier* request_notifier, BlockingResponse* response, FutureHandle handle,
    Listener* listener, Controller* controller_out) {
  if (!storage) return;
  RestOperation::Start(
      storage.get(), CreateWeakReference(storage_uri, weak_storage), request,
      request_notifier, response, listener, handle, controller_out);
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

// Data structure used by SetupMetadataChain.  (See below.)  Owned by
// std::shared_ptr inside std::function OnCompletion lambdas so it is
// automatically destroyed both on normal completion and when
// FutureImplCore::Shutdown() discards pending callbacks during teardown.
struct MetadataChainData {
  MetadataChainData(SafeFutureHandle<Metadata> handle_,
                    const Metadata* metadata_, StorageReference storage_ref_,
                    std::shared_ptr<detail::FutureImplCore> original_core_)
      : handle(handle_),
        storage_ref(std::move(storage_ref_)),
        original_future_core(std::move(original_core_)) {
    if (metadata_) metadata = *metadata_;
    MetadataSetDefaults(&metadata);
  }
  SafeFutureHandle<Metadata> handle;
  Metadata metadata;
  // A non-owning internal copy of the storage ref that started the chain so
  // OnCompletion -> MetadataChainData does not form a cycle with
  // StorageInternal::future_manager_.
  StorageReference storage_ref;
  // Shared FutureImplCore of the original caller.
  std::shared_ptr<detail::FutureImplCore> original_future_core;
};

// Convenience function for handling operations that need to update a file
// on storage, and then update the metadata as well, as part of the same
// operation.  Both PutFile and PutBytes have a version that needs this.
void StorageReferenceInternal::SetupMetadataChain(
    Future<Metadata> starting_future, std::shared_ptr<MetadataChainData> data) {
  if (starting_future.status() == kFutureStatusInvalid) {
    ReferenceCountedFutureImpl::CompleteCore(data->original_future_core,
                                             data->handle, kErrorCancelled,
                                             "Storage instance was destroyed.");
    return;
  }
  starting_future.OnCompletion(
      [on_completion_data = std::move(data)](const Future<Metadata>& result) {
        if (result.error() != 0 ||
            !on_completion_data->storage_ref.is_valid()) {
          // The put failed or Storage was destroyed.  Return with the error.
          const int err =
              result.error() != 0 ? result.error() : kErrorCancelled;
          const std::string err_msg =
              result.error_message() ? result.error_message() : "";
          ReferenceCountedFutureImpl::CompleteCore(
              on_completion_data->original_future_core,
              on_completion_data->handle, err, err_msg.c_str());
        } else {
          // The put succeeded.  Now try setting the metadata of the object
          // we just created.
          Future<Metadata> metadata_future =
              on_completion_data->storage_ref.internal_->UpdateMetadata(
                  &on_completion_data->metadata);
          if (metadata_future.status() == kFutureStatusInvalid) {
            ReferenceCountedFutureImpl::CompleteCore(
                on_completion_data->original_future_core,
                on_completion_data->handle, kErrorCancelled,
                "Storage instance was destroyed.");
            return;
          }
          metadata_future.OnCompletion(
              [on_completion_data](const Future<Metadata>& result) {
                const int err = result.error();
                const std::string err_msg =
                    result.error_message() ? result.error_message() : "";
                const bool has_result =
                    (err == 0 && result.result() != nullptr);
                Metadata metadata_copy =
                    has_result ? *(result.result()) : Metadata();
                if (!has_result) {
                  // Setting metadata failed.  Return the error.
                  ReferenceCountedFutureImpl::CompleteCore(
                      on_completion_data->original_future_core,
                      on_completion_data->handle,
                      err != 0 ? err : kErrorUnknown, err_msg.c_str());
                } else {
                  // Metadata update succeeded.  Return in triumph.
                  ReferenceCountedFutureImpl::CompleteCoreWithResult(
                      on_completion_data->original_future_core,
                      on_completion_data->handle, kErrorNone, "",
                      metadata_copy);
                }
              });
        }
      });
}

// Deletes the object at the current path.
Future<void> StorageReferenceInternal::Delete() {
  auto locked = LockStorage();
  if (!locked) return Future<void>();
  auto* future_api = future();
  if (!future_api) return Future<void>();
  auto handle = future_api->SafeAlloc<void>(kStorageReferenceFnDelete);
  auto final_core = future_api->core();
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;

  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* /*controller_out*/) {
        auto attempt_handle = attempt_api->SafeAlloc<void>(0);
        EmptyResponse* response =
            new EmptyResponse(attempt_handle, attempt_api, http_status);

        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpUrl().c_str(), "DELETE");
        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), nullptr,
                 nullptr);
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_operation_retry_time());
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<void>&>(
                          future_api->LastResult(kStorageReferenceFnDelete))
                    : Future<void>();
}

// The timeout time to wait for the App Check token.
const int kAppCheckTokenTimeoutMs = 10000;

// Handy utility function, since REST calls have similar setup and teardown.
// Can potentially block getting Future results for the Request.
void StorageReferenceInternal::PrepareRequestBlocking(
    const std::shared_ptr<StorageInternal>& storage, rest::Request* request,
    const char* url, const char* method, const char* content_type) {
  if (!storage) return;
  request->set_url(url);
  request->set_method(method);

  // Set this request to have no timeout.
  request->options().timeout_ms = 0;

  // Fetch auth token and apply it, if there is one:
  std::string token = storage->GetAuthToken();
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
                      storage->user_agent().c_str());

  // Fetch App Check token via AppState's typed provider if present.
  if (auto* app_check = storage->app_state().app_check_token_provider()) {
    Future<std::string> app_check_future = app_check->GetTokenAsync();
    if (app_check_future.status() != kFutureStatusInvalid) {
      const std::string* ac_token =
          app_check_future.Await(kAppCheckTokenTimeoutMs);
      if (ac_token) {
        request->add_header("X-Firebase-AppCheck", ac_token->c_str());
      }
    }
  }
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  auto locked = LockStorage();
  if (!locked) return Future<size_t>();
  auto* future_api = future();
  if (!future_api) return Future<size_t>();
  auto handle = future_api->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
  auto final_core = future_api->core();
  std::string final_path = StripProtocol(path);
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;

  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, final_path, listener](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* attempt_controller_out) {
        auto attempt_handle = attempt_api->SafeAlloc<size_t>(0);
        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpUrl().c_str(),
                               rest::util::kGet);
        GetFileResponse* response = new GetFileResponse(
            final_path.c_str(), attempt_handle, attempt_api, http_status);
        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), listener,
                 attempt_controller_out);
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_download_retry_time(), controller_out);

  return GetFileLastResult();
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<size_t>&>(
                          future_api->LastResult(kStorageReferenceFnGetFile))
                    : Future<size_t>();
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  auto locked = LockStorage();
  if (!locked) return Future<size_t>();
  auto* future_api = future();
  if (!future_api) return Future<size_t>();
  auto handle = future_api->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
  auto final_core = future_api->core();
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;

  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, buffer, buffer_size, listener](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* attempt_controller_out) {
        auto attempt_handle = attempt_api->SafeAlloc<size_t>(0);
        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpUrl().c_str(),
                               rest::util::kGet);
        GetBytesResponse* response = new GetBytesResponse(
            buffer, buffer_size, attempt_handle, attempt_api, http_status);
        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), listener,
                 attempt_controller_out);
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_download_retry_time(), controller_out);
  return GetBytesLastResult();
}

// Sends a rest request, and creates a separate thread to retry failures.
template <typename FutureType>
void StorageReferenceInternal::SendRequestWithRetry(
    const std::shared_ptr<StorageInternal>& storage,
    std::weak_ptr<StorageInternal> weak_storage,
    SendRequestFunct send_request_funct,
    std::shared_ptr<detail::FutureImplCore> final_future_core,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
    Controller* first_attempt_controller_out) {
  auto attempt_api = std::make_shared<ReferenceCountedFutureImpl>(1);
  auto http_status = std::make_shared<std::atomic<int>>(400);
  send_request_funct(storage, attempt_api, http_status,
                     first_attempt_controller_out);
  std::thread async_retry_thread(
      &StorageReferenceInternal::AsyncSendRequestWithRetry<FutureType>,
      std::move(weak_storage), std::move(send_request_funct),
      std::move(final_future_core), final_handle, std::move(attempt_api),
      std::move(http_status), max_retry_time_seconds);
  async_retry_thread.detach();
}

const int kInitialSleepTimeMillis = 1000;
const int kMaxSleepTimeMillis = 30000;

// In a separate thread, repeatedly send Rest requests until one succeeds or a
// maximum amount of time has passed.
template <typename FutureType>
void StorageReferenceInternal::AsyncSendRequestWithRetry(
    std::weak_ptr<StorageInternal> weak_storage,
    SendRequestFunct send_request_funct,
    std::shared_ptr<detail::FutureImplCore> final_future_core,
    SafeFutureHandle<FutureType> final_handle,
    std::shared_ptr<ReferenceCountedFutureImpl> attempt_api,
    std::shared_ptr<std::atomic<int>> http_status,
    double max_retry_time_seconds) {
  firebase::FutureBase internal_future;
  auto end_time = std::chrono::steady_clock::now() +
                  std::chrono::duration<double>(max_retry_time_seconds);
  auto current_sleep_time = std::chrono::milliseconds(kInitialSleepTimeMillis);
  auto max_sleep_time = std::chrono::milliseconds(kMaxSleepTimeMillis);
  while (true) {
    internal_future = attempt_api->LastResult(0);
    // Wait for completion, then check status and error.
    while (internal_future.status() == firebase::kFutureStatusPending) {
      internal_future.Wait(100);
    }
    // For any request that succeeds or fails in a non-retryable way, don't
    // bother retrying.
    int httpStatus = http_status->load(std::memory_order_acquire);
    if (internal_future.status() != firebase::kFutureStatusComplete ||
        !IsRetryableFailure(httpStatus)) {
      break;
    }
    // Interrupt the loop if the retry deadline has been reached.
    auto current_time = std::chrono::steady_clock::now();
    if (current_time + current_sleep_time > end_time) {
      break;
    }
    // Sleep in short slices so App/Storage teardown wakes and exits promptly.
    auto wake_time = current_time + current_sleep_time;
    while (std::chrono::steady_clock::now() < wake_time) {
      if (weak_storage.expired()) {
        ReferenceCountedFutureImpl::CompleteCore(
            final_future_core, final_handle, kErrorCancelled,
            "Storage instance was destroyed.");
        return;
      }
      auto remaining = wake_time - std::chrono::steady_clock::now();
      auto step =
          std::min(remaining, std::chrono::duration_cast<decltype(remaining)>(
                                  std::chrono::milliseconds(50)));
      if (step > decltype(step)::zero()) {
        std::this_thread::sleep_for(step);
      }
    }
    current_sleep_time *= 2;
    if (current_sleep_time > max_sleep_time) {
      current_sleep_time = max_sleep_time;
    }
    // Lock StorageInternal only for the duration of dispatching the retry.
    auto storage = weak_storage.lock();
    if (!storage) {
      ReferenceCountedFutureImpl::CompleteCore(
          final_future_core, final_handle, kErrorCancelled,
          "Storage instance was destroyed.");
      return;
    }
    attempt_api = std::make_shared<ReferenceCountedFutureImpl>(1);
    http_status = std::make_shared<std::atomic<int>>(400);
    send_request_funct(storage, attempt_api, http_status, nullptr);
  }
  // Copy from the internal future to the final future.
  Future<FutureType> typed_future =
      static_cast<const Future<FutureType>&>(internal_future);
  if (typed_future.result() != nullptr) {
    if constexpr (std::is_void<FutureType>::value) {
      ReferenceCountedFutureImpl::CompleteCore(final_future_core, final_handle,
                                               internal_future.error(),
                                               internal_future.error_message());
    } else {
      ReferenceCountedFutureImpl::CompleteCoreWithResult(
          final_future_core, final_handle, internal_future.error(),
          internal_future.error_message(), *(typed_future.result()));
    }
  } else {
    ReferenceCountedFutureImpl::CompleteCore(final_future_core, final_handle,
                                             internal_future.error(),
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
  auto* future_api = future();
  return future_api ? static_cast<const Future<size_t>&>(
                          future_api->LastResult(kStorageReferenceFnGetBytes))
                    : Future<size_t>();
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
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  auto* future_api = future();
  if (!future_api) return Future<Metadata>();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  auto final_core = future_api->core();

  std::string content_type_str = content_type ? content_type : "";
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, content_type_str, buffer, buffer_size,
       listener](const std::shared_ptr<StorageInternal>& storage,
                 const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
                 const std::shared_ptr<std::atomic<int>>& http_status,
                 Controller* attempt_controller_out) {
        auto attempt_handle = attempt_api->SafeAlloc<Metadata>(0);

        storage::internal::RequestBinary* request =
            new storage::internal::RequestBinary(
                static_cast<const char*>(buffer), buffer_size);
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpUrl().c_str(),
                               rest::util::kPost, content_type_str.c_str());
        ReturnedMetadataResponse* response = new ReturnedMetadataResponse(
            attempt_handle, attempt_api,
            CreateWeakReference(storage_uri, weak_storage), http_status);
        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), listener,
                 attempt_controller_out);
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_upload_retry_time(), controller_out);
  return PutBytesLastResult();
}

// Asynchronously uploads data to the currently specified StorageReference,
// with metadata included.
Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out) {
  // This is the handle for the actual future returned to the user.
  auto* future_api = future();
  if (!future_api) return Future<Metadata>();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsWeakStorageReference(), future_api->core());
  // This is the future to do the actual putbytes.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutBytesLastResult.
  Future<Metadata> putbytes_internal =
      data->storage_ref.internal_->PutBytesInternal(
          buffer, buffer_size, listener, controller_out,
          metadata ? metadata->content_type() : nullptr);

  SetupMetadataChain(putbytes_internal, std::move(data));

  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<Metadata>&>(
                          future_api->LastResult(kStorageReferenceFnPutBytes))
                    : Future<Metadata>();
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
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  auto* future_api = future();
  if (!future_api) return Future<Metadata>();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  auto final_core = future_api->core();

  std::string final_path = StripProtocol(path);
  std::string content_type_str = content_type ? content_type : "";
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, final_path, content_type_str, listener](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* attempt_controller_out) {
        auto attempt_handle = attempt_api->SafeAlloc<Metadata>(0);

        // Open the file, calculate the length.
        storage::internal::RequestFile* request(
            new storage::internal::RequestFile(final_path.c_str(), 0));
        if (!request->IsFileOpen()) {
          delete request;
          http_status->store(400, std::memory_order_release);
          attempt_api->Complete(attempt_handle, kErrorUnknown,
                                "Could not read file.");
        } else {
          // Everything is good.  Fire off the request.
          ReturnedMetadataResponse* response = new ReturnedMetadataResponse(
              attempt_handle, attempt_api,
              CreateWeakReference(storage_uri, weak_storage), http_status);

          PrepareRequestBlocking(storage, request,
                                 storage_uri.AsHttpUrl().c_str(),
                                 rest::util::kPost, content_type_str.c_str());
          RestCall(storage, storage_uri, weak_storage, request,
                   request->notifier(), response, attempt_handle.get(),
                   listener, attempt_controller_out);
        }
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_upload_retry_time(), controller_out);
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
  if (!future_api) return Future<Metadata>();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsWeakStorageReference(), future_api->core());
  // This is the future to do the actual putfile.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutFileLastResult.
  Future<Metadata> putfile_internal =
      data->storage_ref.internal_->PutFileInternal(
          path, listener, controller_out,
          metadata ? metadata->content_type() : nullptr);

  SetupMetadataChain(putfile_internal, std::move(data));

  return PutFileLastResult();
}

// Returns the result of the most recent call to PutFile();
Future<Metadata> StorageReferenceInternal::PutFileLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<Metadata>&>(
                          future_api->LastResult(kStorageReferenceFnPutFile))
                    : Future<Metadata>();
}

// Retrieves metadata associated with an object at this StorageReference.
Future<Metadata> StorageReferenceInternal::GetMetadata() {
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  auto* future_api = future();
  if (!future_api) return Future<Metadata>();
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);
  auto final_core = future_api->core();
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;

  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* /*controller_out*/) {
        auto attempt_handle = attempt_api->SafeAlloc<Metadata>(0);
        ReturnedMetadataResponse* response = new ReturnedMetadataResponse(
            attempt_handle, attempt_api,
            CreateWeakReference(storage_uri, weak_storage), http_status);

        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpMetadataUrl().c_str(),
                               rest::util::kGet);

        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), nullptr,
                 nullptr);
      };
  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_operation_retry_time());
  return GetMetadataLastResult();
}

// Returns the result of the most recent call to GetMetadata();
Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<Metadata>&>(
                   future_api->LastResult(kStorageReferenceFnGetMetadata))
             : Future<Metadata>();
}

// Updates the metadata associated with this StorageReference.
Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  auto locked = LockStorage();
  if (!locked) return Future<Metadata>();
  auto* future_api = future();
  if (!future_api) return Future<Metadata>();
  auto handle =
      future_api->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  auto final_core = future_api->core();
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  // Serialize metadata up front so retries do not dereference the caller's
  // `const Metadata*` after UpdateMetadata() returns.
  std::string metadata_json = (metadata && metadata->internal_)
                                  ? metadata->internal_->ExportAsJson()
                                  : std::string("{}");

  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, metadata_json](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* /*controller_out*/) {
        auto attempt_handle = attempt_api->SafeAlloc<Metadata>(0);

        ReturnedMetadataResponse* response = new ReturnedMetadataResponse(
            attempt_handle, attempt_api,
            CreateWeakReference(storage_uri, weak_storage), http_status);

        storage::internal::Request* request = new storage::internal::Request();
        PrepareRequestBlocking(storage, request,
                               storage_uri.AsHttpUrl().c_str(), "PATCH",
                               "application/json");

        request->set_post_fields(metadata_json.c_str(), metadata_json.length());

        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), nullptr,
                 nullptr);
      };

  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_operation_retry_time());
  return UpdateMetadataLastResult();
}

// Returns the result of the most recent call to UpdateMetadata();
Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<Metadata>&>(
                   future_api->LastResult(kStorageReferenceFnUpdateMetadata))
             : Future<Metadata>();
}

// List items (files) and prefixes (folders) under this StorageReference.
Future<StorageListResult> StorageReferenceInternal::List(
    int max_results_per_page, const char* page_token) {
  auto locked = LockStorage();
  if (!locked) return Future<StorageListResult>();
  auto* future_api = future();
  if (!future_api) return Future<StorageListResult>();
  auto handle =
      future_api->SafeAlloc<StorageListResult>(kStorageReferenceFnList);
  auto final_core = future_api->core();

  std::string page_token_str = page_token ? page_token : "";
  StoragePath storage_uri = storageUri_;
  std::weak_ptr<StorageInternal> weak_storage = weak_storage_;
  SendRequestFunct send_request_funct =
      [storage_uri, weak_storage, max_results_per_page, page_token_str](
          const std::shared_ptr<StorageInternal>& storage,
          const std::shared_ptr<ReferenceCountedFutureImpl>& attempt_api,
          const std::shared_ptr<std::atomic<int>>& http_status,
          Controller* /*controller_out*/) {
        auto attempt_handle = attempt_api->SafeAlloc<StorageListResult>(0);

        ReturnedListResponse* response = new ReturnedListResponse(
            attempt_handle, attempt_api, storage->GetChildPath(""),
            weak_storage, http_status);

        storage::internal::Request* request = new storage::internal::Request();

        // For listing, we need the bucket URL:
        // [scheme]://[host]/v0/b/[bucket]/o We cannot use
        // storage_uri.AsHttpMetadataUrl() directly because it appends the
        // encoded path
        std::string url = storage->get_scheme();
        url += "://";
        url += storage->get_host();
        url += ":";
        url += std::to_string(storage->get_port());
        url += "/v0/b/";
        url += storage_uri.GetBucket();
        url += "/o";

        // The prefix must end with a slash to be treated as a directory by the
        // REST API.
        std::string prefix = storage_uri.GetPath().str();
        if (!prefix.empty() && prefix.back() != '/') {
          prefix += "/";
        }

        // Append query parameters
        std::string delimiter = "/";
        url += "?delimiter=" + delimiter;
        if (!prefix.empty()) {
          url += "&prefix=" + rest::util::EncodeUrl(prefix);
        }
        if (max_results_per_page > 0) {
          url += "&maxResults=" + std::to_string(max_results_per_page);
        }
        if (!page_token_str.empty()) {
          url += "&pageToken=" + rest::util::EncodeUrl(page_token_str);
        }

        PrepareRequestBlocking(storage, request, url.c_str(), rest::util::kGet);

        RestCall(storage, storage_uri, weak_storage, request,
                 request->notifier(), response, attempt_handle.get(), nullptr,
                 nullptr);
      };

  SendRequestWithRetry(locked, weak_storage, std::move(send_request_funct),
                       std::move(final_core), handle,
                       locked->max_operation_retry_time());
  return ListLastResult();
}

// Returns the result of the most recent call to List();
Future<StorageListResult> StorageReferenceInternal::ListLastResult() {
  auto* future_api = future();
  return future_api ? static_cast<const Future<StorageListResult>&>(
                          future_api->LastResult(kStorageReferenceFnList))
                    : Future<StorageListResult>();
}

// Asynchronously retrieves a long lived download URL with a revokable token.
Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  auto* future_api = future();
  if (!future_api) return Future<std::string>();
  // TODO(b/78908154): Re-implement this function without use of GetMetadata()
  Future<Metadata> metadata_future = GetMetadata();
  auto handle =
      future_api->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);
  auto future_core = future_api->core();

  if (metadata_future.status() == kFutureStatusInvalid) {
    ReferenceCountedFutureImpl::CompleteCore(future_core, handle,
                                             kErrorCancelled,
                                             "Storage instance was destroyed.");
    return GetDownloadUrlLastResult();
  }

  // Set the Metadata Callback.  The callback we return to the user is
  // separate from the metadata one, and we just mark the returned future
  // complete in the metadata onCompletion handler.
  metadata_future.OnCompletion([future_core = std::move(future_core),
                                handle](const Future<Metadata>& result) {
    if (result.error() != 0 || result.result() == nullptr ||
        result.result()->internal_ == nullptr) {
      ReferenceCountedFutureImpl::CompleteCore(
          future_core, handle,
          result.error() != 0 ? result.error() : kErrorUnknown,
          result.error_message());
    } else {
      // Use MetadataInternal to retrieve download_url because the public
      // Metadata API no longer exposes download URLs directly.
      ReferenceCountedFutureImpl::CompleteCoreWithResult(
          future_core, handle, kErrorNone, "",
          std::string(result.result()->internal_->download_url()));
    }
  });

  return GetDownloadUrlLastResult();
}

// Returns the result of the most recent call to GetDownloadUrl();
Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() {
  auto* future_api = future();
  return future_api
             ? static_cast<const Future<std::string>&>(
                   future_api->LastResult(kStorageReferenceFnGetDownloadUrl))
             : Future<std::string>();
}

// Returns the short name of this object.
std::string StorageReferenceInternal::name() {
  return storageUri_.GetPath().GetBaseName();
}

// Returns a new instance of StorageReference pointing to the parent location
// or null if this instance references the root location.
StorageReferenceInternal* StorageReferenceInternal::GetParent() {
  auto locked = LockStorage();
  if (!locked) return nullptr;
  return new StorageReferenceInternal(storageUri_.GetParent(),
                                      std::move(locked));
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() {
  auto locked = LockStorage();
  if (!locked) return nullptr;
  auto* api = locked->future_manager().GetFutureApi(this);
  if (!api) {
    bool expected = false;
    if (has_future_api_.compare_exchange_strong(expected, true,
                                                std::memory_order_acq_rel)) {
      locked->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
    }
    api = locked->future_manager().GetFutureApi(this);
  }
  return api;
}

StorageReference StorageReferenceInternal::CreateWeakReference(
    const StoragePath& storageUri, std::weak_ptr<StorageInternal> storage) {
  return StorageReference(CreateWeak(storageUri, std::move(storage)));
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
