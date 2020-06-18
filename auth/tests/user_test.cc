/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>
#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/user.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/ticker.h"

#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
#include "app/rest/transport_builder.h"
#include "app/rest/transport_mock.h"
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)

namespace firebase {
namespace auth {

namespace {

// Wait for the Future completed when necessary. We do not do so for Android nor
// iOS since their test is based on Ticker-based fake. We do not do so for
// desktop stub since its Future completes immediately.
template <typename T>
inline void MaybeWaitForFuture(const Future<T>& future) {
// Desktop developer sdk has a small delay due to async calls.
#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
  // Once REST implementation is in, we should be able to check this. Almost
  // always the return of last-result is ahead of the future completion. But
  // right now, the return of last-result actually happens after future is
  // completed.
  // EXPECT_EQ(firebase::kFutureStatusPending, future.status());
  while (firebase::kFutureStatusPending == future.status()) {
  }
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)
}

const char* const SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE =
    "    {"
    "      fake: 'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
    "setAccountInfo?key=not_a_real_api_key',"
    "      httpresponse: {"
    "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
    "        body: ['{"
    "          \"email\": \"new@email.com\""
    "        }']"
    "      }"
    "    }";

const char* const VERIFY_PASSWORD_SUCCESSFUL_RESPONSE =
    "    {"
    "      fake: 'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
    "verifyPassword?key=not_a_real_api_key',"
    "      httpresponse: {"
    "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
    "        body: ['{"
    "          \"localId\": \"localid123\","
    "          \"email\": \"testsignin@example.com\","
    "          \"idToken\": \"idtoken123\","
    "          \"registered\": true,"
    "          \"refreshToken\": \"refreshtoken123\","
    "          \"expiresIn\": \"3600\""
    "        }']"
    "      }"
    "    }";

const char* const GET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE =
    "    {"
    "      fake: 'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
    "getAccountInfo?key=not_a_real_api_key',"
    "      httpresponse: {"
    "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
    "        body: ['{"
    "          users: [{"
    "            \"localId\": \"localid123\","
    "            \"email\": \"testsignin@example.com\","
    "            \"emailVerified\": false,"
    "            \"passwordHash\": \"abcdefg\","
    "            \"passwordUpdatedAt\": 31415926,"
    "            \"validSince\": \"123\","
    "            \"lastLoginAt\": \"123\","
    "            \"createdAt\": \"123\","
    "           \"providerUserInfo\": ["
    "             {"
    "               \"providerId\": \"provider\","
    "             }"
    "           ]"
    "          }]"
    "        }']"
    "      }"
    "    }";

}  // anonymous namespace

class UserTest : public ::testing::Test {
 protected:
  void SetUp() override {
#if defined(FIREBASE_WAIT_ASYNC_IN_TEST)
    rest::SetTransportBuilder([]() -> flatbuffers::unique_ptr<rest::Transport> {
      return flatbuffers::unique_ptr<rest::Transport>(
          new rest::TransportMock());
    });
#endif  // defined(FIREBASE_WAIT_ASYNC_IN_TEST)

    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet(
        "{"
        "  config:["
        "    {fake:'FirebaseAuth.signInAnonymously',"
        "     futuregeneric:{ticker:0}},"
        "    {fake:'FIRAuth.signInAnonymouslyWithCompletion:',"
        "     futuregeneric:{ticker:0}},"
        "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
        "signupNewUser?key=not_a_real_api_key',"
        "     httpresponse: {"
        "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
        "       body: ['{"
        " \"kind\": \"identitytoolkit#SignupNewUserResponse\","
        " \"idToken\": \"idtoken123\","
        " \"refreshToken\": \"refreshtoken123\","
        " \"expiresIn\": \"3600\","
        " \"localId\": \"localid123\""
        "}',]"
        "     }"
        "    },"
        "    {fake:'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
        "getAccountInfo?key=not_a_real_api_key',"
        "     httpresponse: {"
        "       header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
        "       body: ['{"
        "         \"users\": [{"
        "           \"localId\": \"localid123\""
        "         }]}',"
        "       ]"
        "     }"
        "    }"
        "  ]"
        "}");
    firebase_app_ = testing::CreateApp();
    firebase_auth_ = Auth::GetAuth(firebase_app_);
    Future<User*> result = firebase_auth_->SignInAnonymously();
    MaybeWaitForFuture(result);
    firebase_user_ = firebase_auth_->current_user();
    EXPECT_NE(nullptr, firebase_user_);
  }

  void TearDown() override {
    // We do not own firebase_user_ object. So just assign it to nullptr here.
    firebase_user_ = nullptr;
    delete firebase_auth_;
    firebase_auth_ = nullptr;
    delete firebase_app_;
    firebase_app_ = nullptr;
    // cppsdk needs to be the last thing torn down, because the mocks are still
    // needed for parts of the firebase destructors.
    firebase::testing::cppsdk::ConfigReset();
  }

