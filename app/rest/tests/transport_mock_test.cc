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

#include "app/rest/transport_mock.h"
#include "app/rest/request.h"
#include "app/rest/response.h"
#include "testing/config.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace rest {
TEST(TransportMockTest, TestCreation) { TransportMock mock; }

TEST(TransportMockTest, TestHttpGet200) {
  Request request;
  Response response;
  EXPECT_EQ(0, response.status());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());
  EXPECT_EQ(nullptr, response.GetHeader("Server"));
  EXPECT_STREQ("", response.GetBody());

  request.set_url("http://my.fake.site");
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'http://my.fake.site',"
      "     httpresponse: {"
      "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "       body: ['this is a mock',]"
      "     }"
      "    }"
      "  ]"
      "}");
  TransportMock transport;
  transport.Perform(request, &response);
  EXPECT_EQ(200, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
  EXPECT_STREQ("mock server 101", response.GetHeader("Server"));
  EXPECT_STREQ("this is a mock", response.GetBody());
}

TEST(TransportMockTest, TestHttpGet404) {
  Request request;
  Response response;
  EXPECT_EQ(0, response.status());
  EXPECT_FALSE(response.header_completed());
  EXPECT_FALSE(response.body_completed());

  request.set_url("http://my.fake.site");
  firebase::testing::cppsdk::ConfigSet("{config:[]}");
  TransportMock transport;
  transport.Perform(request, &response);
  EXPECT_EQ(404, response.status());
  EXPECT_TRUE(response.header_completed());
  EXPECT_TRUE(response.body_completed());
}

}  // namespace rest
}  // namespace firebase
