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

#ifndef FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CURL_REQUESTS_H_
#define FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CURL_REQUESTS_H_

#include <fstream>

#include "app/rest/request_binary.h"
#include "app/rest/request_file.h"
#include "app/rest/response_binary.h"
#include "app/rest/transport_builder.h"
#include "app/rest/util.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/semaphore.h"
#include "storage/src/desktop/listener_desktop.h"
#include "storage/src/desktop/storage_desktop.h"
#include "storage/src/include/firebase/storage/common.h"
#include "storage/src/include/firebase/storage/controller.h"
#include "storage/src/include/firebase/storage/listener.h"
#include "storage/src/include/firebase/storage/storage_reference.h"

namespace firebase {
namespace storage {
namespace internal {

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

  // Used to notify a subscriber of an update to the response state,
  typedef void (*UpdateCallback)(UpdateCallbackType update_type, void* data);

 public:
  Notifier() : update_callback_(nullptr), update_callback_data_(nullptr) {}

  // Set the callback to be notified about this object.
  void set_update_callback(UpdateCallback callback, void* callback_data) {
    update_callback_ = callback;
    update_callback_data_ = callback_data;
  }

  // Report completion of this response.
  // This should be the last thing a request calls.
  // This *must* be performed at a point where it's possible to delete this
  // object.
  void NotifyComplete() {
    if (update_callback_) {
      update_callback_(kUpdateCallbackTypeComplete, update_callback_data_);
    }
  }
  // Report failure of this response.
  // This *must* be performed at a point where it's possible to delete this
  // object.
  void NotifyFailed() {
    if (update_callback_) {
      update_callback_(kUpdateCallbackTypeFailed, update_callback_data_);
    }
  }

  // Convenience function to perform null checks and update the listener when
  // progress has happened.
  void NotifyProgress() {
    if (update_callback_) {
      update_callback_(kUpdateCallbackTypeProgress, update_callback_data_);
    }
  }

 private:
  UpdateCallback update_callback_;
  void* update_callback_data_;
};

// Generates the common body of a request class.
#define FIREBASE_STORAGE_REQUEST_CLASS_BODY(base_class_name)             \
  Notifier* notifier() { return &notifier_; }                            \
                                                                         \
  void MarkCompleted() override {                                        \
    notifier_.NotifyProgress();                                          \
    notifier_.NotifyComplete();                                          \
    base_class_name::MarkCompleted();                                    \
  }                                                                      \
                                                                         \
  void MarkFailed() override {                                           \
    notifier_.NotifyProgress();                                          \
    notifier_.NotifyFailed();                                            \
    base_class_name::MarkFailed();                                       \
  }                                                                      \
                                                                         \
  size_t ReadBody(char* buffer, size_t length, bool* abort) override {   \
    size_t read_size = base_class_name::ReadBody(buffer, length, abort); \
    notifier_.NotifyProgress();                                          \
    return read_size;                                                    \
  }                                                                      \
                                                                         \
 protected:                                                              \
  Notifier notifier_

// Base request.
class Request : public rest::Request {
 public:
  Request() {}

  FIREBASE_STORAGE_REQUEST_CLASS_BODY(rest::Request);
};

// Reads from the specified buffer.
class RequestBinary : public rest::RequestBinary {
 public:
  RequestBinary(const char* buffer, size_t buffer_size)
      : rest::RequestBinary(buffer, buffer_size) {}

  FIREBASE_STORAGE_REQUEST_CLASS_BODY(rest::RequestBinary);
};

// Reads from a file.
class RequestFile : public rest::RequestFile {
 public:
  RequestFile(const char* filename, size_t offset)
      : rest::RequestFile(filename, offset) {}

  FIREBASE_STORAGE_REQUEST_CLASS_BODY(rest::RequestFile);
};

// TODO(b/68854714): merge with the blocking response in query_desktop.
// b/68854714
class BlockingResponse : public rest::Response {
 public:
  // ref_future must be allocated using FutureManager to ensure ref_future
  // remains valid while the future handle isn't complete.
  BlockingResponse(FutureHandle handle, ReferenceCountedFutureImpl* ref_future);
  virtual ~BlockingResponse();

  // NOTE: This does *not* call the UpdateCallback.  Each subclass needs to
  // manually complete with NotifyComplete.
  void MarkCompleted() override;

  // NOTE: This calls NotifyFailed which will notify the UpdateCallback with
  // a failure event.
  void MarkFailed() override;

  // Set the callback to be notified about this object.
  void set_update_callback(Notifier::UpdateCallback callback,
                           void* callback_data) {
    notifier_.set_update_callback(callback, callback_data);
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

 private:
  Notifier notifier_;

 protected:
  FutureHandle handle_;
  ReferenceCountedFutureImpl* ref_future_;
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

 private:
  void* output_buffer_;
  size_t buffer_size_;
  size_t buffer_index_;
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

 private:
  std::string filename_;
  std::string error_buffer_;
  std::fstream file_;
  size_t bytes_written_;
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

// Utility class for parsing a Storage REST error response (in JSON form) and
// deserializing it into usable data.  The input buffer (json_response) does not
// need to remain valid after the constructor has run. The expected JSON should
// have the following format:
//
//  {
//    {
//      "error": {
//      "code": 403,
//      "message": "Permission denied. Could not perform this operation"
//    }
//  }
class StorageNetworkError : rest::util::JsonData {
 public:
  StorageNetworkError() : error_code_(0), is_valid_(false) {}

  bool Parse(const char* json_txt) override;
  int error_code() const { return error_code_; }
  std::string error_message() const { return error_message_; }
  bool is_valid() override { return is_valid_; }

 private:
  int error_code_;
  std::string error_message_;
  bool is_valid_;
};

}  // namespace internal
}  // namespace storage
}  // namespace firebase

#endif  // FIREBASE_STORAGE_CLIENT_CPP_SRC_DESKTOP_CURL_REQUESTS_H_
