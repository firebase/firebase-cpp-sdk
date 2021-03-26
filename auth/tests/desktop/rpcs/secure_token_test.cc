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
#include "auth/src/desktop/rpcs/secure_token_request.h"
#include "auth/src/desktop/rpcs/secure_token_response.h"
#include "auth/tests/desktop/rpcs/test_util.h"

namespace firebase {
namespace auth {

// Test SignUpNewUserRequest using refresh token
TEST(SecureTokenTest, TestSetRefreshRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  SecureTokenRequest request("APIKEY", "token123");
  EXPECT_EQ("https://securetoken.googleapis.com/v1/token?key=APIKEY",
            request.options().url);
  EXPECT_EQ(
      "{\n"
      "  grantType: \"refresh_token\",\n"
      "  refreshToken: \"token123\"\n"
      "}\n",
      request.options().post_fields);
}

// Test SecureTokenResponse
TEST(SecureTokenTest, TestSecureTokenResponse) {
  std::unique_ptr<App> app(testing::CreateApp());
  SecureTokenResponse response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      "  \"access_token\": \"accesstoken123\",\n"
      "  \"expires_in\": \"3600\",\n"
      "  \"token_type\": \"Bearer\",\n"
      "  \"refresh_token\": \"refreshtoken123\",\n"
      "  \"id_token\": \"idtoken123\",\n"
      "  \"user_id\": \"localid123\",\n"
      "  \"project_id\": \"53101460582\""
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_EQ("accesstoken123", response.access_token());
  EXPECT_EQ("refreshtoken123", response.refresh_token());
  EXPECT_EQ("idtoken123", response.id_token());
  EXPECT_EQ(3600, response.expires_in());
}

}  // namespace auth
}  // namespace firebase
