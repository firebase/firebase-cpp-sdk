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

#include "app/rest/request_binary_gzip.h"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include "app/rest/zlibwrapper.h"
#include "app/src/log.h"

namespace firebase {
namespace rest {

RequestBinaryGzip::RequestBinaryGzip(const char* read_buffer,
                                     size_t read_buffer_size)
    : RequestBinary(read_buffer, read_buffer_size),
      uncompressed_size_(read_buffer_size),
      reading_footer_(false) {
  zlib_.SetGzipHeaderMode();
  memset(gzip_footer_, 0, sizeof(gzip_footer_));
  assert(sizeof(gzip_footer_) >= zlib_.MinFooterSize());
}

void RequestBinaryGzip::set_post_fields(const char* data, size_t size) {
  assert(!reading_footer_);
  RequestBinary::set_post_fields(data, size);
  uncompressed_size_ = GetBufferRemaining();
}

void RequestBinaryGzip::set_post_fields(const char* data) {
  assert(!reading_footer_);
  RequestBinary::set_post_fields(data);
  uncompressed_size_ = GetBufferRemaining();
}

size_t RequestBinaryGzip::ReadAndCompress(char* buffer, size_t length,
                                          bool* abort) {
  *abort = false;
  uLong remaining = static_cast<uLong>(GetBufferRemaining());
  uLong source_length = remaining;
  uLong destination_length = static_cast<uLong>(length);
  int status = zlib_.CompressAtMost(
      reinterpret_cast<Bytef*>(buffer), &destination_length,
      reinterpret_cast<const Bytef*>(GetBufferAtOffset()), &source_length);
  AdvanceBufferOffset(
      CheckOk(status, static_cast<size_t>(remaining - source_length), abort));
  return destination_length;
}

size_t RequestBinaryGzip::ReadBody(char* buffer, size_t length, bool* abort) {
  *abort = false;
  if (!reading_footer_) {
    size_t read_size = ReadAndCompress(buffer, length, abort);
    if (*abort || read_size) return read_size;
    reading_footer_ = true;
    uLong footer_size = static_cast<uLong>(sizeof(gzip_footer_));
    if (CheckOk(zlib_.CompressChunkDone(reinterpret_cast<Bytef*>(gzip_footer_),
                                        &footer_size),
                1, abort)) {
      InitializeBuffer(gzip_footer_, static_cast<size_t>(footer_size));
    }
  }
  return RequestBinary::ReadBody(buffer, length, abort);
}

size_t RequestBinaryGzip::CheckOk(int status, size_t read_size, bool* abort) {
  // CompressAtMost() and CompressChunkDone() return Z_BUF_ERROR if the source
  // buffer wasn't entirely consumed, that's ok as we update the buffer read
  // offset so that it will be read by subsequent calls to ReadBody() or
  // ReadAndCompress().
  if (Z_OK == status || Z_BUF_ERROR == status) return read_size;
  LogError("gzip error: %d", status);
  *abort = true;
  return 0;
}

}  // namespace rest
}  // namespace firebase
