// Copyright 2017 Google LLC

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
#include "storage/src/common/metadata_internal.h"
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

Error HttpToErrorCode(int http_status) {
  switch (http_status) {
    case rest::util::HttpSuccess:
      return kErrorNone;
    case rest::util::HttpUnauthorized:
      return kErrorUnauthenticated;
    case rest::util::HttpPaymentRequired:
      return kErrorQuotaExceeded;
    case rest::util::HttpForbidden:
      return kErrorUnauthorized;
    case rest::util::HttpNotFound:
      return kErrorObjectNotFound;
    default:
      return kErrorUnknown;
  }
}

namespace {

template <typename T>
void CompleteHttpError(ReferenceCountedFutureImpl* future,
                       SafeFutureHandle<T> handle, int status,
                       const std::string& body) {
  StorageNetworkError err;
  future->Complete(handle, HttpToErrorCode(status),
                   err.Parse(body.c_str()) ? err.error_message().c_str()
                                           : kInvalidJsonResponse);
}

}  // namespace

BlockingResponse::BlockingResponse(FutureHandle handle,
                                   ReferenceCountedFutureImpl* ref_future)
    : handle_(handle), ref_future_(ref_future) {}

BlockingResponse::~BlockingResponse() {
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
  if (completed_or_failed_) return;
  completed_or_failed_ = true;
  RecordHttpStatus();
  rest::Response::MarkFailed();
  ref_future_->Complete(SafeFutureHandle<void>(handle_),
                        status() == rest::util::HttpRequestTimeout
                            ? kErrorRetryLimitExceeded
                            : kErrorCancelled);
  NotifyFailed();
}

bool StorageNetworkError::Parse(const char* json_txt) {
  if (!json_txt || json_txt[0] == '\0' || !rest::util::JsonData::Parse(json_txt) ||
      !root_.is_map()) {
    return false;
  }
  const auto& root_map = root_.map();
  auto error_it = root_map.find(Variant("error"));
  if (error_it == root_map.end() || !error_it->second.is_map()) return false;
  const auto& error_map = error_it->second.map();
  auto code_it = error_map.find(Variant("code"));
  auto message_it = error_map.find(Variant("message"));
  if (code_it == error_map.end() || message_it == error_map.end() ||
      !code_it->second.is_int64() || !message_it->second.is_string()) {
    return false;
  }
  error_code_ = static_cast<int>(code_it->second.int64_value());
  error_message_ = std::string(message_it->second.string_value()) +
                   "  Http Code: " + code_it->second.AsString().string_value();
  is_valid_ = true;
  return true;
}

GetBytesResponse::GetBytesResponse(void* buffer, size_t buffer_size,
                                   SafeFutureHandle<size_t> handle,
                                   ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      output_buffer_(buffer),
      buffer_size_(buffer_size) {}

bool GetBytesResponse::ProcessBody(const char* buffer, size_t length) {
  if (status() != rest::util::HttpSuccess) {
    if (buffer && length > 0) error_buffer_.append(buffer, length);
    NotifyProgress();
    return true;
  }
  if (length == 0) {
    NotifyProgress();
    return true;
  }
  if (!output_buffer_ || !buffer) {
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
  if (completed_or_failed_) return;
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
    CompleteHttpError(ref_future_, handle, status(), error_buffer_);
  }
  Finish(succeeded);
}

EmptyResponse::EmptyResponse(SafeFutureHandle<void> handle,
                             ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future) {}

bool EmptyResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) buffer_.append(buffer, length);
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
    CompleteHttpError(ref_future_, handle, status(), buffer_);
  }
  Finish(succeeded);
}

GetFileResponse::GetFileResponse(const char* filename,
                                 SafeFutureHandle<size_t> handle,
                                 ReferenceCountedFutureImpl* ref_future)
    : BlockingResponse(handle.get(), ref_future),
      filename_(filename ? filename : "") {}

