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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_OPTIONS_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_OPTIONS_H_

#include <map>
#include <string>

namespace firebase {
namespace rest {

// The request options for making each HTTP/REST request. See the usage in
// transport_interface.h. The actual HTTP transporter could be either library
// Curl or a test mock.
struct RequestOptions {
  RequestOptions()
      : method("GET"),
        stream_post_fields(false),
        timeout_ms(300000),  // Same timeout used by Chromium.
        verbose(false) {}

  // The URL to use in the request.
  std::string url;
  // The method to be performed in the request.
  std::string method;
  // The post fields for POST method.
  // Accessing this directly is deprecated, use Request::ReadBody() or
  // Request::ReadBodyIntoString() instead.
  std::string post_fields;
  // Whether to stream the post fields.
  bool stream_post_fields;
  // Stores key-value pairs in header.
  std::map<std::string, std::string> header;
  // The maximum time in milliseconds to allow the request and response.
  int64_t timeout_ms;

  // Set true to make the library display more verbose info to help debug. Does
  // not really affect the connection.
  bool verbose;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_OPTIONS_H_