  // A helper function to verify future result naively: (1) it completed after
  // one ticker and (2) the result has no error. Since most of the function in
  // user delegate the actual logic into the native SDK, this verification is
  // enough for most of the test case unless we implement some logic into the
  // fake, which is not necessary for unit test.
  template <typename T>
  static void Verify(const Future<T> result) {
// Fake Android & iOS implemented the delay. Desktop stub completed immediately.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
    EXPECT_EQ(firebase::kFutureStatusPending, result.status());
    firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
    MaybeWaitForFuture(result);
    EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
    EXPECT_EQ(0, result.error());
  }

  App* firebase_app_ = nullptr;
  Auth* firebase_auth_ = nullptr;
  User* firebase_user_ = nullptr;
};

TEST_F(UserTest, TestGetToken) {
  // Test get sign-in token.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseUser.getIdToken', futuregeneric:{ticker:1}},"
      "    {fake:'FIRUser.getIDTokenForcingRefresh:completion:',"
      "     futuregeneric:{ticker:1}},"
      "    {"
      "      fake: '"
      "https://securetoken.googleapis.com/v1/token?key=not_a_real_api_key',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "        body: ['{"
      "          \"access_token\": \"fake_access_token\","
      "          \"expires_in\": \"3600\","
      "          \"token_type\": \"Bearer\","
      "          \"refresh_token\": \"fake_refresh_token\","
      "          \"id_token\": \"fake_id_token\","
      "          \"user_id\": \"fake_user_id\","
      "          \"project_id\": \"fake_project_id\""
      "        }']"
      "      }"
      "    }"
      "  ]"
      "}");
  Future<std::string> token =
      firebase_user_->GetToken(false /* force_refresh, doesn't matter here */);

  Verify(token);
  EXPECT_FALSE(token.result()->empty());
}

TEST_F(UserTest, TestGetProviderData) {
  // Test get provider data. Right now, most of the sign-in does not have extra
  // data coming from providers.
  const std::vector<UserInfoInterface*>& provider =
      firebase_user_->provider_data();
  EXPECT_TRUE(provider.empty());
}

TEST_F(UserTest, TestUpdateEmail) {
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.updateEmail', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.updateEmail:completion:', futuregeneric:"
          "{ticker:1}},") +
      SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  EXPECT_NE("new@email.com", firebase_user_->email());
  Future<void> result = firebase_user_->UpdateEmail("new@email.com");

// Fake Android & iOS implemented the delay. Desktop stub completed immediately.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  EXPECT_EQ(firebase::kFutureStatusPending, result.status());
  EXPECT_NE("new@email.com", firebase_user_->email());
  firebase::testing::cppsdk::TickerElapse();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  MaybeWaitForFuture(result);
  EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
  EXPECT_EQ(0, result.error());
  EXPECT_EQ("new@email.com", firebase_user_->email());
}

TEST_F(UserTest, TestUpdatePassword) {
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.updatePassword', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.updatePassword:completion:',"
          "     futuregeneric:{ticker:1}},") +
      SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  Future<void> result = firebase_user_->UpdatePassword("1234567");
  Verify(result);
}

TEST_F(UserTest, TestUpdateUserProfile) {
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.updateProfile', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUserProfileChangeRequest."
          "commitChangesWithCompletion:',"
          "     futuregeneric:{ticker:1}},") +
      SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  User::UserProfile profile;
  Future<void> result = firebase_user_->UpdateUserProfile(profile);
  Verify(result);
}

TEST_F(UserTest, TestReauthenticate) {
  // Test reauthenticate.
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.reauthenticate', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.reauthenticateWithCredential:completion:',"
          "     futuregeneric:{ticker:1}},") +
      VERIFY_PASSWORD_SUCCESSFUL_RESPONSE + "," +
      GET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  Future<void> result = firebase_user_->Reauthenticate(
      EmailAuthProvider::GetCredential("i@email.com", "pw"));
  Verify(result);
}

#if !defined(__APPLE__) && !defined(FIREBASE_WAIT_ASYNC_IN_TEST)
TEST_F(UserTest, TestReauthenticateAndRetrieveData) {
  // Test reauthenticate and retrieve data.
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.reauthenticateAndRetrieveData',"
          "     futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.reauthenticateAndRetrieveDataWithCredential:"
          "completion:',"
          "     futuregeneric:{ticker:1}},") +
      VERIFY_PASSWORD_SUCCESSFUL_RESPONSE + "," +
      GET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  Future<SignInResult> result = firebase_user_->ReauthenticateAndRetrieveData(
      EmailAuthProvider::GetCredential("i@email.com", "pw"));
  Verify(result);
}
#endif  // !defined(__APPLE__) && !defined(FIREBASE_WAIT_ASYNC_IN_TEST)

TEST_F(UserTest, TestSendEmailVerification) {
  // Test send email verification.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseUser.sendEmailVerification',"
      "     futuregeneric:{ticker:1}},"
      "    {fake:'FIRUser.sendEmailVerificationWithCompletion:',"
      "     futuregeneric:{ticker:1}},"
      "    {"
      "      fake: 'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "getOobConfirmationCode?key=not_a_real_api_key',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "        body: ['{"
      "          \"kind\": \"identitytoolkit#GetOobConfirmationCodeResponse\","
      "          \"email\": \"fake_email@fake_domain.com\""
      "        }']"
      "      }"
      "    }"
      "  ]"
      "}");
  Future<void> result = firebase_user_->SendEmailVerification();
  Verify(result);
}

