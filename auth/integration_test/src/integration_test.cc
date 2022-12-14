// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/auth.h"
#include "firebase/auth/credential.h"
#include "firebase/auth/user.h"
#include "firebase/util.h"
#include "firebase/variant.h"
#include "firebase_test_framework.h"  // NOLINT

// The TO_STRING macro is useful for command line defined strings as the quotes
// get stripped.
#define TO_STRING_EXPAND(X) #X
#define TO_STRING(X) TO_STRING_EXPAND(X)

// Path to the Firebase config file to load.
#ifdef FIREBASE_CONFIG
#define FIREBASE_CONFIG_STRING TO_STRING(FIREBASE_CONFIG)
#else
#define FIREBASE_CONFIG_STRING ""
#endif  // FIREBASE_CONFIG

namespace firebase_testapp_automated {

// Set kCustomTestEmail and kCustomTestPassword if you want to test email and
// password login using a custom account you've already set up on your Firebase
// project.
static const char kCustomTestEmail[] = "put_custom_test_account_here@gmail.com";
static const char kCustomTestPassword[] = "";

static const int kWaitIntervalMs = 300;              // NOLINT
static const int kPhoneAuthCodeSendWaitMs = 600000;  // NOLINT
static const int kPhoneAuthCompletionWaitMs = 8000;  // NOLINT
static const int kPhoneAuthTimeoutMs = 0;            // NOLINT

// Set these in Firebase Console for your app.
static const char* kPhoneAuthTestPhoneNumbers[] = {
    "+12345556780", "+12345556781", "+12345556782", "+12345556783",
    "+12345556784", "+12345556785", "+12345556786", "+12345556787",
    "+12345556788", "+12345556789"};                            // NOLINT
static const char kPhoneAuthTestVerificationCode[] = "123456";  // NOLINT
static const int kPhoneAuthTestNumPhoneNumbers =
    sizeof(kPhoneAuthTestPhoneNumbers) / sizeof(kPhoneAuthTestPhoneNumbers[0]);

static const char kTestPassword[] = "testEmailPassword123";
static const char kTestEmailBad[] = "bad.test.email@example.com";
static const char kTestPasswordBad[] = "badTestPassword";
static const char kTestIdTokenBad[] = "bad id token for testing";
static const char kTestAccessTokenBad[] = "bad access token for testing";
static const char kTestPasswordUpdated[] = "testpasswordupdated";
static const char kTestIdProviderIdBad[] = "bad provider id for testing";
static const char kTestServerAuthCodeBad[] = "bad server auth code";  // NOLINT

using app_framework::LogDebug;
using app_framework::LogError;  // NOLINT
using app_framework::LogInfo;
using app_framework::ProcessEvents;

using firebase_test_framework::FirebaseTest;

class FirebaseAuthTest : public FirebaseTest {
 public:
  FirebaseAuthTest();
  ~FirebaseAuthTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App and Firebase Auth.
  void Initialize();
  // Shut down Firebase App and Firebase Auth.
  void Terminate();

  // Sign out of any user we were signed into. This is automatically called
  // before and after every test.
  void SignOut();

  // Delete the current user if it's currently signed in.
  void DeleteUser();

  // Passthrough method to the base class's WaitForCompletion.
  bool WaitForCompletion(firebase::Future<std::string> future, const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone) {
    return FirebaseTest::WaitForCompletion(future, fn, expected_error);
  }

  // Passthrough method to the base class's WaitForCompletion.
  bool WaitForCompletion(firebase::Future<void> future, const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone) {
    return FirebaseTest::WaitForCompletion(future, fn, expected_error);
  }

  // Custom WaitForCompletion that checks if User matches afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::User*> future,
                         const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone);
  // Custom WaitForCompletion that checks if User matches afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::SignInResult> future,
                         const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone);

  // Custom WaitForCompletion that checks if User and Provider ID matches
  // afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::SignInResult> future,
                         const char* fn, const std::string& provider_id);

  bool initialized_;
  firebase::auth::Auth* auth_;
};

