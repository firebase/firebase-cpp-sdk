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

#include "app/rest/response.h"

#include <cstring>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

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

TEST(ResponseTest, ProcessHttp2AndReasonlessStatusLines) {
  Response response;
  EXPECT_EQ(0, response.status());

  // HTTP/2 status lines synthesized by libcurl have no minor version and often
  // omit the reason-phrase.
  ProcessHeader("HTTP/2 200 \r\n", &response);
  EXPECT_EQ(200, response.status());

  ProcessHeader("HTTP/2 204\r\n", &response);
  EXPECT_EQ(204, response.status());

  ProcessHeader("HTTP/2.0 404\r\n", &response);
  EXPECT_EQ(404, response.status());

  ProcessHeader("HTTP/3 201\r\n", &response);
  EXPECT_EQ(201, response.status());

  // HTTP/1.1 with no reason-phrase (valid per RFC 9112 section 4).
  ProcessHeader("HTTP/1.1 503\r\n", &response);
  EXPECT_EQ(503, response.status());

  // Malformed status lines must not overwrite the previously parsed status.
  ProcessHeader("HTTP/ 200 OK\r\n", &response);
  EXPECT_EQ(503, response.status());
  ProcessHeader("HTTP/1. 200 OK\r\n", &response);
  EXPECT_EQ(503, response.status());
  ProcessHeader("HTTP/2 99\r\n", &response);
  EXPECT_EQ(503, response.status());
  ProcessHeader("NOTHTTP/2 200\r\n", &response);
  EXPECT_EQ(503, response.status());
}

TEST(ResponseTest, InterimContinueFollowedByFinalHttp2Response) {
  Response response;
  ProcessHeader("HTTP/1.1 100 Continue\r\n", &response);
  EXPECT_EQ(100, response.status());
  ProcessHeader("\r\n", &response);
  EXPECT_TRUE(response.header_completed());

  // Final status line resets header_completed until the final CRLF arrives.
  ProcessHeader("HTTP/2 200\r\n", &response);
  EXPECT_EQ(200, response.status());
  EXPECT_FALSE(response.header_completed());
  ProcessHeader("content-type: application/json\r\n", &response);
  ProcessHeader("\r\n", &response);
  EXPECT_TRUE(response.header_completed());
}

TEST(ResponseTest, CaseInsensitiveHeadersAndHttp2LowercaseDate) {
  Response response;
  EXPECT_EQ(nullptr, response.GetHeader(nullptr));

  // HTTP/2 headers are always lowercase (RFC 9113 section 8.2).
  ProcessHeader("content-type: application/json\r\n", &response);
  ProcessHeader("etag: \"v1\"\r\n", &response);
  ProcessHeader("date: Wed, 05 Jul 2017 15:55:19 GMT\r\n", &response);

  EXPECT_STREQ("application/json", response.GetHeader("Content-Type"));
  EXPECT_STREQ("application/json", response.GetHeader("content-type"));
  EXPECT_STREQ("application/json", response.GetHeader("CONTENT-TYPE"));
  EXPECT_STREQ("\"v1\"", response.GetHeader("ETag"));
  EXPECT_STREQ("\"v1\"", response.GetHeader("etag"));

  // Redirect or final response overwriting an earlier header of different case.
  ProcessHeader("ETag: \"v2\"\r\n", &response);
  EXPECT_STREQ("\"v2\"", response.GetHeader("etag"));
  EXPECT_STREQ("\"v2\"", response.GetHeader("ETag"));

  response.MarkCompleted();
  EXPECT_EQ(1499270119, response.fetch_time());
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
