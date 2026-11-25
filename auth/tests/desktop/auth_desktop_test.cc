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

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/desktop/sign_in_flow.h"
#include "auth/src/desktop/user_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/types.h"
#include "auth/src/include/firebase/auth/user.h"
#include "auth/tests/desktop/fakes.h"
#include "auth/tests/desktop/test_utils.h"
#include "flatbuffers/stl_emulation.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"
#include "testing/ticker.h"

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
using test::SleepUponDestruction;
using test::VerifyAuthResult;
using test::WaitForFuture;
using ::testing::IsEmpty;

namespace {

const char* const API_KEY = "MY-FAKE-API-KEY";
// Constant, describing how many times we would like to sleep 1ms to wait
// for loading persistence cache.
const int kWaitForLoadMaxTryout = 500;

void VerifyProviderData(const User& user) {
  const std::vector<UserInfoInterface>& provider_data = user.provider_data();
  EXPECT_EQ(1, provider_data.size());
  if (provider_data.empty()) {
    return;  // Avoid crashing on vector out-of-bounds access below
  }
  EXPECT_EQ("fake_uid", provider_data[0].uid());
  EXPECT_EQ("fake_email@example.com", provider_data[0].email());
  EXPECT_EQ("fake_display_name", provider_data[0].display_name());
  EXPECT_EQ("fake_photo_url", provider_data[0].photo_url());
  EXPECT_EQ("fake_provider_id", provider_data[0].provider_id());
  EXPECT_EQ("123123", provider_data[0].phone_number());
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
    firebase_app_ = App::Create(options);
    firebase_auth_ = Auth::GetAuth(firebase_app_);
    EXPECT_TRUE(firebase_auth_.is_valid());

    firebase_auth_.AddIdTokenListener(&id_token_listener);
    firebase_auth_.AddAuthStateListener(&auth_state_listener);

    WaitOnLoadPersistence(firebase_auth_.auth_data_.get());
  }

  void TearDown() override {
    // Reset listeners before signing out.
    id_token_listener.VerifyAndReset();
    auth_state_listener.VerifyAndReset();
    firebase_auth_.SignOut();

    // Wait for the app to finish any remaining tasks in queue,
    // specifically delete app data from persistent cache after SignOut.
    // This is to avoid race conditions where the next test's SignIn
    // doesn't cause a change in auth state or id tokens because persistent
    // cache has valid user logged in, preventing listeners from firing.
    { SleepUponDestruction sleep_for_listeners; }

    firebase_auth_ = Auth();
    firebase_app_ = App();
    // cppsdk needs to be the last thing torn down, because the mocks are still
    // needed for parts of the firebase destructors.
    firebase::testing::cppsdk::ConfigReset();
  }

  Future<AuthResult> ProcessSignInWithProviderFlow(
      FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
      bool trigger_sign_in) {
    InitializeSignInWithProviderFakes(CreateGetAccountInfoFake());
    provider->SetProviderData(GetFakeOAuthProviderData());
    provider->SetAuthHandler(handler);
    Future<AuthResult> future = firebase_auth_.SignInWithProvider(provider);
    if (trigger_sign_in) {
      handler->TriggerSignInComplete();
    }
    return future;
  }

  AuthData* auth_data() { return firebase_auth_.auth_data_.get(); }

  App firebase_app_;
  Auth firebase_auth_;

  test::IdTokenChangesCounter id_token_listener;
  test::AuthStateChangesCounter auth_state_listener;
};

TEST_F(AuthDesktopTest, TestSignInWithProviderReturnsUnsupportedError) {
  FederatedOAuthProvider provider;
  Future<AuthResult> future = firebase_auth_.SignInWithProvider(&provider);
  EXPECT_FALSE(future.result()->user.is_valid());
  EXPECT_EQ(future.error(), kAuthErrorUnimplemented);
  EXPECT_EQ(std::string(future.error_message()),
            "Operation is not supported on non-mobile systems.");
}