FirebaseAuthTest::FirebaseAuthTest() : initialized_(false), auth_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseAuthTest::~FirebaseAuthTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(auth_ == nullptr);
}

void FirebaseAuthTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
  SignOut();
}

void FirebaseAuthTest::TearDown() {
  SignOut();
  Terminate();
  FirebaseTest::TearDown();
}

void FirebaseAuthTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Auth.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, &auth_, [](::firebase::App* app, void* target) {
    LogDebug("Try to initialize Firebase Auth");
    firebase::InitResult result;
    firebase::auth::Auth** auth_ptr =
        reinterpret_cast<firebase::auth::Auth**>(target);
    *auth_ptr = firebase::auth::Auth::GetAuth(app, &result);
    return result;
  });

  FirebaseTest::WaitForCompletion(initializer.InitializeLastResult(),
                                  "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Auth.");

  initialized_ = true;
}

void FirebaseAuthTest::Terminate() {
  if (!initialized_) return;

  if (auth_) {
    LogDebug("Shutdown the Auth library.");
    delete auth_;
    auth_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::User*> future, const char* fn,
    int expected_error) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn, expected_error);

  if (succeeded) {
    if (expected_error == ::firebase::auth::kAuthErrorNone) {
      const firebase::auth::User* future_result_user =
          future.result() ? *future.result() : nullptr;
      const firebase::auth::User* auth_user = auth_->current_user();
      EXPECT_EQ(future_result_user, auth_user)
          << "User returned by Future doesn't match User in Auth";
      return (future_result_user == auth_user);
    }
  }
  return succeeded;
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::SignInResult> future, const char* fn,
    int expected_error) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn, expected_error);

  if (succeeded) {
    if (expected_error == ::firebase::auth::kAuthErrorNone) {
      const firebase::auth::User* future_result_user =
          (future.result() && future.result()->user) ? future.result()->user
                                                     : nullptr;
      const firebase::auth::User* auth_user = auth_->current_user();
      EXPECT_EQ(future_result_user, auth_user)
          << "User returned by Future doesn't match User in Auth";
      return (future_result_user == auth_user);
    }
  }
  return succeeded;
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::SignInResult> future, const char* fn,
    const std::string& provider_id) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn);
  if (succeeded) {
    const firebase::auth::SignInResult* result_ptr = future.result();
    EXPECT_NE(result_ptr->user, nullptr);
    EXPECT_EQ(result_ptr->info.provider_id, provider_id);
  }
  return succeeded;
}

void FirebaseAuthTest::SignOut() {
  if (auth_ == nullptr) {
    // Auth is not set up.
    return;
  }
  if (auth_->current_user() == nullptr) {
    // Already signed out.
    return;
  }
  auth_->SignOut();
  // Wait for the sign-out to finish.
  while (auth_->current_user() != nullptr) {
    if (ProcessEvents(100)) break;
  }
  ProcessEvents(100);
  EXPECT_EQ(auth_->current_user(), nullptr);
}

void FirebaseAuthTest::DeleteUser() {
  if (auth_ != nullptr && auth_->current_user() != nullptr) {
    FirebaseTest::WaitForCompletion(auth_->current_user()->Delete(),
                                    "Delete User");
    ProcessEvents(100);
  }
}

TEST_F(FirebaseAuthTest, TestInitialization) {
  // Initialized in SetUp and terminated in TearDown.
  EXPECT_NE(app_, nullptr);
  EXPECT_NE(auth_, nullptr);
}

