// Copyright 2018 Google LLC
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

#include "functions/src/desktop/callable_reference_desktop.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_interface.h"
#include "app/rest/util.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/common.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/variant_util.h"
#include "flatbuffers/stl_emulation.h"
#include "functions/src/desktop/functions_desktop.h"
#include "functions/src/desktop/serialization.h"
#include "functions/src/include/firebase/functions.h"
#include "functions/src/include/firebase/functions/callable_reference.h"
#include "functions/src/include/firebase/functions/callable_result.h"
#include "functions/src/include/firebase/functions/common.h"

namespace firebase::functions::internal {

enum HttpsCallableReferenceFn {
  kHttpsCallableReferenceFnCall = 0,
  kHttpsCallableReferenceFnCount,
};

FunctionsCallableRequest::FunctionsCallableRequest(const char* url) {
  set_url(url ? url : "");
  options_.method = rest::util::kPost;
  add_header(rest::util::kContentType, rest::util::kApplicationJson);
}

void FunctionsCallableRequest::set_url(const char* url) {
  options_.url = url ? url : "";
}

void FunctionsCallableResponse::SetCallback(Callback callback) {
  callback_ = std::move(callback);
}

void FunctionsCallableResponse::MarkCompleted() {
  rest::Response::MarkCompleted();
  InvokeCallback();
}

void FunctionsCallableResponse::MarkFailed() {
  rest::Response::MarkFailed();
  failed_ = true;
  InvokeCallback();
}

void FunctionsCallableResponse::InvokeCallback() {
  Callback cb = std::move(callback_);
  if (cb) {
    cb();
  }
}

namespace {

// How long Call() waits for a call that completes immediately (e.g. a mock
// transport, or a request that fails before reaching the network) so that the
// returned Future is already complete. Real network transfers continue on the
// background worker thread after this short grace period.
constexpr auto kImmediateCompletionGrace = std::chrono::milliseconds(5);

// Self-contained execution context for a single Call() invocation.
// Owned via std::shared_ptr by the asynchronous callbacks and by the
// background worker thread, so destroying HttpsCallableReference or
// FunctionsInternal while a request is in flight never causes a
// Use-After-Free, and concurrent Call() invocations on the same
// HttpsCallableReference never share mutable Request/Response/Transport.
struct CallExecutionContext
    : public std::enable_shared_from_this<CallExecutionContext> {
  CallExecutionContext(
      const char* url, int64_t timeout_ms,
      ReferenceCountedFutureImpl* future_api_in,
      SafeFutureHandle<HttpsCallableResult> handle_in,
      std::shared_ptr<FunctionsLifetimeState> lifetime_state_in)
      : request(url),
        transport(rest::CreateTransport()),
        future_api(future_api_in),
        handle(handle_in),
        lifetime_state(lifetime_state_in) {
    request.options().timeout_ms =
        timeout_ms > 0 ? timeout_ms : kDefaultTimeoutMilliseconds;
  }

  // Runs the (synchronous) transport Perform() on a dedicated background
  // thread, then completes the Future on that thread: never on the caller's
  // thread for network I/O, never on the shared curl I/O thread, and never
  // while holding FunctionsLifetimeState::mutex.
  void StartPerform() {
    auto self = shared_from_this();
    std::thread([self]() { self->RunOnWorker(); }).detach();

    std::unique_lock<std::mutex> lock(signal_mutex);
    signal_cv.wait(lock, [this]() { return worker_started; });
    signal_cv.wait_for(lock, kImmediateCompletionGrace,
                       [this]() { return worker_done; });
  }

  void RunOnWorker() {
    {
      std::lock_guard<std::mutex> lock(signal_mutex);
      worker_started = true;
    }
    signal_cv.notify_all();

    transport->Perform(&request, &response, nullptr);

    if (auto state = lifetime_state.lock()) {
      const std::thread::id self_id = std::this_thread::get_id();
      bool should_resolve = false;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->alive) {
          state->resolving_threads.push_back(self_id);
          should_resolve = true;
        }
      }
      if (should_resolve) {
        ResolveFuture(future_api, handle, &response);
        {
          std::lock_guard<std::mutex> lock(state->mutex);
          auto& threads = state->resolving_threads;
          auto it = std::find(threads.begin(), threads.end(), self_id);
          if (it != threads.end()) threads.erase(it);
        }
        state->cv.notify_all();
      }
    }

    {
      std::lock_guard<std::mutex> lock(signal_mutex);
      worker_done = true;
    }
    signal_cv.notify_all();
  }

