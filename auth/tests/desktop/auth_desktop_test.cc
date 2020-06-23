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

#include "auth/src/desktop/auth_desktop.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "app/memory/unique_ptr.h"
#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/src/mutex.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/types.h"
#include "auth/src/include/firebase/auth/user.h"
#include "auth/tests/desktop/fakes.h"
#include "auth/tests/desktop/test_utils.h"
#include "testing/config.h"
#include "testing/ticker.h"
#include "flatbuffers/stl_emulation.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace auth {

using test::CreateErrorHttpResponse;
using test::FakeSetT;
using test::FakeSuccessfulResponse;
using test::GetFakeOAuthProviderData;
using test::GetUrlForApi;
using test::InitializeConfigWithAFake;
using test::InitializeConfigWithFakes;
using test::OAuthProviderTestHandler;
using test::VerifySignInResult;
using test::WaitForFuture;
using ::testing::IsEmpty;

namespace {

const char* const API_KEY = "MY-FAKE-API-KEY";
// Constant, describing how many times we would like to sleep 1ms to wait
// for loading persistence cache.
const int kWaitForLoadMaxTryout = 500;

void VerifyProviderData(const User& user) {
  const std::vector<UserInfoInterface*>& provider_data = user.provider_data();
  EXPECT_EQ(1, provider_data.size());
  if (provider_data.empty()) {
    return;  // Avoid crashing on vector out-of-bounds access below
  }
  EXPECT_EQ("fake_uid", provider_data[0]->uid());
  EXPECT_EQ("fake_email@example.com", provider_data[0]->email());
  EXPECT_EQ("fake_display_name", provider_data[0]->display_name());
  EXPECT_EQ("fake_photo_url", provider_data[0]->photo_url());
  EXPECT_EQ("fake_provider_id", provider_data[0]->provider_id());
  EXPECT_EQ("123123", provider_data[0]->phone_number());
}

void VerifyUser(const User& user) {
  EXPECT_EQ("localid123", user.uid());
  EXPECT_EQ("testsignin@example.com", user.email());
  EXPECT_EQ("", user.display_name());
  EXPECT_EQ("", user.photo_url());
  EXPECT_EQ("Firebase", user.provider_id());
  EXPECT_EQ("", user.phone_number());
  EXPECT_FALSE(user.is_email_verified());
  VerifyProviderData(user);
}

std::string GetFakeProviderInfo() {
  return "\"providerUserInfo\": ["
         "  {"
         "    \"federatedId\": \"fake_uid\","
         "    \"email\": \"fake_email@example.com\","
         "    \"displayName\": \"fake_display_name\","
         "    \"photoUrl\": \"fake_photo_url\","
         "    \"providerId\": \"fake_provider_id\","
         "    \"phoneNumber\": \"123123\""
         "  }"
         "]";
}

std::string CreateGetAccountInfoFake() {
  return FakeSuccessfulResponse(
      "GetAccountInfoResponse",
      std::string("\"users\":"
                  "  ["
                  "    {"
                  "       \"localId\": \"localid123\","
                  "       \"email\": \"testsignin@example.com\","
                  "       \"emailVerified\": false,"
                  "       \"passwordHash\": \"abcdefg\","
                  "       \"passwordUpdatedAt\": 31415926,"
                  "       \"validSince\": \"123\","
                  "       \"lastLoginAt\": \"123\","
                  "       \"createdAt\": \"123\",") +
          GetFakeProviderInfo() +
          "    }"
          " ]");
}

std::string CreateVerifyAssertionResponse() {
  return FakeSuccessfulResponse("VerifyAssertionResponse",
                                "\"isNewUser\": true,"
                                "\"localId\": \"localid123\","
                                "\"idToken\": \"idtoken123\","
                                "\"providerId\": \"google.com\","
                                "\"refreshToken\": \"refreshtoken123\","
                                "\"expiresIn\": \"3600\"");
}

std::string CreateVerifyAssertionResponseWithUserInfo(
    const std::string& provider_id, const std::string& raw_user_info) {
  const auto head = std::string(
                        "\"isNewUser\": true,"
                        "\"localId\": \"localid123\","
                        "\"idToken\": \"idtoken123\","
                        "\"providerId\": \"") +
                    provider_id + "\",";

  std::string user_info;
  if (!raw_user_info.empty()) {
    user_info = "\"rawUserInfo\": \"{" + raw_user_info + "}\",";
  }

  const auto tail =
      "\"refreshToken\": \"refreshtoken123\","
      "\"expiresIn\": \"3600\"";

  const auto body = head + user_info + tail;
  return FakeSuccessfulResponse("VerifyAssertionResponse", body);
}

void InitializeSignInWithProviderFakes(
    const std::string& get_account_info_response) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = get_account_info_response;
  InitializeConfigWithFakes(fakes);
}

