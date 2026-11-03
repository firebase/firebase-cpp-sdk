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

// Notifies a subscriber via Notifier::UpdateCallback of UpdateCallbackType
// events (completion, cancelation and progress of a transfer).
class Notifier {
 public:
  // Event used to notify a subscriber when the blocking response has
  // progress to report, is complete or canceled.
  enum UpdateCallbackType {
    kUpdateCallbackTypeComplete = 0,
    kUpdateCallbackTypeFailed,
    kUpdateCallbackTypeProgress,
  };

  // Used to notify a subscriber of an update to the response state.
  using UpdateCallback = void (*)(UpdateCallbackType update_type, void* data);

  Notifier() = default;

  // Set the callback to be notified about this object.
  void set_update_callback(UpdateCallback callback, void* callback_data) {
    MutexLock lock(mutex_);
    update_callback_ = callback;
    update_callback_data_ = callback_data;
  }

  // Report completion of this response.
  // This should be the last thing a request calls.
  // This *must* be performed at a point where it's possible to delete this
  // object.
  void NotifyComplete() { Notify(kUpdateCallbackTypeComplete); }

  // Report failure of this response.
  // This *must* be performed at a point where it's possible to delete this
  // object.
  void NotifyFailed() { Notify(kUpdateCallbackTypeFailed); }

  // Convenience function to perform null checks and update the listener when
  // progress has happened.
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
    if (callback) {
      callback(type, data);
    }
  }

  mutable Mutex mutex_;
  UpdateCallback update_callback_ = nullptr;
  void* update_callback_data_ = nullptr;
};

// C++17 mixin that wraps a rest::Request base class with Notifier callbacks.
template <typename Base>
class NotifyingRequest : public Base {
 public:
  using Base::Base;

  Notifier* notifier() { return &notifier_; }

  // Ordering invariant (deliberately Base first, notifications last):
  //  * Base::MarkCompleted()/MarkFailed() (rest::Request) only set the
  //    `completed_` flag; they signal no future, condition or callback, so
  //    nothing can delete this request while they run. Calling them first
  //    means `completed()` is already accurate when the notifications fire.
  //  * Per the Notifier contract, NotifyComplete()/NotifyFailed() must be the
  //    last thing done: after they return this object may have been deleted,
  //    so no member may be touched afterwards (which is what the original
  //    macro's "notify, then Base::Mark*()" order violated in principle).
  //  * Ownership: the request is owned by RestOperation::request_ and is only
  //    deleted with its RestOperation. RestOperation ignores Complete/Failed
  //    from the request's notifier (it reacts only to progress); the
  //    operation becomes deletable only once the *response's*
  //    MarkCompleted()/MarkFailed() notifies completion, which
  //    RestTransfer::CompleteOperation() calls strictly after this method has
  //    returned, on the same curl I/O thread. Hence neither the Base call nor
  //    the notifications below can race with destruction of this request.
  void MarkCompleted() override {
    Base::MarkCompleted();
    notifier_.NotifyProgress();
    notifier_.NotifyComplete();  // Must be last; see above.
  }

  // See MarkCompleted() for the ordering/ownership invariant.
  void MarkFailed() override {
    Base::MarkFailed();
    notifier_.NotifyProgress();
    notifier_.NotifyFailed();  // Must be last; see MarkCompleted().
  }

  size_t ReadBody(char* buffer, size_t length, bool* abort) override {
    size_t read_size = Base::ReadBody(buffer, length, abort);
    notifier_.NotifyProgress();
    return read_size;
  }

 protected:
  Notifier notifier_;
};

// Base request.
using Request = NotifyingRequest<rest::Request>;

// Reads from the specified buffer.
using RequestBinary = NotifyingRequest<rest::RequestBinary>;

// Reads from a file.
using RequestFile = NotifyingRequest<rest::RequestFile>;

// TODO(b/68854714): merge with the blocking response in query_desktop.
// b/68854714
class BlockingResponse : public rest::Response {
 public:
  // ref_future must be allocated using FutureManager to ensure ref_future
  // remains valid while the future handle isn't complete.
  BlockingResponse(FutureHandle handle, ReferenceCountedFutureImpl* ref_future);
  ~BlockingResponse() override;

  // NOTE: This does *not* call the UpdateCallback.  Each subclass needs to
  // manually complete with NotifyComplete or NotifyFailed.
  void MarkCompleted() override;

  // NOTE: This calls NotifyFailed which will notify the UpdateCallback with
  // a failure event.
  void MarkFailed() override;

  // Set the callback to be notified about this object.
  void set_update_callback(Notifier::UpdateCallback callback,
                           void* callback_data) {
    notifier_.set_update_callback(callback, callback_data);
  }

  // Optional shared atomic to record the final HTTP status code before the
  // response may be destroyed by CleanupCompletedOperations.
  void set_http_status_out(std::shared_ptr<std::atomic<int>> http_status_out) {
    out_http_status_ = std::move(http_status_out);
  }

 protected:
  // Report completion of this response.
  // This should be the last thing a request calls.  This *must* be performed at
  // a point where it's possible to delete this object.
  void NotifyComplete();
  // Report failure of this response.
  // This *must* be performed at a point where it's possible to delete this
  // object.
  void NotifyFailed();
  // Convenience function to perform null checks and update the listener when
  // progress has happened.
  void NotifyProgress() { notifier_.NotifyProgress(); }

  void RecordHttpStatus() {
    if (out_http_status_) {
      out_http_status_->store(status());
    }
  }

  FutureHandle handle_;
  ReferenceCountedFutureImpl* ref_future_ = nullptr;
  bool completed_or_failed_ = false;

 private:
  Notifier notifier_;
  std::shared_ptr<std::atomic<int>> out_http_status_;
};

// Response class for operations that don't return any data.  (i. e. delete.)
class EmptyResponse : public BlockingResponse {
 public:
  EmptyResponse(SafeFutureHandle<void> handle,
                ReferenceCountedFutureImpl* ref_future);
  bool ProcessBody(const char* buffer, size_t length) override;
  void MarkCompleted() override;

 private:
  // In theory there will be no response so we shouldn't need a buffer.
  // In practice, we may receive error messages if we did something wrong,
  // and if that happens, we need a place to store them.
  std::string buffer_;
};

// Response class for downloading a storage resource into memory, via CURL.
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

// Response for downloading a storage resource directly into a file.
// Primarily useful because it doesn't have to fit in memory - the pieces get
// shoved into an fstream as they are received.
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

// Response for any operation that returns a blob of text that we need
// to interpret as metadata.
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

// Response for any operation that returns a blob of text that we need
// to interpret as a list result.
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

// Utility class for parsing a Storage REST error response (in JSON form) and
// deserializing it into usable data.  The input buffer (json_response) does not
// need to remain valid after the constructor has run. The expected JSON should
// have the following format:
//
//  {
//    "error": {
//      "code": 403,
//      "message": "Permission denied. Could not perform this operation"
//    }
//  }
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
