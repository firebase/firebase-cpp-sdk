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

#include <chrono>  // NOLINT
#include <string>
#include <thread>  // NOLINT
#include <utility>

#include "app/rest/request.h"
#include "app/rest/util.h"
#include "app/src/service_interfaces.h"
#include "app/src/variant_util.h"
#include "functions/src/desktop/functions_desktop.h"
#include "functions/src/desktop/serialization.h"
#include "functions/src/include/firebase/functions.h"

namespace firebase {
namespace functions {
namespace internal {

namespace {
Error ErrorFromHttpStatus(int status);
bool ErrorFromStatus(const std::string& status, Error* error);
}  // namespace

enum CallableReferenceFn {
  kCallableReferenceFnCall = 0,
  kCallableReferenceFnCount,
};

CallOperation::CallOperation(
    std::shared_ptr<detail::FutureImplCore> future_core,
    SafeFutureHandle<HttpsCallableResult> future_handle)
    : future_core_(std::move(future_core)), future_handle_(future_handle) {
  // Synchronous mode guarantees BackgroundTransportCurl finishes
  // CompleteOperation() before Perform() returns.
  transport_.set_is_async(false);
}

void CallOperation::Launch(const std::shared_ptr<CallOperation>& self,
                           const std::string* app_check_token) {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (cancelled_) {
      completed_.store(true, std::memory_order_release);
      return;
    }
    if (app_check_token != nullptr) {
      request_.add_header("X-Firebase-AppCheck", app_check_token->c_str());
    }
    in_flight_ = true;
  }
  std::thread([self]() { self->RunOnWorker(); }).detach();
}

void CallOperation::CancelAndWait() {
  std::unique_lock<std::mutex> lock(state_mutex_);
  cancelled_ = true;
  while (in_flight_) {
    lock.unlock();
    transport_.CancelRequest(&response_);
    lock.lock();
    if (!in_flight_) break;
    state_cv_.wait_for(lock, std::chrono::milliseconds(5),
                       [this] { return !in_flight_; });
  }
}

void CallOperation::RunOnWorker() {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (cancelled_) {
      in_flight_ = false;
      completed_.store(true, std::memory_order_release);
      state_cv_.notify_all();
      return;
    }
  }

  transport_.Perform(&request_, &response_, nullptr);

  // Parse the HTTP response into local values before marking in_flight_ false
  // so FunctionsInternal teardown can proceed immediately without waiting for
  // user completion callbacks (avoiding self-deadlock if a callback drops the
  // last App/Functions handle).
  Error error = ErrorFromHttpStatus(response_.status());
  bool has_error = (error != kErrorNone);
  std::string error_description = GetErrorMessage(error);
  Variant error_details = Variant::Null();
  Variant data = Variant::Null();

  std::string body_str = response_.GetBody();
  firebase::LogDebug("Cloud Function response body = %s", body_str.c_str());
  Variant body = util::JsonToVariant(body_str.c_str());
  if (!body.is_map()) {
    has_error = true;
    error = kErrorInternal;
    error_description = "INTERNAL";
  } else {
    auto error_it = body.map().find("error");
    if (error_it != body.map().end()) {
      if (!has_error) {
        has_error = true;
        error = kErrorInternal;
        error_description = GetErrorMessage(error);
      }
      Variant error_variant = error_it->second;
      if (error_variant.is_map()) {
        std::map<Variant, Variant> error_map = error_variant.map();
        if (error_map.find("message") != error_map.end()) {
          Variant message_variant = error_map["message"];
          if (message_variant.is_string()) {
            error_description = message_variant.string_value();
          }
        }
        if (error_map.find("details") != error_map.end()) {
          error_details = Decode(error_map["details"]);
        }
        if (error_map.find("status") != error_map.end()) {
          Variant status_variant = error_map["status"];
          if (status_variant.is_string()) {
            if (!ErrorFromStatus(status_variant.string_value(), &error)) {
              error = kErrorInternal;
              error_description = "INTERNAL";
              error_details = Variant::Null();
            }
          }
        }
      }
    }

    if (!has_error) {
      auto result_it = body.map().find("result");
      auto data_it = body.map().find("data");
      if (result_it != body.map().end()) {
        data = Decode(result_it->second);
      } else if (data_it != body.map().end()) {
        data = Decode(data_it->second);
      } else {
        has_error = true;
        error = kErrorInternal;
        error_description = "Response is missing data field.";
      }
    }
  }

