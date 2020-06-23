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

#include "app/rest/response_binary.h"
#include "app/rest/zlibwrapper.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {

class ResponseBinaryTest : public ::testing::Test {
 protected:
  std::string Compress(const std::string& input) {
    ZLib zlib;
    zlib.SetGzipHeaderMode();
    uLongf result_size = ZLib::MinCompressbufSize(input.length());
    std::unique_ptr<char[]> result(new char[result_size]);
    int err = zlib.Compress(
        reinterpret_cast<unsigned char*>(result.get()), &result_size,
        reinterpret_cast<const unsigned char*>(input.data()), input.length());
    EXPECT_EQ(err, Z_OK);
    return std::string(result.get(), result_size);
  }

  std::string GetBody() {
    const char* data;
    size_t size;
    response_.GetBody(&data, &size);
    return std::string(data, size);
  }

  void SetBody(const std::string& body) {
    response_.ProcessBody(body.data(), body.length());
    response_.MarkCompleted();
  }

  ResponseBinary response_;
};

TEST_F(ResponseBinaryTest, GetBodyWihoutGunzip) {
  std::string s = "hello world";
  SetBody(s);
  EXPECT_EQ(GetBody(), s);
}

TEST_F(ResponseBinaryTest, GetBinaryBodyWihoutGunzip) {
  char buffer[] = {'a', 'b', '\0', 'c', '\0', 'x', 'y', 'z'};
  std::string s(buffer, sizeof(buffer));
  SetBody(s);
  EXPECT_EQ(GetBody(), s);
}

TEST_F(ResponseBinaryTest, GetBodyWihGunzip) {
  response_.set_use_gunzip(true);

  std::string s = "hello world";
  SetBody(Compress(s));

  EXPECT_EQ(GetBody(), s);
}

TEST_F(ResponseBinaryTest, GetBinaryBodyWihGunzip) {
  response_.set_use_gunzip(true);

  char buffer[] = {'a', 'b', '\0', 'c', '\0', 'x', 'y', 'z'};
  std::string s(buffer, sizeof(buffer));
  SetBody(Compress(s));

  EXPECT_EQ(GetBody(), s);
}

TEST_F(ResponseBinaryTest, GetBodyWihGunzipHugeBuffer) {
  response_.set_use_gunzip(true);

  // 10 MB body
  std::string s;
  unsigned int seed = 0;
  srand(seed);
  size_t size = 10 * 1024 * 1024;
  for (size_t i = 0; i < size; i++) {
    s += '0' + (rand() % 10); // NOLINT (rand_r() doesn't work on windows)
  }
  SetBody(Compress(s));

  EXPECT_EQ(GetBody(), s);
}

TEST_F(ResponseBinaryTest, GetBinaryBodyWihGunzipHugeBuffer) {
  response_.set_use_gunzip(true);

  // 10 MB body
  size_t size = 10 * 1024 * 1024;
  char* buffer = new char[size];

  unsigned int seed = 0;
  srand(seed);
  for (size_t i = 0; i < size; i++) {
    // Add 0-9 numbers and '\0' to buffer.
    buffer[i] = (i % 10) ? ('0' + (rand() % 10)): '\0';  // NOLINT
                                                         // (no rand_r on msvc)
  }

  std::string s(buffer, sizeof(buffer));
  SetBody(Compress(s));
  EXPECT_EQ(GetBody(), s);

  delete[] buffer;
}

}  // namespace rest
}  // namespace firebase
