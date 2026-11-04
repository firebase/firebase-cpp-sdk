// Copyright 2017 Google LLC

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
#include "storage/src/common/metadata_internal.h"
#include "storage/src/desktop/controller_desktop.h"
#include "storage/src/desktop/curl_requests.h"
#include "storage/src/desktop/listener_desktop.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/list_result.h"

namespace firebase::storage::internal {

std::atomic<bool> g_retry_all_errors_for_testing{false};

struct FutureApiOwner {
  explicit FutureApiOwner(StorageInternal* storage)
      : storage_token(storage ? storage->lifetime_token() : nullptr) {}
  FutureApiOwner(const FutureApiOwner&) = delete;
  FutureApiOwner& operator=(const FutureApiOwner&) = delete;

  std::shared_ptr<StorageInternal::LifetimeToken> storage_token;
  ReferenceCountedFutureImpl api_impl{kStorageReferenceFnCount};
  ReferenceCountedFutureImpl* api = &api_impl;
};

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
constexpr int kAppCheckTokenTimeoutMs = 10000;
constexpr int kInitialSleepTimeMillis = 1000;
constexpr int kMaxSleepTimeMillis = 30000;
constexpr char kFileProtocol[] = "file://";
constexpr size_t kFileProtocolLength = sizeof(kFileProtocol) - 1;

template <typename FutureType>
void CompleteFinalFutureCancelled(
    const std::shared_ptr<FutureApiOwner>& future_api_owner,
    SafeFutureHandle<FutureType> final_handle) {
  StorageUseScope scope(future_api_owner, StorageUseMode::kAllowShutdown);
  if (scope) {
    future_api_owner->api->Complete(final_handle, kErrorCancelled,
                                    GetErrorMessage(kErrorCancelled));
  }
}

template <typename FutureType>
void CompleteFinalFutureFromAttempt(
    const std::shared_ptr<FutureApiOwner>& future_api_owner,
    SafeFutureHandle<FutureType> final_handle,
    const FutureBase& attempt_future) {
  StorageUseScope scope(future_api_owner, StorageUseMode::kAllowShutdown);
  if (!scope) return;
  if (attempt_future.status() != firebase::kFutureStatusComplete) {
    future_api_owner->api->Complete(final_handle, kErrorCancelled,
                                    GetErrorMessage(kErrorCancelled));
    return;
  }
  const auto& typed_future =
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

std::shared_ptr<RestOperation::OperationHandle> AttemptHandle(
    const ControllerInternal* controller_internal) {
  return controller_internal ? controller_internal->operation_handle()
                             : nullptr;
}

std::string StripProtocol(const char* path_cstr) {
  if (!path_cstr) return {};
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

}  // namespace

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
  if (this != &other) {
    storage_ = other.storage_;
    storage_uri_ = other.storage_uri_;
    future_api_owner_ = std::make_shared<FutureApiOwner>(other.storage_);
  }
  return *this;
}

StorageReferenceInternal::StorageReferenceInternal(
    StorageReferenceInternal&& other) noexcept
    : storage_(std::exchange(other.storage_, nullptr)),
      storage_uri_(std::move(other.storage_uri_)),
      future_api_owner_(std::move(other.future_api_owner_)) {}

StorageReferenceInternal& StorageReferenceInternal::operator=(
    StorageReferenceInternal&& other) noexcept {
  if (this != &other) {
    storage_ = std::exchange(other.storage_, nullptr);
    storage_uri_ = std::move(other.storage_uri_);
    future_api_owner_ = std::move(other.future_api_owner_);
  }
  return *this;
}

StorageReferenceInternal::~StorageReferenceInternal() = default;

Storage* StorageReferenceInternal::storage() const {
  if (!storage_) return nullptr;
  const std::string& storage_url = storage_->url();
  return Storage::GetInstance(
      storage_->app(), storage_url.empty() ? nullptr : storage_url.c_str());
}

std::string StorageReferenceInternal::bucket() const {
  return storage_uri_.GetBucket();
}

std::string StorageReferenceInternal::full_path() const {
  return "/" + storage_uri_.GetPath().str();
}

StorageReferenceInternal* StorageReferenceInternal::Child(
    const char* path) const {
  return path ? new StorageReferenceInternal(storage_uri_.GetChild(path), storage_)
              : nullptr;
}

StorageReference StorageReferenceInternal::AsStorageReference() const {
  return Adopt(std::make_unique<StorageReferenceInternal>(*this));
}

StorageReference StorageReferenceInternal::Adopt(
    std::unique_ptr<StorageReferenceInternal> internal) {
  return StorageReference(internal.release());
}

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

struct MetadataChainData {
  MetadataChainData(SafeFutureHandle<Metadata> handle_,
                    const Metadata* metadata_,
                    const StorageReference& storage_ref_,
                    std::shared_ptr<FutureApiOwner> future_api_owner_)
      : handle(handle_),
        storage_ref(storage_ref_),
        future_api_owner(std::move(future_api_owner_)),
        final_future_ref(future_api_owner ? future_api_owner->api : nullptr,
                         handle_.get()) {
    if (metadata_) metadata = *metadata_;
    MetadataSetDefaults(&metadata);
  }
  Future<Metadata> inner_future;
  SafeFutureHandle<Metadata> handle;
  Metadata metadata;
  StorageReference storage_ref;
  std::shared_ptr<FutureApiOwner> future_api_owner;
  Future<Metadata> final_future_ref;
};

void StorageReferenceInternal::SetupMetadataChain(
    Future<Metadata> starting_future,
    const std::shared_ptr<MetadataChainData>& data) {
  data->inner_future = starting_future;
  starting_future.OnCompletion([data](const Future<Metadata>& result) {
    StorageUseScope scope(data->future_api_owner,
                          StorageUseMode::kAllowShutdown);
    if (!scope) return;
    if (result.error() != 0 || !data->storage_ref.is_valid()) {
      const int err = result.error() != 0 ? result.error() : kErrorCancelled;
      const char* msg = result.error_message() ? result.error_message() : "";
      data->future_api_owner->api->Complete(data->handle, err, msg);
      return;
    }
    Future<Metadata> metadata_future =
        data->storage_ref.UpdateMetadata(data->metadata);
    data->inner_future = metadata_future;
    metadata_future.OnCompletion([data](const Future<Metadata>& update_result) {
      StorageUseScope scope(data->future_api_owner,
                            StorageUseMode::kAllowShutdown);
      if (!scope) return;
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

template <class FutureType, class PrepareFunc>
void StorageReferenceInternal::RunAsyncOp(
    SafeFutureHandle<FutureType> final_handle, double max_retry_time,
    Controller* controller_out, PrepareFunc prepare_func) {
  auto owner = future_api_owner_;
  if (!owner || !owner->api || !storage_) {
    if (controller_out) *controller_out = Controller();
    CompleteFinalFutureCancelled(owner, final_handle);
    return;
  }
  StorageReference self_ref = AsStorageReference();
  auto send_request_funct =
      [self_ref, owner, prepare_func](
          Controller* attempt_controller_out) mutable -> RequestAttempt {
    RequestAttempt attempt;
    attempt.http_status =
        std::make_shared<std::atomic<int>>(rest::util::HttpInvalid);
    StorageUseScope storage_scope(owner, StorageUseMode::kRejectShutdown);
    if (!self_ref.is_valid() || !storage_scope) {
      attempt.local_failure = true;
      return attempt;
    }
    prepare_func(attempt, attempt_controller_out, self_ref, owner->api);
    return attempt;
  };
  SendRequestWithRetry(std::move(send_request_funct), final_handle,
                       max_retry_time, controller_out);
}

void StorageReferenceInternal::DeleteImpl(SafeFutureHandle<void> handle,
                                          ReferenceCountedFutureImpl*) {
  RunAsyncOp(handle, storage_->max_operation_retry_time(), nullptr,
             [](RequestAttempt& attempt, Controller* attempt_controller_out,
                StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
               auto internal_handle = api->SafeAlloc<void>();
               attempt.future = Future<void>(api, internal_handle.get());
               auto* response = new EmptyResponse(internal_handle, api);
               response->set_http_status_out(attempt.http_status);
               auto* request = new storage::internal::Request();
               self_ref.internal_->PrepareRequestBlocking(
                   request,
                   self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
                   "DELETE");
               self_ref.internal_->RestCall(
                   request, request->notifier(), response,
                   internal_handle.get(), nullptr, nullptr,
                   attempt_controller_out);
             });
}

void StorageReferenceInternal::PrepareRequestBlocking(
    rest::Request* request, const char* url, const char* method,
    const char* content_type) {
  request->set_url(url);
  request->set_method(method);
  request->options().timeout_ms = 0;

  std::string token = storage_->GetAuthToken();
  if (!token.empty()) {
    request->add_header("Authorization", ("Bearer " + token).c_str());
  }
  if (content_type && *content_type != '\0') {
    request->add_header("Content-Type", content_type);
  }
  request->add_header("X-Firebase-Storage-Version",
                      storage_->user_agent().c_str());

  Future<std::string> app_check_future;
  if (storage_->app()->function_registry()->CallFunction(
          ::firebase::internal::FnAppCheckGetTokenAsync, storage_->app(),
          nullptr, &app_check_future) &&
      app_check_future.status() != kFutureStatusInvalid) {
    const std::string* app_check_token =
        (app_check_future.status() == kFutureStatusComplete)
            ? app_check_future.result()
            : app_check_future.Await(kAppCheckTokenTimeoutMs);
    if (app_check_token && !app_check_token->empty()) {
      request->add_header("X-Firebase-AppCheck", app_check_token->c_str());
    }
  }
}

void StorageReferenceInternal::GetFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<size_t> handle,
                                           ReferenceCountedFutureImpl* api) {
  std::string final_path = StripProtocol(path);
  if (!path || path[0] == '\0' || final_path.empty()) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "File path cannot be null or empty.");
    return;
  }
  auto listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  RunAsyncOp(handle, storage_->max_download_retry_time(), controller_out,
             [final_path, listener_state](
                 RequestAttempt& attempt, Controller* attempt_controller_out,
                 StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
               auto internal_handle = api->SafeAlloc<size_t>();
               attempt.future = Future<size_t>(api, internal_handle.get());
               auto* request = new storage::internal::Request();
               self_ref.internal_->PrepareRequestBlocking(
                   request,
                   self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
                   rest::util::kGet);
               auto* response = new GetFileResponse(final_path.c_str(),
                                                    internal_handle, api);
               response->set_http_status_out(attempt.http_status);
               self_ref.internal_->RestCall(
                   request, request->notifier(), response,
                   internal_handle.get(), nullptr, listener_state,
                   attempt_controller_out);
             });
}

void StorageReferenceInternal::GetBytesImpl(void* buffer, size_t buffer_size,
                                            Listener* listener,
                                            Controller* controller_out,
                                            SafeFutureHandle<size_t> handle,
                                            ReferenceCountedFutureImpl* api) {
  if (!buffer && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "Buffer cannot be null when buffer_size > 0.");
    return;
  }
  auto listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  RunAsyncOp(handle, storage_->max_download_retry_time(), controller_out,
             [buffer, buffer_size, listener_state](
                 RequestAttempt& attempt, Controller* attempt_controller_out,
                 StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
               auto internal_handle = api->SafeAlloc<size_t>();
               attempt.future = Future<size_t>(api, internal_handle.get());
               auto* request = new storage::internal::Request();
               self_ref.internal_->PrepareRequestBlocking(
                   request,
                   self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
                   rest::util::kGet);
               auto* response = new GetBytesResponse(buffer, buffer_size,
                                                     internal_handle, api);
               response->set_http_status_out(attempt.http_status);
               self_ref.internal_->RestCall(
                   request, request->notifier(), response,
                   internal_handle.get(), nullptr, listener_state,
                   attempt_controller_out);
             });
}

template <typename FutureType>
void StorageReferenceInternal::SendRequestWithRetry(
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
    Controller* controller_out) {
  auto owner = future_api_owner_;
  if (!owner || !owner->api || !storage_) {
    if (controller_out) *controller_out = Controller();
    return;
  }
  const bool retry_all_errors = g_retry_all_errors_for_testing;

  std::shared_ptr<ControllerBinding> binding;
  if (controller_out) {
    binding = std::make_shared<ControllerBinding>();
    Controller bound_controller;
    bound_controller.internal_->Initialize(AsStorageReference(), nullptr);
    bound_controller.internal_->SetBinding(binding);
    *controller_out = std::move(bound_controller);
  }
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
    if (lifetime_token->shutting_down || !lifetime_token->storage) {
      cancel_final = true;
    } else {
      lifetime_token->retry_threads.push_back(std::thread::id());
    }
  }
  if (cancel_final) {
    CompleteFinalFutureCancelled(owner, final_handle);
    return;
  }

  std::thread(&StorageReferenceInternal::AsyncSendRequestWithRetry<FutureType>,
              AsStorageReference(), std::move(owner),
              std::move(send_request_funct), final_handle,
              max_retry_time_seconds, retry_all_errors, std::move(binding))
      .detach();
}

template <typename FutureType>
void StorageReferenceInternal::AsyncSendRequestWithRetry(
    StorageReference self_ref, std::shared_ptr<FutureApiOwner> future_api_owner,
    SendRequestFunct send_request_funct,
    SafeFutureHandle<FutureType> final_handle, double max_retry_time_seconds,
    bool retry_all_errors, std::shared_ptr<ControllerBinding> binding) {
  struct BindingFinisher {
    std::shared_ptr<ControllerBinding> binding;
    ~BindingFinisher() {
      if (binding) binding->MarkFinished();
    }
  } binding_finisher{binding};

  auto lifetime_token =
      future_api_owner ? future_api_owner->storage_token : nullptr;
  if (!lifetime_token || !future_api_owner || !future_api_owner->api) return;
  const auto self_id = std::this_thread::get_id();
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
          std::find(token->retry_threads.begin(), token->retry_threads.end(), id);
      if (it != token->retry_threads.end()) token->retry_threads.erase(it);
      token->cv.notify_all();
    }
  } thread_guard{lifetime_token, self_id};

  Future<FutureType> final_future_ref(future_api_owner->api,
                                      final_handle.get());
  const auto cancelled_by_user = [&binding]() {
    return binding && binding->is_cancelled();
  };
  if (!self_ref.is_valid() || cancelled_by_user()) {
    CompleteFinalFutureCancelled(future_api_owner, final_handle);
    return;
  }

  RequestAttempt attempt;
  {
    Controller attempt_controller;
    attempt = send_request_funct(binding ? &attempt_controller : nullptr);
    if (binding) binding->Bind(AttemptHandle(attempt_controller.internal_));
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
    const int http_status = attempt.http_status ? attempt.http_status->load()
                                                : rest::util::HttpInvalid;
    if (attempt.local_failure ||
        attempt.future.status() != firebase::kFutureStatusComplete ||
        attempt.future.error() == kErrorCancelled ||
        !IsRetryableFailure(http_status, retry_all_errors)) {
      break;
    }
    if (cancelled_by_user()) {
      CompleteFinalFutureCancelled(future_api_owner, final_handle);
      return;
    }
    const auto current_time = std::chrono::steady_clock::now();
    if (current_time >= end_time ||
        current_sleep_time >
            std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - current_time)) {
      break;
    }
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
    current_sleep_time = std::min(current_sleep_time * 2, max_sleep_time);
    Controller attempt_controller;
    attempt = send_request_funct(binding ? &attempt_controller : nullptr);
    if (binding) binding->Bind(AttemptHandle(attempt_controller.internal_));
  }

  if (!self_ref.is_valid()) {
    CompleteFinalFutureCancelled(future_api_owner, final_handle);
    return;
  }
  CompleteFinalFutureFromAttempt(future_api_owner, final_handle,
                                 attempt.future);
}

