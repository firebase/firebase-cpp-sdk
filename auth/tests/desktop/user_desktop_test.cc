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

#include "auth/src/desktop/user_desktop.h"

#include "app/rest/transport_builder.h"
#include "app/rest/transport_curl.h"
#include "app/rest/transport_mock.h"
#include "app/src/include/firebase/app.h"
#include "app/src/mutex.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "auth/src/desktop/auth_desktop.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/user.h"
#include "auth/tests/desktop/fakes.h"
#include "auth/tests/desktop/test_utils.h"
#include "testing/config.h"
#include "testing/ticker.h"
#include "flatbuffers/stl_emulation.h"

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

using ::testing::AnyOf;
using ::testing::IsEmpty;

namespace {

const char* const API_KEY = "MY-FAKE-API-KEY";
// Constant, describing how many times we would like to sleep 1ms to wait
// for loading persistence cache.
const int kWaitForLoadMaxTryout = 500;

void InitializeSignUpFlowFakes() {
  FakeSetT fakes;

  fakes[GetUrlForApi(API_KEY, "signupNewUser")] =
      FakeSuccessfulResponse("SignupNewUserResponse",
                             " \"idToken\": \"idtoken123\","
                             " \"refreshToken\": \"refreshtoken123\","
                             " \"expiresIn\": \"3600\","
                             " \"localId\": \"localid123\"");
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] =
      FakeSuccessfulResponse("GetAccountInfoResponse",
                             " \"users\": ["
                             "  {"
                             "   \"localId\": \"localid123\","
                             "   \"lastLoginAt\": \"123\","
                             "   \"createdAt\": \"456\""
                             "  }"
                             " ]");

  InitializeConfigWithFakes(fakes);
}

std::string GetSingleFakeProvider(const std::string& provider_id) {
  // clang-format off
  return std::string(
      "  {"
      "    \"federatedId\": \"fake_uid\","
      "    \"email\": \"fake_email@example.com\","
      "    \"displayName\": \"fake_display_name\","
      "    \"photoUrl\": \"fake_photo_url\","
      "    \"providerId\": \"") + provider_id + "\","
      "    \"phoneNumber\": \"123123\""
      "  }";
  // clang-format on
}

std::string GetFakeProviderInfo(
    const std::string& provider_id = "fake_provider_id") {
  return std::string("\"providerUserInfo\": [") +
         GetSingleFakeProvider(provider_id) + "]";
}

std::string FakeSetAccountInfoResponse() {
  return FakeSuccessfulResponse(
      "SetAccountInfoResponse",
      std::string("\"localId\": \"fake_local_id\","
                  "\"email\": \"new_fake_email@example.com\","
                  "\"idToken\": \"new_fake_token\","
                  "\"expiresIn\": \"3600\","
                  "\"passwordHash\": \"new_fake_hash\","
                  "\"emailVerified\": false,") +
          GetFakeProviderInfo());
}

std::string FakeSetAccountInfoResponseWithDetails() {
  return FakeSuccessfulResponse(
      "SetAccountInfoResponse",
      std::string("\"localId\": \"fake_local_id\","
                  "\"email\": \"new_fake_email@example.com\","
                  "\"idToken\": \"new_fake_token2\","
                  "\"expiresIn\": \"3600\","
                  "\"passwordHash\": \"new_fake_hash\","
                  "\"displayName\": \"Fake Name\","
                  "\"photoUrl\": \"https://fake_url.com\","
                  "\"emailVerified\": false,") +
          GetFakeProviderInfo());
}

std::string FakeVerifyAssertionResponse() {
  return FakeSuccessfulResponse("VerifyAssertionResponse",
                                "\"isNewUser\": true,"
                                "\"localId\": \"localid123\","
                                "\"idToken\": \"verify_idtoken123\","
                                "\"providerId\": \"google.com\","
                                "\"refreshToken\": \"verify_refreshtoken123\","
                                "\"expiresIn\": \"3600\"");
}

std::string FakeGetAccountInfoResponse() {
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

void InitializeAuthorizeWithProviderFakes(
    const std::string& get_account_info_response) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = get_account_info_response;
  InitializeConfigWithFakes(fakes);
}

