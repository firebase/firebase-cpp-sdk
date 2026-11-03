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

#include <cctype>
#include <string>

#include "app/rest/util.h"
#include "curl/curl.h"

namespace firebase {
namespace rest {

namespace {

// Parses an HTTP/1.x, HTTP/2, or HTTP/3 status line such as:
//   "HTTP/1.1 200 OK\r\n"
//   "HTTP/1.1 204\r\n"
//   "HTTP/2 200 \r\n"
//   "HTTP/2 200\r\n"
// Returns true and writes *status_out when valid; leaves *status_out unchanged
// otherwise.
bool ParseHttpStatusLine(const std::string& line, int* status_out) {
  if (line.size() < 9 || util::ToUpper(line.substr(0, 5)) != "HTTP/") {
    return false;
  }
  size_t pos = 5;
  // Major version: one or more digits.
  size_t major_start = pos;
  while (pos < line.size() &&
         std::isdigit(static_cast<unsigned char>(line[pos]))) {
    ++pos;
  }
  if (pos == major_start) return false;

  // Optional ".minor" version (present in HTTP/1.0, HTTP/1.1; omitted in
  // HTTP/2 and HTTP/3 status lines synthesized by libcurl).
  if (pos < line.size() && line[pos] == '.') {
    ++pos;
    size_t minor_start = pos;
    while (pos < line.size() &&
           std::isdigit(static_cast<unsigned char>(line[pos]))) {
      ++pos;
    }
    if (pos == minor_start) return false;
  }

  // Must be followed by whitespace before the 3-digit status code.
  if (pos >= line.size() || (line[pos] != ' ' && line[pos] != '\t')) {
    return false;
  }
  while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
    ++pos;
  }

  // Status code: 3 digits in [100, 599], followed by whitespace, CR/LF, or EOS.
  if (pos + 3 > line.size()) return false;
  int code = 0;
  for (int i = 0; i < 3; ++i) {
    unsigned char ch = static_cast<unsigned char>(line[pos + i]);
    if (!std::isdigit(ch)) return false;
    code = code * 10 + (ch - '0');
  }
  pos += 3;
  if (pos < line.size()) {
    char next = line[pos];
    if (next != ' ' && next != '\t' && next != '\r' && next != '\n') {
      return false;
    }
  }
  if (code < 100 || code > 599) return false;
  *status_out = code;
  return true;
}

}  // namespace

Response::Response()
    : status_(0),
      header_completed_(false),
      body_completed_(false),
      sdk_error_code_(0),
      fetch_time_(0) {}

bool Response::ProcessHeader(const char* buffer, size_t length) {
  if (buffer == nullptr) {
    return length == 0;
  }
  // Since buffer may NOT necessarily end with \0, pass in length in the init.
  std::string header(buffer, length);
  if (header == util::kCrLf) {
    // A single CRLF line marks the end of header.
    header_completed_ = true;
    return true;
  }
  if (ParseHttpStatusLine(header, &status_)) {
    // A new status line (e.g. final 200 after 100 Continue or 302 redirect)
    // starts a fresh header block.
    header_completed_ = false;
    header_.clear();
    return true;
  }
  size_t colon_index = header.find(util::kHttpHeaderSeparator);
  if (colon_index != std::string::npos) {
    // HTTP header field names are case-insensitive (RFC 9110 section 5.1) and
    // always lowercase in HTTP/2 (RFC 9113 section 8.2). Normalize keys to
    // lowercase and let later headers overwrite interim/redirect ones.
    const std::string key =
        util::ToLower(util::TrimWhitespace(header.substr(0, colon_index)));
    const std::string value =
        util::TrimWhitespace(header.substr(colon_index + 1));
    header_[key] = value;

    // Below we update this response object by each header.
    // Update fetch_time_ from Date.
    if (key == "date") {
      fetch_time_ = curl_getdate(value.c_str(), nullptr /* unused */);
    }
  }
  return true;
}

bool Response::ProcessBody(const char* buffer, size_t length) {
  if (buffer == nullptr) {
    return length == 0;
  }
  if (length > 0) {
    // Since buffer may NOT necessarily end with \0, pass in length in the init.
    body_.emplace_back(buffer, length);
    body_cache_.clear();
  }
  return true;
}

const char* Response::GetHeader(const char* name) const {
  if (name == nullptr) {
    return nullptr;
  }
  auto iter = header_.find(util::ToLower(name));
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
  if (data) *data = body_cache_.data();
  if (size) *size = body_cache_.length();
}

}  // namespace rest
}  // namespace firebase