  FunctionsCallableRequest request;
  FunctionsCallableResponse response;
  flatbuffers::unique_ptr<rest::Transport> transport;
  ReferenceCountedFutureImpl* future_api;
  SafeFutureHandle<HttpsCallableResult> handle;
  std::weak_ptr<FunctionsLifetimeState> lifetime_state;

  std::mutex signal_mutex;
  std::condition_variable signal_cv;
  bool worker_started = false;
  bool worker_done = false;
};

// Returns true if the FunctionsInternal guarded by `weak_state` is still alive.
bool IsAlive(const std::weak_ptr<FunctionsLifetimeState>& weak_state) {
  if (auto state = weak_state.lock()) {
    std::lock_guard<std::mutex> lock(state->mutex);
    return state->alive;
  }
  return false;
}

}  // namespace

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, const char* url)
    : HttpsCallableReferenceInternal(functions, url, /*is_url=*/true,
                                     HttpsCallableOptions()) {}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, const char* target, bool is_url)
    : HttpsCallableReferenceInternal(functions, target, is_url,
                                     HttpsCallableOptions()) {}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, const char* url,
    const HttpsCallableOptions& options)
    : HttpsCallableReferenceInternal(functions, url, /*is_url=*/true, options) {
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, const char* target, bool is_url,
    const HttpsCallableOptions& options)
    : functions_(functions),
      target_(target ? target : ""),
      is_url_(is_url),
      options_(options) {
  if (functions_) {
    functions_->future_manager().AllocFutureApi(this,
                                                kHttpsCallableReferenceFnCount);
  }
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : functions_(other.functions_),
      target_(other.target_),
      is_url_(other.is_url_),
      options_(other.options_) {
  if (functions_) {
    functions_->future_manager().AllocFutureApi(this,
                                                kHttpsCallableReferenceFnCount);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  if (this == &other) {
    return *this;
  }
  if (functions_ != other.functions_) {
    if (functions_) {
      functions_->future_manager().ReleaseFutureApi(this);
    }
    functions_ = other.functions_;
    if (functions_) {
      functions_->future_manager().AllocFutureApi(
          this, kHttpsCallableReferenceFnCount);
    }
  }
  target_ = other.target_;
  is_url_ = other.is_url_;
  options_ = other.options_;
  return *this;
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other) noexcept
    : functions_(std::exchange(other.functions_, nullptr)),
      target_(std::move(other.target_)),
      is_url_(other.is_url_),
      options_(other.options_) {
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&other, this);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
  functions_ = std::exchange(other.functions_, nullptr);
  target_ = std::move(other.target_);
  is_url_ = other.is_url_;
  options_ = other.options_;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&other, this);
  }
  return *this;
}

Functions* HttpsCallableReferenceInternal::functions() const {
  if (!functions_) {
    return nullptr;
  }
  return Functions::GetInstance(functions_->app(), functions_->region());
}

std::string HttpsCallableReferenceInternal::url() const {
  if (is_url_) {
    return target_;
  }
  return functions_ ? functions_->GetUrl(target_) : "";
}

bool HttpsCallableReferenceInternal::is_valid() const {
  return functions_ != nullptr && !url().empty();
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_ ? functions_->future_manager().GetFutureApi(this) : nullptr;
}

Error ErrorFromHttpStatus(int status) {
  switch (status) {
    case 200:
    case 201:
    case 202:
      return kErrorNone;
    case 204:
      // TransportCurl sets 204 (HttpNoContent) when a transfer is canceled.
      return kErrorCancelled;
    case 400:
      return kErrorInvalidArgument;
    case 401:
      return kErrorUnauthenticated;
    case 403:
      return kErrorPermissionDenied;
    case 404:
      return kErrorNotFound;
    case 408:
      // TransportCurl sets 408 (HttpRequestTimeout) on request timeout.
      return kErrorDeadlineExceeded;
    case 409:
      return kErrorAborted;
    case 429:
      return kErrorResourceExhausted;
    case 499:
      return kErrorCancelled;
    case 500:
      return kErrorInternal;
    case 501:
      return kErrorUnimplemented;
    case 503:
      return kErrorUnavailable;
    case 504:
      return kErrorDeadlineExceeded;
    case 0:
      // Wait/connection failure before receiving an HTTP response.
      return kErrorUnavailable;
    default:
      if (status >= 200 && status < 300) {
        return kErrorNone;
      }
      return kErrorInternal;
  }
}