void InitializeSuccessfulAuthenticateWithProviderFlow(
    FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
    const std::string& get_account_info_response) {
  InitializeAuthorizeWithProviderFakes(get_account_info_response);
  provider->SetProviderData(GetFakeOAuthProviderData());
  provider->SetAuthHandler(handler);
}

void InitializeSuccessfulAuthenticateWithProviderFlow(
    FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler) {
  InitializeSuccessfulAuthenticateWithProviderFlow(provider, handler,
                                                   CreateGetAccountInfoFake());
}

void VerifyUser(const User& user) {
  EXPECT_EQ("localid123", user.uid());
  EXPECT_EQ("testsignin@example.com", user.email());
  EXPECT_EQ("", user.display_name());
  EXPECT_EQ("", user.photo_url());
  EXPECT_EQ("Firebase", user.provider_id());
  EXPECT_EQ("", user.phone_number());
  EXPECT_FALSE(user.is_email_verified());
}

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

void InitializeSuccessfulVerifyAssertionFlow(
    const std::string& verify_assertion_response) {
  FakeSetT fakes;
  fakes[GetUrlForApi(API_KEY, "verifyAssertion")] = verify_assertion_response;
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = FakeGetAccountInfoResponse();
  InitializeConfigWithFakes(fakes);
}

void InitializeSuccessfulVerifyAssertionFlow() {
  InitializeSuccessfulVerifyAssertionFlow(FakeVerifyAssertionResponse());
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

class UserDesktopTest : public ::testing::Test {
 protected:
  UserDesktopTest() : sem_(0) {}

  void SetUp() override {
    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new rest::TransportMock());
    });
    AppOptions options = testing::MockAppOptions();
    options.set_api_key(API_KEY);
    firebase_app_ = std::unique_ptr<App>(testing::CreateApp(options));
    firebase_auth_ = std::unique_ptr<Auth>(Auth::GetAuth(firebase_app_.get()));

    InitializeSignUpFlowFakes();

    firebase_auth_->AddIdTokenListener(&id_token_listener);
    firebase_auth_->AddAuthStateListener(&auth_state_listener);

    WaitOnLoadPersistence(firebase_auth_->auth_data_);

    // Current user should be updated upon successful anonymous sign-in.
    // Should expect one extra trigger during either listener add after load
    // credential is done, or load finish after listener added, so changed
    // twice.
    id_token_listener.ExpectChanges(2);
    auth_state_listener.ExpectChanges(2);

    Future<User*> future = firebase_auth_->SignInAnonymously();
    while (future.status() == kFutureStatusPending) {
    }
    firebase_user_ = firebase_auth_->current_user();
    EXPECT_NE(nullptr, firebase_user_);

    // Reset listeners before tests are run.
    id_token_listener.VerifyAndReset();
    auth_state_listener.VerifyAndReset();
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

  Future<SignInResult> ProcessLinkWithProviderFlow(
      FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
      bool trigger_link) {
    InitializeSuccessfulAuthenticateWithProviderFlow(provider, handler);
    Future<SignInResult> future = firebase_user_->LinkWithProvider(provider);
    if (trigger_link) {
      handler->TriggerLinkComplete();
    }
    return future;
  }

  Future<SignInResult> ProcessReauthenticateWithProviderFlow(
      FederatedOAuthProvider* provider, OAuthProviderTestHandler* handler,
      bool trigger_reauthenticate) {
    InitializeSuccessfulAuthenticateWithProviderFlow(provider, handler);
    Future<SignInResult> future =
        firebase_user_->ReauthenticateWithProvider(provider);
    if (trigger_reauthenticate) {
      handler->TriggerReauthenticateComplete();
    }
    return future;
  }

  std::unique_ptr<App> firebase_app_;
  std::unique_ptr<Auth> firebase_auth_;
  User* firebase_user_ = nullptr;

  test::IdTokenChangesCounter id_token_listener;
  test::AuthStateChangesCounter auth_state_listener;

  Semaphore sem_;
};

// Test that metadata is correctly being populated and exposed
TEST_F(UserDesktopTest, TestAccountMetadata) {
  EXPECT_EQ(123,
            firebase_auth_->current_user()->metadata().last_sign_in_timestamp);
  EXPECT_EQ(456, firebase_auth_->current_user()->metadata().creation_timestamp);
}