TEST_F(FirebaseAuthTest, TestAnonymousSignin) {
  // Test notification on SignIn().
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_NE(auth_->current_user(), nullptr);
  EXPECT_TRUE(auth_->current_user()->is_anonymous());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestCredentialCopy) {
  // --- Credential copy tests -------------------------------------------------
  {
    firebase::auth::Credential email_cred =
        firebase::auth::EmailAuthProvider::GetCredential(kCustomTestEmail,
                                                         kTestPassword);
    firebase::auth::Credential facebook_cred =
        firebase::auth::FacebookAuthProvider::GetCredential(
            kTestAccessTokenBad);

    std::string email_provider = email_cred.provider();
    std::string facebook_provider = facebook_cred.provider();

    // Test copy constructor.
    firebase::auth::Credential cred_copy(email_cred);
    EXPECT_EQ(cred_copy.provider(), email_provider);
    // Test assignment operator.
    cred_copy = facebook_cred;
    EXPECT_EQ(cred_copy.provider(), facebook_provider);
  }
}

class TestAuthStateListener : public firebase::auth::AuthStateListener {
 public:
  virtual void OnAuthStateChanged(firebase::auth::Auth* auth) {  // NOLINT
    // Log the provider ID.
    std::string provider =
        auth->current_user() ? auth->current_user()->provider_id() : "";
    LogDebug("OnAuthStateChanged called, provider=%s", provider.c_str());
    if (auth_states_.empty() || auth_states_.back() != provider) {
      // Only log unique events.
      auth_states_.push_back(provider);
    }
  }
  const std::vector<std::string>& auth_states() { return auth_states_; }

 private:
  std::vector<std::string> auth_states_;
};

class TestIdTokenListener : public firebase::auth::IdTokenListener {
 public:
  virtual void OnIdTokenChanged(firebase::auth::Auth* auth) {  // NOLINT
    // Log the auth token (if available).
    std::string token = "";
    if (auth->current_user()) {
      firebase::Future<std::string> token_future =
          auth->current_user()->GetToken(false);
      if (token_future.status() == firebase::kFutureStatusComplete) {
        if (token_future.error() == 0) {
          token = *token_future.result();
        }
      } else {
        token = "[in progress]";
      }
    }
    LogDebug("OnIdTokenChanged called, token=%s", token.c_str());
    if (token_states_.empty() || !token.empty() ||
        token_states_.back() != token) {
      // Only log unique empty events.
      token_states_.push_back(token);
    }
  }

  const std::vector<std::string>& token_states() { return token_states_; }

 private:
  std::vector<std::string> token_states_;
};

using testing::AnyOf;
using testing::ElementsAre;
using testing::Not;
using testing::StrCaseEq;

TEST_F(FirebaseAuthTest, TestTokensAndAuthStateListeners) {
  TestAuthStateListener listener;
  TestIdTokenListener token_listener;
  auth_->AddAuthStateListener(&listener);
  auth_->AddIdTokenListener(&token_listener);
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  // Get an initial token.
  firebase::Future<std::string> token_future =
      auth_->current_user()->GetToken(false);
  WaitForCompletion(token_future, "GetToken(false)");
  std::string first_token = *token_future.result();
  // Force a token refresh.
  ProcessEvents(1000);
  token_future = auth_->current_user()->GetToken(true);
  WaitForCompletion(token_future, "GetToken(true)");
  EXPECT_NE(*token_future.result(), "");
  std::string second_token = *token_future.result();
  EXPECT_NE(first_token, second_token);

  DeleteUser();
  SignOut();
  auth_->RemoveAuthStateListener(&listener);
  auth_->RemoveIdTokenListener(&token_listener);
  // Providers should be blank, then Firebase, then blank.
  EXPECT_THAT(listener.auth_states(),
              ElementsAre("", StrCaseEq("Firebase"), ""));
  // We should have blank, then two (or sometimes three) tokens, then blank.
  EXPECT_THAT(token_listener.token_states(),
              AnyOf(ElementsAre("", Not(""), Not(""), ""),
                    ElementsAre("", Not(""), Not(""), Not(""), "")));
}