void InitializeSuccessfulSignInWithProviderFlow(
    FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
    const std::string& get_account_info_response) {
  InitializeSignInWithProviderFakes(get_account_info_response);
  provider->SetProviderData(GetFakeOAuthProviderData());
  provider->SetAuthHandler(handler);
}

void InitializeSuccessfulSignInWithProviderFlow(
    FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler) {
  InitializeSuccessfulSignInWithProviderFlow(provider, handler,
                                             CreateGetAccountInfoFake());
}

void InitializeSuccessfulVerifyAssertionFlow(
    const std::string& verify_assertion_response) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyAssertion")] = verify_assertion_response;
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);
}

void InitializeSuccessfulVerifyAssertionFlow() {
  InitializeSuccessfulVerifyAssertionFlow(CreateVerifyAssertionResponse());
}

void SetupAuthDataForPersist(AuthData* auth_data) {
  UserData previous_user;
  UserData mock_user;

  mock_user.uid = "persist_id";
  mock_user.email = "test@persist.com";
  mock_user.display_name = "persist_name";
  mock_user.photo_url = "persist_photo";
  mock_user.provider_id = "persist_provider";
  mock_user.phone_number = "persist_phone";
  mock_user.is_anonymous = false;
  mock_user.is_email_verified = true;
  mock_user.id_token = "persist_token";
  mock_user.refresh_token = "persist_refresh_token";
  mock_user.access_token = "persist_access_token";
  mock_user.access_token_expiration_date = 12345;
  mock_user.has_email_password_credential = true;
  mock_user.last_sign_in_timestamp = 67890;
  mock_user.creation_timestamp = 98765;
  UserView::ResetUser(auth_data, mock_user, &previous_user);
}

bool WaitOnLoadPersistence(AuthData* auth_data) {
  bool load_finished = false;
  int load_wait_counter = 0;
  while (!load_finished) {
    if (load_wait_counter >= kWaitForLoadMaxTryout) {
      break;
    }
    load_wait_counter++;
    firebase::internal::Sleep(1);
    {
      MutexLock lock(auth_data->listeners_mutex);
      load_finished = !auth_data->persistent_cache_load_pending;
    }
  }
  return load_finished;
}

}  // namespace

class AuthDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new rest::TransportMock());
    });
    AppOptions options = testing::MockAppOptions();
    options.set_app_id("com.firebase.test");
    options.set_api_key(API_KEY);
    firebase_app_ = std::unique_ptr<App>(App::Create(options));
    firebase_auth_ = std::unique_ptr<Auth>(Auth::GetAuth(firebase_app_.get()));
    EXPECT_NE(nullptr, firebase_auth_);

    firebase_auth_->AddIdTokenListener(&id_token_listener);
    firebase_auth_->AddAuthStateListener(&auth_state_listener);

    WaitOnLoadPersistence(firebase_auth_->auth_data_);
  }

  void TearDown() override {
    // Reset listeners before signing out.
    id_token_listener.VerifyAndReset();
    auth_state_listener.VerifyAndReset();
    firebase_auth_->SignOut();
    firebase_auth_.reset(nullptr);
    firebase_app_.reset(nullptr);
    // cppsdk needs to be the last thing torn down, because the mocks are still
    // needed for parts of the firebase destructors.
    firebase::testing::cppsdk::ConfigReset();
  }

  Future<SignInResult> ProcessSignInWithProviderFlow(
      FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
      bool trigger_sign_in) {
    InitializeSignInWithProviderFakes(CreateGetAccountInfoFake());
    provider->SetProviderData(GetFakeOAuthProviderData());
    provider->SetAuthHandler(handler);
    Future<SignInResult> future = firebase_auth_->SignInWithProvider(provider);
    if (trigger_sign_in) {
      handler->TriggerSignInComplete();
    }
    return future;
  }

  std::unique_ptr<App> firebase_app_;
  std::unique_ptr<Auth> firebase_auth_;

  test::IdTokenChangesCounter id_token_listener;
  test::AuthStateChangesCounter auth_state_listener;
};

