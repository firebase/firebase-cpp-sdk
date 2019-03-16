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

#include "app/rest/request.h"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

namespace firebase {
namespace rest {

Request::Request(const char* post_fields_buffer, size_t post_fields_buffer_size)
    : completed_(false) {
  InitializeBuffer(post_fields_buffer, post_fields_buffer_size);
}

Request::~Request() {}

void Request::set_post_fields(const char* data, size_t size) {
  std::string* post_fields = &options_.post_fields;
  post_fields->assign(data, size);
  InitializeBuffer(post_fields->c_str(), post_fields->size());
}

void Request::set_post_fields(const char* data) {
  set_post_fields(data, strlen(data));
}

std::string Request::ToString() {
  std::string output = options_.url + '\n';
  if (!ReadBodyIntoString(&output)) {
    output += "*** aborted ***\n";
  }
  output += "\n";
  return output;
}

bool Request::ReadBodyIntoString(std::string* destination_string) {
  // If no post fields are set, there is nothing to do.
  if (GetPostFieldsSize() == 0) return false;

  // If we're reading into the post_fields member of request options but it
  // already contains data it was generated using set_post_fields() or
  // generated this method already.
  if (!options_.post_fields.empty() &&
      destination_string == &options_.post_fields) {
    return true;
  }

  // Read into the string.
  // This intentionally doesn't clear the string so that a caller could
  // aggregate data into a single object.
  size_t read_size;
  bool aborted = false;
  do {
    char read_buffer[64];
    read_size = ReadBody(read_buffer, sizeof(read_buffer), &aborted);
    if (read_size) destination_string->append(read_buffer, read_size);
  } while (read_size && !aborted);
  return !aborted;
}

size_t Request::ReadBody(char* buffer, size_t length, bool* abort) {
  *abort = false;
  const char* read_buffer = GetBufferAtOffset();
  assert(read_buffer);
  size_t read_size = AdvanceBufferOffset(length);
  if (read_size) memcpy(buffer, read_buffer, read_size);
  return read_size;
}

void Request::InitializeBuffer(const char* buffer, size_t buffer_size) {
  read_buffer_ = buffer;
  read_buffer_size_ = buffer_size;
  read_buffer_offset_ = 0;
}

size_t Request::AdvanceBufferOffset(const size_t max_bytes_to_read) {
  size_t remaining = GetBufferRemaining();
  size_t read_size =
      max_bytes_to_read > remaining ? remaining : max_bytes_to_read;
  read_buffer_offset_ += read_size;
  return read_size;
}

}  // namespace rest
}  // namespace firebase