TEST_F(UserDesktopTest, TestGetToken) {
  const auto api_url =
      std::string("https://securetoken.googleapis.com/v1/token?key=") + API_KEY;
  InitializeConfigWithAFake(
      api_url,
      FakeSuccessfulResponse("\"access_token\": \"new accesstoken123\","
                             "\"expires_in\": \"3600\","
                             "\"token_type\": \"Bearer\","
                             "\"refresh_token\": \"new refreshtoken123\","
                             "\"id_token\": \"new idtoken123\","
                             "\"user_id\": \"localid123\","
                             "\"project_id\": \"53101460582\""));

  // Token should change, but user stays the same.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  // Call the function and verify results.
  std::string token = WaitForFuture(firebase_user_->GetToken(false));
  EXPECT_EQ("idtoken123", token);

  // Call again won't change token since it is still valid.
  token = WaitForFuture(firebase_user_->GetToken(false));
  EXPECT_NE("new idtoken123", token);

  // Call again to force refreshing token.
  const std::string new_token = WaitForFuture(firebase_user_->GetToken(true));
  EXPECT_NE(token, new_token);
  EXPECT_EQ("new idtoken123", new_token);
}

TEST_F(UserDesktopTest, TestDelete) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "deleteAccount"),
      FakeSuccessfulResponse("DeleteAccountResponse", ""));

  // Expect logout.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(1);

  EXPECT_FALSE(firebase_user_->uid().empty());
  WaitForFuture(firebase_user_->Delete());
  EXPECT_TRUE(firebase_user_->uid().empty());
}

TEST_F(UserDesktopTest, TestSendEmailVerification) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "getOobConfirmationCode"),
      FakeSuccessfulResponse("GetOobConfirmationCodeResponse",
                             "\"email\": \"fake_email@example.com\""));

  // Sending email shouldn't affect the current user in any way.
  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  WaitForFuture(firebase_user_->SendEmailVerification());
}

TEST_F(UserDesktopTest, TestReload) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "getAccountInfo"),
      FakeSuccessfulResponse(
          "GetAccountInfoResponse",
          std::string("\"users\": ["
                      "  {"
                      "    \"localId\": \"fake_local_id\","
                      "    \"email\": \"fake_email@example.com\","
                      "    \"emailVerified\": false,"
                      "    \"passwordHash\": \"fake_hash\","
                      "    \"passwordUpdatedAt\": 1.509402565E12,"
                      // Note: these values are copied from an actual
                      // backend response, so it seems that backend uses
                      // seconds for validSince but microseconds for the
                      // other time fields.
                      "    \"validSince\": \"1509402565\","
                      "    \"lastLoginAt\": \"1509402565000\","
                      "    \"createdAt\": \"1509402565000\",") +
              GetFakeProviderInfo() +
              "  }"
              "]"));

  // User stayed the same, and GetAccountInfoResponse doesn't contain tokens.
  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  WaitForFuture(firebase_user_->Reload());
  VerifyProviderData(*firebase_user_);
}

// Tests the happy case of setting a new email on the currently logged in user.
TEST_F(UserDesktopTest, TestUpdateEmail) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  // SetAccountInfoResponse contains a new token.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const std::string new_email = "new_fake_email@example.com";

  EXPECT_NE(new_email, firebase_user_->email());
  WaitForFuture(firebase_user_->UpdateEmail(new_email.c_str()));
  EXPECT_EQ(new_email, firebase_user_->email());
  VerifyProviderData(*firebase_user_);
}

// Tests the happy case of setting a new password on the currently logged in
// user.
TEST_F(UserDesktopTest, TestUpdatePassword) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  // SetAccountInfoResponse contains a new token.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  WaitForFuture(firebase_user_->UpdatePassword("new_password"));
  VerifyProviderData(*firebase_user_);
}

// Tests the happy case of setting new profile properties (display name and
// photo URL) on the currently logged in user.
TEST_F(UserDesktopTest, TestUpdateProfile_Update) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponseWithDetails());

  // SetAccountInfoResponse contains a new token.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const std::string display_name = "Fake Name";
  const std::string photo_url = "https://fake_url.com";
  User::UserProfile profile;
  profile.display_name = display_name.c_str();
  profile.photo_url = photo_url.c_str();

  EXPECT_NE(display_name, firebase_user_->display_name());
  EXPECT_NE(photo_url, firebase_user_->photo_url());
  WaitForFuture(firebase_user_->UpdateUserProfile(profile));
  EXPECT_EQ(display_name, firebase_user_->display_name());
  EXPECT_EQ(photo_url, firebase_user_->photo_url());
  VerifyProviderData(*firebase_user_);
}