bool StorageReferenceInternal::IsRetryableFailure(int http_status,
                                                  bool retry_all_errors) {
  return (http_status >= 500 && http_status < 600) || http_status == 429 ||
         http_status == 408 ||
         (retry_all_errors && (http_status < 200 || http_status > 299));
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl* api) {
  PutBytesImpl(buffer, buffer_size, nullptr, listener, controller_out, handle,
               api);
}

void StorageReferenceInternal::PutBytesInternalImpl(
    const void* buffer, size_t buffer_size, Listener* listener,
    Controller* controller_out, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl*, const char* content_type) {
  std::string content_type_str = content_type ? content_type : "";
  auto listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  RunAsyncOp(
      handle, storage_->max_upload_retry_time(), controller_out,
      [content_type_str, buffer, buffer_size, listener_state](
          RequestAttempt& attempt, Controller* attempt_controller_out,
          StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
        auto internal_handle = api->SafeAlloc<Metadata>();
        attempt.future = Future<Metadata>(api, internal_handle.get());
        auto* request = new storage::internal::RequestBinary(
            static_cast<const char*>(buffer), buffer_size);
        self_ref.internal_->PrepareRequestBlocking(
            request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
            rest::util::kPost, content_type_str.c_str());
        auto* response =
            new ReturnedMetadataResponse(internal_handle, api, self_ref);
        response->set_http_status_out(attempt.http_status);
        self_ref.internal_->RestCall(request, request->notifier(), response,
                                     internal_handle.get(), nullptr,
                                     listener_state, attempt_controller_out);
      });
}

