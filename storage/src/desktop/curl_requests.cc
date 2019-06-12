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

#include "storage/src/desktop/curl_requests.h"

#include <stdio.h>

#include <string>

#include "app/rest/util.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase {
namespace storage {
namespace internal {

static const char* kInvalidJsonResponse =
    "The server did not return a valid JSON response.  "
    "Contact Firebase support if this issue persists.";

// Utility function to map HTTP status requests onto Firebase Error Codes.
// Note that the mapping is not 1:1, so not all Firebase error codes can be
// returned.  (A lot of them end up as kErrorUnknown, due to ambiguity.)
Error HttpToErrorCode(int http_status) {
  switch (http_status) {
    case rest::util::HttpSuccess:
      return kErrorNone;
    case rest::util::HttpBadRequest:
      return kErrorUnknown;
    case rest::util::HttpUnauthorized:
      return kErrorUnauthenticated;
    case rest::util::HttpPaymentRequired:
      return kErrorQuotaExceeded;
    case rest::util::HttpForbidden:
      return kErrorUnauthorized;
    case rest::util::HttpNotFound:
      // Note: This code is ambiguous, since 404 is also returned
      // from kBucketNotFound errors...
      return kErrorObjectNotFound;
    default:
      // For any other error, we don't have enough information to make
      // a good determination.
      return kErrorUnknown;
  }
}

// ref_future must be allocated using FutureManager to ensure ref_future
// remains valid while the future handle isn't complete.
BlockingResponse::BlockingResponse(FutureHandle handle,
                                   ReferenceCountedFutureImpl* ref_future)
    : handle_(handle), ref_future_(ref_future) {}

BlockingResponse::~BlockingResponse() {
  // If the response isn't complete, cancel it.
  if (status() == rest::util::HttpInvalid) {
    set_status(rest::util::HttpNoContent);
    MarkFailed();
  }
}

void BlockingResponse::MarkCompleted() { rest::Response::MarkCompleted(); }

void BlockingResponse::MarkFailed() {
  rest::Response::MarkFailed();
  if (status() == rest::util::HttpRequestTimeout) {
    ref_future_->Complete(SafeFutureHandle<void>(handle_),
                          kErrorRetryLimitExceeded);
  } else {
    ref_future_->Complete(SafeFutureHandle<void>(handle_), kErrorCancelled);
  }
  NotifyFailed();
}

void BlockingResponse::NotifyComplete() { notifier_.NotifyComplete(); }

void BlockingResponse::NotifyFailed() { notifier_.NotifyFailed(); }

// Read in the raw response string, and try to make sense of it.
bool StorageNetworkError::Parse(const char* json_txt) {
  if (!rest::util::JsonData::Parse(json_txt)) return false;

  // Error checking!  Bail out if...
  if (!root_.is_map()) return false;  // Top level isn't a map.
  // Top level doesn't have a child called "error".
  if (root_.map().find(Variant("error")) == root_.map().end()) return false;
  Variant error = root_.map()[Variant("error")];
  if (!error.is_map()) return false;  // The "error" child is not a map.
  // Error child does not contain code/message children.
  if (error.map().find(Variant("code")) == error.map().end() ||
      error.map().find(Variant("message")) == error.map().end()) {
    return false;
  }
  // Code and Message are not the correct type.
  if (!error.map()[Variant("code")].is_int64() ||
      !error.map()[Variant("message")].is_string()) {
    return false;
  }
  error_code_ = static_cast<int>(error.map()[Variant("code")].int64_value());
  // Add the error code, as the message isn't always useful.
  error_message_ = error.map()[Variant("message")].string_value() +
                   std::string("  Http Code: ") +
                   error.map()[Variant("code")].AsString().string_value();
  is_valid_ = true;
  return true;
}

GetBytesResponse::GetBytesResponse(void* buffer, size_t buffer_size,
                                   SafeFutureHandle<size_t> handle,
                                   ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      output_buffer_(buffer),
      buffer_size_(buffer_size),
      buffer_index_(0) {}

// Since buffer may NOT necessarily end with \0, pass in length.
bool GetBytesResponse::ProcessBody(const char* buffer, size_t length) {
  size_t bytes_to_copy = (std::min)(length, buffer_size_ - buffer_index_);

  if (bytes_to_copy) {
    memcpy(static_cast<char*>(output_buffer_) + buffer_index_, buffer,
           bytes_to_copy);
    buffer_index_ += bytes_to_copy;
    NotifyProgress();
    return true;
  }
  // If we copied zero bytes, then we tried to overrun our buffer.  Return
  // false to signal an error.
  return false;
}

void GetBytesResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<size_t> handle(handle_);
  if (status() == rest::util::HttpSuccess) {
    ref_future_->CompleteWithResult(handle, kErrorNone, buffer_index_);
  } else {
    StorageNetworkError response;
    if (response.Parse(static_cast<const char*>(output_buffer_))) {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            response.error_message().c_str());
    } else {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            kInvalidJsonResponse);
    }
  }
  NotifyProgress();
  BlockingResponse::NotifyComplete();
}

