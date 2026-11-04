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
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <thread>  // NOLINT
#include <type_traits>
#include <utility>
#include <vector>

#include "app/rest/request.h"
#include "app/rest/request_binary.h"
#include "app/rest/request_file.h"
#include "app/rest/transport_curl.h"
#include "app/rest/util.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/thread.h"
#include "app/src/variant_util.h"
#include "storage/src/common/common_internal.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/listener_desktop.h"
#include "storage/src/common/metadata_internal.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/list_result.h"

namespace firebase::storage::internal {

// Can be set in tests to retry all types of errors. Atomic because it is read
// from background retry threads.
std::atomic<bool> g_retry_all_errors_for_testing{false};

// Shared owner that keeps a ReferenceCountedFutureImpl* registered in
// StorageInternal's FutureManager for as long as any StorageReferenceInternal,
// retry thread, or chained OnCompletion callback references it.
struct FutureApiOwner {
  explicit FutureApiOwner(StorageInternal* storage)
      : storage_token(storage ? storage->lifetime_token() : nullptr) {
    if (storage) {
      storage->future_manager().AllocFutureApi(this, kStorageReferenceFnCount);
      api = storage->future_manager().GetFutureApi(this);
    }
  }

  ~FutureApiOwner() {
    // Keep the StorageInternal alive through an active-user scope rather than
    // by holding storage_token->mutex: ReleaseFutureApi() may destroy future
    // results (e.g. Metadata holding StorageReference handles) whose
    // destructors acquire storage_token->mutex themselves.
    StorageLifetimeScope scope(storage_token,
                               StorageLifetimeScope::Mode::kAllowShutdown);
    if (scope) {
      scope.storage()->future_manager().ReleaseFutureApi(this);
    }
  }

  FutureApiOwner(const FutureApiOwner&) = delete;
  FutureApiOwner& operator=(const FutureApiOwner&) = delete;

  std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
  ReferenceCountedFutureImpl* api = nullptr;
};

// Registers the calling thread as an active user of the StorageInternal behind
// a FutureApiOwner's lifetime token (see StorageLifetimeScope). While a scope
// is engaged, `storage()`, the StorageInternal reached through any
// StorageReference of it, and `FutureApiOwner::api` stay valid for the whole
// scope rather than only at the instant of a liveness check.
class StorageUseScope {
 public:
  using Mode = StorageLifetimeScope::Mode;

  StorageUseScope(const std::shared_ptr<FutureApiOwner>& owner, Mode mode)
      : scope_((owner && owner->api) ? owner->storage_token : nullptr, mode) {}

  StorageUseScope(const StorageUseScope&) = delete;
  StorageUseScope& operator=(const StorageUseScope&) = delete;

  explicit operator bool() const { return static_cast<bool>(scope_); }
  StorageInternal* storage() const { return scope_.storage(); }

 private:
  StorageLifetimeScope scope_;
};

namespace {

using StorageUseMode = StorageUseScope::Mode;

// Completes final_handle with kErrorCancelled if the storage is still alive.
template <typename FutureType>
void CompleteFinalFutureCancelled(
    const std::shared_ptr<FutureApiOwner>& future_api_owner,
    SafeFutureHandle<FutureType> final_handle) {
  StorageUseScope scope(future_api_owner, StorageUseMode::kAllowShutdown);
  if (!scope) {
    return;
  }
  future_api_owner->api->Complete(final_handle, kErrorCancelled,
                                  GetErrorMessage(kErrorCancelled));
}

template <typename FutureType>
void CompleteFinalFutureFromAttempt(
    const std::shared_ptr<FutureApiOwner>& future_api_owner,
    SafeFutureHandle<FutureType> final_handle,
    const FutureBase& attempt_future) {
  StorageUseScope scope(future_api_owner, StorageUseMode::kAllowShutdown);
  if (!scope) {
    return;
  }
  if (attempt_future.status() != firebase::kFutureStatusComplete) {
    // The attempt was never started (e.g. storage shutting down) or did not
    // finish; report the final future as cancelled rather than leaving it
    // pending.
    future_api_owner->api->Complete(final_handle, kErrorCancelled,
                                    GetErrorMessage(kErrorCancelled));
    return;
  }
  Future<FutureType> typed_future =
      static_cast<const Future<FutureType>&>(attempt_future);
  const char* err_msg =
      attempt_future.error_message() ? attempt_future.error_message() : "";
  if (typed_future.result() != nullptr) {
    if constexpr (std::is_void_v<FutureType>) {
      future_api_owner->api->Complete(final_handle, attempt_future.error(),
                                      err_msg);
    } else {
      future_api_owner->api->CompleteWithResult(
          final_handle, attempt_future.error(), err_msg,
          *(typed_future.result()));
    }
  } else {
    future_api_owner->api->Complete(final_handle, attempt_future.error(),
                                    err_msg);
  }
}

// Returns the RestOperation handle bound to a Controller populated by
// RestOperation::Start(), or null.
std::shared_ptr<RestOperation::OperationHandle> AttemptHandle(
    const ControllerInternal* controller_internal) {
  return controller_internal ? controller_internal->operation_handle()
                             : nullptr;
}

}  // namespace

/**
 * Represents a reference to a Google Cloud Storage object. Developers can
 * upload and download objects, get/set object metadata, and delete an object
 * at a specified path.
 * (see <a href="https://cloud.google.com/storage/">Google Cloud Storage</a>)
 */

StorageReferenceInternal::StorageReferenceInternal(
    const std::string& storage_uri, StorageInternal* storage)
    : storage_(storage),
      storage_uri_(storage, storage_uri),
      future_api_owner_(std::make_shared<FutureApiOwner>(storage)) {}

StorageReferenceInternal::StorageReferenceInternal(
    const StoragePath& storage_uri, StorageInternal* storage)
    : storage_(storage),
      storage_uri_(storage_uri),
      future_api_owner_(std::make_shared<FutureApiOwner>(storage)) {}

StorageReferenceInternal::StorageReferenceInternal(
    const StorageReferenceInternal& other)
    : storage_(other.storage_),
      storage_uri_(other.storage_uri_),
      future_api_owner_(std::make_shared<FutureApiOwner>(other.storage_)) {}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    const StorageReferenceInternal& other) {
  if (this == &other) {
    return *this;
  }
  storage_ = other.storage_;
  storage_uri_ = other.storage_uri_;
  future_api_owner_ = std::make_shared<FutureApiOwner>(other.storage_);
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& other) noexcept
    : storage_(other.storage_),
      storage_uri_(std::move(other.storage_uri_)),
      future_api_owner_(std::move(other.future_api_owner_)) {
  other.storage_ = nullptr;
}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  storage_ = other.storage_;
  storage_uri_ = std::move(other.storage_uri_);
  future_api_owner_ = std::move(other.future_api_owner_);
  other.storage_ = nullptr;
  return *this;
}

