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
#include "app/tests/include/firebase/app_for_testing.h"
#include "auth/src/include/firebase/auth.h"
#include "auth/src/include/firebase/auth/credential.h"

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/ticker.h"

namespace firebase {
namespace auth {

class CredentialTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    firebase_app_ = testing::CreateApp();
    firebase_auth_ = Auth::GetAuth(firebase_app_);
    EXPECT_NE(nullptr, firebase_auth_);
  }

  void TearDown() override {
    firebase::testing::cppsdk::ConfigReset();
    delete firebase_auth_;
    firebase_auth_ = nullptr;
    delete firebase_app_;
    firebase_app_ = nullptr;
  }

  // Helper function to verify the credential result.
  void Verify(const Credential& credential, const char* provider) {
    EXPECT_TRUE(credential.is_valid());
    EXPECT_EQ(provider, credential.provider());
  }

  App* firebase_app_ = nullptr;
  Auth* firebase_auth_ = nullptr;
};

TEST_F(CredentialTest, TestEmailAuthProvider) {
  // Test get credential from email and password.
  Credential credential = EmailAuthProvider::GetCredential("i@email.com", "pw");
  Verify(credential, "password");
}

TEST_F(CredentialTest, TestFacebookAuthProvider) {
  // Test get credential via Facebook.
  Credential credential = FacebookAuthProvider::GetCredential("aFacebookToken");
  Verify(credential, "facebook.com");
}

TEST_F(CredentialTest, TestGithubAuthProvider) {
  // Test get credential via GitHub.
  Credential credential = GitHubAuthProvider::GetCredential("aGitHubToken");
  Verify(credential, "github.com");
}

TEST_F(CredentialTest, TestGoogleAuthProvider) {
  // Test get credential via Google.
  Credential credential = GoogleAuthProvider::GetCredential("red", "blue");
  Verify(credential, "google.com");
}

#if defined(__ANDROID__) || defined(FIREBASE_ANDROID_FOR_DESKTOP)
TEST_F(CredentialTest, TestPlayGamesAuthProvider) {
  // Test get credential via PlayGames.
  Credential credential = PlayGamesAuthProvider::GetCredential("anAuthCode");
  Verify(credential, "playgames.google.com");
}
#endif  // defined(__ANDROID__) || defined(FIREBASE_ANDROID_FOR_DESKTOP)

TEST_F(CredentialTest, TestTwitterAuthProvider) {
  // Test get credential via Twitter.
  Credential credential = TwitterAuthProvider::GetCredential("token", "secret");
  Verify(credential, "twitter.com");
}

TEST_F(CredentialTest, TestOAuthProvider) {
  // Test get credential via OAuth.
  Credential credential = OAuthProvider::GetCredential("u.test", "id", "acc");
  Verify(credential, "u.test");
}

}  // namespace auth
}  // namespace firebase