Error ErrorFromStatus(std::string_view status) {
  struct StatusEntry {
    std::string_view name;
    Error error;
  };
  static constexpr StatusEntry kStatusTable[] = {
      {"OK", kErrorNone},
      {"CANCELLED", kErrorCancelled},
      {"UNKNOWN", kErrorUnknown},
      {"INVALID_ARGUMENT", kErrorInvalidArgument},
      {"DEADLINE_EXCEEDED", kErrorDeadlineExceeded},
      {"NOT_FOUND", kErrorNotFound},
      {"ALREADY_EXISTS", kErrorAlreadyExists},
      {"PERMISSION_DENIED", kErrorPermissionDenied},
      {"UNAUTHENTICATED", kErrorUnauthenticated},
      {"RESOURCE_EXHAUSTED", kErrorResourceExhausted},
      {"FAILED_PRECONDITION", kErrorFailedPrecondition},
      {"ABORTED", kErrorAborted},
      {"OUT_OF_RANGE", kErrorOutOfRange},
      {"UNIMPLEMENTED", kErrorUnimplemented},
      {"INTERNAL", kErrorInternal},
      {"UNAVAILABLE", kErrorUnavailable},
      {"DATA_LOSS", kErrorDataLoss},
  };
  for (const auto& entry : kStatusTable) {
    if (status == entry.name) {
      return entry.error;
    }
  }
  return kErrorInternal;
}

namespace {
void ResolveFutureImpl(ReferenceCountedFutureImpl* future_api,
                       SafeFutureHandle<HttpsCallableResult> handle,
                       const rest::Response* response, bool transport_failed) {
  if (!future_api || !response) {
    return;
  }

  Error error = ErrorFromHttpStatus(response->status());
  if (transport_failed && error == kErrorNone) {
    error = kErrorUnavailable;
  }
  bool has_error = (error != kErrorNone);

  const char* body_str = response->GetBody();
  Variant body_variant =
      (body_str && body_str[0] != '\0')
          ? firebase::util::JsonToVariant(body_str)
          : Variant::Null();

  if (body_variant.is_null() || !body_variant.is_map()) {
    if (!has_error) {
      error = kErrorInternal;
    }
    future_api->Complete(handle, error, GetErrorMessage(error));
    return;
  }

  const auto& body_map = body_variant.map();
  auto error_it = body_map.find("error");
  if (error_it != body_map.end() && !error_it->second.is_null()) {
    has_error = true;
    if (!error_it->second.is_map()) {
      if (error == kErrorNone) {
        error = kErrorInternal;
      }
      future_api->Complete(handle, error, GetErrorMessage(error));
      return;
    }

    const auto& error_map = error_it->second.map();
    auto status_it = error_map.find("status");
    if (status_it != error_map.end()) {
      if (!status_it->second.is_string()) {
        error = kErrorInternal;
        future_api->Complete(handle, error, GetErrorMessage(error));
        return;
      }
      Error status_error = ErrorFromStatus(status_it->second.string_value());
      // "status": "OK" inside an error envelope is invalid.
      error = (status_error == kErrorNone) ? kErrorInternal : status_error;
    } else if (error == kErrorNone) {
      error = kErrorInternal;
    }

    std::string message;
    auto message_it = error_map.find("message");
    if (message_it != error_map.end() && message_it->second.is_string()) {
      message = message_it->second.string_value();
    } else {
      message = GetErrorMessage(error);
    }

    Variant details;
    auto details_it = error_map.find("details");
    if (details_it != error_map.end()) {
      details = Decode(details_it->second);
      if (details.is_null() && !details_it->second.is_null()) {
        error = kErrorInternal;
        message = GetErrorMessage(error);
      }
    }

    HttpsCallableResult result(std::move(details));
    future_api->CompleteWithResult(handle, error, message.c_str(), result);
    return;
  }

  if (has_error) {
    future_api->Complete(handle, error, GetErrorMessage(error));
    return;
  }

  auto data_it = body_map.find("data");
  if (data_it == body_map.end()) {
    data_it = body_map.find("result");
  }
  if (data_it == body_map.end()) {
    error = kErrorInternal;
    future_api->Complete(handle, error, GetErrorMessage(error));
    return;
  }

  Variant data = Decode(data_it->second);
  if (data.is_null() && !data_it->second.is_null()) {
    error = kErrorInternal;
    future_api->Complete(handle, error, GetErrorMessage(error));
    return;
  }
  HttpsCallableResult result(std::move(data));
  future_api->CompleteWithResult(handle, kErrorNone, "", result);
}
}  // namespace

void ResolveFuture(ReferenceCountedFutureImpl* future_api,
                   SafeFutureHandle<HttpsCallableResult> handle,
                   const FunctionsCallableResponse* response) {
  ResolveFutureImpl(future_api, handle, response,
                    response != nullptr && response->failed());
}

