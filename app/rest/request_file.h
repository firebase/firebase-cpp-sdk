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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_REQUEST_FILE_H_
#define FIREBASE_APP_CLIENT_CPP_REST_REQUEST_FILE_H_

#include <stdio.h>
#include <cstddef>

#include "app/rest/request.h"

namespace firebase {
namespace rest {

class RequestFile : public Request {
 public:
  // Create a request that will read from the specified file.
  RequestFile(const char* filename, size_t offset);
  ~RequestFile() override { CloseFile(); }

  // This object will assert if post fields are set.
  void set_post_fields(const char* data, size_t size) override;
  void set_post_fields(const char* data) override;

  // Get the size of the POST fields.
  size_t GetPostFieldsSize() const override { return file_size(); }

  // Read from the file.
  size_t ReadBody(char* buffer, size_t length, bool* abort) override;

  // Determine whether the file is open.
  bool IsFileOpen() const { return file_ != nullptr; }

  // Size of the file in bytes, if the file references an unseekable stream this
  // will return 0.
  size_t file_size() const { return file_size_; }

 protected:
  // Close the file.
  void CloseFile();

 private:
  FILE* file_;
  size_t file_size_;
};

}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_REQUEST_FILE_H_