TEST_F(AuthDesktopTest,
       DISABLED_TestSignInWithProviderAndHandlerPassingIntegrityChecks) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler(/*extra_integrity_checks_=*/true);

  InitializeSuccessfulSignInWithProviderFlow(&provider, &handler);
  Future<AuthResult> future = firebase_auth_.SignInWithProvider(&provider);
  handler.TriggerSignInComplete();
  AuthResult result = WaitForFuture(future);
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
  Future<AuthResult> future1 = firebase_auth_.SignInWithProvider(&provider1);
  EXPECT_EQ(future1.status(), kFutureStatusPending);
  Future<AuthResult> future2 = firebase_auth_.SignInWithProvider(&provider2);
  VerifyAuthResult(future2, kAuthErrorFederatedProviderAlreadyInUse);
  handler1.TriggerSignInComplete();
  const AuthResult result = WaitForFuture(future1);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteSignInResultUserPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  FederatedAuthProvider::AuthenticatedUserData user_data =
      *(handler.GetAuthenticatedUserData());
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  AuthResult result = WaitForFuture(future);
  EXPECT_TRUE(result.user.is_valid());
  EXPECT_EQ(result.user.is_email_verified(), user_data.is_email_verified);
  EXPECT_FALSE(result.user.is_anonymous());
  EXPECT_EQ(result.user.uid(), user_data.uid);
  EXPECT_EQ(result.user.email(), user_data.email);
  EXPECT_EQ(result.user.display_name(), user_data.display_name);
  EXPECT_EQ(result.user.photo_url(), user_data.photo_url);
  EXPECT_EQ(result.user.provider_id(), user_data.provider_id);
  VerifyProviderData(result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullUIDFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->uid = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifyAuthResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullDisplayNamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->display_name = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  AuthResult result = WaitForFuture(future);
  VerifyProviderData(result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullUsernamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->user_name = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  AuthResult result = WaitForFuture(future);
  VerifyProviderData(result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullPhotoUrlPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->photo_url = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  AuthResult result = WaitForFuture(future);
  VerifyProviderData(result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullProvderIdFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->provider_id = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifyAuthResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullAccessTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->access_token = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifyAuthResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullRefreshTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->refresh_token = nullptr;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  VerifyAuthResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteExpiresInMaxUInt64Passes) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->token_expires_in_seconds = ULONG_MAX;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/true);
  AuthResult result = WaitForFuture(future);
  VerifyProviderData(result.user);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteErrorMessagePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/false);
  const char* error_message = "oh nos!";
  handler.TriggerSignInCompleteWithError(kAuthErrorApiNotAvailable,
                                         error_message);
  VerifyAuthResult(future, kAuthErrorApiNotAvailable, error_message);
}

TEST_F(AuthDesktopTest, DISABLED_TestSignInCompleteNullErrorMessageFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<AuthResult> future = ProcessSignInWithProviderFlow(
      &provider, &handler, /*trigger_sign_in=*/false);
  handler.TriggerSignInCompleteWithError(kAuthErrorApiNotAvailable, nullptr);
  VerifyAuthResult(future, kAuthErrorApiNotAvailable);
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
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "getAccountInfo"), response);

  // getAccountInfo never returns new tokens, and can't change current user.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  // Call the function and verify results.
  const GetAccountInfoResult result =
      GetAccountInfo(*auth_data(), "fake_access_token");
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
  AuthResult result =
      WaitForFuture(firebase_auth_.SignInAnonymously(), kAuthErrorFailure);
  EXPECT_FALSE(result.user.is_valid());
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
  AuthResult result =
      WaitForFuture(firebase_auth_.SignInAnonymously(), kAuthErrorFailure);
  EXPECT_FALSE(result.user.is_valid());
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

  AuthResult result = WaitForFuture(firebase_auth_.SignInAnonymously());
  const User user(result.user);
  EXPECT_TRUE(user.is_anonymous());
  EXPECT_EQ("localid123", user.uid());
  EXPECT_EQ("", user.email());
  EXPECT_EQ("", user.display_name());
  EXPECT_EQ("", user.photo_url());
  EXPECT_EQ("Firebase", user.provider_id());
  EXPECT_EQ("", user.phone_number());
  EXPECT_FALSE(user.is_email_verified());
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
  const Future<AuthResult> future = firebase_auth_.SignInWithEmailAndPassword(
      "testsignin@example.com", "testsignin");
  AuthResult result = WaitForFuture(future);
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);
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

  const Future<AuthResult> future =
      firebase_auth_.CreateUserWithEmailAndPassword("testsignin@example.com",
                                                    "testsignin");
  AuthResult result = WaitForFuture(future);
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);
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

  AuthResult result =
      WaitForFuture(firebase_auth_.SignInWithCustomToken("fake_custom_token"));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);
}