void StorageReferenceInternal::PutBytesImpl(
    const void* buffer, size_t buffer_size, const Metadata* metadata,
    Listener* listener, Controller* controller_out,
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api) {
  if (!buffer && buffer_size > 0) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "Buffer cannot be null when buffer_size > 0.");
    return;
  }
  if (!metadata) {
    PutBytesInternalImpl(buffer, buffer_size, listener, controller_out, handle,
                         api, "application/octet-stream");
    return;
  }
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsStorageReference(), future_api_owner_);
  auto internal_handle = api->SafeAlloc<Metadata>();
  data->storage_ref.internal_->PutBytesInternalImpl(
      buffer, buffer_size, listener, controller_out, internal_handle, api,
      data->metadata.content_type());
  SetupMetadataChain(Future<Metadata>(api, internal_handle.get()), data);
}

void StorageReferenceInternal::PutFileImpl(const char* path, Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<Metadata> handle,
                                           ReferenceCountedFutureImpl* api) {
  PutFileImpl(path, nullptr, listener, controller_out, handle, api);
}

void StorageReferenceInternal::PutFileInternalImpl(
    const char* path, Listener* listener, Controller* controller_out,
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* api,
    const char* content_type) {
  std::string final_path = StripProtocol(path);
  if (!path || path[0] == '\0' || final_path.empty()) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "File path cannot be null or empty.");
    return;
  }
  std::string content_type_str = content_type ? content_type : "";
  auto listener_state =
      (listener && listener->impl_) ? listener->impl_->shared_state() : nullptr;
  RunAsyncOp(
      handle, storage_->max_upload_retry_time(), controller_out,
      [content_type_str, final_path, listener_state](
          RequestAttempt& attempt, Controller* attempt_controller_out,
          StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
        auto internal_handle = api->SafeAlloc<Metadata>();
        attempt.future = Future<Metadata>(api, internal_handle.get());
        auto* request =
            new storage::internal::RequestFile(final_path.c_str(), 0);
        if (!request->IsFileOpen()) {
          delete request;
          if (attempt_controller_out) *attempt_controller_out = Controller();
          attempt.local_failure = true;
          api->Complete(internal_handle, kErrorUnknown, "Could not read file.");
          return;
        }
        self_ref.internal_->PrepareRequestBlocking(
            request, self_ref.internal_->storage_uri_.AsHttpUrl().c_str(),
            rest::util::kPost, content_type_str.c_str());
        auto* response =
            new ReturnedMetadataResponse(internal_handle, api, self_ref);
        response->set_http_status_out(attempt.http_status);
        self_ref.internal_->RestCall(request, request->notifier(), response,
                                     internal_handle.get(), nullptr,
                                     listener_state, attempt_controller_out);
      });
}