TEST_F(AuthDesktopTest,
       TestSignInWithProviderReturnsUnsupportedError) {
    FederatedOAuthProvider provider;
  Future<SignInResult> future = firebase_auth_->SignInWithProvider(&provider);
  EXPECT_EQ(future.result()->user, nullptr);
  EXPECT_EQ(future.error(), kAuthErrorUnimplemented);
  EXPECT_EQ(std::string(future.error_message()),
            "Operation is not supported on non-mobile systems.");
}

TEST_F(AuthDesktopTest,
       DISABLED_TestSignInWithProviderAndHandlerPassingIntegrityChecks) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler(/*extra_integrity_checks_=*/true);

  InitializeSuccessfulSignInWithProviderFlow(&provider, &handler);
  Future<SignInResult> future = firebase_auth_->SignInWithProvider(&provider);
  handler.TriggerSignInComplete();
  SignInResult sign_in_result = WaitForFuture(future);
}

TEST_F(AuthDesktopTest,
       DISABLED_TestPendingSignInWithProviderSecondConcurrentSignInFails) {
  FederatedOAuthProvider provider1;
  OAuthProviderTestHandler handler1;
  InitializeSuccessfulSignInWithProviderFlow(&provider1, &handler1);

  FederatedOAuthProvider provider2;
  provider2.SetProviderData(GetFakeOAuthProviderData());

  OAuthProviderTestHandler handler2;
  provider2.SetAuthHandler(&handler2);
  Future<SignInResult> future1 = firebase_auth_->SignInWithProvider(&provider1);
  EXPECT_EQ(future1.status(), kFutureStatusPending);
  Future<SignInResult> future2 = firebase_auth_->SignInWithProvider(&provider2);
  VerifySignInResult(future2, kAuthErrorFederatedProviderAreadyInUse);
  handler1.TriggerSignInComplete();
  const SignInResult sign_in_result = WaitForFuture(future1);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteSignInResultUserPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  FederatedAuthProvider::AuthenticatedUserData user_data =
      *(handler.GetAuthenticatedUserData());
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  EXPECT_NE(sign_in_result.user, nullptr);
  EXPECT_EQ(sign_in_result.user->is_email_verified(),
            user_data.is_email_verified);
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  EXPECT_EQ(sign_in_result.user->uid(), user_data.uid);
  EXPECT_EQ(sign_in_result.user->email(), user_data.email);
  EXPECT_EQ(sign_in_result.user->display_name(), user_data.display_name);
  EXPECT_EQ(sign_in_result.user->photo_url(), user_data.photo_url);
  EXPECT_EQ(sign_in_result.user->provider_id(), user_data.provider_id);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullUIDFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->uid = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullDisplayNamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->display_name = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullUsernamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->user_name = nullptr;
    Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullPhotoUrlPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->photo_url = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullProvderIdFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->provider_id = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullAccessTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->access_token = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullRefreshTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->refresh_token = nullptr;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteExpiresInMaxUInt64Passes) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->token_expires_in_seconds = ULONG_MAX;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteErrorMessagePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/false);
  const char* error_message = "oh nos!";
  handler.TriggerSignInCompleteWithError(kAuthErrorApiNotAvailable,
                                         error_message);
  VerifySignInResult(future, kAuthErrorApiNotAvailable, error_message);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullErrorMessageFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/false);
  handler.TriggerSignInCompleteWithError(kAuthErrorApiNotAvailable, nullptr);
  VerifySignInResult(future, kAuthErrorApiNotAvailable);
}

// Test the helper function GetAccountInfo.
TEST_F(AuthDesktopTest, TestGetAccountInfo) {
  const auto response =
      FakeSuccessfulResponse("GetAccountInfoResponse",
                             "\"users\": "
                             "  ["
                             "    {"
                             "      \"localId\": \"localid123\","
                             "      \"displayName\": \"dp name\","
                             "      \"email\": \"abc@efg\","
                             "      \"photoUrl\": \"www.photo\","
                             "      \"emailVerified\": false,"
                             "      \"passwordHash\": \"abcdefg\","
                             "      \"phoneNumber\": \"519\","
                             "      \"passwordUpdatedAt\": 31415926,"
                             "      \"validSince\": \"123\","
                             "      \"lastLoginAt\": \"123\","
                             "      \"createdAt\": \"123\""
                             "    }"
                             "  ]");
  InitializeConfigWithAFake(GetUrlForApi("APIKEY", "getAccountInfo"), response);

  // getAccountInfo never returns new tokens, and can't change current user.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  // Call the function and verify results.
  AuthData auth_data;
  AuthImpl auth;
  auth_data.auth_impl = &auth;
  auth.api_key = "APIKEY";
  const GetAccountInfoResult result =
      GetAccountInfo(auth_data, "fake_access_token");
  EXPECT_TRUE(result.IsValid());
  const UserData& user = result.user();
  EXPECT_EQ("localid123", user.uid);
  EXPECT_EQ("abc@efg", user.email);
  EXPECT_EQ("dp name", user.display_name);
  EXPECT_EQ("www.photo", user.photo_url);
  EXPECT_EQ("519", user.phone_number);
  EXPECT_FALSE(user.is_email_verified);
  EXPECT_TRUE(user.has_email_password_credential);
}

