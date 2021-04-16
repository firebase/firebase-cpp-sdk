/*
 * Copyright 2018 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_GZIP_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_GZIP_H_

#include <cstddef>
#include <string>

#include "app/rest/request_binary.h"
#include "app/rest/zlibwrapper.h"

namespace firebase {
namespace rest {

class RequestBinaryGzip : public RequestBinary {
 public:
  RequestBinaryGzip() : RequestBinaryGzip(nullptr) {}

  // Create a binary request that will read from the specified buffer.
  // NOTE: This will *not copy* data into the request.  The buffer must not be
  // deallocate while a transfer of this request is in progress.
  explicit RequestBinaryGzip(const char* read_buffer,
                             size_t read_buffer_size = 0);

  // If a read buffer wasn't specified on construction, it's possible to copy
  // data into this class to read here.
  void set_post_fields(const char* data, size_t size) override;
  void set_post_fields(const char* data) override;

  // Get the size of the POST fields.
  size_t GetPostFieldsSize() const override { return uncompressed_size_; }

  // Called to read the body of the request to send to the server.
  // Returns the number of bytes written into the buffer, or 0 if the no more
  // data is available to send. To stop the transfer set abort to true.
  size_t ReadBody(char* buffer, size_t length, bool* abort) override;

 private:
  // Read from the read_buffer and compress into the specified buffer.
  size_t ReadAndCompress(char* buffer, size_t length, bool* abort);

  // Check for a zlib error, returning read_size if no error occurred.
  // If an error occurred, abort is set to true and 0 is returned.
  static size_t CheckOk(int status, size_t read_size, bool* abort);

 private:
  ZLib zlib_;
  size_t uncompressed_size_;
  // Footer of ZLib::MinFooterSize() bytes to write when we've finished reading
  // the buffer.  The footer is up to 10 bytes in size with a header, see
  // Zlib::MinFooterSize().
  char gzip_footer_[10];
  bool reading_footer_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_GZIP_H_
