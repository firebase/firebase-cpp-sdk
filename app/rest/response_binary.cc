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

#include "app/rest/response_binary.h"

#include <climits>
#include <memory>
#include <string>

#include "app/rest/zlibwrapper.h"
#include "app/src/log.h"

namespace firebase {
namespace rest {

static size_t kMaxGzipLengthAccepted = 100 * 1024 * 1024;  // 100 MB

ResponseBinary::ResponseBinary() : use_gunzip_(false), gunzip_cached_(false) {}

bool ResponseBinary::ProcessBody(const char* buffer, size_t length) {
  body_gunzip_cache_.clear();
  gunzip_cached_ = false;
  return Response::ProcessBody(buffer, length);
}

void ResponseBinary::GetBody(const char** data, size_t* size) const {
  if (!use_gunzip_) {
    Response::GetBody(data, size);
    return;
  }
  if (!gunzip_cached_) {
    const char* body_data = nullptr;
    size_t body_size = 0;
    Response::GetBody(&body_data, &body_size);
    body_gunzip_cache_ = Gunzip(body_data, body_size);
    gunzip_cached_ = true;
  }
  if (data) *data = body_gunzip_cache_.data();
  if (size) *size = body_gunzip_cache_.length();
}

std::string ResponseBinary::Gunzip(const char* input_data,
                                   size_t input_size) const {
  if (!input_data || input_size == 0 ||
      input_size > static_cast<size_t>(INT_MAX)) {
    return std::string();
  }
  if (!ZLib::HasGzipHeader(input_data, static_cast<int>(input_size))) {
    LogError("gunzip error: invalid gzip header");
    return std::string();
  }
  ZLib zlib;
  zlib.SetGzipHeaderMode();
  uLongf result_length = zlib.GzipUncompressedLength(
      reinterpret_cast<const unsigned char*>(input_data), input_size);
  if (result_length > kMaxGzipLengthAccepted) {
    LogError("gunzip error, %lu bytes is greater than the maximum allowed",
             static_cast<unsigned long>(result_length));
    return std::string();
  }
  std::string result(result_length, '\0');
  Bytef dummy = 0;
  Bytef* dest_ptr =
      result.empty() ? &dummy : reinterpret_cast<Bytef*>(&result[0]);
  int err = zlib.Uncompress(
      dest_ptr, &result_length,
      reinterpret_cast<const unsigned char*>(input_data), input_size);
  if (err != Z_OK) {
    LogError("gunzip error: %d", err);
    return std::string();
  }
  result.resize(result_length);
  return result;
}

}  // namespace rest
}  // namespace firebase