StorageReferenceInternal::~StorageReferenceInternal() = default;

// Gets the storage to which we refer.
Storage* StorageReferenceInternal::storage() const {
  if (!storage_) return nullptr;
  const std::string& storage_url = storage_->url();
  return Storage::GetInstance(
      storage_->app(), storage_url.empty() ? nullptr : storage_url.c_str());
}

// Return the Google Cloud Storage bucket that holds this object.
std::string StorageReferenceInternal::bucket() const {
  return storage_uri_.GetBucket();
}

// Return the full path of the object.
std::string StorageReferenceInternal::full_path() const {
  return "/" + storage_uri_.GetPath().str();
}

// Gets a reference to a location relative to this one. Ownership passes to
// the caller.
StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  if (path == nullptr) return nullptr;
  return new StorageReferenceInternal(storage_uri_.GetChild(path), storage_);
}

StorageReference StorageReferenceInternal::AsStorageReference() const {
  return Adopt(std::make_unique<StorageReferenceInternal>(*this));
}

StorageReference StorageReferenceInternal::Adopt(
    std::unique_ptr<StorageReferenceInternal> internal) {
  return StorageReference(internal.release());
}

// Handy utility function.  Takes ownership of request/response controllers
// passed in, and will delete them when the request is complete.
// (listener and controller_out are not deleted, since they are owned by the
// calling function, if they exist.)
void StorageReferenceInternal::RestCall(
    rest::Request* request, Notifier* request_notifier,
    BlockingResponse* response, FutureHandle handle, Listener* listener,
    const std::shared_ptr<ListenerSharedState>& listener_state,
    Controller* controller_out) {
  if (listener != nullptr) {
    RestOperation::Start(storage_, AsStorageReference(), request,
                         request_notifier, response, listener, handle,
                         controller_out);
  } else {
    RestOperation::Start(storage_, AsStorageReference(), request,
                         request_notifier, response, listener_state, handle,
                         controller_out);
  }
}

const char kFileProtocol[] = "file://";
const size_t kFileProtocolLength = sizeof(kFileProtocol) - 1;

// Remove the "file://" header from any file paths we are given.  (Desktop
// targets don't need them when opening files through stdio.)
std::string StripProtocol(const char* path_cstr) {
  if (!path_cstr) {
    return std::string();
  }
  std::string s(path_cstr);
  if (s.compare(0, kFileProtocolLength, kFileProtocol) == 0) {
    s = rest::util::DecodeUrl(s.substr(kFileProtocolLength));
#if FIREBASE_PLATFORM_WINDOWS
    if (s.size() >= 3 && s[0] == '/' &&
        ((s[1] >= 'A' && s[1] <= 'Z') || (s[1] >= 'a' && s[1] <= 'z')) &&
        s[2] == ':') {
      s.erase(0, 1);
    }
#endif
  }
  return s;
}

