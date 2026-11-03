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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
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
  Verify(credential, EmailAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestFacebookAuthProvider) {
  // Test get credential via Facebook.
  Credential credential = FacebookAuthProvider::GetCredential("aFacebookToken");
  Verify(credential, FacebookAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestGithubAuthProvider) {
  // Test get credential via GitHub.
  Credential credential = GitHubAuthProvider::GetCredential("aGitHubToken");
  Verify(credential, GitHubAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestGoogleAuthProvider) {
  // Test get credential via Google.
  Credential credential = GoogleAuthProvider::GetCredential("red", "blue");
  Verify(credential, GoogleAuthProvider::kProviderId);

  Credential id_only = GoogleAuthProvider::GetCredential("red", nullptr);
  Verify(id_only, GoogleAuthProvider::kProviderId);

  Credential access_only = GoogleAuthProvider::GetCredential(nullptr, "blue");
  Verify(access_only, GoogleAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestPlayGamesAuthProvider) {
  // Test get credential via PlayGames.
  Credential credential = PlayGamesAuthProvider::GetCredential("anAuthCode");
  Verify(credential, PlayGamesAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestTwitterAuthProvider) {
  // Test get credential via Twitter.
  Credential credential = TwitterAuthProvider::GetCredential("token", "secret");
  Verify(credential, TwitterAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestOAuthProvider) {
  // Test get credential via OAuth.
  Credential credential = OAuthProvider::GetCredential("u.test", "id", "acc");
  Verify(credential, "u.test");

  // Test 4-argument overload with raw_nonce.
  Credential nonce_credential =
      OAuthProvider::GetCredential("microsoft.com", "id", "raw_nonce", "acc");
  Verify(nonce_credential, "microsoft.com");
}

TEST_F(CredentialTest, TestDefaultCredentialAndCopyMove) {
  Credential invalid_cred;
  EXPECT_FALSE(invalid_cred.is_valid());

  Credential valid_cred = EmailAuthProvider::GetCredential("i@email.com", "pw");
  Credential copied_cred(valid_cred);
  Verify(copied_cred, EmailAuthProvider::kProviderId);

  Credential assigned_cred;
  assigned_cred = valid_cred;
  Verify(assigned_cred, EmailAuthProvider::kProviderId);

  Credential moved_cred(std::move(copied_cred));
  Verify(moved_cred, EmailAuthProvider::kProviderId);
}

TEST_F(CredentialTest, TestFederatedOAuthProviderData) {
  FederatedOAuthProviderData default_data;
  EXPECT_TRUE(default_data.provider_id.empty());

  FederatedOAuthProviderData provider_only("github.com");
  EXPECT_EQ("github.com", provider_only.provider_id);

  FederatedOAuthProviderData full_data(
      "google.com", {"email", "profile"}, {{"prompt", "select_account"}});
  EXPECT_EQ("google.com", full_data.provider_id);
  EXPECT_EQ(2U, full_data.scopes.size());
  EXPECT_EQ("select_account", full_data.custom_parameters["prompt"]);

  FederatedOAuthProviderData copy_data = full_data;
  EXPECT_EQ(full_data, copy_data);
  EXPECT_FALSE(full_data != copy_data);
  EXPECT_NE(full_data, provider_only);
}

}  // namespace auth
}  // namespace firebase