void StorageReferenceInternal::PutFileImpl(const char* path,
                                           const Metadata* metadata,
                                           Listener* listener,
                                           Controller* controller_out,
                                           SafeFutureHandle<Metadata> handle,
                                           ReferenceCountedFutureImpl* api) {
  std::string final_path = StripProtocol(path);
  if (!path || path[0] == '\0' || final_path.empty()) {
    if (controller_out) *controller_out = Controller();
    api->Complete(handle, kErrorInvalidArgument,
                  "File path cannot be null or empty.");
    return;
  }
  if (!metadata) {
    PutFileInternalImpl(path, listener, controller_out, handle, api,
                        "application/octet-stream");
    return;
  }
  auto data = std::make_shared<MetadataChainData>(
      handle, metadata, AsStorageReference(), future_api_owner_);
  auto internal_handle = api->SafeAlloc<Metadata>();
  data->storage_ref.internal_->PutFileInternalImpl(
      path, listener, controller_out, internal_handle, api,
      data->metadata.content_type());
  SetupMetadataChain(Future<Metadata>(api, internal_handle.get()), data);
}

void StorageReferenceInternal::GetMetadataImpl(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl*) {
  RunAsyncOp(handle, storage_->max_operation_retry_time(), nullptr,
             [](RequestAttempt& attempt, Controller* attempt_controller_out,
                StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
               auto internal_handle = api->SafeAlloc<Metadata>();
               attempt.future = Future<Metadata>(api, internal_handle.get());
               auto* response =
                   new ReturnedMetadataResponse(internal_handle, api, self_ref);
               response->set_http_status_out(attempt.http_status);
               auto* request = new storage::internal::Request();
               self_ref.internal_->PrepareRequestBlocking(
                   request,
                   self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
                   rest::util::kGet);
               self_ref.internal_->RestCall(
                   request, request->notifier(), response,
                   internal_handle.get(), nullptr, nullptr,
                   attempt_controller_out);
             });
}