// Data structure used by SetupMetadataChain.  (See below.)  Basically all the
// data that needs to be preserved throughout the chain of OnCompletion calls.
struct MetadataChainData {
  MetadataChainData(SafeFutureHandle<Metadata> handle_,
                    const Metadata* metadata_,
                    const StorageReference& storage_ref_,
                    std::shared_ptr<FutureApiOwner> future_api_owner_)
      : handle(handle_),
        storage_ref(storage_ref_),
        future_api_owner(std::move(future_api_owner_)),
        final_future_ref(
            future_api_owner ? future_api_owner->api : nullptr,
            handle_.get()) {
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
  // Shared owner of the caller's ReferenceCountedFutureImpl.
  std::shared_ptr<FutureApiOwner> future_api_owner;
  Future<Metadata> final_future_ref;
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
    Future<Metadata> starting_future,
    const std::shared_ptr<MetadataChainData>& data) {
  data->inner_future = starting_future;
  starting_future.OnCompletion([data](const Future<Metadata>& result) {
    StorageUseScope scope(data->future_api_owner,
                          StorageUseMode::kAllowShutdown);
    if (!scope) {
      return;
    }
    if (result.error() != 0 || !data->storage_ref.is_valid()) {
      const int err = result.error() != 0 ? result.error() : kErrorCancelled;
      const char* msg = result.error_message() ? result.error_message() : "";
      data->future_api_owner->api->Complete(data->handle, err, msg);
      return;
    }

    // The upload succeeded. Now try setting the metadata of the object we just
    // created.
    Future<Metadata> metadata_future =
        data->storage_ref.internal_->UpdateMetadata(&data->metadata);
    data->inner_future = metadata_future;

    metadata_future.OnCompletion([data](const Future<Metadata>& update_result) {
      StorageUseScope scope(data->future_api_owner,
                            StorageUseMode::kAllowShutdown);
      if (!scope) {
        return;
      }
      if (update_result.error() != 0 || update_result.result() == nullptr) {
        const int err =
            update_result.error() != 0 ? update_result.error() : kErrorUnknown;
        const char* msg = update_result.error_message()
                              ? update_result.error_message()
                              : "";
        data->future_api_owner->api->Complete(data->handle, err, msg);
      } else {
        data->future_api_owner->api->CompleteWithResult(
            data->handle, kErrorNone, *(update_result.result()));
      }
    });
  });
}

// Deletes the object at the current path.
Future<void> StorageReferenceInternal::Delete() {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<void>(kStorageReferenceFnDelete);
  StorageReference self_ref = AsStorageReference();

  auto send_request_funct =
      [self_ref, owner](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<void>();
    attempt.future = Future<void>(owner->api, internal_handle.get());
    auto* response = new EmptyResponse(internal_handle, owner->api);
    response->set_http_status_out(attempt.http_status);

    auto* request = new storage::internal::Request();
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
        "DELETE");
    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, nullptr,
                                 attempt_controller_out);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_operation_retry_time());
  return DeleteLastResult();
}

Future<void> StorageReferenceInternal::DeleteLastResult() const {
  return static_cast<const Future<void>&>(
      future()->LastResult(kStorageReferenceFnDelete));
}

// The timeout time to wait for the App Check token.
const int kAppCheckTokenTimeoutMs = 10000;

// Handy utility function, since REST calls have similar setup and teardown.
// May block for up to kAppCheckTokenTimeoutMs waiting for an App Check token,
// so it is only called on the retry thread (see SendRequestWithRetry()),
// never on the caller's thread.
void StorageReferenceInternal::PrepareRequestBlocking(
    rest::Request* request, const char* url, const char* method,
    const char* content_type) {
  request->set_url(url);
  request->set_method(method);

  // Set this request to have no timeout.
  request->options().timeout_ms = 0;

  // Fetch auth token and apply it, if there is one:
  std::string token = storage_->GetAuthToken();
  if (!token.empty()) {
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
    const std::string* app_check_token =
        (app_check_future.status() == kFutureStatusComplete)
            ? app_check_future.result()
            : app_check_future.Await(kAppCheckTokenTimeoutMs);
    if (app_check_token != nullptr && !app_check_token->empty()) {
      request->add_header("X-Firebase-AppCheck", app_check_token->c_str());
    }
  }
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetFile(const char* path,
                                                 Listener* listener,
                                                 Controller* controller_out) {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<size_t>(kStorageReferenceFnGetFile);
  std::string final_path = StripProtocol(path);
  if (path == nullptr || path[0] == '\0' || final_path.empty()) {
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "File path cannot be null or empty.");
    return GetFileLastResult();
  }
  StorageReference self_ref = AsStorageReference();
  std::shared_ptr<ListenerSharedState> listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  auto send_request_funct =
      [self_ref, owner, final_path,
       listener_state](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<size_t>();
    attempt.future = Future<size_t>(owner->api, internal_handle.get());
    auto* request = new storage::internal::Request();
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
        rest::util::kGet);
    auto* response =
        new GetFileResponse(final_path.c_str(), internal_handle, owner->api);
    response->set_http_status_out(attempt.http_status);
    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, listener_state,
                                 attempt_controller_out);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_download_retry_time(), controller_out);

  return GetFileLastResult();
}

Future<size_t> StorageReferenceInternal::GetFileLastResult() const {
  return static_cast<const Future<size_t>&>(
      future()->LastResult(kStorageReferenceFnGetFile));
}

// Asynchronously downloads the object from this StorageReference.
Future<size_t> StorageReferenceInternal::GetBytes(void* buffer,
                                                  size_t buffer_size,
                                                  Listener* listener,
                                                  Controller* controller_out) {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<size_t>(kStorageReferenceFnGetBytes);
  if (buffer == nullptr && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "Buffer cannot be null when buffer_size > 0.");
    return GetBytesLastResult();
  }
  StorageReference self_ref = AsStorageReference();
  std::shared_ptr<ListenerSharedState> listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  auto send_request_funct =
      [self_ref, owner, buffer, buffer_size,
       listener_state](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<size_t>();
    attempt.future = Future<size_t>(owner->api, internal_handle.get());
    auto* request = new storage::internal::Request();
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
        rest::util::kGet);
    auto* response =
        new GetBytesResponse(buffer, buffer_size, internal_handle, owner->api);
    response->set_http_status_out(attempt.http_status);
    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, listener_state,
                                 attempt_controller_out);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_download_retry_time(), controller_out);
  return GetBytesLastResult();
}

