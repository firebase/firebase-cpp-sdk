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
#include "auth/src/desktop/rpcs/delete_account_request.h"
#include "auth/src/desktop/rpcs/delete_account_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

// Test DeleteAccountRequest
TEST(DeleteAccountTest, TestDeleteAccountRequest) {
  std::unique_ptr<App> app(testing::CreateApp());
  DeleteAccountRequest request(*app, "APIKEY");
  request.SetIdToken("token");
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "deleteAccount?key=APIKEY",
      request.options().url);
  EXPECT_EQ(
      "{\n"
      "  \"idToken\": \"token\"\n"
      "}\n",
      request.options().post_fields);
}

TEST(DeleteAccountTest, TestDeleteAccountRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  DeleteAccountRequest request(*app, "APIKEY");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/deleteAccount?key=APIKEY",
      request.options().url);
}

// Test DeleteAccountResponse
TEST(DeleteAccountTest, TestDeleteAccountResponse) {
  DeleteAccountResponse response;
  const char body[] =
      "{\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ(kAuthErrorNone, response.error_code());
}

TEST(DeleteAccountTest, TestDeleteAccountErrorResponse) {
  DeleteAccountResponse response;
  const char body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"CREDENTIAL_TOO_OLD_LOGIN_AGAIN\"\n"
      "  }\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_FALSE(response.IsSuccessful());
  EXPECT_EQ(kAuthErrorRequiresRecentLogin, response.error_code());
}

}  // namespace auth
}  // namespace firebase
