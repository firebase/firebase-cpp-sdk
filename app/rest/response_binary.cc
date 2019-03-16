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

#include <memory>
#include <string>

#include "app/rest/zlibwrapper.h"
#include "app/src/log.h"

namespace firebase {
namespace rest {

static size_t kMaxGzipLengthAccepted = 100 * 1024 * 1024;  // 100 MB

ResponseBinary::ResponseBinary() : use_gunzip_(false) {
  zlib_.SetGzipHeaderMode();
}

void ResponseBinary::GetBody(const char** data, size_t* size) const {
  if (!use_gunzip_) {
    Response::GetBody(data, size);
    return;
  }
  if (body_gunzip_cache_.empty()) {
    const char* body_data;
    size_t body_size;
    Response::GetBody(&body_data, &body_size);
    body_gunzip_cache_ = Gunzip(body_data, body_size);
  }
  *data = body_gunzip_cache_.data();
  *size = body_gunzip_cache_.length();
}

std::string ResponseBinary::Gunzip(const char* input_data,
                                   size_t input_size) const {
  uLongf result_length = zlib_.GzipUncompressedLength(
      reinterpret_cast<const unsigned char*>(input_data), input_size);
  if (result_length > kMaxGzipLengthAccepted) {
    LogError("gunzip error, %lu bytes is greater than the maximum allowed",
             result_length);
    return std::string();
  }
  std::unique_ptr<char[]> result(new char[result_length]);
  int err = zlib_.Uncompress(
      reinterpret_cast<unsigned char*>(result.get()), &result_length,
      reinterpret_cast<const unsigned char*>(input_data), input_size);
  if (err != Z_OK) {
    LogError("gunzip error: %d", err);
    return std::string();
  }
  return std::string(result.get(), result_length);
}

}  // namespace rest
}  // namespace firebase