// Prepares and sends the request, and retries failures, on a separate thread.
//
// Even the first attempt is prepared on that thread: preparing a request may
// block for up to kAppCheckTokenTimeoutMs while fetching an App Check token
// (and a custom AppCheckProvider may need the caller's thread to produce it),
// so the caller of this Future-returning API never blocks.
template <typename FutureType>
void StorageReferenceInternal::SendRequestWithRetry(
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
    Controller* controller_out) {
  std::shared_ptr<FutureApiOwner> owner = future_api_owner_;
  if (!owner || !owner->api || !storage_) {
    if (controller_out) *controller_out = Controller();
    return;
  }
  const bool retry_all_errors = g_retry_all_errors_for_testing;

  // If the caller wants a Controller, every attempt is published to a shared
  // ControllerBinding so that the caller's Controller, and all its copies,
  // always control the current attempt; Cancel() also stops further retries,
  // and a Pause() issued before the first attempt starts is applied to it.
  std::shared_ptr<ControllerBinding> binding;
  if (controller_out) {
    binding = std::make_shared<ControllerBinding>();
    Controller bound_controller;
    bound_controller.internal_->Initialize(AsStorageReference(), nullptr);
    bound_controller.internal_->SetBinding(binding);
    *controller_out = std::move(bound_controller);
  }
  // Marks the binding finished on every path that does not hand it to the
  // retry thread.
  struct BindingFinisher {
    std::shared_ptr<ControllerBinding>* binding;
    ~BindingFinisher() {
      if (*binding) (*binding)->MarkFinished();
    }
  } binding_finisher{&binding};

  auto lifetime_token = owner->storage_token;
  bool cancel_final = false;
  {
    std::lock_guard<std::mutex> lock(lifetime_token->mutex);
    if (lifetime_token->shutting_down || lifetime_token->storage == nullptr) {
      cancel_final = true;
    } else {
      lifetime_token->retry_threads.push_back(std::thread::id());
    }
  }
  if (cancel_final) {
    // Completed outside lifetime_token->mutex: completion runs user callbacks
    // synchronously, which may re-enter code that takes that mutex.
    CompleteFinalFutureCancelled(owner, final_handle);
    return;
  }

  StorageReference self_ref = AsStorageReference();
  std::thread async_retry_thread(
      &StorageReferenceInternal::AsyncSendRequestWithRetry<FutureType>,
      std::move(self_ref), std::move(owner), std::move(send_request_funct),
      final_handle, max_retry_time_seconds, retry_all_errors,
      std::move(binding));
  async_retry_thread.detach();
}

const int kInitialSleepTimeMillis = 1000;
const int kMaxSleepTimeMillis = 30000;