// Test the helper function CompleteSignIn. Since we do not have the access to
// the private members of Auth, we use SignInAnonymously to test it indirectly.
// Let the REST request failed with 503.
TEST_F(AuthDesktopTest, CompleteSignInWithFailedResponse) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] = CreateErrorHttpResponse();
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  // Because the API call fails, current user shouldn't have changed.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  // Call the function and verify results.
  const User* const user =
      WaitForFuture(firebase_auth_->SignInAnonymously(), kAuthErrorFailure);
  EXPECT_EQ(nullptr, user);
}

// Test the helper function CompleteSignIn. Since we do not have the access to
// the private members of Auth, we use SignInAnonymously to test it indirectly.
// Let it failed to get account info.
TEST_F(AuthDesktopTest, CompleteSignInWithGetAccountInfoFailure) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateErrorHttpResponse();
  InitializeConfigWithFakes(fakes);

  // User is not updated until getAccountInfo succeeds; calls to signupNewUser
  // and getAccountInfo are considered a single "transaction". So if
  // getAccountInfo fails, current user shouldn't have changed.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  // Call the function and verify results.
  const User* const user =
      WaitForFuture(firebase_auth_->SignInAnonymously(), kAuthErrorFailure);
  EXPECT_EQ(nullptr, user);
}

// Test Auth::SignInAnonymously.
TEST_F(AuthDesktopTest, TestSignInAnonymously) {
  FakeSetT fakes;

  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");

  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] =
      FakeSuccessfulResponse("GetAccountInfoResponse",
                             "\"users\": "
                             "  ["
                             "    {"
                             "      \"localId\": \"localid123\","
                             "      \"lastLoginAt\": \"123\","
                             "      \"createdAt\": \"123\""
                             "    }"
                             "  ]");
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const User* const user = WaitForFuture(firebase_auth_->SignInAnonymously());
  EXPECT_TRUE(user->is_anonymous());
  EXPECT_EQ("localid123", user->uid());
  EXPECT_EQ("", user->email());
  EXPECT_EQ("", user->display_name());
  EXPECT_EQ("", user->photo_url());
  EXPECT_EQ("Firebase", user->provider_id());
  EXPECT_EQ("", user->phone_number());
  EXPECT_FALSE(user->is_email_verified());
}

