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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_H_

#include <cstddef>

#include "app/rest/request.h"

namespace firebase {
namespace rest {

class RequestBinary : public Request {
 public:
  RequestBinary() : RequestBinary(nullptr) {}

  // Create a binary request that will read from the specified buffer.
  // NOTE: This will *not copy* data into the request.  The buffer must not be
  // deallocate while a transfer of this request is in progress.
  explicit RequestBinary(const char* read_buffer, size_t read_buffer_size = 0)
      : Request(read_buffer, read_buffer_size) {
    // Only pick the streaming option if we constructed with a buffer.
    // The default constructor for RequestBinaryGzip calls this will a nullptr,
    // but in that case we don't actually want to turn it on by default.
    if (read_buffer) {
      options_.stream_post_fields = true;
    }
  }
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_BINARY_H_
