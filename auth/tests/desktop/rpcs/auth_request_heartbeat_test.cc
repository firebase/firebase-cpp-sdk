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
#include <string>

#include "app/rest/transport_builder.h"
#include "app/src/app_common.h"
#include "app/src/function_registry.h"
#include "app/src/heartbeat/heartbeat_controller_desktop.h"
#include "app/src/heartbeat/heartbeat_storage_desktop.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/reference_counted_future_impl.h"
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

#if FIREBASE_PLATFORM_DESKTOP

using ::firebase::heartbeat::HeartbeatStorageDesktop;
using ::firebase::heartbeat::LoggedHeartbeats;

// Heartbeats are persisted per app name in a store shared across processes, so
// use an app name unique to this binary; otherwise other test binaries using
// the default app interfere when ctest runs them in parallel.
constexpr char kHeartbeatTestAppName[] = "auth_request_heartbeat_test";

class AuthRequestHeartbeatTest : public ::testing::Test {
 public:
  AuthRequestHeartbeatTest() : had_saved_heartbeats_(false) {}

 protected:
  std::unique_ptr<App> app_;
  LoggedHeartbeats saved_heartbeats_;
  bool had_saved_heartbeats_;

  void SetUp() override {
    Logger logger(nullptr);
    HeartbeatStorageDesktop storage(kHeartbeatTestAppName, logger);
    had_saved_heartbeats_ = storage.ReadTo(saved_heartbeats_);
    // Clear any pre-existing stored heartbeats for the test.
    LoggedHeartbeats empty_heartbeats_struct;
    storage.Write(empty_heartbeats_struct);
    // Update the unique ptr to point at the created app.
    app_.reset(
        testing::CreateApp(testing::MockAppOptions(), kHeartbeatTestAppName));
  }

  void TearDown() override {
    app_.reset();
    Logger logger(nullptr);
    HeartbeatStorageDesktop storage(kHeartbeatTestAppName, logger);
    if (had_saved_heartbeats_) {
      storage.Write(saved_heartbeats_);
    } else {
      LoggedHeartbeats empty_heartbeats_struct;
      storage.Write(empty_heartbeats_struct);
    }
  }

  void ExpectHasHeartbeat(AuthRequest& request) {
    request.PrepareRequestHeaders();
    // The first request headers should include both heartbeat payload and GMP
    // App ID.
    EXPECT_NE("", request.options().header[app_common::kApiClientHeader]);
    EXPECT_EQ("com.google.firebase.testing",
              request.options().header[app_common::kXFirebaseGmpIdHeader]);
  }
};

TEST_F(AuthRequestHeartbeatTest, TestCreateAuthUriRequestHasHeartbeat) {
  CreateAuthUriRequest request(*app_, "APIKEY", "email");
  ExpectHasHeartbeat(request);

  // Subsequent request when no new heartbeats exist should omit both headers.
  CreateAuthUriRequest second_request(*app_, "APIKEY", "email");
  second_request.PrepareRequestHeaders();
  EXPECT_EQ("", second_request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ(
      "", second_request.options().header[app_common::kXFirebaseGmpIdHeader]);
}

TEST_F(AuthRequestHeartbeatTest, TestDeleteAccountRequestHasHeartbeat) {
  DeleteAccountRequest request(*app_, "APIKEY");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest, TestGetAccountInfoRequestHasHeartbeat) {
  GetAccountInfoRequest request(*app_, "APIKEY");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest, TestOobSendEmailRequestHasHeartbeat) {
  auto request =
      GetOobConfirmationCodeRequest::CreateSendEmailVerificationRequest(
          *app_, "APIKEY");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestOobSendPasswordResetRequestHasHeartbeat) {
  auto request =
      GetOobConfirmationCodeRequest::CreateSendPasswordResetEmailRequest(
          *app_, "APIKEY", "email");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestResetPasswordRequestHasHeartbeat) {
  ResetPasswordRequest request(*app_, "APIKEY", "oob", "password");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest, TestSecureTokenRequestDoesNotHaveHeartbeat) {
  SecureTokenRequest request(*app_, "APIKEY", "email");
  request.PrepareRequestHeaders();

  // SecureTokenRequest should not have heartbeat payload since it is sent
  // to a backend that does not support the payload.
  EXPECT_EQ("", request.options().header[app_common::kApiClientHeader]);
  EXPECT_EQ("", request.options().header[app_common::kXFirebaseGmpIdHeader]);
}

TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdatePasswordRequestHasHeartbeat) {
  auto request = SetAccountInfoRequest::CreateUpdatePasswordRequest(
      *app_, "APIKEY", "fakepassword");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdateEmailRequestHasHeartbeat) {
  auto request =
      SetAccountInfoRequest::CreateUpdateEmailRequest(*app_, "APIKEY", "email");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestSetInfoUpdateProfileRequestHasHeartbeat) {
  auto request = SetAccountInfoRequest::CreateUpdateProfileRequest(
      *app_, "APIKEY", "New Name", "new_url");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestSetInfoUnlinkProviderRequestHasHeartbeat) {
  auto request = SetAccountInfoRequest::CreateUnlinkProviderRequest(
      *app_, "APIKEY", "provider");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestSignUpNewUserRequestHasHeartbeat) {
  SignUpNewUserRequest request(*app_, "APIKEY");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromIdTokenRequestHasHeartbeat) {
  auto request = VerifyAssertionRequest::FromIdToken(*app_, "APIKEY",
                                                     "provider", "id_token");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromAccessTokenRequestHasHeartbeat) {
  auto request = VerifyAssertionRequest::FromAccessToken(
      *app_, "APIKEY", "provider", "access_token");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest,
       TestVerifyAssertionFromAccessTokenAndOauthRequestHasHeartbeat) {
  auto request = VerifyAssertionRequest::FromAccessTokenAndOAuthSecret(
      *app_, "APIKEY", "provider", "access_token", "oauth_secret");
  ExpectHasHeartbeat(*request);
}

TEST_F(AuthRequestHeartbeatTest, TestVerifyCustomTokenRequestHasHeartbeat) {
  VerifyCustomTokenRequest request(*app_, "APIKEY", "email");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest, TestVerifyPasswordRequestHasHeartbeat) {
  VerifyPasswordRequest request(*app_, "APIKEY", "abc@email", "pwd");
  ExpectHasHeartbeat(request);
}

TEST_F(AuthRequestHeartbeatTest, TestRequestAttachesAppCheckTokenHeader) {
  static ReferenceCountedFutureImpl s_future_impl(1);
  app_->function_registry()->RegisterFunction(
      internal::FnAppCheckGetTokenAsync,
      [](App* /*app*/, void* /*args*/, void* out) -> bool {
        SafeFutureHandle<std::string> handle =
            s_future_impl.SafeAlloc<std::string>(0);
        s_future_impl.CompleteWithResult(handle, 0, "",
                                         std::string("fake_app_check_token"));
        *static_cast<Future<std::string>*>(out) =
            MakeFuture(&s_future_impl, handle);
        return true;
      });

  CreateAuthUriRequest request(*app_, "APIKEY", "email");
  request.PrepareRequestHeaders();
  EXPECT_EQ("fake_app_check_token",
            request.options().header["X-Firebase-AppCheck"]);

  app_->function_registry()->UnregisterFunction(
      internal::FnAppCheckGetTokenAsync);
}

#endif  // FIREBASE_PLATFORM_DESKTOP

}  // namespace auth
}  // namespace firebase