// In a separate thread, repeatedly send Rest requests until one succeeds or a
// maximum amount of time has passed.
template <typename FutureType>
void StorageReferenceInternal::AsyncSendRequestWithRetry(
    StorageReference self_ref, std::shared_ptr<FutureApiOwner> future_api_owner,
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
    bool retry_all_errors, std::shared_ptr<ControllerBinding> binding) {
  // Whatever happens below, the user-facing operation is over once this
  // function returns.
  struct BindingFinisher {
    std::shared_ptr<ControllerBinding> binding;
    ~BindingFinisher() {
      if (binding) binding->MarkFinished();
    }
  } binding_finisher{binding};

  auto lifetime_token =
      future_api_owner ? future_api_owner->storage_token : nullptr;
  if (!lifetime_token || !future_api_owner || !future_api_owner->api) {
    return;
  }
  const std::thread::id self_id = std::this_thread::get_id();
  {
    std::lock_guard<std::mutex> lock(lifetime_token->mutex);
    auto it =
        std::find(lifetime_token->retry_threads.begin(),
                  lifetime_token->retry_threads.end(), std::thread::id());
    if (it != lifetime_token->retry_threads.end()) {
      *it = self_id;
    } else {
      lifetime_token->retry_threads.push_back(self_id);
    }
  }
  struct RetryThreadGuard {
    std::shared_ptr<StorageInternal::LifetimeToken> token;
    std::thread::id id;
    ~RetryThreadGuard() {
      std::lock_guard<std::mutex> lock(token->mutex);
      auto it =
          std::find(token->retry_threads.begin(), token->retry_threads.end(),
                    id);
      if (it != token->retry_threads.end()) {
        token->retry_threads.erase(it);
      }
      token->cv.notify_all();
    }
  } thread_guard{lifetime_token, self_id};

  // Hold a Future reference to final_handle so ReferenceCountedFutureImpl keeps
  // its backing data alive until completion.
  Future<FutureType> final_future_ref(future_api_owner->api,
                                      final_handle.get());

  const auto cancelled_by_user = [&binding]() {
    return binding && binding->is_cancelled();
  };

  if (!self_ref.is_valid() || cancelled_by_user()) {
    CompleteFinalFutureCancelled(future_api_owner, final_handle);
    return;
  }
  // Prepare and start the first attempt (see SendRequestWithRetry()). Bind()
  // cancels it right away if Cancel() raced with this send, and applies a
  // Pause() issued before it started.
  RequestAttempt attempt;
  {
    Controller attempt_controller;
    attempt = send_request_funct(binding ? &attempt_controller : nullptr);
    if (binding) {
      binding->Bind(AttemptHandle(attempt_controller.internal_));
    }
  }
  const auto end_time = std::chrono::steady_clock::now() +
                        std::chrono::duration<double>(max_retry_time_seconds);
  auto current_sleep_time = std::chrono::milliseconds(kInitialSleepTimeMillis);
  const auto max_sleep_time = std::chrono::milliseconds(kMaxSleepTimeMillis);

  while (true) {
    if (!self_ref.is_valid()) {
      CompleteFinalFutureCancelled(future_api_owner, final_handle);
      return;
    }

    if (attempt.future.status() == firebase::kFutureStatusPending) {
      auto attempt_done = std::make_shared<bool>(false);
      attempt.future.OnCompletion(
          [lifetime_token, attempt_done](const FutureBase&) {
            std::lock_guard<std::mutex> lock(lifetime_token->mutex);
            *attempt_done = true;
            lifetime_token->cv.notify_all();
          });
      std::unique_lock<std::mutex> lock(lifetime_token->mutex);
      lifetime_token->cv.wait(lock, [&]() {
        return lifetime_token->shutting_down || *attempt_done;
      });
      if (lifetime_token->shutting_down && !*attempt_done) {
        lock.unlock();
        CompleteFinalFutureCancelled(future_api_owner, final_handle);
        return;
      }
    }

    if (!self_ref.is_valid()) {
      CompleteFinalFutureCancelled(future_api_owner, final_handle);
      return;
    }

    // For any request that succeeds or fails in a non-retryable way, don't
    // bother retrying.
    const int http_status = attempt.http_status ? attempt.http_status->load()
                                                : rest::util::HttpInvalid;
    if (attempt.local_failure ||
        attempt.future.status() != firebase::kFutureStatusComplete ||
        attempt.future.error() == kErrorCancelled ||
        !IsRetryableFailure(http_status, retry_all_errors)) {
      break;
    }
    // A retryable failure after the user cancelled: don't retry.
    if (cancelled_by_user()) {
      CompleteFinalFutureCancelled(future_api_owner, final_handle);
      return;
    }

    // Interrupt the loop if the retry deadline has been reached.
    const auto current_time = std::chrono::steady_clock::now();
    if (current_time >= end_time) {
      break;
    }
    const auto remaining =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                              current_time);
    if (current_sleep_time > remaining) {
      break;
    }

    // Wait for an exponentially increasing duration (or early shutdown), then
    // retry the request.
    {
      std::unique_lock<std::mutex> lock(lifetime_token->mutex);
      if (lifetime_token->cv.wait_for(
              lock, current_sleep_time,
              [&]() { return lifetime_token->shutting_down; })) {
        lock.unlock();
        CompleteFinalFutureCancelled(future_api_owner, final_handle);
        return;
      }
    }
    if (!self_ref.is_valid() || cancelled_by_user()) {
      CompleteFinalFutureCancelled(future_api_owner, final_handle);
      return;
    }
    current_sleep_time *= 2;
    if (current_sleep_time > max_sleep_time) {
      current_sleep_time = max_sleep_time;
    }
    // Re-point the caller's Controller at the new attempt. Bind() cancels the
    // new attempt right away if Cancel() raced with this send.
    Controller attempt_controller;
    attempt = send_request_funct(binding ? &attempt_controller : nullptr);
    if (binding) {
      binding->Bind(AttemptHandle(attempt_controller.internal_));
    }
  }

  if (!self_ref.is_valid()) {
    CompleteFinalFutureCancelled(future_api_owner, final_handle);
    return;
  }
  CompleteFinalFutureFromAttempt(future_api_owner, final_handle,
                                 attempt.future);
}

// Returns whether or not an http status represents a failure that should be
// retried.
bool StorageReferenceInternal::IsRetryableFailure(int http_status,
                                                  bool retry_all_errors) {
  return (http_status >= 500 && http_status < 600) || http_status == 429 ||
         http_status == 408 ||
         (retry_all_errors && (http_status < 200 || http_status > 299));
}

// Returns the result of the most recent call to GetBytes();
Future<size_t> StorageReferenceInternal::GetBytesLastResult() const {
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
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  if (buffer == nullptr && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "Buffer cannot be null when buffer_size > 0.");
    return PutBytesLastResult();
  }

  std::string content_type_str = content_type ? content_type : "";
  StorageReference self_ref = AsStorageReference();
  std::shared_ptr<ListenerSharedState> listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  auto send_request_funct =
      [self_ref, owner, content_type_str, buffer, buffer_size,
       listener_state](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<Metadata>();
    attempt.future = Future<Metadata>(owner->api, internal_handle.get());

    auto* request = new storage::internal::RequestBinary(
        static_cast<const char*>(buffer), buffer_size);
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
        rest::util::kPost, content_type_str.c_str());
    auto* response =
        new ReturnedMetadataResponse(internal_handle, owner->api, self_ref);
    response->set_http_status_out(attempt.http_status);
    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, listener_state,
                                 attempt_controller_out);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_upload_retry_time(), controller_out);
  return PutBytesLastResult();
}

