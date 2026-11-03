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

#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/util.h"
#include "app/src/log.h"
#include "app/src/variant_util.h"
#include "storage/src/common/list_result_internal.h"
#include "storage/src/desktop/metadata_desktop.h"
#include "storage/src/desktop/rest_operation.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/desktop/storage_reference_desktop.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/metadata.h"

namespace firebase::storage::internal {

static const char* const kInvalidJsonResponse =
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
  // If the response wasn't completed or failed yet, cancel it.
  if (!completed_or_failed_ && status() == rest::util::HttpInvalid) {
    set_status(rest::util::HttpNoContent);
    BlockingResponse::MarkFailed();
  }
}

void BlockingResponse::MarkCompleted() {
  completed_or_failed_ = true;
  RecordHttpStatus();
  rest::Response::MarkCompleted();
}

void BlockingResponse::MarkFailed() {
  if (completed_or_failed_) {
    return;
  }
  completed_or_failed_ = true;
  RecordHttpStatus();
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
  if (!json_txt || json_txt[0] == '\0') return false;
  if (!rest::util::JsonData::Parse(json_txt)) return false;

  // Error checking!  Bail out if...
  if (!root_.is_map()) return false;  // Top level isn't a map.
  const auto& root_map = root_.map();
  auto error_it = root_map.find(Variant("error"));
  if (error_it == root_map.end() || !error_it->second.is_map()) {
    return false;
  }
  const auto& error_map = error_it->second.map();
  auto code_it = error_map.find(Variant("code"));
  auto message_it = error_map.find(Variant("message"));
  if (code_it == error_map.end() || message_it == error_map.end()) {
    return false;
  }
  if (!code_it->second.is_int64() || !message_it->second.is_string()) {
    return false;
  }
  error_code_ = static_cast<int>(code_it->second.int64_value());
  // Add the error code, as the message isn't always useful.
  error_message_ = std::string(message_it->second.string_value()) +
                   "  Http Code: " +
                   code_it->second.AsString().string_value();
  is_valid_ = true;
  return true;
}

GetBytesResponse::GetBytesResponse(void* buffer, size_t buffer_size,
                                   SafeFutureHandle<size_t> handle,
                                   ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      output_buffer_(buffer),
      buffer_size_(buffer_size),
      buffer_index_(0),
      buffer_overflow_(false) {}

// Since buffer may NOT necessarily end with \0, pass in length.
bool GetBytesResponse::ProcessBody(const char* buffer, size_t length) {
  if (status() != rest::util::HttpSuccess) {
    if (buffer && length > 0) {
      error_buffer_.append(buffer, length);
    }
    NotifyProgress();
    return true;
  }
  if (length == 0) {
    NotifyProgress();
    return true;
  }
  if (output_buffer_ == nullptr || buffer == nullptr) {
    buffer_overflow_ = true;
    return false;
  }
  size_t remaining =
      buffer_index_ <= buffer_size_ ? (buffer_size_ - buffer_index_) : 0;
  if (length > remaining) {
    if (remaining > 0) {
      std::memcpy(static_cast<char*>(output_buffer_) + buffer_index_, buffer,
                  remaining);
      buffer_index_ += remaining;
    }
    buffer_overflow_ = true;
    return false;
  }
  std::memcpy(static_cast<char*>(output_buffer_) + buffer_index_, buffer,
              length);
  buffer_index_ += length;
  NotifyProgress();
  return true;
}

void GetBytesResponse::MarkFailed() {
  if (completed_or_failed_) {
    return;
  }
  if (buffer_overflow_) {
    completed_or_failed_ = true;
    RecordHttpStatus();
    rest::Response::MarkFailed();
    ref_future_->CompleteWithResult(
        SafeFutureHandle<size_t>(handle_), kErrorDownloadSizeExceeded,
        GetErrorMessage(kErrorDownloadSizeExceeded), buffer_index_);
    NotifyFailed();
    return;
  }
  BlockingResponse::MarkFailed();
}

void GetBytesResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<size_t> handle(handle_);
  bool succeeded = false;
  if (buffer_overflow_) {
    ref_future_->CompleteWithResult(
        handle, kErrorDownloadSizeExceeded,
        GetErrorMessage(kErrorDownloadSizeExceeded), buffer_index_);
  } else if (status() == rest::util::HttpSuccess) {
    ref_future_->CompleteWithResult(handle, kErrorNone, buffer_index_);
    succeeded = true;
  } else {
    StorageNetworkError response;
    if (response.Parse(error_buffer_.c_str())) {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            response.error_message().c_str());
    } else {
      ref_future_->Complete(handle, HttpToErrorCode(status()),
                            kInvalidJsonResponse);
    }
  }
  NotifyProgress();
  if (succeeded) {
    BlockingResponse::NotifyComplete();
  } else {
    BlockingResponse::NotifyFailed();
  }
}

