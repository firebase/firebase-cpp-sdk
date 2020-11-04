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

#include <stdio.h>

#include <cstddef>
#include <string>

#include "app/rest/tests/request_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "absl/flags/flag.h"

namespace firebase {
namespace rest {
namespace test {

class RequestFileTest : public ::testing::Test {
 public:
  RequestFileTest()
      : filename_(absl::GetFlag(FLAGS_test_tmpdir) + "/a_file.txt"),
        file_(nullptr),
        file_size_(0) {}

  void SetUp() override;
  void TearDown() override;

 protected:
  std::string filename_;
  FILE* file_;
  size_t file_size_;

  static const char kFileContents[];
};

const char RequestFileTest::kFileContents[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
    "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
    "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut "
    "aliquip ex ea commodo consequat. Duis aute irure dolor in "
    "reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla "
    "pariatur. Excepteur sint occaecat cupidatat non proident, sunt in "
    "culpa qui officia deserunt mollit anim id est laborum.";

void RequestFileTest::SetUp() {
  file_size_ = sizeof(kFileContents) - 1;
  file_ = fopen(filename_.c_str(), "wb");
  CHECK(file_ != nullptr);
  CHECK_EQ(file_size_, fwrite(kFileContents, 1, file_size_, file_));
  CHECK_EQ(0, fclose(file_));
}

void RequestFileTest::TearDown() { CHECK_EQ(0, unlink(filename_.c_str())); }

TEST_F(RequestFileTest, NonExistentFile) {
  RequestFile request("a_file_that_doesnt_exist.txt", 0);
  EXPECT_FALSE(request.IsFileOpen());
}

TEST_F(RequestFileTest, OpenFile) {
  RequestFile request(filename_.c_str(), 0);
  EXPECT_TRUE(request.IsFileOpen());
}

TEST_F(RequestFileTest, GetFileSize) {
  RequestFile request(filename_.c_str(), 0);
  EXPECT_EQ(file_size_, request.file_size());
}

TEST_F(RequestFileTest, ReadFile) {
  RequestFile request(filename_.c_str(), 0);
  EXPECT_EQ(kFileContents, ReadRequestBody(&request));
}

TEST_F(RequestFileTest, ReadFileFromOffset) {
  size_t read_offset = 29;
  RequestFile request(filename_.c_str(), read_offset);
  EXPECT_EQ(&kFileContents[read_offset], ReadRequestBody(&request));
}

}  // namespace test
}  // namespace rest
}  // namespace firebase