void StorageReferenceInternal::UpdateMetadataImpl(
    const Metadata* metadata, SafeFutureHandle<Metadata> handle,
    ReferenceCountedFutureImpl*) {
  std::string metadata_json_str = metadata->internal_->ExportAsJson();
  RunAsyncOp(handle, storage_->max_operation_retry_time(), nullptr,
             [metadata_json_str](
                 RequestAttempt& attempt, Controller* attempt_controller_out,
                 StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
               auto internal_handle = api->SafeAlloc<Metadata>();
               attempt.future = Future<Metadata>(api, internal_handle.get());
               auto* request = new storage::internal::Request();
               self_ref.internal_->PrepareRequestBlocking(
                   request,
                   self_ref.internal_->storage_uri_.AsHttpMetadataUrl().c_str(),
                   "PATCH", "application/json");
               request->set_post_fields(metadata_json_str.c_str(),
                                        metadata_json_str.length());
               auto* response =
                   new ReturnedMetadataResponse(internal_handle, api, self_ref);
               response->set_http_status_out(attempt.http_status);
               self_ref.internal_->RestCall(
                   request, request->notifier(), response,
                   internal_handle.get(), nullptr, nullptr,
                   attempt_controller_out);
             });
}

void StorageReferenceInternal::ListImpl(
    int max_results_per_page, const char* page_token,
    SafeFutureHandle<StorageListResult> handle, ReferenceCountedFutureImpl*) {
  std::string page_token_str = page_token ? page_token : "";
  RunAsyncOp(
      handle, storage_->max_operation_retry_time(), nullptr,
      [max_results_per_page, page_token_str](
          RequestAttempt& attempt, Controller* attempt_controller_out,
          StorageReference& self_ref, ReferenceCountedFutureImpl* api) {
        auto internal_handle = api->SafeAlloc<StorageListResult>();
        attempt.future = Future<StorageListResult>(api, internal_handle.get());
        auto* response = new ReturnedListResponse(
            internal_handle, api, self_ref.internal_->storage_);
        response->set_http_status_out(attempt.http_status);
        auto* request = new storage::internal::Request();
        std::string url =
            self_ref.internal_->storage_->get_scheme() + "://" +
            self_ref.internal_->storage_->get_host() + ":" +
            std::to_string(self_ref.internal_->storage_->get_port()) +
            "/v0/b/" + rest::util::EncodeUrl(self_ref.internal_->bucket()) +
            "/o?delimiter=/";
        std::string prefix = self_ref.internal_->storage_uri_.GetPath().str();
        if (!prefix.empty()) {
          if (prefix.back() != '/') prefix += "/";
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
      });
}

struct ListAllChainData {
  ListAllChainData(SafeFutureHandle<StorageListResult> handle_,
                   const StorageReference& storage_ref_,
                   std::shared_ptr<FutureApiOwner> future_api_owner_)
      : handle(handle_),
        storage_ref(storage_ref_),
        future_api_owner(std::move(future_api_owner_)),
        final_future_ref(future_api_owner ? future_api_owner->api : nullptr,
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
    if (!scope) return;
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
      auto handle = data->future_api_owner->api->SafeAlloc<StorageListResult>();
      data->storage_ref.internal_->ListImpl(1000,
                                            page.next_page_token().c_str(),
                                            handle, data->future_api_owner->api);
      ContinueListAll(
          Future<StorageListResult>(data->future_api_owner->api, handle.get()),
          data);
    } else {
      data->future_api_owner->api->CompleteWithResult(
          data->handle, kErrorNone, "",
          StorageListResultInternal::AsStorageListResult(
              new StorageListResultInternal(std::move(data->prefixes),
                                            std::move(data->items), {})));
    }
  });
}

