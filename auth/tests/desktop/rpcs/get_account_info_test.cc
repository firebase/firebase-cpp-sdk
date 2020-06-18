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
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"
#include "auth/tests/desktop/rpcs/test_util.h"

namespace firebase {
namespace auth {

// Test GetAccountInfoRequest
TEST(GetAccountInfoTest, TestGetAccountInfoRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  GetAccountInfoRequest request("APIKEY", "token");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  idToken: \"token\"\n"
      "}\n",
      request.options().post_fields);
}

// Test GetAccountInfoResponse
TEST(GetAccountInfoTest, TestGetAccountInfoResponse) {
  std::unique_ptr<App> app(App::Create(AppOptions()));
  GetAccountInfoResponse response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      " \"kind\": \"identitytoolkit#GetAccountInfoResponse\",\n"
      " \"users\": [\n"
      "  {\n"
      "   \"localId\": \"localid123\",\n"
      "   \"displayName\": \"dp name\",\n"
      "   \"email\": \"abc@efg\",\n"
      "   \"photoUrl\": \"www.photo\",\n"
      "   \"emailVerified\": false,\n"
      "   \"passwordHash\": \"abcdefg\",\n"
      "   \"phoneNumber\": \"519\",\n"
      "   \"passwordUpdatedAt\": 31415926,\n"
      "   \"validSince\": \"123\",\n"
      "   \"lastLoginAt\": \"123\",\n"
      "   \"createdAt\": \"123\"\n"
      "  }\n"
      " ]\n"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_EQ("localid123", response.local_id());
  EXPECT_EQ("dp name", response.display_name());
  EXPECT_EQ("abc@efg", response.email());
  EXPECT_EQ("www.photo", response.photo_url());
  EXPECT_FALSE(response.email_verified());
  EXPECT_EQ("abcdefg", response.password_hash());
  EXPECT_EQ("519", response.phone_number());
}

}  // namespace auth
}  // namespace firebase