// Test Auth::SignInWithEmailAndPassword.
TEST_F(AuthDesktopTest, TestSignInWithEmailAndPassword) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyPassword")] =
      FakeSuccessfulResponse("VerifyPasswordResponse",
                             "\"localId\": \"localid123\","
                             "\"email\": \"testsignin@example.com\","
                             "\"idToken\": \"idtoken123\","
                             "\"registered\": true,"
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  // Call the function and verify results.
  const Future<User*> future = firebase_auth_->SignInWithEmailAndPassword(
      "testsignin@example.com", "testsignin");
  const User* const user = WaitForFuture(future);
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

// Test Auth::CreateUserWithEmailAndPassword.
TEST_F(AuthDesktopTest, TestCreateUserWithEmailAndPassword) {
  FakeSetT fakes;

  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"displayName\": \"\","
                             "\"email\": \"testsignin@example.com\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");

  fakes[GetUrlForApi(API_KEY, "verifyPassword")] =
      FakeSuccessfulResponse("VerifyPasswordResponse",
                             "\"localId\": \"localid123\","
                             "\"email\": \"testsignin@example.com\","
                             "\"idToken\": \"idtoken123\","
                             "\"registered\": true,"
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\"");

  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Future<User*> future = firebase_auth_->CreateUserWithEmailAndPassword(
      "testsignin@example.com", "testsignin");
  const User* const user = WaitForFuture(future);
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

// Test Auth::SignInWithCustomToken.
TEST_F(AuthDesktopTest, TestSignInWithCustomToken) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyCustomToken")] =
      FakeSuccessfulResponse("VerifyCustomTokenResponse",
                             "\"isNewUser\": true,"
                             "\"localId\": \"localid123\","
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const User* const user =
      WaitForFuture(firebase_auth_->SignInWithCustomToken("fake_custom_token"));
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

// Test Auth::TestSignInWithCredential.

TEST_F(AuthDesktopTest, TestSignInWithCredential_GoogleIdToken) {
  InitializeSuccessfulVerifyAssertionFlow();

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  const User* const user =
      WaitForFuture(firebase_auth_->SignInWithCredential(credential));
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

TEST_F(AuthDesktopTest, TestSignInWithCredential_GoogleAccessToken) {
  InitializeSuccessfulVerifyAssertionFlow();

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential =
      GoogleAuthProvider::GetCredential("", "fake_access_token");
  const User* const user =
      WaitForFuture(firebase_auth_->SignInWithCredential(credential));
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

TEST_F(AuthDesktopTest,
       TestSignInWithCredential_WithFailedVerifyAssertionResponse) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyAssertion")] = CreateErrorHttpResponse();
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  const Credential credential =
      GoogleAuthProvider::GetCredential("", "fake_access_token");
  const User* const user = WaitForFuture(
      firebase_auth_->SignInWithCredential(credential), kAuthErrorFailure);
  EXPECT_EQ(nullptr, user);
}

TEST_F(AuthDesktopTest,
       TestSignInWithCredential_WithFailedGetAccountInfoResponse) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyAssertion")] =
      CreateVerifyAssertionResponse();
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateErrorHttpResponse();
  InitializeConfigWithFakes(fakes);

  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  const Credential credential =
      GoogleAuthProvider::GetCredential("", "fake_access_token");
  const User* const user = WaitForFuture(
      firebase_auth_->SignInWithCredential(credential), kAuthErrorFailure);
  EXPECT_EQ(nullptr, user);
}

TEST_F(AuthDesktopTest, TestSignInWithCredential_NeedsConfirmation) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "verifyAssertion"),
      FakeSuccessfulResponse("verifyAssertion", "\"needConfirmation\": true"));

  // needConfirmation is considered an error by the SDK, so current user
  // shouldn't have been updated.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  WaitForFuture(firebase_auth_->SignInWithCredential(credential),
                kAuthErrorAccountExistsWithDifferentCredentials);
}

TEST_F(AuthDesktopTest, TestSignInAndRetrieveDataWithCredential_GitHub) {
  const auto response = CreateVerifyAssertionResponseWithUserInfo(
      "github.com",
      "\\\\\"login\\\\\": \\\\\"fake_user_name\\\\\","
      "\\\\\"some_str_key\\\\\": \\\\\"some_value\\\\\","
      "\\\\\"some_num_key\\\\\": 123");
  InitializeSuccessfulVerifyAssertionFlow(response);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential =
      GitHubAuthProvider::GetCredential("fake_access_token");
  const SignInResult sign_in_result = WaitForFuture(
      firebase_auth_->SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);

  EXPECT_STREQ("github.com", sign_in_result.info.provider_id.c_str());
  EXPECT_STREQ("fake_user_name", sign_in_result.info.user_name.c_str());

  const auto found_str_value =
      sign_in_result.info.profile.find(Variant("some_str_key"));
  EXPECT_NE(found_str_value, sign_in_result.info.profile.end());
  EXPECT_STREQ("some_value", found_str_value->second.string_value());

  const auto found_num_value =
      sign_in_result.info.profile.find(Variant("some_num_key"));
  EXPECT_NE(found_num_value, sign_in_result.info.profile.end());
  EXPECT_EQ(123, found_num_value->second.int64_value());
}

TEST_F(AuthDesktopTest, TestSignInAndRetrieveDataWithCredential_Twitter) {
  const auto response = CreateVerifyAssertionResponseWithUserInfo(
      "twitter.com", "\\\\\"screen_name\\\\\": \\\\\"fake_user_name\\\\\"");
  InitializeSuccessfulVerifyAssertionFlow(response);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential = TwitterAuthProvider::GetCredential(
      "fake_access_token", "fake_oauth_token");
  const SignInResult sign_in_result = WaitForFuture(
      firebase_auth_->SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);

  EXPECT_EQ("twitter.com", sign_in_result.info.provider_id);
  EXPECT_EQ("fake_user_name", sign_in_result.info.user_name);
}