TEST_F(UserTest, TestLinkWithCredential) {
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.linkWithCredential', "
          "futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.linkWithCredential:completion:',"
          "     futuregeneric:{ticker:1}},") +
      SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  Future<User*> result = firebase_user_->LinkWithCredential(
      EmailAuthProvider::GetCredential("i@email.com", "pw"));
  Verify(result);
}

#if !defined(__APPLE__) && !defined(FIREBASE_WAIT_ASYNC_IN_TEST)
TEST_F(UserTest, TestLinkAndRetrieveDataWithCredential) {
  // Test link and retrieve data with credential. This calls the same native SDK
  // function as LinkWithCredential().
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseUser.linkWithCredential', futuregeneric:{ticker:1}},"
      "    {fake:'FIRUser.linkAndRetrieveDataWithCredential:completion:',"
      "     futuregeneric:{ticker:1}}"
      "  ]"
      "}");
  Future<SignInResult> result =
      firebase_user_->LinkAndRetrieveDataWithCredential(
          EmailAuthProvider::GetCredential("i@email.com", "pw"));
  Verify(result);
}
#endif  // !defined(__APPLE__) && !defined(FIREBASE_WAIT_ASYNC_IN_TEST)

TEST_F(UserTest, TestUnlink) {
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.unlink', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.unlinkFromProvider:completion:',"
          "     futuregeneric:{ticker:1}},") +
      GET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE + "," +
      SET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  // Mobile wrappers and desktop have different implementations: desktop checks
  // for valid provider before doing the RPC call, while wrappers leave that to
  // platform implementation, which is faked out in the test. To minimize the
  // divergence, for desktop only, first prepare server GetAccountInfo response
  // which contains a provider, and then Reload, to make sure that the given
  // provider ID is valid. For mobile wrappers, this will be a no-op. Use
  // MaybeWaitForFuture because to Reload will return immediately for mobile
  // wrappers, and Verify expects at least a single "tick".
  MaybeWaitForFuture(firebase_user_->Reload());
  Future<User*> result = firebase_user_->Unlink("provider");
  Verify(result);
  // For desktop, the provider must have been removed. For mobile wrappers, the
  // whole flow must have been a no-op, and the provider list was empty to begin
  // with.
  EXPECT_TRUE(firebase_user_->provider_data().empty());
}

TEST_F(UserTest, TestReload) {
  // Test reload.
  const std::string config =
      std::string(
          "{"
          "  config:["
          "    {fake:'FirebaseUser.reload', futuregeneric:{ticker:1}},"
          "    {fake:'FIRUser.reloadWithCompletion:', "
          "futuregeneric:{ticker:1}},") +
      GET_ACCOUNT_INFO_SUCCESSFUL_RESPONSE +
      "  ]"
      "}";
  firebase::testing::cppsdk::ConfigSet(config.c_str());

  Future<void> result = firebase_user_->Reload();
  Verify(result);
}

TEST_F(UserTest, TestDelete) {
  // Test delete.
  firebase::testing::cppsdk::ConfigSet(
      "{"
      "  config:["
      "    {fake:'FirebaseUser.delete', futuregeneric:{ticker:1}},"
      "    {fake:'FIRUser.deleteWithCompletion:', futuregeneric:{ticker:1}},"
      "    {"
      "      fake: 'https://www.googleapis.com/identitytoolkit/v3/relyingparty/"
      "deleteAccount?key=not_a_real_api_key',"
      "      httpresponse: {"
      "        header: ['HTTP/1.1 200 Ok','Server:mock server 101'],"
      "        body: ['{"
      "          \"kind\": \"identitytoolkit#DeleteAccountResponse\""
      "        }']"
      "      }"
      "    }"
      "  ]"
      "}");
  Future<void> result = firebase_user_->Delete();
  Verify(result);
}

TEST_F(UserTest, TestIsEmailVerified) {
  // Test is email verified. Right now both stub and fake will return false
  // unanimously.
  EXPECT_FALSE(firebase_user_->is_email_verified());
}

TEST_F(UserTest, TestIsAnonymous) {
  // Test is anonymous.
  EXPECT_TRUE(firebase_user_->is_anonymous());
}

TEST_F(UserTest, TestGetter) {
// Test getter functions. The fake value are different between stub and fake.
#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  EXPECT_EQ("fake email", firebase_user_->email());
  EXPECT_EQ("fake display name", firebase_user_->display_name());
  EXPECT_EQ("fake provider id", firebase_user_->provider_id());
#else   // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS
  EXPECT_TRUE(firebase_user_->email().empty());
  EXPECT_TRUE(firebase_user_->display_name().empty());
  EXPECT_EQ("Firebase", firebase_user_->provider_id());
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || FIREBASE_PLATFORM_IOS

  EXPECT_FALSE(firebase_user_->uid().empty());
  EXPECT_TRUE(firebase_user_->photo_url().empty());
}
}  // namespace auth
}  // namespace firebase
