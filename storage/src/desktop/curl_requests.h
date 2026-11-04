// Copyright 2017 Google LLC

#ifndef FIREBASE_STORAGE_SRC_DESKTOP_CURL_REQUESTS_H_
#define FIREBASE_STORAGE_SRC_DESKTOP_CURL_REQUESTS_H_

#include <atomic>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/rest/request.h"
#include "app/rest/request_binary.h"
#include "app/rest/request_file.h"
#include "app/rest/response.h"
#include "app/rest/util.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/list_result.h"
#include "storage/src/include/firebase/storage/metadata.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase::storage::internal {

class Notifier {
 public:
  enum UpdateCallbackType {
    kUpdateCallbackTypeComplete = 0,
    kUpdateCallbackTypeFailed,
    kUpdateCallbackTypeProgress,
  };
  using UpdateCallback = void (*)(UpdateCallbackType update_type, void* data);

  Notifier() = default;
  void set_update_callback(UpdateCallback callback, void* callback_data) {
    MutexLock lock(mutex_);
    update_callback_ = callback;
    update_callback_data_ = callback_data;
  }
  void NotifyComplete() { Notify(kUpdateCallbackTypeComplete); }
  void NotifyFailed() { Notify(kUpdateCallbackTypeFailed); }
  void NotifyProgress() { Notify(kUpdateCallbackTypeProgress); }

 private:
  void Notify(UpdateCallbackType type) {
    UpdateCallback callback = nullptr;
    void* data = nullptr;
    {
      MutexLock lock(mutex_);
      callback = update_callback_;
      data = update_callback_data_;
    }
    if (callback) callback(type, data);
  }

  mutable Mutex mutex_;
  UpdateCallback update_callback_ = nullptr;
  void* update_callback_data_ = nullptr;
};

template <typename Base>
class NotifyingRequest : public Base {
 public:
  using Base::Base;
  Notifier* notifier() { return &notifier_; }

  void MarkCompleted() override {
    Base::MarkCompleted();
    notifier_.NotifyProgress();
    notifier_.NotifyComplete();
  }
  void MarkFailed() override {
    Base::MarkFailed();
    notifier_.NotifyProgress();
    notifier_.NotifyFailed();
  }
  size_t ReadBody(char* buffer, size_t length, bool* abort) override {
    size_t read_size = Base::ReadBody(buffer, length, abort);
    notifier_.NotifyProgress();
    return read_size;
  }

 protected:
  Notifier notifier_;
};

using Request = NotifyingRequest<rest::Request>;
using RequestBinary = NotifyingRequest<rest::RequestBinary>;
using RequestFile = NotifyingRequest<rest::RequestFile>;

class BlockingResponse : public rest::Response {
 public:
  BlockingResponse(FutureHandle handle, ReferenceCountedFutureImpl* ref_future);
  ~BlockingResponse() override;

  void MarkCompleted() override;
  void MarkFailed() override;

  void set_update_callback(Notifier::UpdateCallback callback,
                           void* callback_data) {
    notifier_.set_update_callback(callback, callback_data);
  }
  void set_http_status_out(std::shared_ptr<std::atomic<int>> http_status_out) {
    out_http_status_ = std::move(http_status_out);
  }

 protected:
  void NotifyComplete() { notifier_.NotifyComplete(); }
  void NotifyFailed() { notifier_.NotifyFailed(); }
  void NotifyProgress() { notifier_.NotifyProgress(); }
  void Finish(bool succeeded) {
    NotifyProgress();
    if (succeeded) {
      NotifyComplete();
    } else {
      NotifyFailed();
    }
  }
  void RecordHttpStatus() {
    if (out_http_status_) out_http_status_->store(status());
  }

  FutureHandle handle_;
  ReferenceCountedFutureImpl* ref_future_ = nullptr;
  bool completed_or_failed_ = false;

 private:
  Notifier notifier_;
  std::shared_ptr<std::atomic<int>> out_http_status_;
};

class EmptyResponse : public BlockingResponse {
 public:
  EmptyResponse(SafeFutureHandle<void> handle,
                ReferenceCountedFutureImpl* ref_future);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;

 private:
  std::string buffer_;
};

class GetBytesResponse : public BlockingResponse {
 public:
  GetBytesResponse(void* buffer, size_t buffer_size,
                   SafeFutureHandle<size_t> handle,
                   ReferenceCountedFutureImpl* ref_future);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;
  void MarkFailed() override;

 private:
  void* output_buffer_ = nullptr;
  size_t buffer_size_ = 0;
  size_t buffer_index_ = 0;
  std::string error_buffer_;
  bool buffer_overflow_ = false;
};

class GetFileResponse : public BlockingResponse {
 public:
  GetFileResponse(const char* filename, SafeFutureHandle<size_t> handle,
                  ReferenceCountedFutureImpl* ref_future);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;
  void MarkFailed() override;

 private:
  std::string filename_;
  std::string error_buffer_;
  std::fstream file_;
  size_t bytes_written_ = 0;
  bool file_error_ = false;
};

class ReturnedMetadataResponse : public BlockingResponse {
 public:
  ReturnedMetadataResponse(SafeFutureHandle<Metadata> handle,
                           ReferenceCountedFutureImpl* ref_future,
                           const StorageReference& storage_reference);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;

 private:
  std::string buffer_;
  StorageReference storage_reference_;
};

class ReturnedListResponse : public BlockingResponse {
 public:
  ReturnedListResponse(SafeFutureHandle<StorageListResult> handle,
                       ReferenceCountedFutureImpl* ref_future,
                       StorageInternal* storage);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;

 private:
  std::string buffer_;
  std::shared_ptr<StorageInternal::LifetimeToken> storage_token_;
};

class StorageNetworkError : private rest::util::JsonData {
 public:
  StorageNetworkError() = default;
  bool Parse(const char* json_txt) override;
  int error_code() const { return error_code_; }
  const std::string& error_message() const { return error_message_; }
  bool is_valid() override { return is_valid_; }

 private:
  int error_code_ = 0;
  std::string error_message_;
  bool is_valid_ = false;
};

}  // namespace firebase::storage::internal

#endif  // FIREBASE_STORAGE_SRC_DESKTOP_CURL_REQUESTS_H_
