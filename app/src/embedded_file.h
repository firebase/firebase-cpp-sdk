/*
 * Copyright 2019 Google Inc. All rights reserved.
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

#ifndef FIREBASE_APP_CLIENT_CPP_SRC_EMBEDDED_FILE_H_
#define FIREBASE_APP_CLIENT_CPP_SRC_EMBEDDED_FILE_H_

#include <stddef.h>

#include <vector>

#if !defined(FIREBASE_NAMESPACE)
#define FIREBASE_NAMESPACE firebase
#endif  // !defined(FIREBASE_NAMESPACE)

namespace FIREBASE_NAMESPACE {
namespace internal {

// File embedded in the binary.
struct EmbeddedFile {
  EmbeddedFile() : name(nullptr), data(nullptr), size(0) {}
  EmbeddedFile(const char* _name, const unsigned char* _data, size_t _size)
      : name(_name), data(_data), size(_size) {}

  const char* name;
  const unsigned char* data;
  size_t size;

  // Create a vector with a single EmbeddedFile structure.
  static std::vector<EmbeddedFile> ToVector(const char* name,
                                            const unsigned char* data,
                                            size_t size) {
    std::vector<EmbeddedFile> vector;
    vector.push_back(EmbeddedFile(name, data, size));
    return vector;
  }
};

}  // namespace internal
}  // namespace FIREBASE_NAMESPACE

#endif  // FIREBASE_APP_CLIENT_CPP_SRC_EMBEDDED_FILE_H_