static std::string GenerateEmailAddress() {
  char time_string[22];
  snprintf(time_string, sizeof(time_string), "%lld",
           app_framework::GetCurrentTimeInMicroseconds());
  std::string email = "random_user_";
  email.append(time_string);
  email.append("@gmail.com");
  LogDebug("Generated email address: %s", email.c_str());
  return email;
}

TEST_F(FirebaseAuthTest, TestEmailAndPasswordSignin) {
  std::string email = GenerateEmailAddress();
  // Register a random email and password. This signs us in as that user.
  std::string password = kTestPassword;
  firebase::Future<firebase::auth::User*> create_user =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  // Sign out and log in using SignInWithCredential(EmailCredential).
  SignOut();
  {
    firebase::auth::Credential email_credential =
        firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                         password.c_str());
    WaitForCompletion(auth_->SignInWithCredential(email_credential),
                      "SignInWithCredential");
    EXPECT_NE(auth_->current_user(), nullptr);
  }
  // Sign out and log in using
  // SignInAndRetrieveDataWithCredential(EmailCredential).
  SignOut();
  {
    firebase::auth::Credential email_credential =
        firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                         password.c_str());
    WaitForCompletion(
        auth_->SignInAndRetrieveDataWithCredential(email_credential),
        "SignAndRetrieveDataInWithCredential");
    EXPECT_NE(auth_->current_user(), nullptr);
  }
  SignOut();
  // Sign in with SignInWithEmailAndPassword values.
  firebase::Future<firebase::auth::User*> sign_in_user =
      auth_->SignInWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(sign_in_user, "SignInWithEmailAndPassword");
  ASSERT_NE(auth_->current_user(), nullptr);

  // Then delete the account.
  firebase::Future<void> delete_user = auth_->current_user()->Delete();
  WaitForCompletion(delete_user, "Delete");
  firebase::Future<firebase::auth::User*> invalid_sign_in_user =
      auth_->SignInWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(invalid_sign_in_user,
                    "SignInWithEmailAndPassword (invalid user)",
                    firebase::auth::kAuthErrorUserNotFound);
  EXPECT_EQ(auth_->current_user(), nullptr);
}

TEST_F(FirebaseAuthTest, TestUpdateUserProfile) {
  std::string email = GenerateEmailAddress();
  firebase::Future<firebase::auth::User*> create_user =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword);
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  // Set some user profile properties.
  firebase::auth::User* user = *create_user.result();
  const char kDisplayName[] = "Hello World";
  const char kPhotoUrl[] = "http://example.com/image.jpg";
  firebase::auth::User::UserProfile user_profile;
  user_profile.display_name = kDisplayName;
  user_profile.photo_url = kPhotoUrl;
  firebase::Future<void> update_profile = user->UpdateUserProfile(user_profile);
  WaitForCompletion(update_profile, "UpdateUserProfile");
  EXPECT_EQ(user->display_name(), kDisplayName);
  EXPECT_EQ(user->photo_url(), kPhotoUrl);
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_EQ(user->display_name(), kDisplayName);
  EXPECT_EQ(user->photo_url(), kPhotoUrl);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestUpdateEmailAndPassword) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  ASSERT_NE(auth_->current_user(), nullptr);
  firebase::auth::User* user = auth_->current_user();

  // Update the user's email and password.
  const std::string new_email = "new_" + email;
  WaitForCompletion(user->UpdateEmail(new_email.c_str()), "UpdateEmail");
  WaitForCompletion(user->UpdatePassword(kTestPasswordUpdated),
                    "UpdatePassword");

  firebase::auth::Credential new_email_cred =
      firebase::auth::EmailAuthProvider::GetCredential(new_email.c_str(),
                                                       kTestPasswordUpdated);
  WaitForCompletion(user->Reauthenticate(new_email_cred), "Reauthenticate");
  EXPECT_NE(auth_->current_user(), nullptr);

  WaitForCompletion(user->SendEmailVerification(), "SendEmailVerification");
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithEmailCredential) {
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  firebase::auth::User* user = auth_->current_user();
  std::string email = GenerateEmailAddress();
  firebase::auth::Credential credential =
      firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkAndRetrieveDataWithCredential(credential),
                    "LinkAndRetrieveDataWithCredential");
  WaitForCompletion(user->Unlink(credential.provider().c_str()), "Unlink");
  SignOut();
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_NE(auth_->current_user(), nullptr);
  std::string email1 = GenerateEmailAddress();
  firebase::auth::Credential credential1 =
      firebase::auth::EmailAuthProvider::GetCredential(email1.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkWithCredential(credential1),
                    "LinkWithCredential 1");
  std::string email2 = GenerateEmailAddress();
  firebase::auth::Credential credential2 =
      firebase::auth::EmailAuthProvider::GetCredential(email2.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkWithCredential(credential2),
                    "LinkWithCredential 2",
                    firebase::auth::kAuthErrorProviderAlreadyLinked);
  WaitForCompletion(user->Unlink(credential.provider().c_str()), "Unlink 2");
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithBadCredential) {
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  firebase::auth::User* pre_link_user = auth_->current_user();
  firebase::auth::Credential twitter_cred =
      firebase::auth::TwitterAuthProvider::GetCredential(kTestIdTokenBad,
                                                         kTestAccessTokenBad);
  WaitForCompletion(pre_link_user->LinkWithCredential(twitter_cred),
                    "LinkWithCredential",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that user stays the same.
  EXPECT_EQ(auth_->current_user(), pre_link_user);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadEmailFails) {
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(kTestEmailBad, kTestPassword),
      "SignInWithEmailAndPassword", firebase::auth::kAuthErrorUserNotFound);
  EXPECT_EQ(auth_->current_user(), nullptr);
}