// Test Auth::TestSignInWithCredential.

TEST_F(AuthDesktopTest, TestSignInWithCredential_GoogleIdToken) {
  InitializeSuccessfulVerifyAssertionFlow();

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);
}

TEST_F(AuthDesktopTest, TestSignInWithCredential_GoogleAccessToken) {
  InitializeSuccessfulVerifyAssertionFlow();

  id_token_listener.ExpectChanges(2);
  auth_state_listener.ExpectChanges(2);

  const Credential credential =
      GoogleAuthProvider::GetCredential("", "fake_access_token");
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);
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
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential),
      kAuthErrorFailure);
  EXPECT_FALSE(result.user.is_valid());
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
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential),
      kAuthErrorFailure);
  EXPECT_FALSE(result.user.is_valid());
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
  WaitForFuture(firebase_auth_.SignInWithCredential(credential),
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
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);

  EXPECT_STREQ("github.com", result.additional_user_info.provider_id.c_str());
  EXPECT_STREQ("fake_user_name", result.additional_user_info.user_name.c_str());

  const auto found_str_value =
      result.additional_user_info.profile.find(Variant("some_str_key"));
  EXPECT_NE(found_str_value, result.additional_user_info.profile.end());
  EXPECT_STREQ("some_value", found_str_value->second.string_value());

  const auto found_num_value =
      result.additional_user_info.profile.find(Variant("some_num_key"));
  EXPECT_NE(found_num_value, result.additional_user_info.profile.end());
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
  AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);

  EXPECT_EQ("twitter.com", result.additional_user_info.provider_id);
  EXPECT_EQ("fake_user_name", result.additional_user_info.user_name);
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
  const AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);

  EXPECT_EQ("github.com", result.additional_user_info.provider_id);
  EXPECT_THAT(result.additional_user_info.profile, IsEmpty());
  EXPECT_THAT(result.additional_user_info.user_name, IsEmpty());
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
  const AuthResult result = WaitForFuture(
      firebase_auth_.SignInAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(result.user.is_anonymous());
  VerifyUser(result.user);

  EXPECT_EQ("twitter.com", result.additional_user_info.provider_id);
  EXPECT_THAT(result.additional_user_info.user_name, IsEmpty());
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
      firebase_auth_.FetchProvidersForEmail("fake_email@example.com"));
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
      firebase_auth_.SendPasswordResetEmail("fake_email@example.com"));
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

TEST_F(AuthDesktopTest, InvalidAppReturnsInvalidAuth) {
  InitResult init_result = kInitResultSuccess;
  Auth invalid = Auth::GetAuth(App(), &init_result);
  EXPECT_FALSE(invalid.is_valid());
  EXPECT_EQ(kInitResultFailedMissingDependency, init_result);
  EXPECT_FALSE(invalid.app().is_valid());
  EXPECT_FALSE(invalid.current_user().is_valid());
}