EmptyResponse::EmptyResponse(SafeFutureHandle<void> handle,
                             ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future) {}

bool EmptyResponse::ProcessBody(const char* buffer, size_t length) {
  buffer_ += std::string(buffer, length);
  NotifyProgress();
  return true;
}

void EmptyResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<void> handle(handle_);
  if (status() == rest::util::HttpNoContent) {
    ref_future_->Complete(handle, kErrorNone);
  } else {
    StorageNetworkError response;
    if (response.Parse(buffer_.c_str())) {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            response.error_message().c_str());
    } else {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            kInvalidJsonResponse);
    }
  }
  NotifyProgress();
  BlockingResponse::NotifyComplete();
}

GetFileResponse::GetFileResponse(const char* filename,
                                 SafeFutureHandle<size_t> handle,
                                 ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      filename_(filename),
      bytes_written_(0) {}

// Since buffer may NOT necessarily end with \0, pass in length.
bool GetFileResponse::ProcessBody(const char* buffer, size_t length) {
  // Things are fine, send the received data to a file.
  if (status() == rest::util::HttpSuccess) {
    if (!file_.is_open()) {
      // Writing in binary mode prevents Windows from converting characters such
      // as "\n" to "\r\n".
      file_.open(filename_, std::ios::out | std::ios::binary);
    }
    file_.write(buffer, length);
    bytes_written_ += length;
  } else {
    // Things are not fine.  Send to a buffer so we can parse the error
    // response later.
    error_buffer_.append(buffer);
  }
  NotifyProgress();
  return true;
}

void GetFileResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<size_t> future_handle_with_size(handle_);
  if (status() == rest::util::HttpSuccess) {
    file_.close();
    ref_future_->CompleteWithResult(future_handle_with_size, kErrorNone,
                                    bytes_written_);
  } else {
    StorageNetworkError response;
    if (response.Parse(error_buffer_.c_str())) {
      ref_future_->CompleteWithResult(
          future_handle_with_size, HttpToErrorCode(status()),
          response.error_message().c_str(), bytes_written_);
    } else {
      // No dice, couldn't parse the bytes we got.
      ref_future_->CompleteWithResult(future_handle_with_size,
                                      HttpToErrorCode(status()),
                                      kInvalidJsonResponse, bytes_written_);
    }
  }
  NotifyProgress();
  BlockingResponse::NotifyComplete();
}

ReturnedMetadataResponse::ReturnedMetadataResponse(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* ref_future,
    const StorageReference& storage_reference)
    : BlockingResponse(handle.get(), ref_future),
      storage_reference_(storage_reference) {}

bool ReturnedMetadataResponse::ProcessBody(const char* buffer, size_t length) {
  buffer_ += std::string(buffer, length);
  NotifyProgress();
  return true;
}

void ReturnedMetadataResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<Metadata> handle(handle_);
  if (status() == rest::util::HttpSuccess) {
    MetadataInternal* metadata_internal =
        new MetadataInternal(storage_reference_);
    if (metadata_internal->ImportFromJson(buffer_.c_str())) {
      ref_future_->CompleteWithResult(
          handle, kErrorNone, MetadataInternal::AsMetadata(metadata_internal));
    } else {
      // The HTTP request was successful, but it returned invalid metadata JSON.
      ref_future_->Complete(handle, kErrorUnknown, kInvalidJsonResponse);
      delete metadata_internal;
    }
  } else {
    StorageNetworkError response;
    if (response.Parse(buffer_.c_str())) {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            response.error_message().c_str());
    } else {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            kInvalidJsonResponse);
    }
  }
  NotifyProgress();
  BlockingResponse::NotifyComplete();
}

}  // namespace internal
}  // namespace storage
}  // namespace firebase