void StorageReferenceInternal::ListAllImpl(
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* api) {
  auto data = std::make_shared<ListAllChainData>(
      handle, AsStorageReference(), future_api_owner_);
  auto internal_handle = api->SafeAlloc<StorageListResult>();
  data->storage_ref.internal_->ListImpl(1000, nullptr, internal_handle, api);
  ContinueListAll(Future<StorageListResult>(api, internal_handle.get()), data);
}

void StorageReferenceInternal::GetDownloadUrlImpl(
    SafeFutureHandle<std::string> handle, ReferenceCountedFutureImpl* api) {
  struct GetUrlOnCompletionData {
    GetUrlOnCompletionData(std::shared_ptr<FutureApiOwner> future_api_owner_,
                           SafeFutureHandle<std::string> handle_,
                           StorageReference storage_ref_)
        : future_api_owner(std::move(future_api_owner_)),
          handle(handle_),
          storage_ref(std::move(storage_ref_)),
          final_future_ref(future_api_owner ? future_api_owner->api : nullptr,
                           handle_.get()) {}
    std::shared_ptr<FutureApiOwner> future_api_owner;
    SafeFutureHandle<std::string> handle;
    StorageReference storage_ref;
    Future<Metadata> metadata_future;
    Future<std::string> final_future_ref;
  };
  auto data = std::make_shared<GetUrlOnCompletionData>(
      future_api_owner_, handle, AsStorageReference());
  auto internal_handle = api->SafeAlloc<Metadata>();
  data->storage_ref.internal_->GetMetadataImpl(internal_handle, api);
  data->metadata_future = Future<Metadata>(api, internal_handle.get());
  data->metadata_future.OnCompletion([data](const Future<Metadata>& result) {
    StorageUseScope scope(data->future_api_owner,
                          StorageUseMode::kAllowShutdown);
    if (!scope) return;
    if (result.error() != 0) {
      data->future_api_owner->api->Complete(data->handle, result.error(),
                                            result.error_message());
      return;
    }
    std::string download_url;
    if (result.result() &&
        !result.result()->internal_->download_tokens().empty()) {
      const auto* m = result.result()->internal_;
      download_url =
          StoragePath(data->storage_ref.internal_->storage_internal(),
                      std::string("gs://") + m->bucket() + "/" + m->path())
              .AsHttpUrl() +
          "&token=" + m->download_tokens().front();
    }
    if (download_url.empty()) {
      data->future_api_owner->api->Complete(
          data->handle, kErrorUnknown, "Could not retrieve download URL.");
    } else {
      data->future_api_owner->api->CompleteWithResult(data->handle, kErrorNone,
                                                      download_url);
    }
  });
}

std::string StorageReferenceInternal::name() const {
  return storage_uri_.GetPath().GetBaseName();
}

StorageReferenceInternal* StorageReferenceInternal::GetParent() const {
  if (storage_uri_.GetPath().str().empty()) return nullptr;
  return new StorageReferenceInternal(storage_uri_.GetParent(), storage_);
}

ReferenceCountedFutureImpl* StorageReferenceInternal::future() const {
  return future_api_owner_ ? future_api_owner_->api : nullptr;
}

}  // namespace firebase::storage::internal