TEST_F(AuthDesktopTest, AuthHandleKeepsAppStateAliveAfterAppDropped) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.keepalive");
  options.set_api_key(API_KEY);
  Auth auth;
  std::weak_ptr<internal::AppState> weak_state;
  {
    App app = App::Create(options, "auth_keepalive_app");
    weak_state = internal::AppState::FromApp(app);
    auth = Auth::GetAuth(app);
    ASSERT_TRUE(auth.is_valid());
  }
  // App handle dropped, but Auth handle must keep AppState alive.
  EXPECT_FALSE(weak_state.expired());
  EXPECT_TRUE(auth.is_valid());
  EXPECT_TRUE(auth.app().is_valid());
  EXPECT_STREQ("auth_keepalive_app", auth.app().name());

  AuthResult result = WaitForFuture(auth.SignInAnonymously());
  EXPECT_TRUE(result.user.is_valid());
  EXPECT_TRUE(auth.current_user().is_valid());

  // User copies (from current_user() or AuthResult::user) promote to strong
  // handles so user code can outlive App/Auth, while AuthResult stored in
  // FutureManager keeps its internal User weak to avoid reference cycles.
  User user = auth.current_user();
  User promoted_from_result = result.user;
  EXPECT_TRUE(user.is_valid());
  EXPECT_TRUE(promoted_from_result.is_valid());
  auth = Auth();
  EXPECT_FALSE(weak_state.expired());
  EXPECT_TRUE(user.is_valid());
  EXPECT_TRUE(promoted_from_result.is_valid());
  EXPECT_FALSE(user.uid().empty());

  user = User();
  promoted_from_result = User();
  // Wait briefly for any transient scheduler worker ref to release.
  for (int i = 0; i < 200 && !weak_state.expired(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  // Even while `result` (holding a weak User) is still alive, AppState tears
  // down cleanly with no cycle, and `result.user` reports invalid safely.
  EXPECT_TRUE(weak_state.expired());
  EXPECT_FALSE(result.user.is_valid());
  EXPECT_EQ("", result.user.uid());
  EXPECT_EQ(kFutureStatusInvalid, result.user.GetToken(false).status());
}

TEST_F(AuthDesktopTest, MultiAppAuthIsolationAndTokenProvider) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.isolation");
  options.set_api_key(API_KEY);
  App app1 = App::Create(options, "auth_iso_1");
  App app2 = App::Create(options, "auth_iso_2");
  Auth auth1 = Auth::GetAuth(app1);
  Auth auth2 = Auth::GetAuth(app2);
  EXPECT_NE(auth1, auth2);

  auto state1 = internal::AppState::FromApp(app1);
  auto state2 = internal::AppState::FromApp(app2);
  auto* provider1 = state1->auth_token_provider();
  auto* provider2 = state2->auth_token_provider();
  ASSERT_NE(nullptr, provider1);
  ASSERT_NE(nullptr, provider2);
  EXPECT_NE(provider1, provider2);

  std::atomic<int> callback_count{0};
  auto subscription = state1->SubscribeAuthState(
      [&callback_count]() { callback_count.fetch_add(1); });

  WaitForFuture(auth1.SignInAnonymously());
  EXPECT_TRUE(auth1.current_user().is_valid());
  EXPECT_FALSE(auth2.current_user().is_valid());
  EXPECT_FALSE(provider1->GetCurrentUserUid().empty());
  EXPECT_TRUE(provider2->GetCurrentUserUid().empty());
  EXPECT_FALSE(provider1->GetCurrentToken().empty());
  EXPECT_GT(callback_count.load(), 0);

  subscription.Reset();
  auth1.SignOut();
  { SleepUponDestruction sleep_for_listeners; }
}

