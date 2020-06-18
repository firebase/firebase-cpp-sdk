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
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"

namespace {
void CheckUrl(const firebase::auth::VerifyAssertionRequest& request) {
  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "verifyAssertion?key=APIKEY",
      request.options().url);
}
}  // namespace

namespace firebase {
namespace auth {

// Test VerifyAssertionRequest
TEST(VerifyAssertionTest, TestVerifyAssertionRequest_FromIdToken) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      VerifyAssertionRequest::FromIdToken("APIKEY", "provider", "id_token");
  CheckUrl(*request);
}

TEST(VerifyAssertionTest, TestVerifyAssertionRequest_FromAccessToken) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = VerifyAssertionRequest::FromAccessToken("APIKEY", "provider",
                                                         "access_token");
  CheckUrl(*request);
}

TEST(VerifyAssertionTest, TestVerifyAssertionRequest_FromAccessTokenAndSecret) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
      "APIKEY", "provider", "access_token", "oauth_secret");
  CheckUrl(*request);
}

TEST(VerifyAssertionTest, TestErrorResponse) {
  std::unique_ptr<App> app(testing::CreateApp());
  VerifyAssertionResponse response;
  const char body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"INVALID_IDP_RESPONSE\",\n"
      "    \"errors\": [\n"
      "      {\n"
      "        \"reason\": \"some reason\"\n"
      "      }\n"
      "    ]\n"
      "  }\n"
      "}";
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();

  EXPECT_EQ(kAuthErrorInvalidCredential, response.error_code());

  // Make sure response doesn't crash on access.
  EXPECT_EQ("", response.local_id());
  EXPECT_EQ("", response.id_token());
  EXPECT_EQ("", response.refresh_token());
  EXPECT_EQ(0, response.expires_in());
}

}  // namespace auth
}  // namespace firebase
