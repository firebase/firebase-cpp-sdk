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
#include "auth/src/desktop/rpcs/reset_password_request.h"
#include "auth/src/desktop/rpcs/reset_password_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

// Test ResetPasswordRequest
TEST(ResetPasswordTest, TestResetPasswordRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  ResetPasswordRequest request(*app, "APIKEY", "oob", "password");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "resetPassword?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  \"oobCode\": \"oob\",\n"
      "  \"newPassword\": \"password\"\n"
      "}\n",
      request.options().post_fields);
}

TEST(ResetPasswordTest, TestResetPasswordRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  ResetPasswordRequest request(*app, "APIKEY", "oob", "password");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/resetPassword?key=APIKEY",
      request.options().url);
}

// Test ResetPasswordResponse
TEST(ResetPasswordTest, TestResetPasswordResponse) {
  ResetPasswordResponse response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      " \"kind\": \"identitytoolkit#ResetPasswordResponse\",\n"
      " \"email\": \"abc@email\",\n"
      " \"requestType\": \"PASSWORD_RESET\"\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ(kAuthErrorNone, response.error_code());

  ResetPasswordResponse error_response;
  const char error_body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"EXPIRED_OOB_CODE\"\n"
      "  }\n"
      "}";
  error_response.ProcessBody(error_body, sizeof(error_body));
  error_response.MarkCompleted();
  EXPECT_FALSE(error_response.IsSuccessful());
  EXPECT_EQ(kAuthErrorExpiredActionCode, error_response.error_code());
}

}  // namespace auth
}  // namespace firebase
