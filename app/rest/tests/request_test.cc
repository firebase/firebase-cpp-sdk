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

#include "app/rest/request.h"

#include "app/rest/tests/request_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {
namespace test {

TEST(RequestTest, SetUrl) {
  Request request;
  EXPECT_EQ("", request.options().url);

  request.set_url("some.url");
  EXPECT_EQ("some.url", request.options().url);
}

TEST(RequestTest, GetSmallPostFields) {
  TestCreateAndReadRequestBody<Request>(kSmallString, sizeof(kSmallString));
}

TEST(RequestTest, GetLargePostFields) {
  std::string large_buffer = CreateLargeTextData();
  TestCreateAndReadRequestBody<Request>(large_buffer.c_str(),
                                        large_buffer.size());
}

TEST(RequestTest, GetSmallBinaryPostFields) {
  TestCreateAndReadRequestBody<Request>(kSmallBinary, sizeof(kSmallBinary));
}

TEST(RequestTest, GetLargeBinaryPostFields) {
  std::string large_buffer = CreateLargeBinaryData();
  TestCreateAndReadRequestBody<Request>(large_buffer.c_str(),
                                        large_buffer.size());
}

}  // namespace test
}  // namespace rest
}  // namespace firebase