TEST_F(FirebaseAuthTest, TestSignInWithBadPasswordFails) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPasswordBad),
      "SignInWithEmailAndPassword", firebase::auth::kAuthErrorWrongPassword);
  EXPECT_EQ(auth_->current_user(), nullptr);
  SignOut();
  // Sign back in and delete the user.
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestCreateUserWithExistingEmailFails) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword 1");
  EXPECT_NE(auth_->current_user(), nullptr);
  SignOut();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword 2",
      firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_EQ(auth_->current_user(), nullptr);
  SignOut();
  // Try again with a different password.
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPasswordBad),
      "CreateUserWithEmailAndPassword 3",
      firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_EQ(auth_->current_user(), nullptr);
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadCredentials) {
  // Get an anonymous user first.
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  // Hold on to the existing user, to make sure it is unchanged by bad signins.
  firebase::auth::User* existing_user = auth_->current_user();
  // Test signing in with a variety of bad credentials.
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::FacebookAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential (Facebook)",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that failing to sign in with a credential doesn't modify the user.
  EXPECT_EQ(auth_->current_user(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::TwitterAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential (Twitter)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GitHubAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential (GitHub)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential (Google 1)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, nullptr)),
                    "SignInWithCredential (Google 2)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user(), existing_user);
  WaitForCompletion(
      auth_->SignInWithCredential(firebase::auth::OAuthProvider::GetCredential(
          kTestIdProviderIdBad, kTestIdTokenBad, kTestAccessTokenBad)),
      "SignInWithCredential (OAuth)", firebase::auth::kAuthErrorFailure);
  EXPECT_EQ(auth_->current_user(), existing_user);

#if defined(__ANDROID__)
  // Test Play Games sign-in on Android only.
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::PlayGamesAuthProvider::GetCredential(
                            kTestServerAuthCodeBad)),
                    "SignInWithCredential (Play Games)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user(), existing_user);
#endif  // defined(__ANDROID__)
  DeleteUser();
}