// Tests the happy case of deleting profile properties from the currently logged
// in user (setting display name and photo URL to be blank).
TEST_F(UserDesktopTest, TestUpdateProfile_Delete) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponseWithDetails());

  const std::string display_name = "Fake Name";
  const std::string photo_url = "https://fake_url.com";
  User::UserProfile profile;
  profile.display_name = display_name.c_str();
  profile.photo_url = photo_url.c_str();

  WaitForFuture(firebase_user_->UpdateUserProfile(profile));
  EXPECT_EQ(display_name, firebase_user_->display_name());
  EXPECT_EQ(photo_url, firebase_user_->photo_url());

  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  User::UserProfile blank_profile;
  blank_profile.display_name = blank_profile.photo_url = "";
  WaitForFuture(firebase_user_->UpdateUserProfile(blank_profile));
  EXPECT_TRUE(firebase_user_->display_name().empty());
  EXPECT_TRUE(firebase_user_->photo_url().empty());
}

// Tests the happy case of unlinking a provider from the currently logged in
// user.
TEST_F(UserDesktopTest, TestUnlink) {
  FakeSetT fakes;
  // So that the user has an associated provider
  fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = FakeGetAccountInfoResponse();
  fakes[GetUrlForApi(API_KEY, "setAccountInfo")] = FakeSetAccountInfoResponse();
  InitializeConfigWithFakes(fakes);

  // SetAccountInfoResponse contains a new token.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  WaitForFuture(firebase_user_->Reload());
  WaitForFuture(firebase_user_->Unlink("fake_provider_id"));
  VerifyProviderData(*firebase_user_);
}

TEST_F(UserDesktopTest, TestUnlink_NonLinkedProvider) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  WaitForFuture(firebase_user_->Unlink("no_such_provider"),
                kAuthErrorNoSuchProvider);
}

TEST_F(UserDesktopTest, TestLinkWithCredential_OauthCredential) {
  InitializeSuccessfulVerifyAssertionFlow();

  // Response contains a new ID token, but user should have stayed the same.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  EXPECT_TRUE(firebase_user_->is_anonymous());
  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  const User* const user =
      WaitForFuture(firebase_user_->LinkWithCredential(credential));
  EXPECT_FALSE(user->is_anonymous());
  VerifyUser(*user);
}

TEST_F(UserDesktopTest, TestLinkWithCredential_EmailCredential) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  // Response contains a new ID token, but user should have stayed the same.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const std::string new_email = "new_fake_email@example.com";

  EXPECT_NE(new_email, firebase_user_->email());

  EXPECT_TRUE(firebase_user_->is_anonymous());
  const Credential credential =
      EmailAuthProvider::GetCredential(new_email.c_str(), "fake_password");
  WaitForFuture(firebase_user_->LinkWithCredential(credential));
  EXPECT_EQ(new_email, firebase_user_->email());
  EXPECT_FALSE(firebase_user_->is_anonymous());
}

TEST_F(UserDesktopTest, TestLinkWithCredential_NeedsConfirmation) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "verifyAssertion"),
      FakeSuccessfulResponse("verifyAssertion", "\"needConfirmation\": true"));

  // If response contains needConfirmation, the whole operation should fail, and
  // current user should be unaffected.
  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  WaitForFuture(firebase_user_->LinkWithCredential(credential),
                kAuthErrorAccountExistsWithDifferentCredentials);
}