bool GetFileResponse::ProcessBody(const char* buffer, size_t length) {
  if (status() == rest::util::HttpSuccess) {
    if (!file_.is_open()) {
      if (filename_.empty()) {
        file_error_ = true;
        return false;
      }
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
  } else if (buffer && length > 0) {
    error_buffer_.append(buffer, length);
  }
  NotifyProgress();
  return true;
}

void GetFileResponse::MarkFailed() {
  if (completed_or_failed_) return;
  if (file_.is_open()) file_.close();
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
  SafeFutureHandle<size_t> handle(handle_);
  bool succeeded = false;
  if (status() == rest::util::HttpSuccess) {
    if (!file_.is_open() && !file_error_) {
      if (filename_.empty()) {
        file_error_ = true;
      } else {
        file_.open(filename_,
                   std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file_.is_open() || file_.fail()) file_error_ = true;
      }
    }
    if (file_.is_open()) {
      file_.close();
      if (file_.fail()) file_error_ = true;
    }
    if (file_error_) {
      ref_future_->CompleteWithResult(
          handle, kErrorUnknown,
          "Failed to open or write local destination file.", bytes_written_);
    } else {
      ref_future_->CompleteWithResult(handle, kErrorNone, bytes_written_);
      succeeded = true;
    }
  } else {
    if (file_.is_open()) file_.close();
    StorageNetworkError response;
    ref_future_->CompleteWithResult(
        handle, HttpToErrorCode(status()),
        response.Parse(error_buffer_.c_str()) ? response.error_message().c_str()
                                              : kInvalidJsonResponse,
        bytes_written_);
  }
  Finish(succeeded);
}

ReturnedMetadataResponse::ReturnedMetadataResponse(
    SafeFutureHandle<Metadata> handle, ReferenceCountedFutureImpl* ref_future,
    const StorageReference& storage_reference)
    : BlockingResponse(handle.get(), ref_future),
      storage_reference_(storage_reference) {}

bool ReturnedMetadataResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) buffer_.append(buffer, length);
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
      ref_future_->Complete(handle, kErrorUnknown, kInvalidJsonResponse);
    }
  } else {
    CompleteHttpError(ref_future_, handle, status(), buffer_);
  }
  Finish(succeeded);
}

ReturnedListResponse::ReturnedListResponse(
    SafeFutureHandle<StorageListResult> handle,
    ReferenceCountedFutureImpl* ref_future, StorageInternal* storage)
    : BlockingResponse(handle.get(), ref_future),
      storage_token_(storage ? storage->lifetime_token() : nullptr) {}

bool ReturnedListResponse::ProcessBody(const char* buffer, size_t length) {
  if (buffer && length > 0) buffer_.append(buffer, length);
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
      std::optional<StorageLifetimeScope> storage_scope(
          std::in_place, storage_token_,
          StorageLifetimeScope::Mode::kRejectShutdown);
      StorageInternal* storage = storage_scope->storage();
      if (!storage) {
        storage_scope.reset();
        ref_future_->Complete(handle, kErrorCancelled,
                              GetErrorMessage(kErrorCancelled));
      } else {
        const auto& map = response_variant.map();
        std::vector<StorageReference> prefixes, items;
        std::string next_page_token;

        auto prefixes_it = map.find(Variant("prefixes"));
        if (prefixes_it != map.end() && prefixes_it->second.is_vector()) {
          for (const auto& pv : prefixes_it->second.vector()) {
            if (pv.is_string()) {
              std::string p = pv.string_value();
              if (!p.empty() && p.back() == '/') p.pop_back();
              prefixes.push_back(StorageReferenceInternal::Adopt(
                  std::unique_ptr<StorageReferenceInternal>(
                      storage->GetReference(p.c_str()))));
            }
          }
        }
        auto items_it = map.find(Variant("items"));
        if (items_it != map.end() && items_it->second.is_vector()) {
          for (const auto& iv : items_it->second.vector()) {
            if (iv.is_map()) {
              auto name_it = iv.map().find(Variant("name"));
              if (name_it != iv.map().end() && name_it->second.is_string()) {
                items.push_back(StorageReferenceInternal::Adopt(
                    std::unique_ptr<StorageReferenceInternal>(
                        storage->GetReference(name_it->second.string_value()))));
              }
            }
          }
        }
        storage_scope.reset();

        auto token_it = map.find(Variant("nextPageToken"));
        if (token_it != map.end() && token_it->second.is_string()) {
          next_page_token = token_it->second.string_value();
        }
        ref_future_->CompleteWithResult(
            handle, kErrorNone, "",
            internal::StorageListResultInternal::AsStorageListResult(
                new internal::StorageListResultInternal(
                    std::move(prefixes), std::move(items), next_page_token)));
        succeeded = true;
      }
    } else {
      LogWarning("Failed to parse list response. Buffer: %s", buffer_.c_str());
      ref_future_->Complete(handle, kErrorUnknown, kInvalidJsonResponse);
    }
  } else {
    LogWarning("List request failed with status %d. Buffer: %s", status(),
               buffer_.c_str());
    CompleteHttpError(ref_future_, handle, status(), buffer_);
  }
  Finish(succeeded);
}

}  // namespace firebase::storage::internal
