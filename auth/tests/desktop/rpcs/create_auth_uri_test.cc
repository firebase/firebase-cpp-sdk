// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "app/rest/transport_builder.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/desktop/rpcs/create_auth_uri_request.h"
#include "auth/src/desktop/rpcs/create_auth_uri_response.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

// Test CreateAuthUriRequest
TEST(CreateAuthUriTest, TestCreateAuthUriRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  CreateAuthUriRequest request(*app, "APIKEY", "email");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "createAuthUri?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  identifier: \"email\",\n"
      "  continueUri: \"http://localhost\"\n"
      "}\n",
      request.options().post_fields);
  EXPECT_EQ("ACTUAL_PAYLOAD",
            request.options().header[app_common::kApiClientHeader]);
}

TEST(CreateAuthUriTest, TestCreateAuthUriRequestWithHeartbeatPayload) {
  std::unique_ptr<App> app(testing::CreateApp());
  // Clear heartbeat storage, then call auth getter to log a heartbeat
  CreateAuthUriRequest request(*app, "APIKEY", "email");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "createAuthUri?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  identifier: \"email\",\n"
      "  continueUri: \"http://localhost\"\n"
      "}\n",
      request.options().post_fields);
  EXPECT_EQ("ACTUAL_PAYLOAD",
            request.options().header[app_common::kApiClientHeader]);
}

// Test CreateAuthUriResponse
TEST(CreateAuthUriTest, TestCreateAuthUriResponse) {
  std::unique_ptr<App> app(testing::CreateApp());
  CreateAuthUriResponse response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      " \"kind\": \"identitytoolkit#CreateAuthUriResponse\",\n"
      " \"allProviders\": [\n"
      "  \"password\"\n"
      " ],\n"
      " \"registered\": true,\n"
      " \"sessionId\": \"cdefgab\"\n"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_THAT(response.providers(), ::testing::ElementsAre("password"));
  EXPECT_TRUE(response.registered());
}

}  // namespace auth
}  // namespace firebase
