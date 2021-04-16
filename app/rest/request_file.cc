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

#include "app/rest/request_file.h"

#include "app/src/include/firebase/internal/platform.h"

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif  // _FILE_OFFSET_BITS
#include <stdio.h>

#include <cassert>
#include <cstddef>

// Map to POSIX compliant fseek on Windows.
#if FIREBASE_PLATFORM_WINDOWS
#define fseeko _fseeki64
#define ftello _ftelli64
#endif  // FIREBASE_PLATFORM_WINDOWS

namespace firebase {
namespace rest {

// Create a request that will read from the specified file.
// This file isn't opened until the first call to ReadBody().
RequestFile::RequestFile(const char* filename, size_t offset)
    : file_(fopen(filename, "rb")), file_size_(0) {
  options_.stream_post_fields = true;
  // If the file exists, seek to the end of the file and get the size.
  if (file_ && fseeko(file_, 0, SEEK_END) == 0) {
    file_size_ = static_cast<size_t>(ftello(file_));
    if (fseeko(file_, offset, SEEK_SET) != 0) CloseFile();
  }
}

void RequestFile::CloseFile() {
  if (file_) fclose(file_);
  file_ = nullptr;
  file_size_ = 0;
}

// This object will assert if post fields are set.
void RequestFile::set_post_fields(const char* /*data*/, size_t /*size*/) {
  assert(false);
}

void RequestFile::set_post_fields(const char* /*data*/) { assert(false); }

size_t RequestFile::ReadBody(char* buffer, size_t length, bool* abort) {
  size_t data_read = 0;
  *abort = false;
  if (IsFileOpen()) {
    if (feof(file_)) {
      CloseFile();
      return 0;
    }
    data_read = fread(buffer, 1, length, file_);
    *abort = ferror(file_) != 0 && feof(file_) == 0;
  }
  return data_read;
}

}  // namespace rest
}  // namespace firebase