EmptyResponse::EmptyResponse(SafeFutureHandle<void> handle,
                             ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future) {}

bool EmptyResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) {
    buffer_.append(buffer, length);
  }
  NotifyProgress();
  return true;
}

void EmptyResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<void> handle(handle_);
  const bool succeeded = (status() == rest::util::HttpNoContent ||
                          status() == rest::util::HttpSuccess);
  if (succeeded) {
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
  if (succeeded) {
    BlockingResponse::NotifyComplete();
  } else {
    BlockingResponse::NotifyFailed();
  }
}

GetFileResponse::GetFileResponse(const char* filename,
                                 SafeFutureHandle<size_t> handle,
                                 ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      filename_(filename ? filename : ""),
      bytes_written_(0),
      file_error_(false) {}

// Since buffer may NOT necessarily end with \0, pass in length.
bool GetFileResponse::ProcessBody(const char* buffer, size_t length) {
  // Things are fine, send the received data to a file.
  if (status() == rest::util::HttpSuccess) {
    if (!file_.is_open()) {
      if (filename_.empty()) {
        file_error_ = true;
        return false;
      }
      // Writing in binary mode prevents Windows from converting characters such
      // as "\n" to "\r\n".
      file_.open(filename_, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!file_.is_open() || file_.fail()) {
        file_error_ = true;
        return false;
      }
    }
    if (buffer && length > 0) {
      file_.write(buffer, static_cast<std::streamsize>(length));
      if (file_.fail()) {
        file_error_ = true;
        return false;
      }
      bytes_written_ += length;
    }
  } else {
    // Things are not fine. Send to a buffer so we can parse the error
    // response later.
    if (buffer && length > 0) {
      error_buffer_.append(buffer, length);
    }
  }
  NotifyProgress();
  return true;
}

void GetFileResponse::MarkFailed() {
  if (completed_or_failed_) {
    return;
  }
  if (file_.is_open()) {
    file_.close();
  }
  if (file_error_) {
    completed_or_failed_ = true;
    RecordHttpStatus();
    rest::Response::MarkFailed();
    ref_future_->CompleteWithResult(
        SafeFutureHandle<size_t>(handle_), kErrorUnknown,
        "Failed to open or write local destination file.", bytes_written_);
    NotifyFailed();
    return;
  }
  BlockingResponse::MarkFailed();
}

void GetFileResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<size_t> future_handle_with_size(handle_);
  bool succeeded = false;
  if (status() == rest::util::HttpSuccess) {
    if (!file_.is_open() && !file_error_) {
      if (filename_.empty()) {
        file_error_ = true;
      } else {
        file_.open(filename_,
                   std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open() || file_.fail()) {
          file_error_ = true;
        }
      }
    }
    if (file_.is_open()) {
      file_.close();
      if (file_.fail()) {
        file_error_ = true;
      }
    }
    if (file_error_) {
      ref_future_->CompleteWithResult(
          future_handle_with_size, kErrorUnknown,
          "Failed to open or write local destination file.", bytes_written_);
    } else {
      ref_future_->CompleteWithResult(future_handle_with_size, kErrorNone,
                                      bytes_written_);
      succeeded = true;
    }
  } else {
    if (file_.is_open()) {
      file_.close();
    }
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
  if (succeeded) {
    BlockingResponse::NotifyComplete();
  } else {
    BlockingResponse::NotifyFailed();
  }
}

ReturnedMetadataResponse::ReturnedMetadataResponse(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* ref_future,
    const StorageReference& storage_reference)
    : BlockingResponse(handle.get(), ref_future),
      storage_reference_(storage_reference) {}

bool ReturnedMetadataResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) {
    buffer_.append(buffer, length);
  }
  NotifyProgress();
  return true;
}

void ReturnedMetadataResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<Metadata> handle(handle_);
  bool succeeded = false;
  if (status() == rest::util::HttpSuccess) {
    auto metadata_internal =
        std::make_unique<MetadataInternal>(storage_reference_);
    if (metadata_internal->ImportFromJson(buffer_.c_str())) {
      ref_future_->CompleteWithResult(
          handle, kErrorNone,
          MetadataInternal::AsMetadata(metadata_internal.release()));
      succeeded = true;
    } else {
      // The HTTP request was successful, but it returned invalid metadata JSON.
      ref_future_->Complete(handle, kErrorUnknown, kInvalidJsonResponse);
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
  if (succeeded) {
    BlockingResponse::NotifyComplete();
  } else {
    BlockingResponse::NotifyFailed();
  }
}

ReturnedListResponse::ReturnedListResponse(
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* ref_future, StorageInternal* storage)
    : BlockingResponse(handle.get(), ref_future),
      storage_token_(storage ? storage->lifetime_token() : nullptr) {}

bool ReturnedListResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) {
    buffer_.append(buffer, length);
  }
  NotifyProgress();
  return true;
}

void ReturnedListResponse::MarkCompleted() {
  BlockingResponse::MarkCompleted();
  SafeFutureHandle<StorageListResult> handle(handle_);
  bool succeeded = false;
  if (status() == rest::util::HttpSuccess) {
    Variant response_variant = util::JsonToVariant(buffer_.c_str());
    if (response_variant.is_map()) {
      // Keep the StorageInternal alive while references are created from it
      // by registering as an active user of its lifetime token, rather than
      // by holding LifetimeToken::mutex: the temporary
      // StorageReferenceInternal objects created below lock that same
      // (non-recursive) mutex in their destructors (~FutureApiOwner), so
      // holding it here would self-deadlock.
      std::optional<StorageLifetimeScope> storage_scope;
      storage_scope.emplace(storage_token_,
                            StorageLifetimeScope::Mode::kRejectShutdown);
      StorageInternal* storage = storage_scope->storage();
      if (!storage) {
        storage_scope.reset();
        ref_future_->Complete(handle, kErrorCancelled,
                              GetErrorMessage(kErrorCancelled));
      } else {
        const auto& map = response_variant.map();
        std::vector<StorageReference> prefixes;
        std::vector<StorageReference> items;
        std::string next_page_token;

        auto prefixes_it = map.find(Variant("prefixes"));
        if (prefixes_it != map.end() && prefixes_it->second.is_vector()) {
          for (const auto& prefix_variant : prefixes_it->second.vector()) {
            if (prefix_variant.is_string()) {
              std::string prefix_str = prefix_variant.string_value();
              // The prefix returned from GCS/Firebase Storage is a path
              // relative to the bucket root.
              if (!prefix_str.empty() && prefix_str.back() == '/') {
                prefix_str.pop_back();
              }
              // StorageInternal::GetReference() transfers ownership; Adopt()
              // hands it to the StorageReference without a temporary copy.
              prefixes.push_back(StorageReferenceInternal::Adopt(
                  std::unique_ptr<StorageReferenceInternal>(
                      storage->GetReference(prefix_str.c_str()))));
            }
          }
        }

        auto items_it = map.find(Variant("items"));
        if (items_it != map.end() && items_it->second.is_vector()) {
          for (const auto& item_variant : items_it->second.vector()) {
            if (item_variant.is_map()) {
              const auto& item_map = item_variant.map();
              auto name_it = item_map.find(Variant("name"));
              if (name_it != item_map.end() && name_it->second.is_string()) {
                // Ownership transferred to the StorageReference (see above).
                items.push_back(StorageReferenceInternal::Adopt(
                    std::unique_ptr<StorageReferenceInternal>(
                        storage->GetReference(
                            name_it->second.string_value()))));
              }
            }
          }
        }

        storage_scope.reset();

        auto token_it = map.find(Variant("nextPageToken"));
        if (token_it != map.end() && token_it->second.is_string()) {
          next_page_token = token_it->second.string_value();
        }

        auto* result_internal = new internal::StorageListResultInternal(
            std::move(prefixes), std::move(items), next_page_token);
        ref_future_->CompleteWithResult(
            handle, kErrorNone, "",
            internal::StorageListResultInternal::AsStorageListResult(
                result_internal));
        succeeded = true;
      }
    } else {
      LogWarning("Failed to parse list response. Buffer: %s", buffer_.c_str());
      ref_future_->Complete(handle, kErrorUnknown, kInvalidJsonResponse);
    }
  } else {
    LogWarning("List request failed with status %d. Buffer: %s", status(),
               buffer_.c_str());
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
  if (succeeded) {
    BlockingResponse::NotifyComplete();
  } else {
    BlockingResponse::NotifyFailed();
  }
}

}  // namespace firebase::storage::internal
