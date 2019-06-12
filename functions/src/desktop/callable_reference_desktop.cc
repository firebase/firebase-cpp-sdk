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
#include "app/rest/request.h"
#include "app/rest/util.h"
#include "app/src/function_registry.h"
#include "app/src/variant_util.h"
#include "functions/src/desktop/functions_desktop.h"
#include "functions/src/desktop/serialization.h"
#include "functions/src/include/firebase/functions.h"

namespace firebase {
namespace functions {
namespace internal {

enum CallableReferenceFn {
  kCallableReferenceFnCall = 0,
  kCallableReferenceFnCount,
};

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    FunctionsInternal* functions, const char* name)
    : functions_(functions), name_(name) {
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
  rest::InitTransportCurl();
  transport_.set_is_async(true);
}

HttpsCallableReferenceInternal::~HttpsCallableReferenceInternal() {
  functions_->future_manager().ReleaseFutureApi(this);
  rest::CleanupTransportCurl();
}

HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    const HttpsCallableReferenceInternal& other)
    : functions_(other.functions_), name_(other.name_) {
  functions_->future_manager().AllocFutureApi(this, kCallableReferenceFnCount);
  rest::InitTransportCurl();
  transport_.set_is_async(true);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    const HttpsCallableReferenceInternal& other) {
  functions_ = other.functions_;
  name_ = other.name_;
  return *this;
}

#if defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)
HttpsCallableReferenceInternal::HttpsCallableReferenceInternal(
    HttpsCallableReferenceInternal&& other)
    : functions_(other.functions_), name_(std::move(other.name_)) {
  other.functions_ = nullptr;
  functions_->future_manager().MoveFutureApi(&other, this);
  rest::InitTransportCurl();
  transport_.set_is_async(true);
}

HttpsCallableReferenceInternal& HttpsCallableReferenceInternal::operator=(
    HttpsCallableReferenceInternal&& other) {
  functions_ = other.functions_;
  other.functions_ = nullptr;
  name_ = std::move(other.name_);
  functions_->future_manager().MoveFutureApi(&other, this);
  return *this;
}
#endif  // defined(FIREBASE_USE_MOVE_OPERATORS) || defined(DOXYGEN)

Functions* HttpsCallableReferenceInternal::functions() const {
  return Functions::GetInstance(functions_->app());
}

std::string HttpsCallableReferenceInternal::GetAuthToken() const {
  std::string result;
  App* app = functions_->app();
  app->function_registry()->CallFunction(
      ::firebase::internal::FnAuthGetCurrentToken, app, nullptr, &result);
  return result;
}

void HttpsCallableRequest::MarkCompleted() {
  rest::Request::MarkCompleted();
  HttpsCallableReferenceInternal::ResolveFuture(future_impl_, future_handle_,
                                                response_);
}

void HttpsCallableRequest::MarkFailed() {
  rest::Request::MarkFailed();
  HttpsCallableReferenceInternal::ResolveFuture(future_impl_, future_handle_,
                                                response_);
}

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

/* static */
void HttpsCallableReferenceInternal::ResolveFuture(
    ReferenceCountedFutureImpl* future_impl,
    SafeFutureHandle<HttpsCallableResult> future_handle,
    rest::Response* response) {
  // See if the HTTP status code indicates an error.
  Error error = ErrorFromHttpStatus(response->status());
  bool has_error = (error != kErrorNone);

  // Set default values for the rest of the fields.
  std::string error_description = GetErrorMessage(error);
  Variant error_details = Variant::Null();
  Variant data = Variant::Null();

  // Try to parse the body of the response.
  std::string body_str = response->GetBody();
  firebase::LogDebug("Cloud Function response body = %s", body_str.c_str());
  Variant body = util::JsonToVariant(body_str.c_str());
  if (!body.is_map()) {
    has_error = true;
    error = kErrorInternal;
    error_description = "INTERNAL";
  } else {
    // Try to parse error info out of the body.
    auto error_it = body.map().find("error");
    if (error_it != body.map().end()) {
      if (!has_error) {
        // If there is an error field, treat this as an error regardless.
        has_error = true;
        error = kErrorInternal;
        error_description = GetErrorMessage(error);
      }
      Variant error_variant = error_it->second;
      if (error_variant.is_map()) {
        std::map<Variant, Variant> error_map = error_variant.map();
        // Try to parse the message.
        if (error_map.find("message") != error_map.end()) {
          Variant message_variant = error_map["message"];
          if (message_variant.is_string()) {
            error_description = message_variant.string_value();
          }
        }
        // Try to parse the details.
        if (error_map.find("details") != error_map.end()) {
          error_details = Decode(error_map["details"]);
          // TODO(klimt): Include error details in C++ future somehow.
        }
        // Try to parse the status.
        if (error_map.find("status") != error_map.end()) {
          Variant status_variant = error_map["status"];
          if (status_variant.is_string()) {
            if (!ErrorFromStatus(status_variant.string_value(), &error)) {
              // The status was invalid, so clear everything.
              error = kErrorInternal;
              error_description = "INTERNAL";
              error_details = Variant::Null();
            }
          }
        }
      }
    }

    if (!has_error) {
      // Try to parse the returned data.
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

  HttpsCallableResult callable_result(std::move(data));
  future_impl->CompleteWithResult(future_handle, error,
                                  error_description.c_str(), callable_result);
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::Call(
    const Variant& data) {
  // Set up the request.
  std::string url = functions_->GetUrl(name_);
  request_.set_url(url.data());
  request_.set_method(rest::util::kPost);
  request_.add_header(rest::util::kContentType, rest::util::kApplicationJson);

  // Add the auth token header.
  std::string token = GetAuthToken();
  if (!token.empty()) {
    const char bearer[] = "Bearer ";
    request_.add_header("Authorization", (std::string(bearer) + token).c_str());
  }

  // Add the params as the JSON body.
  Variant body = Variant::EmptyMap();
  body.map()["data"] = Encode(data);
  std::string json = util::VariantToJson(body);
  request_.set_post_fields(json.data());

  firebase::LogDebug("Calling Cloud Function with name: %s\nurl: %s\ndata: %s",
      name_.c_str(), url.c_str(), json.c_str());

  // Set up the future to resolve when the request is complete.
  ReferenceCountedFutureImpl* future_impl = future();
  HttpsCallableResult null_result(Variant::Null());
  SafeFutureHandle<HttpsCallableResult> handle =
      future_impl->SafeAlloc(kCallableReferenceFnCall, null_result);
  request_.set_future_impl(future());
  request_.set_future_handle(handle);
  request_.set_response(&response_);

  // Start the request.
  transport_.Perform(&request_, &response_, nullptr);

  return CallLastResult();
}

Future<HttpsCallableResult> HttpsCallableReferenceInternal::CallLastResult() {
  return static_cast<const Future<HttpsCallableResult>&>(
      future()->LastResult(kCallableReferenceFnCall));
}

ReferenceCountedFutureImpl* HttpsCallableReferenceInternal::future() {
  return functions_->future_manager().GetFutureApi(this);
}

}  // namespace internal
}  // namespace functions
}  // namespace firebase
