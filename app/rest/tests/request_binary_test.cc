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

#include <memory>
#include <string>
#include <vector>

#include "app/rest/request_binary.h"
#include "app/rest/request_binary_gzip.h"
#include "app/rest/request_options.h"
#include "app/rest/zlibwrapper.h"
#include "app/rest/tests/request_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {
namespace test {

class RequestBinaryTest : public ::testing::Test {
 protected:
  // Codec that decompresses a gzip encoded string.
  static std::string Decompress(const std::string& input) {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    uLongf result_length = zlib.GzipUncompressedLength(
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    std::unique_ptr<char[]> result(new char[result_length]);
    int err = zlib.Uncompress(
        reinterpret_cast<unsigned char*>(result.get()), &result_length,
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    EXPECT_EQ(err, Z_OK);
    return std::string(result.get(), result_length);
  }
};

TEST_F(RequestBinaryTest, GetSmallPostFields) {
  TestCreateAndReadRequestBody<RequestBinary>(kSmallString,
                                              sizeof(kSmallString));
}

TEST_F(RequestBinaryTest, GetLargePostFields) {
  std::string large_buffer = CreateLargeTextData();
  TestCreateAndReadRequestBody<RequestBinary>(large_buffer.c_str(),
                                              large_buffer.size());
}

TEST_F(RequestBinaryTest, GetSmallBinaryPostFields) {
  TestCreateAndReadRequestBody<RequestBinary>(kSmallBinary,
                                              sizeof(kSmallBinary));
}

TEST_F(RequestBinaryTest, GetLargeBinaryPostFields) {
  std::string large_buffer = CreateLargeBinaryData();
  TestCreateAndReadRequestBody<RequestBinary>(large_buffer.c_str(),
                                              large_buffer.size());
}

TEST_F(RequestBinaryTest, GetSmallPostFieldsWithGzip) {
  TestCreateAndReadRequestBody<RequestBinaryGzip>(
      kSmallString, sizeof(kSmallString), Decompress);
}

TEST_F(RequestBinaryTest, GetLargePostFieldsWithGzip) {
  std::string large_buffer = CreateLargeTextData();
  TestCreateAndReadRequestBody<RequestBinaryGzip>(
      large_buffer.c_str(), large_buffer.size(), Decompress);
}

TEST_F(RequestBinaryTest, GetSmallBinaryPostFieldsWithGzip) {
  TestCreateAndReadRequestBody<RequestBinaryGzip>(
      kSmallBinary, sizeof(kSmallBinary), Decompress);
}

TEST_F(RequestBinaryTest, GetLargeBinaryPostFieldsWithGzip) {
  std::string large_buffer = CreateLargeBinaryData();
  TestCreateAndReadRequestBody<RequestBinaryGzip>(
      large_buffer.c_str(), large_buffer.size(), Decompress);
}

}  // namespace test
}  // namespace rest
}  // namespace firebase
