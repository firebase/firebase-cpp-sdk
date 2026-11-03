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
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

typedef GetOobConfirmationCodeRequest RequestT;
typedef GetOobConfirmationCodeResponse ResponseT;

// Test GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest
TEST(GetOobConfirmationCodeTest, SendVerifyEmailRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateSendEmailVerificationRequest(*app, "APIKEY");
  request->SetIdToken("token");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getOobConfirmationCode?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"idToken\": \"token\",\n"
      "  \"requestType\": \"VERIFY_EMAIL\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(GetOobConfirmationCodeTest, SendVerifyEmailRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  auto request = RequestT::CreateSendEmailVerificationRequest(*app, "APIKEY");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/getOobConfirmationCode?key=APIKEY",
      request->options().url);
}

TEST(GetOobConfirmationCodeTest, SendPasswordResetEmailRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateSendPasswordResetEmailRequest(*app, "APIKEY", "email");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getOobConfirmationCode?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"email\": \"email\",\n"
      "  \"requestType\": \"PASSWORD_RESET\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(GetOobConfirmationCodeTest,
     SendEmailVerificationBeforeUpdatingEmailRequestAndLanguageCode) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateSendEmailVerificationBeforeUpdatingEmailRequest(
          *app, "APIKEY", "new@email", "fr\r\nX-Injected: bad");
  request->SetIdToken("token");
  EXPECT_EQ("frX-Injected: bad",
            request->options().header[kHeaderFirebaseLocale]);
  EXPECT_EQ(
      "{\n"
      "  \"idToken\": \"token\",\n"
      "  \"requestType\": \"VERIFY_AND_CHANGE_EMAIL\",\n"
      "  \"newEmail\": \"new@email\"\n"
      "}\n",
      request->options().post_fields);
}

// Test GetOobConfirmationCodeResponse
TEST(GetOobConfirmationCodeTest, TestGetOobConfirmationCodeResponse) {
  ResponseT response;
  // An example HTTP response JSON in the exact format we get from real server
  // with token string replaced by dummy string.
  const char body[] =
      "{\n"
      " \"kind\": \"identitytoolkit#GetOobConfirmationCodeResponse\",\n"
      " \"email\": \"my@email\"\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ(kAuthErrorNone, response.error_code());
  EXPECT_EQ("my@email", response.email());

  ResponseT error_response;
  const char error_body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"EMAIL_NOT_FOUND\"\n"
      "  }\n"
      "}";
  error_response.ProcessBody(error_body, sizeof(error_body));
  error_response.MarkCompleted();
  EXPECT_FALSE(error_response.IsSuccessful());
  EXPECT_EQ(kAuthErrorUserNotFound, error_response.error_code());
}

}  // namespace auth
}  // namespace firebase