#if TARGET_OS_IPHONE
TEST_F(FirebaseAuthTest, TestGameCenterSignIn) {
  // Test Game Center sign-in on iPhone only.
  if (!firebase::auth::GameCenterAuthProvider::IsPlayerAuthenticated()) {
    LogInfo("Not signed into Game Center, skipping test.");
    GTEST_SKIP();
    return;
  }
  LogDebug("Signed in, testing Game Center authentication.");
  firebase::Future<firebase::auth::Credential> credential_future =
      firebase::auth::GameCenterAuthProvider::GetCredential();
  FirebaseTest::WaitForCompletion(credential_future,
                                  "GameCenterAuthProvider::GetCredential()");

  EXPECT_NE(credential_future.result(), nullptr);
  if (credential_future.result()) {
    WaitForCompletion(auth_->SignInWithCredential(*credential_future.result()),
                      "SignInWithCredential (Game Center)");
  }
  DeleteUser();
}
#endif  // TARGET_OS_IPHONE

TEST_F(FirebaseAuthTest, TestSendPasswordResetEmail) {
  // Test Auth::SendPasswordResetEmail().
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  SignOut();
  // Send to correct email.
  WaitForCompletion(auth_->SendPasswordResetEmail(email.c_str()),
                    "SendPasswordResetEmail (good)");
  // Send to incorrect email.
  WaitForCompletion(auth_->SendPasswordResetEmail(kTestEmailBad),
                    "SendPasswordResetEmail (bad)",
                    firebase::auth::kAuthErrorUserNotFound);
  // Delete user now that we are done with it.
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestWithCustomEmailAndPassword) {
  if (strlen(kCustomTestEmail) == 0 || strlen(kCustomTestPassword) == 0) {
    LogInfo(
        "Skipping %s. To enable this test, set "
        "kCustomTestEmail and kCustomTestPassword in integration_test.cc.",
        test_info_->name());
    GTEST_SKIP();
    return;
  }
  firebase::Future<firebase::auth::User*> sign_in_user =
      auth_->SignInWithEmailAndPassword(kCustomTestEmail, kCustomTestPassword);
  WaitForCompletion(sign_in_user, "SignInWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
}

TEST_F(FirebaseAuthTest, TestAuthPersistenceWithAnonymousSignin) {
  // Automated test is disabled on linux due to the need to unlock the keystore.
  SKIP_TEST_ON_LINUX;

  FLAKY_TEST_SECTION_BEGIN();

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_NE(auth_->current_user(), nullptr);
  EXPECT_TRUE(auth_->current_user()->is_anonymous());
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_NE(auth_->current_user(), nullptr);
  EXPECT_TRUE(auth_->current_user()->is_anonymous());
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}
TEST_F(FirebaseAuthTest, TestAuthPersistenceWithEmailSignin) {
  // Automated test is disabled on linux due to the need to unlock the keystore.
  SKIP_TEST_ON_LINUX;

  FLAKY_TEST_SECTION_BEGIN();

  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  EXPECT_FALSE(auth_->current_user()->is_anonymous());
  std::string prev_provider_id = auth_->current_user()->provider_id();
  // Save the old provider ID list so we can make sure it's the same once
  // it's loaded again.
  std::vector<std::string> prev_provider_data_ids;
  for (int i = 0; i < auth_->current_user()->provider_data().size(); i++) {
    prev_provider_data_ids.push_back(
        auth_->current_user()->provider_data()[i]->provider_id());
  }
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_NE(auth_->current_user(), nullptr);
  EXPECT_FALSE(auth_->current_user()->is_anonymous());
  // Make sure the provider IDs are the same as they were before.
  EXPECT_EQ(auth_->current_user()->provider_id(), prev_provider_id);
  std::vector<std::string> loaded_provider_data_ids;
  for (int i = 0; i < auth_->current_user()->provider_data().size(); i++) {
    loaded_provider_data_ids.push_back(
        auth_->current_user()->provider_data()[i]->provider_id());
  }
  EXPECT_TRUE(loaded_provider_data_ids == prev_provider_data_ids);

  // Cleanup, ensure we are signed in as the user so we can delete it.
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_NE(auth_->current_user(), nullptr);
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}

class PhoneListener : public firebase::auth::PhoneAuthProvider::Listener {
 public:
  PhoneListener()
      : on_verification_complete_count_(0),
        on_verification_failed_count_(0),
        on_code_sent_count_(0),
        on_code_auto_retrieval_time_out_count_(0) {}

  void OnVerificationCompleted(firebase::auth::Credential credential) override {
    LogDebug("PhoneListener: successful automatic verification.");
    on_verification_complete_count_++;
    credential_ = credential;
  }

  void OnVerificationFailed(const std::string& error) override {
    LogError("PhoneListener verification failed with error, %s", error.c_str());
    on_verification_failed_count_++;
  }

  void OnCodeSent(const std::string& verification_id,
                  const firebase::auth::PhoneAuthProvider::ForceResendingToken&
                      force_resending_token) override {
    LogDebug("PhoneListener: code sent. verification_id=%s",
             verification_id.c_str());
    verification_id_ = verification_id;
    force_resending_token_ = force_resending_token;
    on_code_sent_count_++;
  }

  void OnCodeAutoRetrievalTimeOut(const std::string& verification_id) override {
    LogDebug("PhoneListener: auto retrieval timeout. verification_id=%s",
             verification_id.c_str());
    verification_id_ = verification_id;
    on_code_auto_retrieval_time_out_count_++;
  }

  const std::string& verification_id() const { return verification_id_; }
  const firebase::auth::PhoneAuthProvider::ForceResendingToken&
  force_resending_token() const {
    return force_resending_token_;
  }
  int on_verification_complete_count() const {
    return on_verification_complete_count_;
  }
  int on_verification_failed_count() const {
    return on_verification_failed_count_;
  }
  int on_code_sent_count() const { return on_code_sent_count_; }
  int on_code_auto_retrieval_time_out_count() const {
    return on_code_auto_retrieval_time_out_count_;
  }

  // Helper functions for workflow.
  bool waiting_to_send_code() {
    return on_verification_complete_count() == 0 &&
           on_verification_failed_count() == 0 && on_code_sent_count() == 0;
  }

  bool waiting_for_verification_id() {
    return on_verification_complete_count() == 0 &&
           on_verification_failed_count() == 0 &&
           on_code_auto_retrieval_time_out_count() == 0;
  }

  firebase::auth::Credential credential() { return credential_; }

 private:
  std::string verification_id_;
  firebase::auth::PhoneAuthProvider::ForceResendingToken force_resending_token_;
  firebase::auth::Credential credential_;
  int on_verification_complete_count_;
  int on_verification_failed_count_;
  int on_code_sent_count_;
  int on_code_auto_retrieval_time_out_count_;
};

TEST_F(FirebaseAuthTest, TestPhoneAuth) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_TVOS;
  SKIP_TEST_ON_ANDROID_EMULATOR;

#if TARGET_OS_IPHONE
  // Note: This test requires interactivity on iOS, as it displays a CAPTCHA.
  TEST_REQUIRES_USER_INTERACTION;
#endif  // TARGET_OS_IPHONE

  FLAKY_TEST_SECTION_BEGIN();

  firebase::auth::PhoneAuthProvider& phone_provider =
      firebase::auth::PhoneAuthProvider::GetInstance(auth_);
  LogDebug("Creating listener.");
  PhoneListener listener;
  LogDebug("Calling VerifyPhoneNumber.");
  // Randomly choose one of the phone numbers to avoid collisions.
  const int random_phone_number =
      app_framework::GetCurrentTimeInMicroseconds() %
      kPhoneAuthTestNumPhoneNumbers;
  phone_provider.VerifyPhoneNumber(
      kPhoneAuthTestPhoneNumbers[random_phone_number], kPhoneAuthTimeoutMs,
      nullptr, &listener);

  // Wait for OnCodeSent() callback.
  int wait_ms = 0;
  LogDebug("Waiting for code send.");
  while (listener.waiting_to_send_code()) {
    if (wait_ms > kPhoneAuthCodeSendWaitMs) break;
    ProcessEvents(kWaitIntervalMs);
    wait_ms += kWaitIntervalMs;
  }
  EXPECT_EQ(listener.on_verification_failed_count(), 0);

  LogDebug("Waiting for verification ID.");
  // Wait for the listener to have a verification ID.
  wait_ms = 0;
  while (listener.waiting_for_verification_id()) {
    if (wait_ms > kPhoneAuthCompletionWaitMs) break;
    ProcessEvents(kWaitIntervalMs);
    wait_ms += kWaitIntervalMs;
  }
  if (listener.on_verification_complete_count() > 0) {
    LogDebug("Signing in with automatic verification code.");
    WaitForCompletion(auth_->SignInWithCredential(listener.credential()),
                      "SignInWithCredential(PhoneCredential) automatic");
  } else if (listener.on_verification_failed_count() > 0) {
    FAIL() << "Automatic verification failed.";
  } else {
    // Did not automatically verify, submit verification code manually.
    EXPECT_GT(listener.on_code_auto_retrieval_time_out_count(), 0);
    EXPECT_NE(listener.verification_id(), "");
    LogDebug("Signing in with verification code.");
    const firebase::auth::Credential phone_credential =
        phone_provider.GetCredential(listener.verification_id().c_str(),
                                     kPhoneAuthTestVerificationCode);

    WaitForCompletion(auth_->SignInWithCredential(phone_credential),
                      "SignInWithCredential(PhoneCredential)");
  }

  ProcessEvents(1000);
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}