void ResolveFuture(ReferenceCountedFutureImpl* future_api,
                   SafeFutureHandle<HttpsCallableResult> handle,
                   rest::Response* response) {
  ResolveFutureImpl(future_api, handle, response, /*transport_failed=*/false);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call() {
  return Call(Variant::Null());
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  ReferenceCountedFutureImpl* future_api = future();
  if (!future_api || !functions_) {
    return Future<HttpsCallableResult>();
  }
  SafeFutureHandle<HttpsCallableResult> handle =
      future_api->SafeAlloc<HttpsCallableResult>(kHttpsCallableReferenceFnCall);

  const std::string resolved_url = url();
  if (resolved_url.empty()) {
    future_api->Complete(handle, kErrorInvalidArgument,
                         GetErrorMessage(kErrorInvalidArgument));
    return MakeFuture(future_api, handle);
  }

  Variant body_variant = Variant::EmptyMap();
  Variant encoded_data = Encode(data);
  if (encoded_data.is_null() && !data.is_null()) {
    future_api->Complete(handle, kErrorInvalidArgument,
                         GetErrorMessage(kErrorInvalidArgument));
    return MakeFuture(future_api, handle);
  }
  body_variant.map()["data"] = std::move(encoded_data);
  std::string body = firebase::util::VariantToJson(body_variant);

  auto ctx = std::make_shared<CallExecutionContext>(
      resolved_url.c_str(), options_.timeout_ms, future_api, handle,
      functions_->lifetime_state());
  ctx->request.set_post_fields(body.c_str(), body.length());

  App* app = functions_->app();
  if (!app) {
    ctx->StartPerform();
    return MakeFuture(future_api, handle);
  }

  auto fetch_app_check_and_perform = [ctx, app,
                                      limited_use =
                                          options_.limited_use_app_check_token]() {
    Future<std::string> future_app_check;
    bool has_app_check_future = false;
    if (auto state = ctx->lifetime_state.lock()) {
      std::lock_guard<std::mutex> lock(state->mutex);
      if (!state->alive) {
        return;
      }
      ::firebase::internal::FunctionId app_check_fn =
          limited_use ? ::firebase::internal::FnAppCheckGetLimitedUseTokenAsync
                      : ::firebase::internal::FnAppCheckGetTokenAsync;
      has_app_check_future = app->function_registry()->CallFunction(
          app_check_fn, app, nullptr, &future_app_check);
    } else {
      return;
    }
    if (has_app_check_future &&
        future_app_check.status() != kFutureStatusInvalid) {
      future_app_check.OnCompletion(
          [ctx](const Future<std::string>& future_token) {
            if (!IsAlive(ctx->lifetime_state)) {
              return;
            }
            if (future_token.error() == 0 && future_token.result() != nullptr &&
                !future_token.result()->empty()) {
              ctx->request.add_header("X-Firebase-AppCheck",
                                      future_token.result()->c_str());
            } else if (future_token.error() != 0) {
              LogWarning("Failed to get AppCheck token with Error %d: %s",
                         future_token.error(), future_token.error_message());
            }
            ctx->StartPerform();
          });
    } else {
      ctx->StartPerform();
    }
  };

  bool force_refresh = false;
  Future<std::string> future_auth;
  bool has_async_auth = app->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetTokenAsync, app, &force_refresh,
      &future_auth);
  if (has_async_auth && future_auth.status() != kFutureStatusInvalid) {
    future_auth.OnCompletion([ctx, fetch_app_check_and_perform](
                                 const Future<std::string>& completed_auth) {
      if (!IsAlive(ctx->lifetime_state)) {
        return;
      }
      if (completed_auth.error() == 0 && completed_auth.result() != nullptr &&
          !completed_auth.result()->empty()) {
        std::string header("Bearer ");
        header += *completed_auth.result();
        ctx->request.add_header(rest::util::kAuthorization, header.c_str());
      }
      fetch_app_check_and_perform();
    });
  } else {
    std::string auth_token;
    if (app->function_registry()->CallFunction(
            ::firebase::internal::FnAuthGetCurrentToken, app, nullptr,
            &auth_token) &&
        !auth_token.empty()) {
      std::string header("Bearer ");
      header += auth_token;
      ctx->request.add_header(rest::util::kAuthorization, header.c_str());
    }
    fetch_app_check_and_perform();
  }

  return MakeFuture(future_api, handle);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  ReferenceCountedFutureImpl* future_api = future();
  return future_api ? static_cast<const Future<HttpsCallableResult>&>(
                          future_api->LastResult(kHttpsCallableReferenceFnCall))
                    : Future<HttpsCallableResult>();
}

}  // namespace firebase::functions::internal
