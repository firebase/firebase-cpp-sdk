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
#include "auth/src/desktop/rpcs/sign_up_new_user_request.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_response.h"

namespace firebase {
namespace auth {

// Test SignUpNewUserRequest for making anonymous signin
TEST(SignUpNewUserTest, TestAnonymousSignInRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  SignUpNewUserRequest request("APIKEY");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true\n"
      "}\n",
      request.options().post_fields);
}

// Test SignUpNewUserRequest for using password signin
TEST(SignUpNewUserTest, TestEmailPasswordSignInRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  SignUpNewUserRequest request("APIKEY", "e@mail", "pwd", "rabbit");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "signupNewUser?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  email: \"e@mail\",\n"
      "  password: \"pwd\",\n"
      "  displayName: \"rabbit\",\n"
      "  returnSecureToken: true\n"
      "}\n",
      request.options().post_fields);
}

// Test SignUpNewUserResponse
TEST(SignUpNewUserTest, TestSignUpNewUserResponse) {
  std::unique_ptr<App> app(testing::CreateApp());
  SignUpNewUserResponse response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      " \"kind\": \"identitytoolkit#SignupNewUserResponse\",\n"
      " \"idToken\": \"idtoken123\",\n"
      " \"refreshToken\": \"refreshtoken123\",\n"
      " \"expiresIn\": \"3600\",\n"
      " \"localId\": \"localid123\"\n"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_EQ("idtoken123", response.id_token());
  EXPECT_EQ("refreshtoken123", response.refresh_token());
  EXPECT_EQ(3600, response.expires_in());
}

TEST(SignUpNewUserTest, TestErrorResponse) {
  std::unique_ptr<App> app(testing::CreateApp());
  SignUpNewUserResponse response;
  const char body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"OPERATION_NOT_ALLOWED\",\n"
      "    \"errors\": [\n"
      "      {\n"
      "        \"reason\": \"some reason\"\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();

  EXPECT_EQ(kAuthErrorOperationNotAllowed, response.error_code());

  // Make sure response doesn't crash on access.
  EXPECT_EQ("", response.local_id());
  EXPECT_EQ("", response.id_token());
  EXPECT_EQ("", response.refresh_token());
  EXPECT_EQ(0, response.expires_in());
}

}  // namespace auth
}  // namespace firebase