TEST_F(AuthDesktopTest,
       TestSignInAndRetrieveDataWithCredential_NoAdditionalInfo) {
  const auto response =
      CreateVerifyAssertionResponseWithUserInfo("github.com", "");
  InitializeSuccessfulVerifyAssertionFlow(response);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential = TwitterAuthProvider::GetCredential(
      "fake_access_token", "fake_oauth_token");
  const SignInResult sign_in_result = WaitForFuture(
      firebase_auth_->SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);

  EXPECT_EQ("github.com", sign_in_result.info.provider_id);
  EXPECT_THAT(sign_in_result.info.profile, IsEmpty());
  EXPECT_THAT(sign_in_result.info.user_name, IsEmpty());
}

TEST_F(AuthDesktopTest,
       TestSignInAndRetrieveDataWithCredential_BadUserNameFormat) {
  // Deliberately using a number instead of a string, let's make sure it doesn't
  // cause a crash.
  const auto response = CreateVerifyAssertionResponseWithUserInfo(
      "twitter.com", "\\\\\"screen_name\\\\\": 123");
  InitializeSuccessfulVerifyAssertionFlow(response);

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential = TwitterAuthProvider::GetCredential(
      "fake_access_token", "fake_oauth_token");
  const SignInResult sign_in_result = WaitForFuture(
      firebase_auth_->SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);

  EXPECT_EQ("twitter.com", sign_in_result.info.provider_id);
  EXPECT_THAT(sign_in_result.info.user_name, IsEmpty());
}

TEST_F(AuthDesktopTest, TestFetchProvidersForEmail) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "createAuthUri"),
                            FakeSuccessfulResponse("CreateAuthUriResponse",
                                                   "\"allProviders\": ["
                                                   "  \"password\","
                                                   "  \"example.com\""
                                                   "],"
                                                   "\"registered\": true"));

  // Fetch providers flow shouldn't affect current user in any way.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  const Auth::FetchProvidersResult result = WaitForFuture(
      firebase_auth_->FetchProvidersForEmail("fake_email@example.com"));
  EXPECT_EQ(2, result.providers.size());
  EXPECT_EQ("password", result.providers[0]);
  EXPECT_EQ("example.com", result.providers[1]);
}

TEST_F(AuthDesktopTest, TestSendPasswordResetEmail) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "getOobConfirmationCode"),
      FakeSuccessfulResponse("GetOobConfirmationCodeResponse",
                             "\"email\": \"fake_email@example.com\""));

  // Sending password reset email shouldn't affect current user in any way.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  WaitForFuture(
      firebase_auth_->SendPasswordResetEmail("fake_email@example.com"));
}

TEST(UserViewTest, TestCopyUserView) {
  // Construct from UserData.
  UserData user1;
  user1.uid = "mrsspoon";
  UserView view1(user1);
  UserView view3(view1);
  UserView view4 = view3;
  EXPECT_EQ("mrsspoon", view1.user_data().uid);
  EXPECT_EQ("mrsspoon", view3.user_data().uid);
  EXPECT_EQ("mrsspoon", view4.user_data().uid);

  // Construct from a UserView.
  UserData user2;
  user2.uid = "dangerm";
  UserView view2(user2);
  EXPECT_EQ("dangerm", view2.user_data().uid);

  // Copy a UserView.
  view3 = view2;
  EXPECT_EQ("mrsspoon", view1.user_data().uid);
  EXPECT_EQ("dangerm", view2.user_data().uid);
  EXPECT_EQ("dangerm", view3.user_data().uid);
}

#if defined(FIREBASE_USE_MOVE_OPERATORS)
TEST(UserViewTest, TestMoveUserView) {
  UserData user1;
  user1.uid = "mrsspoon";
  UserData user2;
  user2.uid = "dangerm";
  UserView view1(user1);
  UserView view2(user2);
  UserView view3(user2);
  UserView view4(std::move(view3));
  EXPECT_EQ("mrsspoon", view1.user_data().uid);
  EXPECT_EQ("dangerm", view2.user_data().uid);
  EXPECT_EQ("dangerm", view4.user_data().uid);
  view2 = std::move(view1);
  EXPECT_EQ("mrsspoon", view2.user_data().uid);
}
#endif  // defined(defined(FIREBASE_USE_MOVE_OPERATORS)

}  // namespace auth
}  // namespace firebase