  auto core = std::move(future_core_);
  SafeFutureHandle<HttpsCallableResult> handle = future_handle_;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    in_flight_ = false;
    completed_.store(true, std::memory_order_release);
  }
  state_cv_.notify_all();

  HttpsCallableReferenceInternal::ResolveFuture(
      core, handle, error, error_description, std::move(data));
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    std::shared_ptr<FunctionsInternal> functions, const char* url,
    const HttpsCallableOptions& options)
    : functions_(std::move(functions)), url_(url), options_(options) {
  if (functions_) {
    functions_->future_manager().AllocFutureApi(this,
                                                kCallableReferenceFnCount);
  }
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : functions_(other.functions_), url_(other.url_), options_(other.options_) {
  if (functions_) {
    functions_->future_manager().AllocFutureApi(this,
                                                kCallableReferenceFnCount);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  if (this == &other) return *this;
  if (functions_ != other.functions_) {
    if (functions_) {
      functions_->future_manager().ReleaseFutureApi(this);
    }
    functions_ = other.functions_;
    if (functions_) {
      functions_->future_manager().AllocFutureApi(this,
                                                  kCallableReferenceFnCount);
    }
  }
  url_ = other.url_;
  options_ = other.options_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other)
    : functions_(std::move(other.functions_)),
      url_(std::move(other.url_)),
      options_(other.options_) {
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&other, this);
  }
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) {
  if (this == &other) return *this;
  if (functions_) {
    functions_->future_manager().ReleaseFutureApi(this);
  }
  functions_ = std::move(other.functions_);
  url_ = std::move(other.url_);
  options_ = other.options_;
  if (functions_) {
    functions_->future_manager().MoveFutureApi(&other, this);
  }
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Functions HttpsCallableReferenceInternal::functions() const {
  return Functions(functions_);
}

std::string HttpsCallableReferenceInternal::GetAuthToken() const {
  if (!functions_) return std::string();
  if (auto* auth = functions_->app_state().auth_token_provider()) {
    return auth->GetCurrentToken();
  }
  return std::string();
}

namespace {
// Takes an HTTP status code and returns the corresponding FUNErrorCode error
// code. This is the standard HTTP status code -> error mapping defined in:
// https://github.com/googleapis/googleapis/blob/master/google/rpc/code.proto
//
// @param status An HTTP status code.
// @return The corresponding Code, or Code.UNKNOWN if none.
Error ErrorFromHttpStatus(int status) {
  switch (status) {
    case 200:
      return kErrorNone;
    case 400:
      return kErrorInvalidArgument;
    case 401:
      return kErrorUnauthenticated;
    case 403:
      return kErrorPermissionDenied;
    case 404:
      return kErrorNotFound;
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
  }
  return kErrorUnknown;
}

// Returns true if the given status string is a known error, and sets the out
// parameter with the enum code number for it.
bool ErrorFromStatus(const std::string& status, Error* error) {
  if (status == "OK") {
    *error = kErrorNone;
    return true;
  }
  if (status == "CANCELLED") {
    *error = kErrorCancelled;
    return true;
  }
  if (status == "UNKNOWN") {
    *error = kErrorUnknown;
    return true;
  }
  if (status == "INVALID_ARGUMENT") {
    *error = kErrorInvalidArgument;
    return true;
  }
  if (status == "DEADLINE_EXCEEDED") {
    *error = kErrorDeadlineExceeded;
    return true;
  }
  if (status == "NOT_FOUND") {
    *error = kErrorNotFound;
    return true;
  }
  if (status == "ALREADY_EXISTS") {
    *error = kErrorAlreadyExists;
    return true;
  }
  if (status == "PERMISSION_DENIED") {
    *error = kErrorPermissionDenied;
    return true;
  }
  if (status == "UNAUTHENTICATED") {
    *error = kErrorUnauthenticated;
    return true;
  }
  if (status == "RESOURCE_EXHAUSTED") {
    *error = kErrorResourceExhausted;
    return true;
  }
  if (status == "FAILED_PRECONDITION") {
    *error = kErrorFailedPrecondition;
    return true;
  }
  if (status == "ABORTED") {
    *error = kErrorAborted;
    return true;
  }
  if (status == "OUT_OF_RANGE") {
    *error = kErrorOutOfRange;
    return true;
  }
  if (status == "UNIMPLEMENTED") {
    *error = kErrorUnimplemented;
    return true;
  }
  if (status == "INTERNAL") {
    *error = kErrorInternal;
    return true;
  }
  if (status == "UNAVAILABLE") {
    *error = kErrorUnavailable;
    return true;
  }
  if (status == "DATA_LOSS") {
    *error = kErrorDataLoss;
    return true;
  }
  *error = kErrorUnknown;
  return false;
}
}  // namespace

/* static */
void HttpsCallableReferenceInternal::ResolveFuture(
    const std::shared_ptr<detail::FutureImplCore>& future_core,
    SafeFutureHandle<HttpsCallableResult> future_handle, Error error,
    const std::string& error_description, Variant data) {
  HttpsCallableResult callable_result(std::move(data));
  ReferenceCountedFutureImpl::CompleteCoreWithResult(
      future_core, future_handle, error, error_description.c_str(),
      callable_result);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  ReferenceCountedFutureImpl* future_impl = future();
  if (!functions_ || !future_impl) {
    return Future<HttpsCallableResult>();
  }

  // Set up the future to resolve when the request is complete.
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);

  auto op = std::make_shared<CallOperation>(future_impl->core(), handle);

  // Set up the request.
  op->request().set_url(url_.data());
  op->request().set_method(rest::util::kPost);
  op->request().add_header(rest::util::kContentType,
                           rest::util::kApplicationJson);

  // Add the auth token header.
  std::string token = GetAuthToken();
  if (!token.empty()) {
    const char bearer[] = "Bearer ";
    op->request().add_header("Authorization",
                             (std::string(bearer) + token).c_str());
  }

  // Add the params as the JSON body.
  Variant body = Variant::EmptyMap();
  body.map()["data"] = Encode(data);
  std::string json = util::VariantToJson(body);
  op->request().set_post_fields(json.data());

  firebase::LogDebug("Calling Cloud Function with url: %s\ndata: %s",
                     url_.c_str(), json.c_str());

  functions_->AddOperation(op);

  // Check for App Check token provider on AppState.
  Future<std::string> app_check_future;
  if (auto* app_check = functions_->app_state().app_check_token_provider()) {
    app_check_future = options_.limited_use_app_check_token
                           ? app_check->GetLimitedUseTokenAsync()
                           : app_check->GetTokenAsync();
  }
  if (app_check_future.status() != kFutureStatusInvalid) {
    // Capture `op` by value; never capture `this` or stack references.
    app_check_future.OnCompletion(
        [op](const Future<std::string>& future_token) {
          op->Launch(op, future_token.result());
        });
  } else {
    op->Launch(op, nullptr);
  }

  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  ReferenceCountedFutureImpl* future_impl = future();
  return future_impl ? static_cast<const Future<HttpsCallableResult>&>(
                           future_impl->LastResult(kCallableReferenceFnCall))
                     : Future<HttpsCallableResult>();
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_ ? functions_->future_manager().GetFutureApi(this) : nullptr;
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
