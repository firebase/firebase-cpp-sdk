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

#include <cstring>

#include "app/rest/response.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {
// A helper function that prepares char buffer and calls the response's
// ProcessHeader. Param str must be a C string.
void ProcessHeader(const char* str, Response* response) {
  // Prepare the char buffer to call ProcessHeader and make sure the
  // implementation does not rely on that buffer is \0 terminated.
  size_t length = strlen(str);
  char* buffer = new char[length + 20];  // We pad the buffer with a few '#'.
  memset(buffer, '#', length + 20);
  memcpy(buffer, str, length);  // Intentionally not copy the \0.

  // Now call the ProcessHeader.
  response->ProcessHeader(buffer, length);
  delete[] buffer;
}

TEST(ResponseTest, ProcessStatusLine) {
  Response response;
  EXPECT_EQ(0, response.status());

  ProcessHeader("HTTP/1.1 200 OK\r\n", &response);
  EXPECT_EQ(200, response.status());

  ProcessHeader("HTTP/1.1 302 Found\r\n", &response);
  EXPECT_EQ(302, response.status());
}

TEST(ResponseTest, ProcessHeaderEnding) {
  Response response;
  EXPECT_FALSE(response.header_completed());

  ProcessHeader("HTTP/1.1 200 OK\r\n", &response);
  EXPECT_FALSE(response.header_completed());

  ProcessHeader("\r\n", &response);
  EXPECT_TRUE(response.header_completed());
}

TEST(ResponseTest, ProcessHeaderField) {
  Response response;
  EXPECT_STREQ(nullptr, response.GetHeader("Content-Type"));
  EXPECT_STREQ(nullptr, response.GetHeader("Date"));
  EXPECT_STREQ(nullptr, response.GetHeader("key"));

  ProcessHeader("Content-Type: text/html; charset=UTF-8\r\n", &response);
  ProcessHeader("Date: Wed, 05 Jul 2017 15:55:19 GMT\r\n", &response);
  ProcessHeader("key: value\r\n", &response);
  EXPECT_STREQ("text/html; charset=UTF-8", response.GetHeader("Content-Type"));
  EXPECT_STREQ("Wed, 05 Jul 2017 15:55:19 GMT", response.GetHeader("Date"));
  EXPECT_STREQ("value", response.GetHeader("key"));
}

// Below test the fetch-time logic for various test cases.
TEST(ResponseTest, ProcessDateHeaderValidDate) {
  Response response;
  EXPECT_EQ(0, response.fetch_time());
  ProcessHeader("Date: Wed, 05 Jul 2017 15:55:19 GMT\r\n", &response);
  response.MarkCompleted();
  EXPECT_EQ(1499270119, response.fetch_time());
}

TEST(ResponseTest, ProcessDateHeaderInvalidDate) {
  Response response;
  EXPECT_EQ(0, response.fetch_time());
  ProcessHeader("Date: here is a invalid date\r\n", &response);
  response.MarkCompleted();
  EXPECT_LT(1499270119, response.fetch_time());
}

TEST(ResponseTest, ProcessDateHeaderMissing) {
  Response response;
  EXPECT_EQ(0, response.fetch_time());
  response.MarkCompleted();
  EXPECT_LT(1499270119, response.fetch_time());
}

}  // namespace rest
}  // namespace firebase
