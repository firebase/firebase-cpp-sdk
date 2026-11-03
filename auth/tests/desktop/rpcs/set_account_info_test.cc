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
#include "auth/src/desktop/rpcs/set_account_info_request.h"
#include "auth/src/desktop/rpcs/set_account_info_response.h"
#include "auth/src/include/firebase/auth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

typedef SetAccountInfoRequest RequestT;
typedef SetAccountInfoResponse ResponseT;

// Test SetAccountInfoRequest
TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateEmail) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateEmailRequest(*app, "APIKEY", "fakeemail");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"email\": \"fakeemail\",\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestWithEmulator) {
  std::unique_ptr<App> app(testing::CreateApp());
  std::unique_ptr<Auth> auth(Auth::GetAuth(app.get()));
  auth->UseEmulator("localhost", 9099);
  auto request =
      RequestT::CreateUpdateEmailRequest(*app, "APIKEY", "fakeemail");
  EXPECT_EQ(
      "http://localhost:9099/www.googleapis.com/identitytoolkit/v3/"
      "relyingparty/setAccountInfo?key=APIKEY",
      request->options().url);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdatePassword) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdatePasswordRequest(*app, "APIKEY", "fakepassword");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"password\": \"fakepassword\",\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Full) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY",
                                                      "New Name", "new_url");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"displayName\": \"New Name\",\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\",\n"
      "  \"photoUrl\": \"new_url\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Partial) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", nullptr, "new_url");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\",\n"
      "  \"photoUrl\": \"new_url\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_DeleteFields) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\",\n"
      "  \"deleteAttribute\": [\n"
      "    \"DISPLAY_NAME\",\n"
      "    \"PHOTO_URL\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest,
     TestSetAccountInfoRequest_UpdateProfile_DeleteAndUpdate) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "new_url");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\",\n"
      "  \"photoUrl\": \"new_url\",\n"
      "  \"deleteAttribute\": [\n"
      "    \"DISPLAY_NAME\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_Unlink) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUnlinkProviderRequest(*app, "APIKEY", "fakeprovider");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  \"returnSecureToken\": true,\n"
      "  \"idToken\": \"token\",\n"
      "  \"deleteProvider\": [\n"
      "    \"fakeprovider\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoResponse) {
  ResponseT response;
  const char body[] =
      "{\n"
      "  \"kind\": \"identitytoolkit#SetAccountInfoResponse\",\n"
      "  \"localId\": \"localid123\",\n"
      "  \"email\": \"new@email.com\",\n"
      "  \"displayName\": \"New Name\",\n"
      "  \"photoUrl\": \"https://photo.url\",\n"
      "  \"idToken\": \"idtoken123\",\n"
      "  \"refreshToken\": \"refreshtoken123\",\n"
      "  \"expiresIn\": \"3600\",\n"
      "  \"passwordHash\": \"hash123\",\n"
      "  \"emailVerified\": true,\n"
      "  \"providerUserInfo\": [\n"
      "    {\n"
      "      \"providerId\": \"google.com\"\n"
      "    }\n"
      "  ]\n"
      "}";
  response.set_status(rest::util::HttpSuccess);
  response.ProcessBody(body, sizeof(body));
  response.MarkCompleted();
  EXPECT_TRUE(response.IsSuccessful());
  EXPECT_EQ("localid123", response.local_id());
  EXPECT_EQ("new@email.com", response.email());
  EXPECT_EQ("New Name", response.display_name());
  EXPECT_EQ("https://photo.url", response.photo_url());
  EXPECT_EQ("idtoken123", response.id_token());
  EXPECT_EQ("refreshtoken123", response.refresh_token());
  EXPECT_EQ(3600, response.expires_in());
  EXPECT_EQ("hash123", response.password_hash());
  EXPECT_TRUE(response.email_verified());
  EXPECT_EQ(1u, response.providerUserInfos().size());

  ResponseT error_response;
  const char error_body[] =
      "{\n"
      "  \"error\": {\n"
      "    \"code\": 400,\n"
      "    \"message\": \"EMAIL_EXISTS\"\n"
      "  }\n"
      "}";
  error_response.ProcessBody(error_body, sizeof(error_body));
  error_response.MarkCompleted();
  EXPECT_FALSE(error_response.IsSuccessful());
  EXPECT_EQ(kAuthErrorEmailAlreadyInUse, error_response.error_code());
}

}  // namespace auth
}  // namespace firebase
