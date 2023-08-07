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
      RequestT::CreateUpdateEmailRequest(*app, "APIKEY", "fakeemail", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  email: \"fakeemail\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_UpdateEmail) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateEmailRequest(*app, "APIKEY", "fakeemail", "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  email: \"fakeemail\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdatePassword) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdatePasswordRequest(*app, "APIKEY", "fakepassword", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  password: \"fakepassword\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_UpdatePassword) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdatePasswordRequest(*app, "APIKEY", "fakepassword", nullptr, "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  password: \"fakepassword\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Full) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY",
                                                      "New Name", "new_url",
                                                      nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  displayName: \"New Name\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\"\n"
      "}\n",
      request->options().post_fields);
}


TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_UpdateProfile_Full) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY",
                                                      "New Name", "new_url",
                                                      "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  displayName: \"New Name\",\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\",\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Partial) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", nullptr, "new_url", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_UpdateProfile_Partial) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", nullptr, "new_url", "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\",\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_DeleteFields) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  deleteAttribute: [\n"
      "    \"DISPLAY_NAME\",\n"
      "    \"PHOTO_URL\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_UpdateProfile_DeleteFields) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "", "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  deleteAttribute: [\n"
      "    \"DISPLAY_NAME\",\n"
      "    \"PHOTO_URL\"\n"
      "  ],\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest,
     TestSetAccountInfoRequest_UpdateProfile_DeleteAndUpdate) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "new_url", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\",\n"
      "  deleteAttribute: [\n"
      "    \"DISPLAY_NAME\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest,
     TestSetAccountInfoRequest_UpdateProfileTenant_DeleteAndUpdate) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest(*app, "APIKEY", "", "new_url", "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  photoUrl: \"new_url\",\n"
      "  deleteAttribute: [\n"
      "    \"DISPLAY_NAME\"\n"
      "  ],\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_Unlink) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUnlinkProviderRequest(*app, "APIKEY", "fakeprovider", nullptr);
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  deleteProvider: [\n"
      "    \"fakeprovider\"\n"
      "  ]\n"
      "}\n",
      request->options().post_fields);
}

TEST(SetAccountInfoTest, TestSetAccountInfoRequestTenant_Unlink) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUnlinkProviderRequest(*app, "APIKEY", "fakeprovider", "tenant123");
  request->SetIdToken("token");

  EXPECT_EQ(
      "https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "setAccountInfo?key=APIKEY",
      request->options().url);
  EXPECT_EQ(
      "{\n"
      "  returnSecureToken: true,\n"
      "  idToken: \"token\",\n"
      "  deleteProvider: [\n"
      "    \"fakeprovider\"\n"
      "  ],\n"
      "  tenantId: \"tenant123\"\n"
      "}\n",
      request->options().post_fields);
}

}  // namespace auth
}  // namespace firebase