TEST_F(UserDesktopTest, TestLinkWithCredential_ChecksAlreadyLinkedProviders) {
  {
    FakeSetT fakes;
    fakes[GetUrlForApi(API_KEY, "verifyAssertion")] =
        FakeVerifyAssertionResponse();
    fakes[GetUrlForApi(API_KEY, "getAccountInfo")] = FakeSuccessfulResponse(
        // clang-format off
        "GetAccountInfoResponse",
        std::string(
            "\"users\":"
            "  ["
            "    {"
            "      \"localId\": \"localid123\",") +
            GetFakeProviderInfo("google.com") +
            "    }"
            " ]");
    // clang-format on
    InitializeConfigWithFakes(fakes);
  }

  // Upon linking, user should stay the same, but ID token should be updated.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const Credential google_credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  WaitForFuture(firebase_user_->LinkWithCredential(google_credential));

  // The same provider shouldn't be linked twice.
  WaitForFuture(firebase_user_->LinkWithCredential(google_credential),
                kAuthErrorProviderAlreadyLinked);

  id_token_listener.VerifyAndReset();
  auth_state_listener.VerifyAndReset();
  // Linking already linked provider, should fail, so current user shouldn't be
  // updated at all.
  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  {
    FakeSetT fakes;
    fakes[GetUrlForApi(API_KEY, "verifyAssertion")] =
        FakeVerifyAssertionResponse();
    fakes[GetUrlForApi(API_KEY, "getAccountInfo")] =
        // clang-format off
        FakeSuccessfulResponse("GetAccountInfoResponse",
          std::string(
              "\"users\":"
              "  ["
              "    {"
              "       \"localId\": \"localid123\","
              "      \"providerUserInfo\": [") +
              GetSingleFakeProvider("google.com") + "," +
              GetSingleFakeProvider("facebook.com") +
              "      ]"
              "    }"
              " ]");
    // clang-format on
    InitializeConfigWithFakes(fakes);
  }

  // Should be able to link a different provider.
  const Credential facebook_credential =
      FacebookAuthProvider::GetCredential("fake_access_token");
  WaitForFuture(firebase_user_->LinkWithCredential(facebook_credential));

  // The same provider shouldn't be linked twice.
  WaitForFuture(firebase_user_->LinkWithCredential(facebook_credential),
                kAuthErrorProviderAlreadyLinked);
  // Check that the previously linked provider wasn't overridden.
  WaitForFuture(firebase_user_->LinkWithCredential(google_credential),
                kAuthErrorProviderAlreadyLinked);
}

TEST_F(UserDesktopTest, TestLinkWithCredentialAndRetrieveData) {
  InitializeSuccessfulVerifyAssertionFlow();

  // Upon linking, user should stay the same, but ID token should be updated.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  const SignInResult sign_in_result = WaitForFuture(
      firebase_user_->LinkAndRetrieveDataWithCredential(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);
}

TEST_F(UserDesktopTest, TestReauthenticate) {
  InitializeSuccessfulVerifyAssertionFlow();

  // Upon reauthentication, user should have stayed the same, but ID token
  // should have changed.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  WaitForFuture(firebase_user_->Reauthenticate(credential));
}

TEST_F(UserDesktopTest, TestReauthenticate_NeedsConfirmation) {
  InitializeConfigWithAFake(
      GetUrlForApi(API_KEY, "verifyAssertion"),
      FakeSuccessfulResponse("verifyAssertion", "\"needConfirmation\": true"));

  // If response contains needConfirmation, the whole operation should fail, and
  // current user should be unaffected.
  id_token_listener.ExpectChanges(0);
  auth_state_listener.ExpectChanges(0);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  WaitForFuture(firebase_user_->Reauthenticate(credential),
                kAuthErrorAccountExistsWithDifferentCredentials);
}

TEST_F(UserDesktopTest, TestReauthenticateAndRetrieveData) {
  InitializeSuccessfulVerifyAssertionFlow();

  // Upon reauthentication, user should have stayed the same, but ID token
  // should have changed.
  id_token_listener.ExpectChanges(1);
  auth_state_listener.ExpectChanges(0);

  const Credential credential =
      GoogleAuthProvider::GetCredential("fake_id_token", "");
  const SignInResult sign_in_result =
      WaitForFuture(firebase_user_->ReauthenticateAndRetrieveData(credential));
  EXPECT_FALSE(sign_in_result.user->is_anonymous());
  VerifyUser(*sign_in_result.user);
}

