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

#ifndef FIREBASE_APP_CLIENT_CPP_REST_TESTS_REQUEST_TEST_H_
#define FIREBASE_APP_CLIENT_CPP_REST_TESTS_REQUEST_TEST_H_

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "app/rest/request.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {
namespace test {

const char kSmallString[] = "hello world";
const char kSmallBinary[] = {'a', 'b', '\0', 'c', '\0', 'x', 'y', 'z'};
const size_t kLargeDataSize = 10 * 1024 * 1024;

// Read data from a request into a string.
static std::string ReadRequestBody(Request* request) {
  std::string output;
  EXPECT_TRUE(request->ReadBodyIntoString(&output));
  return output;
}

// No-op codec method that returns the specified string.
static std::string NoCodec(const std::string& string_to_decode) {
  return string_to_decode;
}

// Test creating and reading from a request.
template <typename T>
void TestCreateAndReadRequestBody(
    const char* buffer, size_t size,
    std::function<std::string(const std::string&)> codec = NoCodec) {
  {
    // Test read without copying into the request.
    std::vector<char> modified_expected(buffer, buffer + size);
    std::vector<char> copy(modified_expected);
    T request(&copy[0], copy.size());
    // Modify the buffer to validate it wasn't copied by the request.
    for (size_t i = 0; i < size; ++i) {
      copy[i]++;
      modified_expected[i]++;
    }
    EXPECT_EQ(std::string(&modified_expected[0], size),
              codec(ReadRequestBody(&request)));
  }
  {
    const std::string expected(buffer, size);
    T request;
    {
      // This allocates the string on the heap to ensure the memory is stomped
      // with a pattern when deallocated in debug mode.
      // Same below.
      std::unique_ptr<std::string> copy(new std::string(expected));
      request.set_post_fields(copy->c_str(), copy->length());
    }
    EXPECT_EQ(expected, codec(ReadRequestBody(&request)));
  }
  {
    const std::string expected(buffer);
    T request;
    {
      std::unique_ptr<std::string> copy(new std::string(expected));
      request.set_post_fields(copy->c_str());
    }
    EXPECT_EQ(expected, codec(ReadRequestBody(&request)));
  }
}

// Create a random data stream of characters 0-9.
static const std::string CreateLargeTextData() {
  std::string s;
  unsigned int seed = 0;
  srand(seed);
  for (size_t i = 0; i < kLargeDataSize; i++) {
    s += '0' + (rand() % 10); // NOLINT (rand_r() doesn't work on MSVC)
  }
  return s;
}

// Create a random stream of binary data.
static const std::string CreateLargeBinaryData() {
  std::string s;
  unsigned int seed = 0;
  srand(seed);
  for (size_t i = 0; i < kLargeDataSize; i++) {
    s += static_cast<char>(rand()); // NOLINT (rand_r() doesn't work on MSVC)
  }
  return s;
}

}  // namespace test
}  // namespace rest
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_REST_TESTS_REQUEST_TEST_H_