TEST_F(AuthDesktopTest, DropLastAuthHandleInsideOnCompletionDoesNotDeadlock) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.drop_in_cb");
  options.set_api_key(API_KEY);

  std::weak_ptr<internal::AppState> weak_state;
  auto auth_holder = std::make_shared<Auth>();
  {
    App app = App::Create(options, "auth_drop_in_cb_app");
    weak_state = internal::AppState::FromApp(app);
    *auth_holder = Auth::GetAuth(app);
    ASSERT_TRUE(auth_holder->is_valid());
  }

  std::mutex done_mutex;
  std::condition_variable done_cv;
  bool callback_ran = false;

  Future<AuthResult> future = auth_holder->SignInAnonymously();
  future.OnCompletion([auth_holder, &done_mutex, &done_cv,
                       &callback_ran](const Future<AuthResult>& completed) {
    EXPECT_EQ(kFutureStatusComplete, completed.status());
    // Drop the last owner of AppState directly inside OnCompletion.
    *auth_holder = Auth();
    {
      std::lock_guard<std::mutex> lock(done_mutex);
      callback_ran = true;
    }
    done_cv.notify_one();
  });
  auth_holder.reset();

  {
    std::unique_lock<std::mutex> lock(done_mutex);
    ASSERT_TRUE(done_cv.wait_for(lock, std::chrono::seconds(10),
                                 [&callback_ran]() { return callback_ran; }));
  }
  for (int i = 0; i < 200 && !weak_state.expired(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_TRUE(weak_state.expired());
  EXPECT_NE(kFutureStatusPending, future.status());
}

TEST_F(AuthDesktopTest, MovedFromAndUniformHandleOperators) {
  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.auth_ops");
  options.set_api_key(API_KEY);
  App app = App::Create(options, "auth_ops_app");
  Auth auth1 = Auth::GetAuth(app);
  Auth auth2 = auth1;
  EXPECT_TRUE(static_cast<bool>(auth1));
  EXPECT_EQ(auth1, auth2);
  EXPECT_FALSE(auth1 != auth2);

  Auth moved_auth(std::move(auth2));
  EXPECT_FALSE(static_cast<bool>(auth2));  // NOLINT
  EXPECT_FALSE(auth2.is_valid());          // NOLINT
  EXPECT_NE(moved_auth, auth2);            // NOLINT
  EXPECT_FALSE(auth2.app().is_valid());    // NOLINT
  EXPECT_FALSE(auth2.current_user());      // NOLINT

  PhoneAuthProvider provider1 = PhoneAuthProvider::GetInstance(moved_auth);
  PhoneAuthProvider provider2 = PhoneAuthProvider::GetInstance(moved_auth);
  EXPECT_TRUE(static_cast<bool>(provider1));
  EXPECT_EQ(provider1, provider2);
  EXPECT_FALSE(provider1 != provider2);

  PhoneAuthProvider default_provider;
  EXPECT_FALSE(static_cast<bool>(default_provider));
  EXPECT_NE(provider1, default_provider);

  User default_user;
  EXPECT_FALSE(static_cast<bool>(default_user));
  EXPECT_FALSE(default_user.is_valid());
  EXPECT_EQ(default_user, User());
}

TEST_F(AuthDesktopTest, StaticDurationAuthSurvivesProcessExit) {
  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.static_auth");
  options.set_api_key(API_KEY);
  static Auth static_auth = [&options]() {
    App app = App::Create(options, "static_auth_app");
    return Auth::GetAuth(app);
  }();
  EXPECT_TRUE(static_auth.is_valid());
}

TEST_F(AuthDesktopTest,
       ConcurrentRemoveAuthStateListenerBlocksAndAllowsSameThreadReentry) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.listener_drain");
  options.set_api_key(API_KEY);
  App app = App::Create(options, "auth_listener_drain_app");
  Auth auth = Auth::GetAuth(app);
  ASSERT_TRUE(auth.is_valid());
  // Ensure initial persistent-cache load has completed so the next listener
  // notification comes from SignInAnonymously on the scheduler thread.
  (void)auth.current_user();

  std::promise<void> entered_promise;
  std::future<void> entered_future = entered_promise.get_future();
  std::promise<void> release_promise;
  std::shared_future<void> release_future =
      release_promise.get_future().share();
  std::atomic<bool> callback_exited{false};
  std::atomic<int> calls{0};

  struct BlockingListener : public AuthStateListener {
    std::promise<void>* entered;
    std::shared_future<void> release;
    std::atomic<bool>* exited;
    std::atomic<int>* calls;
    int magic = 0x12345678;

    void OnAuthStateChanged(Auth auth_handle) override {
      if (!auth_handle.current_user().is_valid()) return;
      if (calls->fetch_add(1) == 0) {
        entered->set_value();
        release.wait();
        EXPECT_EQ(magic, 0x12345678);
        exited->store(true, std::memory_order_release);
      }
    }
  };

  auto listener = std::make_unique<BlockingListener>();
  listener->entered = &entered_promise;
  listener->release = release_future;
  listener->exited = &callback_exited;
  listener->calls = &calls;
  auth.AddAuthStateListener(listener.get());

  Future<AuthResult> sign_in_future = auth.SignInAnonymously();
  ASSERT_EQ(entered_future.wait_for(std::chrono::seconds(10)),
            std::future_status::ready);

  std::atomic<bool> remove_finished{false};
  std::thread remover([&]() {
    auth.RemoveAuthStateListener(listener.get());
    listener.reset();
    remove_finished.store(true, std::memory_order_release);
  });

  // RemoveAuthStateListener must remain blocked while OnAuthStateChanged runs
  // on the scheduler thread.
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  EXPECT_FALSE(remove_finished.load(std::memory_order_acquire));

  release_promise.set_value();
  remover.join();
  EXPECT_TRUE(callback_exited.load(std::memory_order_acquire));
  EXPECT_TRUE(remove_finished.load(std::memory_order_acquire));
  WaitForFuture(sign_in_future);

  // Part 2: Same-thread re-entrant RemoveAuthStateListener inside
  // OnAuthStateChanged must not deadlock.
  struct SelfRemovingListener : public AuthStateListener {
    std::atomic<int> count{0};
    void OnAuthStateChanged(Auth auth_handle) override {
      count.fetch_add(1);
      auth_handle.RemoveAuthStateListener(this);
    }
  };
  SelfRemovingListener self_remover;
  auth.AddAuthStateListener(&self_remover);
  EXPECT_EQ(self_remover.count.load(), 1);
}