// Checks that current user is signed out upon receiving errors from the
// backend indicating the user is no longer valid.
class UserDesktopTestSignOutOnError : public UserDesktopTest {
 protected:
  // Reduces boilerplate in similar tests checking for sign out in several API
  // methods.
  template <typename OperationT>
  void CheckSignOutIfUserIsInvalid(const std::string& api_endpoint,
                                   const std::string& backend_error,
                                   const AuthError sdk_error,
                                   const OperationT operation) {
    // Receiving error from the backend should make the operation fail, and
    // current user shouldn't be affected.
    id_token_listener.ExpectChanges(0);
    auth_state_listener.ExpectChanges(0);

    // First check that sign out doesn't happen on just any error
    // (kAuthErrorOperationNotAllowed is chosen arbitrarily).
    InitializeConfigWithAFake(api_endpoint,
                              CreateErrorHttpResponse("OPERATION_NOT_ALLOWED"));
    EXPECT_FALSE(firebase_user_->uid().empty());
    WaitForFuture(operation(), kAuthErrorOperationNotAllowed);
    EXPECT_FALSE(firebase_user_->uid().empty());  // User is still signed in.

    id_token_listener.VerifyAndReset();
    auth_state_listener.VerifyAndReset();
    // Expect sign out.
    id_token_listener.ExpectChanges(1);
    auth_state_listener.ExpectChanges(1);

    // Now check that the user will be logged out upon receiving a certain
    // error from the backend.
    InitializeConfigWithAFake(api_endpoint,
                              CreateErrorHttpResponse(backend_error));
    WaitForFuture(operation(), sdk_error);
    EXPECT_THAT(firebase_user_->uid(), IsEmpty());
  }
};

TEST_F(UserDesktopTestSignOutOnError, Reauth) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "verifyAssertion"), "USER_NOT_FOUND",
      kAuthErrorUserNotFound, [&] {
        sem_.Post();
        return firebase_user_->Reauthenticate(
            GoogleAuthProvider::GetCredential("fake_id_token", ""));
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, Reload) {
  CheckSignOutIfUserIsInvalid(GetUrlForApi(API_KEY, "getAccountInfo"),
                              "USER_NOT_FOUND", kAuthErrorUserNotFound, [&] {
                                sem_.Post();
                                return firebase_user_->Reload();
                              });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, UpdateEmail) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "setAccountInfo"), "USER_NOT_FOUND",
      kAuthErrorUserNotFound, [&] {
        sem_.Post();
        return firebase_user_->UpdateEmail("fake_email@example.com");
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, UpdatePassword) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "setAccountInfo"), "USER_DISABLED",
      kAuthErrorUserDisabled, [&] {
        sem_.Post();
        return firebase_user_->UpdatePassword("fake_password");
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, UpdateProfile) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "setAccountInfo"), "TOKEN_EXPIRED",
      kAuthErrorUserTokenExpired, [&] {
        sem_.Post();
        return firebase_user_->UpdateUserProfile(User::UserProfile());
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, Unlink) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "getAccountInfo"),
                            FakeGetAccountInfoResponse());
  WaitForFuture(firebase_user_->Reload());

  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "setAccountInfo"), "USER_NOT_FOUND",
      kAuthErrorUserNotFound, [&] {
        sem_.Post();
        return firebase_user_->Unlink("fake_provider_id");
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, LinkWithEmail) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "setAccountInfo"), "USER_NOT_FOUND",
      kAuthErrorUserNotFound, [&] {
        sem_.Post();
        return firebase_user_->LinkWithCredential(
            EmailAuthProvider::GetCredential("fake_email@example.com",
                                             "fake_password"));
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, LinkWithOauthCredential) {
  CheckSignOutIfUserIsInvalid(
      GetUrlForApi(API_KEY, "verifyAssertion"), "USER_NOT_FOUND",
      kAuthErrorUserNotFound, [&] {
        sem_.Post();
        return firebase_user_->LinkWithCredential(
            GoogleAuthProvider::GetCredential("fake_id_token", ""));
      });
  sem_.Wait();
}

TEST_F(UserDesktopTestSignOutOnError, GetToken) {
  const auto api_url =
      std::string("https://securetoken.googleapis.com/v1/token?key=") + API_KEY;
  CheckSignOutIfUserIsInvalid(api_url, "USER_NOT_FOUND", kAuthErrorUserNotFound,
                              [&] {
                                sem_.Post();
                                return firebase_user_->GetToken(true);
                              });
  sem_.Wait();
}

