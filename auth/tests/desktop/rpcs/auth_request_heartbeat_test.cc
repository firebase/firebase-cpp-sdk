// Copyright 2022 Google LLC
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
#include "app/src/app_common.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/desktop/rpcs/create_auth_uri_request.h"
#include "auth/src/desktop/rpcs/delete_account_request.h"
#include "auth/src/desktop/rpcs/get_account_info_request.h"
#include "auth/src/desktop/rpcs/get_oob_confirmation_code_request.h"
#include "auth/src/desktop/rpcs/reset_password_request.h"
#include "auth/src/desktop/rpcs/secure_token_request.h"
#include "auth/src/desktop/rpcs/set_account_info_request.h"
#include "auth/src/desktop/rpcs/sign_up_new_user_request.h"
#include "auth/src/desktop/rpcs/verify_assertion_request.h"
#include "auth/src/desktop/rpcs/verify_custom_token_request.h"
#include "auth/src/desktop/rpcs/verify_password_request.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {

using ::firebase::heartbeat::HeartbeatStorageDesktop;
using ::firebase::heartbeat::LoggedHeartbeats;

class AuthRequestHeartbeatTest : public ::testing::Test {
 public:
  AuthRequestHeartbeatTest() : app_(testing::CreateApp()) {}

 protected:
  std::unique_ptr<App> app_;

  void SetUp() override {
    Logger logger(nullptr);
    HeartbeatStorageDesktop storage(app_->name(), logger);
    // For the sake of testing, clear any pre-existing stored heartbeats.
    LoggedHeartbeats empty_heartbeats_struct;
    storage.Write(empty_heartbeats_struct);
  }
};

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestCreateAuthUriRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  CreateAuthUriRequest request(*app_, "APIKEY", "email");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestDeleteAccountRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  DeleteAccountRequest request(*app_, "APIKEY");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestGetAccountInfoRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  GetAccountInfoRequest request(*app_, "APIKEY");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestOobSendEmailRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request =
      GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest(
          *app_, "APIKEY");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestOobSendPasswordResetRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request =
      GetOobConfirmationCodeRequest::CreateSendPasswordResetEmailRequest(
          *app_, "APIKEY", "email");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestResetPasswordRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  ResetPasswordRequest request(*app_, "APIKEY", "oob", "password");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSecureTokenRequestDoesNotHaveHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  SecureTokenRequest request(*app_, "APIKEY", "email");

  // SecureTokenRequest should not have heartbeat payload since it is sent
  // to a backend that does not support the payload.
  EXPECT_EQ("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("", request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdatePasswordRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = SetAccountInfoRequest::CreateUpdatePasswordRequest(
      *app_, "APIKEY", "fakepassword");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdateEmailRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request =
      SetAccountInfoRequest::CreateUpdateEmailRequest(*app_, "APIKEY", "email");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdateProfileRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = SetAccountInfoRequest::CreateUpdateProfileRequest(
      *app_, "APIKEY", "New Name", "new_url");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSetInfoUnlinkProviderRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = SetAccountInfoRequest::CreateUnlinkProviderRequest(
      *app_, "APIKEY", "provider");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestSignUpNewUserRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  SignUpNewUserRequest request(*app_, "APIKEY");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromIdTokenRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = VerifyAssertionRequest::FromIdToken(*app_, "APIKEY",
                                                     "provider", "id_token");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromAccessTokenRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = VerifyAssertionRequest::FromAccessToken(
      *app_, "APIKEY", "provider", "access_token");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromAccessTokenAndOauthRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  auto request = VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
      *app_, "APIKEY", "provider", "access_token", "oauth_secret");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request->options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request->options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestVerifyCustomTokenRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  VerifyCustomTokenRequest request(*app_, "APIKEY", "email");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

#if FIREBASE_PLATFORM_DESKTOP
TEST_F(AuthRequestHeartbeatTest, TestVerifyPasswordRequestHasHeartbeat) {
  // Log a single heartbeat for today's date.
  app_->GetHeartbeatController()->LogHeartbeat();
  VerifyPasswordRequest request(*app_, "APIKEY", "abc@email", "pwd");

  // The request headers should include both hearbeat payload and GMP App ID.
  EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("com.google.firebase.testing",
            request.options().header[app_common::kXFirebaseGmpIdHeader]);
}
#endif  // FIREBASE_PLATFORM_DESKTOP

}  // namespace auth
}  // namespace firebase
