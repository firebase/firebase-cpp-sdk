/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_H_

#include <cstddef>
#include <string>

#include "app/rest/request_options.h"
#include "app/rest/transfer_interface.h"

namespace firebase {
namespace rest {

// The base class to deal with HTTP/REST request.
class Request : public Transfer {
 public:
  Request() : Request(nullptr) {}

  // Create a request that will read post fields from the specified buffer.
  // NOTE: This will *not copy* data into the request.  The buffer must not be
  // deallocated while a transfer of this request is in progress.
  explicit Request(const char* post_fields_buffer,
                   size_t post_fields_buffer_size = 0);
  ~Request() override;

  // Sets URL for HTTP/REST request.
  virtual void set_url(const char* url) { options_.url = url; }

  // Sets HTTP/REST method.
  virtual void set_method(const char* method) { options_.method = method; }

  // If a POST fields were not specified on construction, it's possible to copy
  // data into this class to read here.
  // data must be formatted by hand; no conversion or encoding will be performed
  // on it.
  virtual void set_post_fields(const char* data, size_t size);
  virtual void set_post_fields(const char* data);

  // Get the size of the POST fields.
  virtual size_t GetPostFieldsSize() const { return read_buffer_size_; }

  // Adds a header line.
  virtual void add_header(const char* name, const char* value) {
    options_.header.emplace(name, value);
  }

  // Sets verbose to true to display more verbose info for debug.
  virtual void set_verbose(bool verbose) { options_.verbose = verbose; }

  // Returns all request options.
  const RequestOptions& options() const { return options_; }
  RequestOptions& options() { return options_; }

  // Convert into a human readable string (exposed for debugging).
  std::string ToString();

  // Called to read the body of the request to send to the server.
  // Returns the number of bytes written into the buffer, or 0 if the no more
  // data is available to send. To stop the transfer set abort to true.
  virtual size_t ReadBody(char* buffer, size_t length, bool* abort);

  // Read data from a request into a string.
  // Returns false if the request was aborted, true otherwise.
  bool ReadBodyIntoString(std::string* destination_string);

  // Mark the transfer completed.
  void MarkCompleted() override { completed_ = true; }

  // Mark the transfer failed, usually from cancellation or timeout.
  void MarkFailed() override { completed_ = false; }

  bool completed() const { return completed_; }

 protected:
  // Initialize the buffer pointer.
  void InitializeBuffer(const char* buffer, size_t buffer_size);

  // Get a pointer to the buffer at the current read offset.
  const char* GetBufferAtOffset() const {
    return read_buffer_ + read_buffer_offset_;
  }

  // Move the buffer read offset up to max_bytes_to_read returning the amount
  // the offset advanced.
  size_t AdvanceBufferOffset(const size_t max_bytes_to_read);

  // Get the bytes remaining in the buffer
  size_t GetBufferRemaining() const {
    return read_buffer_size_ - read_buffer_offset_;
  }

 protected:
  // The only thing that matters to a HTTP transport.
  RequestOptions options_;

 private:
  const char* read_buffer_;
  size_t read_buffer_size_;
  size_t read_buffer_offset_;
  bool completed_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_H_