// This test is to expose potential race condition and is primarily intended to
// be run with --config=tsan
TEST_F(UserDesktopTest, TestRaceCondition_SetAccountInfoAndSignOut) {
  InitializeConfigWithAFake(GetUrlForApi(API_KEY, "setAccountInfo"),
                            FakeSetAccountInfoResponse());

  // SignOut is engaged on the main thread, whereas UpdateEmail will be executed
  // on the background thread; consequently, the order in which they are
  // executed is not defined. Nevertheless, this should not lead to any data
  // corruption, when UpdateEmail writes to user profile while it's being
  // deleted by SignOut. Whichever method succeeds first, user must be signed
  // out once both are finished: if SignOut finishes last, it overrides the
  // updated user, and if UpdateEmail finishes last, it should note that there
  // is no currently signed in user and fail with kAuthErrorUserNotFound.

  auto future = firebase_user_->UpdateEmail("some_email");
  firebase_auth_->SignOut();
  while (future.status() == firebase::kFutureStatusPending) {
  }

  EXPECT_THAT(future.error(), AnyOf(kAuthErrorNone, kAuthErrorNoSignedInUser));
  EXPECT_EQ(nullptr, firebase_auth_->current_user());
}

// LinkWithProvider tests.
TEST_F(UserDesktopTest, TestLinkWithProviderReturnsUnsupportedError) {
  FederatedOAuthProvider provider;
  Future<SignInResult> future = firebase_user_->LinkWithProvider(&provider);
  EXPECT_EQ(future.result()->user, nullptr);
  EXPECT_EQ(future.error(), kAuthErrorUnimplemented);
  EXPECT_EQ(std::string(future.error_message()),
            "Operation is not supported on non-mobile systems.");
}

// TODO(drsanta) The following tests are disabled as the AuthHandler support has
// not yet been released.
TEST_F(UserDesktopTest,
       DISABLED_TestLinkWithProviderAndHandlerPassingIntegrityChecks) {
  FederatedOAuthProvider provider;
  test::OAuthProviderTestHandler handler(/*extra_integrity_checks_=*/true);
  InitializeSuccessfulAuthenticateWithProviderFlow(&provider, &handler);

  Future<SignInResult> future = firebase_user_->LinkWithProvider(&provider);
  handler.TriggerLinkComplete();
  SignInResult sign_in_result = WaitForFuture(future);
}

TEST_F(UserDesktopTest,
       DISABLED_TestPendingLinkWithProviderSecondConcurrentSignInFails) {
  FederatedOAuthProvider provider1;
  OAuthProviderTestHandler handler1;
  InitializeSuccessfulAuthenticateWithProviderFlow(&provider1, &handler1);

  FederatedOAuthProvider provider2;
  provider2.SetProviderData(GetFakeOAuthProviderData());

  OAuthProviderTestHandler handler2;
  provider2.SetAuthHandler(&handler2);
  Future<SignInResult> future1 = firebase_user_->LinkWithProvider(&provider1);
  EXPECT_EQ(future1.status(), kFutureStatusPending);
  Future<SignInResult> future2 = firebase_user_->LinkWithProvider(&provider2);
  VerifySignInResult(future2, kAuthErrorFederatedProviderAreadyInUse);
  handler1.TriggerLinkComplete();
  const SignInResult sign_in_result = WaitForFuture(future1);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkWithProviderSignInResultUserPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  FederatedAuthProvider::AuthenticatedUserData user_data =
      *(handler.GetAuthenticatedUserData());
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
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

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullUIDFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->uid = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullDisplayNamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->display_name = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullUsernamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->user_name = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullPhotoUrlPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->photo_url = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullProvderIdFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->provider_id = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest,
       DISABLED_TestLinkCompleteNuDISABLED_llAccessTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->access_token = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullRefreshTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->refresh_token = nullptr;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteExpiresInMaxUInt64Passes) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->token_expires_in_seconds = ULONG_MAX;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteErrorMessagePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/false);
  const char* error_message = "oh nos!";
  handler.TriggerLinkCompleteWithError(kAuthErrorApiNotAvailable,
                                       error_message);
  VerifySignInResult(future, kAuthErrorApiNotAvailable, error_message);
}

TEST_F(UserDesktopTest, DISABLED_TestLinkCompleteNullErrorMessageFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future =
      ProcessLinkWithProviderFlow(&provider, &handler, /*trigger_link=*/false);
  handler.TriggerLinkCompleteWithError(kAuthErrorApiNotAvailable, nullptr);
  VerifySignInResult(future, kAuthErrorApiNotAvailable);
}