// Asynchronously uploads data to the currently specified StorageReference,
// with metadata included.
Future<Metadata> StorageReferenceInternal::PutBytes(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out) {
  if (buffer == nullptr && buffer_size > 0) {
    auto* future_api = future();
    auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "Buffer cannot be null when buffer_size > 0.");
    return PutBytesLastResult();
  }
  if (metadata == nullptr) {
    return PutBytesInternal(buffer, buffer_size, listener, controller_out,
                            "application/octet-stream");
  }
  // This is the handle for the actual future returned to the user.
  auto owner = future_api_owner_;
  auto handle = owner->api->SafeAlloc<Metadata>(kStorageReferenceFnPutBytes);
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsStorageReference(), owner);
  // This is the future to do the actual putbytes.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutBytesLastResult.
  Future<Metadata> putbytes_internal =
      data->storage_ref.internal_->PutBytesInternal(
          buffer, buffer_size, listener, controller_out,
          data->metadata.content_type());

  SetupMetadataChain(putbytes_internal, data);

  return PutBytesLastResult();
}

Future<Metadata> StorageReferenceInternal::PutBytesLastResult() const {
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
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  std::string final_path = StripProtocol(path);
  if (path == nullptr || path[0] == '\0' || final_path.empty()) {
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "File path cannot be null or empty.");
    return PutFileLastResult();
  }

  std::string content_type_str = content_type ? content_type : "";
  StorageReference self_ref = AsStorageReference();
  std::shared_ptr<ListenerSharedState> listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  auto send_request_funct =
      [self_ref, owner, final_path, content_type_str,
       listener_state](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<Metadata>();
    attempt.future = Future<Metadata>(owner->api, internal_handle.get());

    // Open the file, calculate the length.
    auto* request = new storage::internal::RequestFile(final_path.c_str(), 0);
    if (!request->IsFileOpen()) {
      delete request;
      if (attempt_controller_out) {
        *attempt_controller_out = Controller();
      }
      attempt.local_failure = true;
      owner->api->Complete(internal_handle, kErrorUnknown,
                           "Could not read file.");
      return attempt;
    }

    // Everything is good. Fire off the request.
    auto* response =
        new ReturnedMetadataResponse(internal_handle, owner->api, self_ref);
    response->set_http_status_out(attempt.http_status);

    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
        rest::util::kPost, content_type_str.c_str());
    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, listener_state,
                                 attempt_controller_out);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_upload_retry_time(), controller_out);
  return PutFileLastResult();
}

// Asynchronously uploads data to the currently specified StorageReference,
// with additional metadata.
Future<Metadata> StorageReferenceInternal::PutFile(const char* path,
                                                   const Metadata* metadata,
                                                   Listener* listener,
                                                   Controller* controller_out) {
  std::string final_path = StripProtocol(path);
  if (path == nullptr || path[0] == '\0' || final_path.empty()) {
    auto* future_api = future();
    auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
    if (controller_out) *controller_out = Controller();
    future_api->Complete(handle, kErrorInvalidArgument,
                         "File path cannot be null or empty.");
    return PutFileLastResult();
  }
  if (metadata == nullptr) {
    return PutFileInternal(path, listener, controller_out,
                           "application/octet-stream");
  }
  // This is the handle for the actual future returned to the user.
  auto owner = future_api_owner_;
  auto handle = owner->api->SafeAlloc<Metadata>(kStorageReferenceFnPutFile);
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsStorageReference(), owner);
  // This is the future to do the actual putfile.  Note that it is on a
  // different storage reference than the original, so the caller of this
  // function can't access it via PutFileLastResult.
  Future<Metadata> putfile_internal =
      data->storage_ref.internal_->PutFileInternal(
          path, listener, controller_out, data->metadata.content_type());

  SetupMetadataChain(putfile_internal, data);

  return PutFileLastResult();
}

// Returns the result of the most recent call to PutFile();
Future<Metadata> StorageReferenceInternal::PutFileLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnPutFile));
}

// Retrieves metadata associated with an object at this StorageReference.
Future<Metadata> StorageReferenceInternal::GetMetadata() {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle = future_api->SafeAlloc<Metadata>(kStorageReferenceFnGetMetadata);
  StorageReference self_ref = AsStorageReference();

  auto send_request_funct =
      [self_ref, owner](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<Metadata>();
    attempt.future = Future<Metadata>(owner->api, internal_handle.get());
    auto* response =
        new ReturnedMetadataResponse(internal_handle, owner->api, self_ref);
    response->set_http_status_out(attempt.http_status);

    auto* request = new storage::internal::Request();
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
        rest::util::kGet);

    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, nullptr,
                                 attempt_controller_out);

    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_operation_retry_time());
  return GetMetadataLastResult();
}

// Returns the result of the most recent call to GetMetadata();
Future<Metadata> StorageReferenceInternal::GetMetadataLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnGetMetadata));
}