#if defined(ENABLE_OAUTH_TESTS)
// SignInWithProvider
TEST_F(FirebaseAuthTest, TestSuccessfulSignInFederatedProviderNoScopes) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulSignInFederatedProviderNoScopesNoCustomParameters) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSuccessfulSignInFederatedProvider) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id,
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSignInFederatedProviderBadProviderIdFails) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  firebase::auth::FederatedOAuthProviderData provider_data(
      /*provider=*/"MadeUpProvider",
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "5321"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider",
                    firebase::auth::kAuthErrorInvalidProviderId);
}

// ReauthenticateWithProvider
TEST_F(FirebaseAuthTest, TestSuccessfulReauthenticateWithProvider) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id,
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSuccessfulReauthenticateWithProviderNoScopes) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulReauthenticateWithProviderNoScopesNoCustomParameters) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestReauthenticateWithProviderBadProviderIdFails) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(provider_id);
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider", provider_id)) {
    provider_data.provider_id = "MadeUpProvider";
    firebase::auth::FederatedOAuthProvider provider(provider_data);
    firebase::Future<firebase::auth::SignInResult> reauth_future =
        auth_->current_user()->ReauthenticateWithProvider(&provider);
    WaitForCompletion(reauth_future, "ReauthenticateWithProvider",
                      firebase::auth::kAuthErrorInvalidProviderId);
  }
  DeleteUser();
}

// LinkWithProvider
TEST_F(FirebaseAuthTest, TestSuccessfulLinkFederatedProviderNoScopes) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user()->LinkWithProvider(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulLinkFederatedProviderNoScopesNoCustomParameters) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user()->LinkWithProvider(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSuccessfulLinkFederatedProvider) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id,
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user()->LinkWithProvider(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestLinkFederatedProviderBadProviderIdFails) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user(), nullptr);
  firebase::auth::FederatedOAuthProviderData provider_data(
      /*provider=*/"MadeUpProvider",
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user()->LinkWithProvider(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider",
                    firebase::auth::kAuthErrorInvalidProviderId);
  DeleteUser();
}

#endif  // defined(ENABLE_OAUTH_TESTS)

}  // namespace firebase_testapp_automated