// ReauthenticateWithProvider tests.
TEST_F(UserDesktopTest, TestReauthentciateWithProviderReturnsUnsupportedError) {
  FederatedOAuthProvider provider;
  Future<SignInResult> future =
      firebase_user_->ReauthenticateWithProvider(&provider);
  EXPECT_EQ(future.result()->user, nullptr);
  EXPECT_EQ(future.error(), kAuthErrorUnimplemented);
  EXPECT_EQ(std::string(future.error_message()),
            "Operation is not supported on non-mobile systems.");
}

// TODO(drsanta) The following tests are disabled as the AuthHandler support has
// not yet been released.
TEST_F(
    UserDesktopTest,
    DISABLED_TestReauthenticateWithProviderAndHandlerPassingIntegrityChecks) {
  FederatedOAuthProvider provider;
  test::OAuthProviderTestHandler handler(/*extra_integrity_checks_=*/true);
  InitializeSuccessfulAuthenticateWithProviderFlow(&provider, &handler);

  Future<SignInResult> future =
      firebase_user_->ReauthenticateWithProvider(&provider);
  handler.TriggerReauthenticateComplete();
  SignInResult sign_in_result = WaitForFuture(future);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateWithProviderSecondConcurrentSignInFails) {
  FederatedOAuthProvider provider1;
  OAuthProviderTestHandler handler1;
  InitializeSuccessfulAuthenticateWithProviderFlow(&provider1, &handler1);

  FederatedOAuthProvider provider2;
  provider2.SetProviderData(GetFakeOAuthProviderData());

  OAuthProviderTestHandler handler2;
  provider2.SetAuthHandler(&handler2);
  Future<SignInResult> future1 =
      firebase_user_->ReauthenticateWithProvider(&provider1);
  EXPECT_EQ(future1.status(), kFutureStatusPending);
  Future<SignInResult> future2 =
      firebase_user_->ReauthenticateWithProvider(&provider2);
  VerifySignInResult(future2, kAuthErrorFederatedProviderAreadyInUse);
  handler1.TriggerReauthenticateComplete();
  const SignInResult sign_in_result = WaitForFuture(future1);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateWithProviderSignInResultUserPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  FederatedAuthProvider::AuthenticatedUserData user_data =
      *(handler.GetAuthenticatedUserData());
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
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

TEST_F(UserDesktopTest, DISABLED_TestReauthenticateCompleteNullUIDFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->uid = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateCompleteNullDisplayNamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->display_name = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestReauthenticateCompleteNullUsernamePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->user_name = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestReauthenticateCompleteNullPhotoUrlPasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->photo_url = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestReauthenticateCompleteNullProvderIdFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->provider_id = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateCompleteNullAccessTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->access_token = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateCompleteNullRefreshTokenFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->refresh_token = nullptr;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  VerifySignInResult(future, kAuthErrorInvalidAuthenticatedUserData);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateCompleteExpiresInMaxUInt64Passes) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  handler.GetAuthenticatedUserData()->token_expires_in_seconds = ULONG_MAX;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/true);
  SignInResult sign_in_result = WaitForFuture(future);
  VerifyProviderData(*sign_in_result.user);
}

TEST_F(UserDesktopTest, DISABLED_TestReauthenticateCompleteErrorMessagePasses) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/false);
  const char* error_message = "oh nos!";
  handler.TriggerReauthenticateCompleteWithError(kAuthErrorApiNotAvailable,
                                                 error_message);
  VerifySignInResult(future, kAuthErrorApiNotAvailable, error_message);
}

TEST_F(UserDesktopTest,
       DISABLED_TestReauthenticateCompleteNullErrorMessageFails) {
  FederatedOAuthProvider provider;
  OAuthProviderTestHandler handler;
  Future<SignInResult> future = ProcessReauthenticateWithProviderFlow(
      &provider, &handler, /*trigger_reauthenticate=*/false);
  handler.TriggerReauthenticateCompleteWithError(kAuthErrorApiNotAvailable,
                                                 nullptr);
  VerifySignInResult(future, kAuthErrorApiNotAvailable);
}

}  // namespace auth
}  // namespace firebase
