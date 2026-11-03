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
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

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
  auto request = VerifyAssertionRequest::FromIdToken(*app, "APIKEY", "provider",
                                                     "id_token");
  CheckUrl(*request);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"requestUri\": \"http://localhost\",\n"
      "  \"postBody\": \"providerId=provider&id_token=id_token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(VerifyAssertionTest, TestVerifyAssertionRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  auto request = VerifyAssertionRequest::FromIdToken(*app, "APIKEY", "provider",
                                                     "id_token");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/verifyAssertion?key=APIKEY",
      request->options().url);
}

TEST(VerifyAssertionTest, TestVerifyAssertionRequest_FromAccessToken) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = VerifyAssertionRequest::FromAccessToken(
      *app, "APIKEY", "provider", "access_token");
  CheckUrl(*request);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"requestUri\": \"http://localhost\",\n"
      "  \"postBody\": \"providerId=provider&access_token=access_token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(VerifyAssertionTest, TestVerifyAssertionRequest_FromAccessTokenAndSecret) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
      *app, "APIKEY", "provider", "access_token", "oauth_secret");
  CheckUrl(*request);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"requestUri\": \"http://localhost\",\n"
      "  \"postBody\": "
      "\"providerId=provider&access_token=access_token&"
      "oauth_token_secret=oauth_secret\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(VerifyAssertionTest,
     TestVerifyAssertionRequest_FromAuthCodeAndNonceAndUrlEncoding) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto auth_code_req = VerifyAssertionRequest::FromAuthCode(
      *app, "APIKEY", "github.com", "code&1=2");
  CheckUrl(*auth_code_req);
  auth_code_req->SetIdToken("user_id_token");
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"user_id_token\",\n"
      "  \"requestUri\": \"http://localhost\",\n"
      "  \"postBody\": \"providerId=github.com&code=code%261%3D2\"\n"
      "}\n",
      auth_code_req->options().post_fields);

  auto nonce_req = VerifyAssertionRequest::FromIdToken(
      *app, "APIKEY", "apple.com", "id_tok", "nonce+val");
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"requestUri\": \"http://localhost\",\n"
      "  \"postBody\": "
      "\"providerId=apple.com&id_token=id_tok&nonce=nonce%2Bval\"\n"
      "}\n",
      nonce_req->options().post_fields);
}

TEST(VerifyAssertionTest, TestVerifyAssertionResponseSuccess) {
  VerifyAssertionResponse response;
  const char body[] =
      "{\n"
      "  \"kind\": \"identitytoolkit#VerifyAssertionResponse\",\n"
      "  \"localId\": \"localid123\",\n"
      "  \"idToken\": \"idtoken123\",\n"
      "  \"refreshToken\": \"refreshtoken123\",\n"
      "  \"expiresIn\": \"3600\",\n"
      "  \"providerId\": \"google.com\",\n"
      "  \"oauthIdToken\": \"oauth_id_123\",\n"
      "  \"oauthAccessToken\": \"oauth_access_123\",\n"
      "  \"oauthTokenSecret\": \"oauth_secret_123\",\n"
      "  \"pendingToken\": \"pending_123\",\n"
      "  \"needConfirmation\": true,\n"
      "  \"isNewUser\": true\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ("localid123", response.local_id());
  EXPECT_EQ("idtoken123", response.id_token());
  EXPECT_EQ("refreshtoken123", response.refresh_token());
  EXPECT_EQ(3600, response.expires_in());
  EXPECT_EQ("google.com", response.provider_id());
  EXPECT_EQ("oauth_id_123", response.oauth_id_token());
  EXPECT_EQ("oauth_access_123", response.oauth_access_token());
  EXPECT_EQ("oauth_secret_123", response.oauth_token_secret());
  EXPECT_EQ("pending_123", response.pending_token());
  EXPECT_TRUE(response.need_confirmation());
  EXPECT_TRUE(response.is_new_user());
}

TEST(VerifyAssertionTest, TestErrorResponse) {
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