// Updates the metadata associated with this StorageReference.
Future<Metadata> StorageReferenceInternal::UpdateMetadata(
    const Metadata* metadata) {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle =
      future_api->SafeAlloc<Metadata>(kStorageReferenceFnUpdateMetadata);
  if (!metadata || !metadata->is_valid()) {
    future_api->Complete(handle, kErrorInvalidArgument,
                         "Metadata cannot be null or invalid.");
    return UpdateMetadataLastResult();
  }

  std::string metadata_json = metadata->internal_->ExportAsJson();
  StorageReference self_ref = AsStorageReference();
  auto send_request_funct =
      [self_ref, owner,
       metadata_json](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<Metadata>();
    attempt.future = Future<Metadata>(owner->api, internal_handle.get());

    auto* response =
        new ReturnedMetadataResponse(internal_handle, owner->api, self_ref);
    response->set_http_status_out(attempt.http_status);

    auto* request = new storage::internal::Request();
    self_ref.internal_->PrepareRequestBlocking(
        request, self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
        "PATCH", "application/json");

    request->set_post_fields(metadata_json.c_str(), metadata_json.length());

    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, nullptr,
                                 attempt_controller_out);
    return attempt;
  };

  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_operation_retry_time());
  return UpdateMetadataLastResult();
}

// Returns the result of the most recent call to UpdateMetadata();
Future<Metadata> StorageReferenceInternal::UpdateMetadataLastResult() const {
  return static_cast<const Future<Metadata>&>(
      future()->LastResult(kStorageReferenceFnUpdateMetadata));
}

// List items (files) and prefixes (folders) under this StorageReference.
Future<StorageListResult> StorageReferenceInternal::List(
    int max_results_per_page, const char* page_token) {
  auto owner = future_api_owner_;
  auto* future_api = owner->api;
  auto handle =
      future_api->SafeAlloc<StorageListResult>(kStorageReferenceFnList);
  if (max_results_per_page <= 0 || max_results_per_page > 1000) {
    future_api->Complete(
        handle, kErrorInvalidArgument,
        "max_results_per_page must be between 1 and 1000 inclusive.");
    return ListLastResult();
  }

  std::string page_token_str = page_token ? page_token : "";
  StorageReference self_ref = AsStorageReference();
  auto send_request_funct =
      [self_ref, owner, max_results_per_page,
       page_token_str](Controller* attempt_controller_out) -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    // Keeps self_ref's StorageInternal alive while this attempt is prepared
    // and started (PrepareRequestBlocking, get_scheme(), RestCall, ...).
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    auto internal_handle = owner->api->SafeAlloc<StorageListResult>();
    attempt.future =
        Future<StorageListResult>(owner->api, internal_handle.get());

    auto* response = new ReturnedListResponse(internal_handle, owner->api,
                                              self_ref.internal_->storage_);
    response->set_http_status_out(attempt.http_status);

    auto* request = new storage::internal::Request();

    // For listing, we need the bucket URL:
    // [scheme]://[host]:[port]/v0/b/[bucket]/o. We cannot use
    // storage_uri_.AsHttpMetadataUrl() directly because it appends the encoded
    // path.
    std::string url = self_ref.internal_->storage_->get_scheme();
    url += "://";
    url += self_ref.internal_->storage_->get_host();
    url += ":";
    url += std::to_string(self_ref.internal_->storage_->get_port());
    url += "/v0/b/";
    url += rest::util::EncodeUrl(self_ref.internal_->bucket());
    url += "/o";

    // The prefix must end with a slash to be treated as a directory by the
    // REST API.
    std::string prefix = self_ref.internal_->storage_uri_.GetPath().str();
    if (!prefix.empty() && prefix.back() != '/') {
      prefix += "/";
    }

    // Append query parameters
    url += "?delimiter=/";
    if (!prefix.empty()) {
      url += "&prefix=" + rest::util::EncodeUrl(prefix);
    }
    if (max_results_per_page > 0) {
      url += "&maxResults=" + std::to_string(max_results_per_page);
    }
    if (!page_token_str.empty()) {
      url += "&pageToken=" + rest::util::EncodeUrl(page_token_str);
    }

    self_ref.internal_->PrepareRequestBlocking(request, url.c_str(),
                                               rest::util::kGet);

    self_ref.internal_->RestCall(request, request->notifier(), response,
                                 internal_handle.get(), nullptr, nullptr,
                                 attempt_controller_out);
    return attempt;
  };

  SendRequestWithRetry(std::move(send_request_funct), handle,
                       storage_->max_operation_retry_time());
  return ListLastResult();
}

// Returns the result of the most recent call to List();
Future<StorageListResult> StorageReferenceInternal::ListLastResult() const {
  return static_cast<const Future<StorageListResult>&>(
      future()->LastResult(kStorageReferenceFnList));
}

struct ListAllChainData {
  ListAllChainData(SafeFutureHandle<StorageListResult> handle_,
                   const StorageReference& storage_ref_,
                   std::shared_ptr<FutureApiOwner> future_api_owner_)
      : handle(handle_),
        storage_ref(storage_ref_),
        future_api_owner(std::move(future_api_owner_)),
        final_future_ref(
            future_api_owner ? future_api_owner->api : nullptr,
            handle_.get()) {}
  Future<StorageListResult> inner_future;
  SafeFutureHandle<StorageListResult> handle;
  StorageReference storage_ref;
  std::shared_ptr<FutureApiOwner> future_api_owner;
  Future<StorageListResult> final_future_ref;
  std::vector<StorageReference> prefixes;
  std::vector<StorageReference> items;
};