TEST_F(AuthDesktopTest, AuthStateListenerDroppingLastHandleDuringSignInNoUaf) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             "\"idToken\": \"idtoken123\","
                             "\"refreshToken\": \"refreshtoken123\","
                             "\"expiresIn\": \"3600\","
                             "\"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = CreateGetAccountInfoFake();
  InitializeConfigWithFakes(fakes);

  AppOptions options = testing::MockAppOptions();
  options.set_app_id("com.firebase.listener_drop_last");
  options.set_api_key(API_KEY);

  std::weak_ptr<internal::AppState> weak_state;
  auto auth_holder = std::make_shared<Auth>();
  {
    App app = App::Create(options, "auth_listener_drop_last_app");
    weak_state = internal::AppState::FromApp(app);
    *auth_holder = Auth::GetAuth(app);
    ASSERT_TRUE(auth_holder->is_valid());
    (void)auth_holder->current_user();
  }

  struct DroppingListener : public AuthStateListener {
    std::shared_ptr<Auth> holder;
    std::atomic<bool> dropped{false};
    void OnAuthStateChanged(Auth auth_handle) override {
      if (auth_handle.current_user().is_valid()) {
        holder.reset();
        dropped.store(true, std::memory_order_release);
      }
    }
  };
  DroppingListener listener;
  listener.holder = auth_holder;
  auth_holder->AddAuthStateListener(&listener);

  Future<AuthResult> sign_in_future = auth_holder->SignInAnonymously();
  auth_holder.reset();

  // Wait for completion; CallAsync pins AuthData across listener notification
  // and post-notification promise completion so dropping the last external
  // handle in OnAuthStateChanged cannot UAF AuthData or Promise.
  ASSERT_TRUE(sign_in_future.Wait(10000));
  EXPECT_TRUE(listener.dropped.load(std::memory_order_acquire));
  // Wait briefly for scheduler worker thread to release its local pinned ref.
  for (int i = 0; i < 200 && !weak_state.expired(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_TRUE(weak_state.expired());
}

}  // namespace auth
}  // namespace firebase
