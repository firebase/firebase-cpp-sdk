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
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/rpcs/get_account_info_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

// Test GetAccountInfoRequest
TEST(GetAccountInfoTest, TestGetAccountInfoRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  GetAccountInfoRequest request(*app, "APIKEY", "token");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getAccountInfo?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  \"idToken\": \"token\"\n"
      "}\n",
      request.options().post_fields);
}

TEST(GetAccountInfoTest, TestGetAccountInfoRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  GetAccountInfoRequest request(*app, "APIKEY", "token");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/getAccountInfo?key=APIKEY",
      request.options().url);
}

// Test GetAccountInfoResponse
TEST(GetAccountInfoTest, TestGetAccountInfoResponse) {
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
      "   \"lastLoginAt\": \"1700000000123\",\n"
      "   \"createdAt\": \"1600000000456\",\n"
      "   \"providerUserInfo\": [\n"
      "    {\n"
      "     \"providerId\": \"google.com\",\n"
      "     \"federatedId\": \"fed123\"\n"
      "    }\n"
      "   ]\n"
      "  }\n"
      " ]\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ("localid123", response.local_id());
  EXPECT_EQ("dp name", response.display_name());
  EXPECT_EQ("abc@efg", response.email());
  EXPECT_EQ("www.photo", response.photo_url());
  EXPECT_FALSE(response.email_verified());
  EXPECT_EQ("abcdefg", response.password_hash());
  EXPECT_EQ("519", response.phone_number());
  EXPECT_EQ(1700000000123ULL, response.last_login_at());
  EXPECT_EQ(1600000000456ULL, response.created_at());
  EXPECT_EQ(1u, response.providerUserInfos().size());
}

TEST(GetAccountInfoTest, TestGetAccountInfoEmptyAndErrorResponses) {
  GetAccountInfoResponse empty_response;
  const char empty_body[] = "{\"users\": []}";
  empty_response.ProcessBody(empty_body, sizeof(empty_body));
  empty_response.MarkCompleted();
  EXPECT_EQ("", empty_response.local_id());
  EXPECT_EQ(0u, empty_response.last_login_at());
  EXPECT_EQ(0u, empty_response.created_at());
  EXPECT_TRUE(empty_response.providerUserInfos().empty());

  GetAccountInfoResponse error_response;
  const char error_body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"USER_NOT_FOUND\"\n"
      "  }\n"
      "}";
  error_response.ProcessBody(error_body, sizeof(error_body));
  error_response.MarkCompleted();
  EXPECT_FALSE(error_response.IsSuccessful());
  EXPECT_EQ(kAuthErrorUserNotFound, error_response.error_code());
}

}  // namespace auth
}  // namespace firebase