void StorageReferenceInternal::ContinueListAll(
    Future<StorageListResult> page_future,
    const std::shared_ptr<ListAllChainData>& data) {
  data->inner_future = page_future;
  page_future.OnCompletion([data](const Future<StorageListResult>& result) {
    StorageUseScope scope(data->future_api_owner,
                          StorageUseMode::kAllowShutdown);
    if (!scope) {
      return;
    }
    if (result.error() != 0 || !data->storage_ref.is_valid() ||
        result.result() == nullptr) {
      const int err = result.error() != 0 ? result.error() : kErrorUnknown;
      const char* msg = result.error_message() ? result.error_message() : "";
      data->future_api_owner->api->Complete(data->handle, err, msg);
      return;
    }
    const StorageListResult& page = *result.result();
    data->prefixes.insert(data->prefixes.end(), page.prefixes().begin(),
                          page.prefixes().end());
    data->items.insert(data->items.end(), page.items().begin(),
                       page.items().end());
    if (!page.next_page_token().empty()) {
      Future<StorageListResult> next_page =
          data->storage_ref.internal_->List(
              1000, page.next_page_token().c_str());
      ContinueListAll(next_page, data);
    } else {
      StorageListResult final_result =
          StorageListResultInternal::AsStorageListResult(
              new StorageListResultInternal(std::move(data->prefixes),
                                            std::move(data->items),
                                            std::string()));
      data->future_api_owner->api->CompleteWithResult(
          data->handle, kErrorNone, "", final_result);
    }
  });
}

Future<StorageListResult> StorageReferenceInternal::ListAll() {
  auto owner = future_api_owner_;
  auto handle =
      owner->api->SafeAlloc<StorageListResult>(kStorageReferenceFnListAll);
  auto data =
      std::make_shared<ListAllChainData>(handle, AsStorageReference(), owner);
  Future<StorageListResult> first_page =
      data->storage_ref.internal_->List(1000, nullptr);
  ContinueListAll(first_page, data);
  return ListAllLastResult();
}

Future<StorageListResult> StorageReferenceInternal::ListAllLastResult() const {
  return static_cast<const Future<StorageListResult>&>(
      future()->LastResult(kStorageReferenceFnListAll));
}

// Asynchronously retrieves a long lived download URL with a revokable token.
Future<std::string> StorageReferenceInternal::GetDownloadUrl() {
  // TODO(b/78908154): Re-implement this function without use of GetMetadata()
  auto owner = future_api_owner_;
  auto handle =
      owner->api->SafeAlloc<std::string>(kStorageReferenceFnGetDownloadUrl);

  struct GetUrlOnCompletionData {
    GetUrlOnCompletionData(std::shared_ptr<FutureApiOwner> future_api_owner_,
                           SafeFutureHandle<std::string> handle_,
                           StorageReference storage_ref_)
        : future_api_owner(std::move(future_api_owner_)),
          handle(handle_),
          storage_ref(std::move(storage_ref_)),
          final_future_ref(
              future_api_owner ? future_api_owner->api : nullptr,
              handle_.get()) {}
    std::shared_ptr<FutureApiOwner> future_api_owner;
    SafeFutureHandle<std::string> handle;
    StorageReference storage_ref;
    Future<Metadata> metadata_future;
    Future<std::string> final_future_ref;
  };

  auto data = std::make_shared<GetUrlOnCompletionData>(owner, handle,
                                                       AsStorageReference());
  data->metadata_future = data->storage_ref.internal_->GetMetadata();

  // Set the Metadata Callback.  The callback we return to the user is
  // separate from the metadata one, and we just mark the returned future
  // complete in the metadata onCompletion handler.
  data->metadata_future.OnCompletion([data](const Future<Metadata>& result) {
    StorageUseScope scope(data->future_api_owner,
                          StorageUseMode::kAllowShutdown);
    if (!scope) {
      return;
    }
    if (result.error() != 0) {
      data->future_api_owner->api->Complete(data->handle, result.error(),
                                            result.error_message());
    } else {
      std::string download_url;
      if (result.result() && result.result()->internal_) {
        auto* metadata_internal = result.result()->internal_;
        if (!metadata_internal->download_tokens().empty()) {
          const std::string& token = metadata_internal->download_tokens().front();
          download_url = StoragePath(data->storage_ref.internal_->storage_internal(), std::string("gs://") + metadata_internal->bucket() + "/" + metadata_internal->path()).AsHttpUrl();
          if (!token.empty()) download_url += "&token=" + token;
        }
      }
      if (download_url.empty()) {
        data->future_api_owner->api->Complete(
            data->handle, kErrorUnknown, "Could not retrieve download URL.");
      } else {
        data->future_api_owner->api->CompleteWithResult(
            data->handle, kErrorNone, download_url);
      }
    }
  });

  return GetDownloadUrlLastResult();
}

// Returns the result of the most recent call to GetDownloadUrl();
Future<std::string> StorageReferenceInternal::GetDownloadUrlLastResult() const {
  return static_cast<const Future<std::string>&>(
      future()->LastResult(kStorageReferenceFnGetDownloadUrl));
}

// Returns the short name of this object.
std::string StorageReferenceInternal::name() const {
  return storage_uri_.GetPath().GetBaseName();
}

// Returns a new instance pointing to the parent location, or null if this
// instance references the root location. Ownership passes to the caller.
StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  if (storage_uri_.GetPath().str().empty()) {
    return nullptr;
  }
  return new StorageReferenceInternal(storage_uri_.GetParent(), storage_);
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() const {
  return future_api_owner_ ? future_api_owner_->api : nullptr;
}

}  // namespace firebase::storage::internal
