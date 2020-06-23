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
#include "auth/src/desktop/rpcs/set_account_info_request.h"
#include "auth/src/desktop/rpcs/set_account_info_response.h"

namespace firebase {
namespace auth {

typedef SetAccountInfoRequest RequestT;
typedef SetAccountInfoResponse ResponseT;

// Test SetAccountInfoRequest
TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateEmail) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateEmailRequest("APIKEY", "fakeemail");
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

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdatePassword) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdatePasswordRequest("APIKEY", "fakepassword");
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

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Full) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest("APIKEY", "New Name", "new_url");
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

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_Partial) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUpdateProfileRequest("APIKEY", nullptr, "new_url");
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

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_UpdateProfile_DeleteFields) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest("APIKEY", "", "");
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

TEST(SetAccountInfoTest,
     TestSetAccountInfoRequest_UpdateProfile_DeleteAndUpdate) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request = RequestT::CreateUpdateProfileRequest("APIKEY", "", "new_url");
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

TEST(SetAccountInfoTest, TestSetAccountInfoRequest_Unlink) {
  std::unique_ptr<App> app(testing::CreateApp());
  auto request =
      RequestT::CreateUnlinkProviderRequest("APIKEY", "fakeprovider");
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

}  // namespace auth
}  // namespace firebase
