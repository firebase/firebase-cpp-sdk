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

#include "app/rest/response.h"
#include <string>
#include "app/rest/util.h"
#include "curl/curl.h"

namespace firebase {
namespace rest {

Response::Response()
    : status_(0), header_completed_(false), body_completed_(false),
      sdk_error_code_(0), fetch_time_(0) {}

bool Response::ProcessHeader(const char* buffer, size_t length) {
  // Since buffer may NOT neccessarily end with \0, pass in length in the init.
  std::string header(buffer, length);
  size_t colon_index = header.find(util::kHttpHeaderSeparator);
  if (colon_index == std::string::npos) {
    // Must be either the status line or CRLF line.
    if (header.compare(util::kCrLf) == 0) {
      // A single CRLF line marks the end of header.
      header_completed_ = true;
    } else {
      // Scan status code and ignore version as well as reason-phrase strings.
      sscanf(header.c_str(), "HTTP/%*d.%*d %d %*s\r\n", &status_);
    }
  } else {
    // A header line with key and value, separated by colon.
    const std::string& key =
        util::TrimWhitespace(header.substr(0, colon_index));
    const std::string& value =
        util::TrimWhitespace(header.substr(colon_index + 1));
    header_.emplace(key, value);

    // Below we update this response object by each header.
    // Update fetch_time_ from Date.
    if (key == util::kDate) {
      fetch_time_ = curl_getdate(value.c_str(), nullptr  /* unused */);
    }
  }
  return true;
}

bool Response::ProcessBody(const char* buffer, size_t length) {
  // Since buffer may NOT neccessarily end with \0, pass in length in the init.
  std::string body(buffer, length);
  body_.push_back(body);
  return true;
}

const char* Response::GetHeader(const char* name) {
  auto iter = header_.find(name);
  if (iter == header_.end()) {
    return nullptr;
  } else {
    return iter->second.c_str();
  }
}

const char* Response::GetBody() const {
  // If already concatenated before, return it.
  if (!body_cache_.empty()) {
    return body_cache_.c_str();
  }
  // Compute the length of message body.
  size_t length = 0;
  for (const std::string& body : body_) {
    length += body.length();
  }
  // Concatenate the message body.
  body_cache_.reserve(length);
  for (const std::string& body : body_) {
    body_cache_ += body;
  }
  return body_cache_.c_str();
}

void Response::GetBody(const char** data, size_t* size) const {
  GetBody();
  *data = body_cache_.data();
  *size = body_cache_.length();
}

}  // namespace rest
}  // namespace firebase
