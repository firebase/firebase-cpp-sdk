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

  // Delete the current user if it's currently signed in using the deprecated
  // API surface.
  void DeleteUser_DEPRECATED();

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
  bool WaitForCompletion(firebase::Future<firebase::auth::User> future,
                         const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone);

  // Custom WaitForCompletion that checks if User is valid afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::AuthResult> future,
                         const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone);
  // Custom WaitForCompletion that checks if User matches afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::SignInResult> future,
                         const char* fn,
                         int expected_error = firebase::auth::kAuthErrorNone);

  // Custom WaitForCompletions which checks if user's Provider ID matches
  // afterwards.
  bool WaitForCompletion(firebase::Future<firebase::auth::AuthResult> future,
                         const char* fn, const std::string& provider_id);

  bool WaitForCompletion(firebase::Future<firebase::auth::SignInResult> future,
                         const char* fn, const std::string& provider_id);

  // Waits for the future to be marked as either complete or invalid.
  void WaitForCompletionOrInvalidStatus(const firebase::FutureBase& future,
                                        const char* name) {
    app_framework::LogDebug("WaitForCompletionOrInvalidStatus %s", name);
    while (future.status() == firebase::kFutureStatusPending) {
      app_framework::ProcessEvents(100);
    }
  }

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
      const firebase::auth::User* auth_user = auth_->current_user_DEPRECATED();
      EXPECT_EQ(future_result_user, auth_user)
          << "User returned by Future doesn't match User in Auth";
      return (future_result_user == auth_user);
    }
  }
  return succeeded;
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::User> future, const char* fn,
    int expected_error) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn, expected_error);

  if (succeeded) {
    if (expected_error == ::firebase::auth::kAuthErrorNone) {
      const firebase::auth::User* future_result_user = future.result();
      const firebase::auth::User auth_user = auth_->current_user();
      EXPECT_TRUE(auth_user.is_valid());
      EXPECT_TRUE(future_result_user->is_valid());
      EXPECT_EQ(future_result_user->uid(), auth_user.uid())
          << "User returned by Future doesn't match User in Auth";
      return auth_user.is_valid() &&
             (auth_user.uid() == future_result_user->uid());
    }
  }
  return succeeded;
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::AuthResult> future, const char* fn,
    int expected_error) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn, expected_error);

  if (succeeded) {
    if (expected_error == ::firebase::auth::kAuthErrorNone) {
      if (future.result() != nullptr) {
        EXPECT_TRUE(future.result()->user.is_valid());
        EXPECT_TRUE(auth_->current_user().is_valid());
        EXPECT_EQ(future.result()->user.uid(), auth_->current_user().uid())
            << "User returned by Future doesn't match User in Auth";
        succeeded = future.result()->user.is_valid() &&
                    auth_->current_user().is_valid();
      }
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
      const firebase::auth::User* auth_user = auth_->current_user_DEPRECATED();
      EXPECT_EQ(future_result_user, auth_user)
          << "User returned by Future doesn't match User in Auth";
      return (future_result_user == auth_user);
    }
  }
  return succeeded;
}

bool FirebaseAuthTest::WaitForCompletion(
    firebase::Future<firebase::auth::AuthResult> future, const char* fn,
    const std::string& provider_id) {
  bool succeeded = FirebaseTest::WaitForCompletion(future, fn);
  if (succeeded) {
    const firebase::auth::AuthResult* result_ptr = future.result();
    EXPECT_TRUE(result_ptr->user.is_valid());
    EXPECT_EQ(result_ptr->additional_user_info.provider_id, provider_id);
    EXPECT_EQ(result_ptr->user.uid(), auth_->current_user().uid());
    EXPECT_TRUE(auth_->current_user().is_valid());
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
  if (!auth_->current_user().is_valid()) {
    // Already signed out.
    return;
  }
  auth_->SignOut();
  // Wait for the sign-out to finish.
  while (auth_->current_user().is_valid()) {
    if (ProcessEvents(100)) break;
  }
  ProcessEvents(100);
  EXPECT_FALSE(auth_->current_user().is_valid());
}

void FirebaseAuthTest::DeleteUser() {
  if (auth_ != nullptr && auth_->current_user().is_valid()) {
    FirebaseTest::WaitForCompletion(auth_->current_user().Delete(),
                                    "Delete User");
    ProcessEvents(100);
  }
}

void FirebaseAuthTest::DeleteUser_DEPRECATED() {
  if (auth_ != nullptr && auth_->current_user_DEPRECATED() != nullptr) {
    FirebaseTest::WaitForCompletion(auth_->current_user_DEPRECATED()->Delete(),
                                    "Delete User Deprecated");
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
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(auth_->current_user().is_anonymous());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestAnonymousSignin_DEPRECATED) {
  // Test notification on SignIn().
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  if (auth_->current_user_DEPRECATED() != nullptr) {
    EXPECT_TRUE(auth_->current_user_DEPRECATED()->is_anonymous());
  }
  DeleteUser_DEPRECATED();
}

// TODO(jonsimantov): Add TestDeleteUser.

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
    std::string provider;
    if (auth->current_user().is_valid()) {
      provider = auth->current_user().provider_id();
    }
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
    if (auth->current_user().is_valid()) {
      firebase::Future<std::string> token_future =
          auth->current_user().GetToken(false);
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
      auth_->current_user().GetToken(false);
  WaitForCompletion(token_future, "GetToken(false)");
  std::string first_token = *token_future.result();
  // Force a token refresh.
  ProcessEvents(1000);
  token_future = auth_->current_user().GetToken(true);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(auth_result_future, "CreateUserWithEmailAndPassword");

  EXPECT_TRUE(auth_->current_user().is_valid());
  if (auth_result_future.result() != nullptr) {
    EXPECT_TRUE(auth_result_future.result()->user.is_valid());
  }
  // Sign out and log in using SignInWithCredential(EmailCredential).
  SignOut();
  {
    firebase::auth::Credential email_credential =
        firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                         password.c_str());
    firebase::Future<firebase::auth::User> user_future =
        auth_->SignInWithCredential(email_credential);
    WaitForCompletion(user_future, "SignInWithCredential");
    EXPECT_TRUE(auth_->current_user().is_valid());
    if (user_future.result() != nullptr) {
      EXPECT_TRUE(user_future.result()->is_valid());
      EXPECT_EQ(user_future.result()->email(), email);
    }
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
    EXPECT_TRUE(auth_->current_user().is_valid());
  }
  SignOut();
  // Sign in with SignInWithEmailAndPassword values.
  auth_result_future =
      auth_->SignInWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(auth_result_future, "SignInWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  if (auth_result_future.result() != nullptr) {
    EXPECT_TRUE(auth_result_future.result()->user.is_valid());
    EXPECT_EQ(auth_result_future.result()->user.uid(),
              auth_->current_user().uid());
    EXPECT_EQ(auth_result_future.result()->user.email(), email);
  }

  // Then delete the account.
  firebase::Future<void> delete_user = auth_->current_user().Delete();
  WaitForCompletion(delete_user, "Delete");
  EXPECT_FALSE(auth_->current_user().is_valid());
  auth_result_future =
      auth_->SignInWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(auth_result_future,
                    "SignInWithEmailAndPassword (invalid user)",
                    firebase::auth::kAuthErrorUserNotFound);
  EXPECT_FALSE(auth_->current_user().is_valid());
}

TEST_F(FirebaseAuthTest, TestEmailAndPasswordSignin_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  // Register a random email and password. This signs us in as that user.
  std::string password = kTestPassword;
  firebase::Future<firebase::auth::User*> create_user =
      auth_->CreateUserWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                       password.c_str());
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  // Sign out and log in using SignInWithCredential_DEPRECATED(EmailCredential).
  SignOut();
  {
    firebase::auth::Credential email_credential =
        firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                         password.c_str());
    WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(email_credential),
                      "SignInWithCredential_DEPRECATED");
    EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  }
  // Sign out and log in using
  // SignInAndRetrieveDataWithCredential_DEPRECATED(EmailCredential).
  SignOut();
  {
    firebase::auth::Credential email_credential =
        firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                         password.c_str());
    WaitForCompletion(
        auth_->SignInAndRetrieveDataWithCredential_DEPRECATED(email_credential),
        "SignInAndRetrieveDataWithCredential_DEPRECATED");
    EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  }
  SignOut();
  // Sign in with SignInWithEmailAndPassword values.
  firebase::Future<firebase::auth::User*> sign_in_user =
      auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                   password.c_str());
  WaitForCompletion(sign_in_user, "SignInWithEmailAndPassword_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);

  // Then delete the account.
  firebase::Future<void> delete_user =
      auth_->current_user_DEPRECATED()->Delete();
  WaitForCompletion(delete_user, "Delete");
  firebase::Future<firebase::auth::User*> invalid_sign_in_user =
      auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                   password.c_str());
  WaitForCompletion(invalid_sign_in_user,
                    "SignInWithEmailAndPassword_DEPRECATED (invalid user)",
                    firebase::auth::kAuthErrorUserNotFound);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
}

TEST_F(FirebaseAuthTest, TestCopyUser) {
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_TRUE(auth_->current_user().is_valid());
  if (!auth_->current_user().is_valid()) return;

  EXPECT_TRUE(auth_->current_user().is_anonymous());
  EXPECT_NE(auth_->current_user().uid().size(), 0);

  // Copy Constructor
  firebase::auth::User copy_of_user(auth_->current_user());
  EXPECT_TRUE(copy_of_user.is_valid());
  EXPECT_TRUE(copy_of_user.is_anonymous());
  EXPECT_EQ(auth_->current_user().uid(), copy_of_user.uid());

  // Assignment operator
  firebase::auth::User assigned_user = copy_of_user;
  EXPECT_TRUE(assigned_user.is_valid());
  EXPECT_TRUE(assigned_user.is_anonymous());
  EXPECT_EQ(auth_->current_user().uid(), assigned_user.uid());

  DeleteUser();

  EXPECT_FALSE(copy_of_user.is_valid());
  EXPECT_FALSE(assigned_user.is_valid());
  EXPECT_EQ(copy_of_user.uid(), "");
  EXPECT_EQ(assigned_user.uid(), "");
}

TEST_F(FirebaseAuthTest, TestCopyUser_DEPRECATED) {
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  if (auth_->current_user_DEPRECATED() == nullptr) return;

  EXPECT_TRUE(auth_->current_user_DEPRECATED()->is_valid());
  EXPECT_TRUE(auth_->current_user_DEPRECATED()->is_anonymous());
  EXPECT_NE(auth_->current_user_DEPRECATED()->uid().size(), 0);

  // Copy Constructor
  firebase::auth::User copy_of_user(*auth_->current_user_DEPRECATED());
  EXPECT_TRUE(copy_of_user.is_valid());
  EXPECT_TRUE(copy_of_user.is_anonymous());
  EXPECT_EQ(auth_->current_user_DEPRECATED()->uid(), copy_of_user.uid());

  // Assignment operator
  firebase::auth::User assigned_user = copy_of_user;
  EXPECT_TRUE(assigned_user.is_valid());
  EXPECT_TRUE(assigned_user.is_anonymous());
  EXPECT_EQ(auth_->current_user_DEPRECATED()->uid(), assigned_user.uid());

  DeleteUser_DEPRECATED();

  EXPECT_FALSE(copy_of_user.is_valid());
  EXPECT_FALSE(assigned_user.is_valid());
  EXPECT_EQ(copy_of_user.uid(), "");
  EXPECT_EQ(assigned_user.uid(), "");
}

TEST_F(FirebaseAuthTest, TestRetainedUser) {
  std::string email = GenerateEmailAddress();
  // Register a random email and password. This signs us in as that user.
  std::string password = kTestPassword;
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(auth_result_future, "CreateUserWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  if (!auth_->current_user().is_valid()) return;

  firebase::auth::User retained_user = auth_->current_user();

  DeleteUser();

  EXPECT_EQ(retained_user.uid(), "");
  EXPECT_EQ(retained_user.email(), "");

  // Sign in a new account.
  email = GenerateEmailAddress();
  auth_result_future =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), password.c_str());
  WaitForCompletion(auth_result_future, "CreateUserWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());

  EXPECT_NE(retained_user.uid(), "");
  EXPECT_NE(retained_user.email(), "");
  EXPECT_EQ(retained_user.uid(), auth_->current_user().uid());
  EXPECT_EQ(retained_user.email(), auth_->current_user().email());

  // Then delete the retained user.
  firebase::Future<void> delete_user = retained_user.Delete();
  WaitForCompletion(delete_user, "Delete retained user");

  EXPECT_FALSE(auth_->current_user().is_valid());
  EXPECT_FALSE(retained_user.is_valid());
}

TEST_F(FirebaseAuthTest, TestRetainedUser_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  // Register a random email and password. This signs us in as that user.
  std::string password = kTestPassword;
  firebase::Future<firebase::auth::User*> create_user =
      auth_->CreateUserWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                       password.c_str());
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  if (auth_->current_user_DEPRECATED() == nullptr) return;

  firebase::auth::User retained_user = *auth_->current_user_DEPRECATED();

  DeleteUser_DEPRECATED();

  EXPECT_EQ(retained_user.uid(), "");
  EXPECT_EQ(retained_user.email(), "");

  // Sign in a new account.
  email = GenerateEmailAddress();
  create_user = auth_->CreateUserWithEmailAndPassword_DEPRECATED(
      email.c_str(), password.c_str());
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);

  EXPECT_NE(retained_user.uid(), "");
  EXPECT_NE(retained_user.email(), "");

  if (auth_->current_user_DEPRECATED() != nullptr) {
    EXPECT_EQ(retained_user.uid(), auth_->current_user_DEPRECATED()->uid());
    EXPECT_EQ(retained_user.email(), auth_->current_user_DEPRECATED()->email());
  }

  // Then delete the retained user.
  firebase::Future<void> delete_user = retained_user.Delete();
  WaitForCompletion(delete_user, "Delete retained user");

  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
  EXPECT_FALSE(retained_user.is_valid());
}

TEST_F(FirebaseAuthTest, TestOperationsOnInvalidUser) {
  EXPECT_FALSE(auth_->current_user().is_valid());

  firebase::auth::User invalid_user = auth_->current_user();
  firebase::Future<firebase::auth::AuthResult> auth_result_future;
  firebase::Future<std::string> string_future;
  firebase::Future<firebase::auth::User> user_future;
  firebase::Future<firebase::auth::User*> user_ptr_future;
  firebase::Future<void> void_future;

  LogDebug("Attempting to use invalid user.");
  string_future = invalid_user.GetToken(/*force_refresh=*/true);
  WaitForCompletionOrInvalidStatus(string_future, "GetToken");
  EXPECT_NE(string_future.error(), firebase::auth::kAuthErrorNone);

  void_future = invalid_user.UpdateEmail(GenerateEmailAddress().c_str());
  WaitForCompletionOrInvalidStatus(void_future, "UpdateEmail");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  void_future = invalid_user.UpdatePassword(kTestPassword);
  WaitForCompletionOrInvalidStatus(void_future, "UpdatePassword");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  firebase::auth::Credential email_cred =
      firebase::auth::EmailAuthProvider::GetCredential(
          GenerateEmailAddress().c_str(), kTestPasswordUpdated);
  void_future = invalid_user.Reauthenticate(email_cred);
  WaitForCompletionOrInvalidStatus(void_future, "Reauthenticate");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  auth_result_future = invalid_user.ReauthenticateAndRetrieveData(email_cred);
  WaitForCompletionOrInvalidStatus(auth_result_future,
                                   "ReauthenticateAndRetrieveData");
  EXPECT_NE(auth_result_future.error(), firebase::auth::kAuthErrorNone);

  void_future = invalid_user.SendEmailVerification();
  WaitForCompletionOrInvalidStatus(void_future, "SendEmailVerification");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  firebase::auth::User::UserProfile profile;
  void_future = invalid_user.UpdateUserProfile(profile);
  WaitForCompletionOrInvalidStatus(void_future, "UpdateUserProfile");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  auth_result_future = invalid_user.LinkWithCredential(email_cred);
  WaitForCompletionOrInvalidStatus(auth_result_future, "LinkWithCredential");
  EXPECT_NE(auth_result_future.error(), firebase::auth::kAuthErrorNone);

  auth_result_future = invalid_user.Unlink(email_cred.provider().c_str());
  WaitForCompletionOrInvalidStatus(auth_result_future, "Unlink_DEPRECATED");
  EXPECT_NE(user_ptr_future.error(), firebase::auth::kAuthErrorNone);

  user_ptr_future =
      invalid_user.UpdatePhoneNumberCredential_DEPRECATED(email_cred);
  WaitForCompletionOrInvalidStatus(user_ptr_future,
                                   "UpdatePhoneNumberCredential_DEPRECATED");
  EXPECT_NE(user_ptr_future.error(), firebase::auth::kAuthErrorNone);

  void_future = invalid_user.Reload();
  WaitForCompletionOrInvalidStatus(void_future, "Reload");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);

  void_future = invalid_user.Delete();
  WaitForCompletionOrInvalidStatus(void_future, "Delete");
  EXPECT_NE(void_future.error(), firebase::auth::kAuthErrorNone);
}

TEST_F(FirebaseAuthTest, TestUpdateUserProfile) {
  std::string email = GenerateEmailAddress();
  firebase::Future<firebase::auth::AuthResult> create_user =
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword);
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  // Set some user profile properties.
  firebase::auth::User user = create_user.result()->user;
  const char kDisplayName[] = "Hello World";
  const char kPhotoUrl[] = "http://example.com/image.jpg";
  firebase::auth::User::UserProfile user_profile;
  user_profile.display_name = kDisplayName;
  user_profile.photo_url = kPhotoUrl;
  firebase::Future<void> update_profile = user.UpdateUserProfile(user_profile);
  WaitForCompletion(update_profile, "UpdateUserProfile");
  user = auth_->current_user();
  EXPECT_EQ(user.display_name(), kDisplayName);
  EXPECT_EQ(user.photo_url(), kPhotoUrl);
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  user = auth_->current_user();
  EXPECT_EQ(user.display_name(), kDisplayName);
  EXPECT_EQ(user.photo_url(), kPhotoUrl);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestUpdateUserProfile_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  firebase::Future<firebase::auth::User*> create_user =
      auth_->CreateUserWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                       kTestPassword);
  WaitForCompletion(create_user, "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
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
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword");
  EXPECT_EQ(user->display_name(), kDisplayName);
  EXPECT_EQ(user->photo_url(), kPhotoUrl);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestUpdateEmailAndPassword) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  firebase::auth::User user = auth_->current_user();
  EXPECT_TRUE(user.is_valid());

  // Update the user's email and password.
  const std::string new_email = "new_" + email;
  WaitForCompletion(user.UpdateEmail(new_email.c_str()), "UpdateEmail");
  WaitForCompletion(user.UpdatePassword(kTestPasswordUpdated),
                    "UpdatePassword");

  firebase::auth::Credential new_email_cred =
      firebase::auth::EmailAuthProvider::GetCredential(new_email.c_str(),
                                                       kTestPasswordUpdated);
  WaitForCompletion(user.Reauthenticate(new_email_cred), "Reauthenticate");
  EXPECT_TRUE(user.is_valid());

  WaitForCompletion(user.SendEmailVerification(), "SendEmailVerification");
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestUpdateEmailAndPassword_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  firebase::auth::User* user = auth_->current_user_DEPRECATED();

  // Update the user's email and password.
  const std::string new_email = "new_" + email;
  WaitForCompletion(user->UpdateEmail(new_email.c_str()), "UpdateEmail");
  WaitForCompletion(user->UpdatePassword(kTestPasswordUpdated),
                    "UpdatePassword");

  firebase::auth::Credential new_email_cred =
      firebase::auth::EmailAuthProvider::GetCredential(new_email.c_str(),
                                                       kTestPasswordUpdated);
  WaitForCompletion(user->Reauthenticate(new_email_cred), "Reauthenticate");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);

  WaitForCompletion(user->SendEmailVerification(), "SendEmailVerification");
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithEmailCredential) {
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  firebase::auth::User user = auth_->current_user();
  EXPECT_TRUE(user.is_valid());
  std::string email = GenerateEmailAddress();
  firebase::auth::Credential credential =
      firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user.LinkWithCredential(credential), "LinkWithCredential");
  WaitForCompletion(user.Unlink(credential.provider().c_str()), "Unlink");
  SignOut();
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  user = auth_->current_user();
  EXPECT_TRUE(user.is_valid());
  std::string email1 = GenerateEmailAddress();
  firebase::auth::Credential credential1 =
      firebase::auth::EmailAuthProvider::GetCredential(email1.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user.LinkWithCredential(credential1),
                    "LinkWithCredential 1");
  user = auth_->current_user();
  EXPECT_TRUE(user.is_valid());

  std::string email2 = GenerateEmailAddress();
  firebase::auth::Credential credential2 =
      firebase::auth::EmailAuthProvider::GetCredential(email2.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user.LinkWithCredential(credential2),
                    "LinkWithCredential 2",
                    firebase::auth::kAuthErrorProviderAlreadyLinked);
  WaitForCompletion(user.Unlink(credential.provider().c_str()), "Unlink 2");
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithEmailCredential_DEPRECATED) {
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  firebase::auth::User* user = auth_->current_user_DEPRECATED();
  std::string email = GenerateEmailAddress();
  firebase::auth::Credential credential =
      firebase::auth::EmailAuthProvider::GetCredential(email.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkAndRetrieveDataWithCredential(credential),
                    "LinkAndRetrieveDataWithCredential");
  WaitForCompletion(user->Unlink_DEPRECATED(credential.provider().c_str()),
                    "Unlink");
  SignOut();
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  std::string email1 = GenerateEmailAddress();
  firebase::auth::Credential credential1 =
      firebase::auth::EmailAuthProvider::GetCredential(email1.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkWithCredential_DEPRECATED(credential1),
                    "LinkWithCredential_DEPRECATED 1");
  std::string email2 = GenerateEmailAddress();
  firebase::auth::Credential credential2 =
      firebase::auth::EmailAuthProvider::GetCredential(email2.c_str(),
                                                       kTestPassword);
  WaitForCompletion(user->LinkWithCredential_DEPRECATED(credential2),
                    "LinkWithCredential_DEPRECATED 2",
                    firebase::auth::kAuthErrorProviderAlreadyLinked);
  WaitForCompletion(user->Unlink_DEPRECATED(credential.provider().c_str()),
                    "Unlink_DEPRECATED 2");
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithBadCredential) {
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  firebase::auth::User pre_link_user = auth_->current_user();
  EXPECT_TRUE(pre_link_user.is_valid());
  firebase::auth::Credential twitter_cred =
      firebase::auth::TwitterAuthProvider::GetCredential(kTestIdTokenBad,
                                                         kTestAccessTokenBad);
  WaitForCompletion(pre_link_user.LinkWithCredential(twitter_cred),
                    "LinkWithCredential",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that user stays the same.
  EXPECT_TRUE(pre_link_user.is_valid());
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_EQ(auth_->current_user().uid(), pre_link_user.uid());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestLinkAnonymousUserWithBadCredential_DEPRECATED) {
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  firebase::auth::User* pre_link_user = auth_->current_user_DEPRECATED();
  firebase::auth::Credential twitter_cred =
      firebase::auth::TwitterAuthProvider::GetCredential(kTestIdTokenBad,
                                                         kTestAccessTokenBad);
  WaitForCompletion(pre_link_user->LinkWithCredential_DEPRECATED(twitter_cred),
                    "LinkWithCredential_DEPRECATED",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that user stays the same.
  EXPECT_EQ(auth_->current_user_DEPRECATED(), pre_link_user);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadEmailFails) {
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(kTestEmailBad, kTestPassword),
      "SignInWithEmailAndPassword", firebase::auth::kAuthErrorUserNotFound);
  EXPECT_FALSE(auth_->current_user().is_valid());
}

TEST_F(FirebaseAuthTest, TestSignInWithBadEmailFails_DEPRECATED) {
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(kTestEmailBad,
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword_DEPRECATED",
                    firebase::auth::kAuthErrorUserNotFound);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
}

TEST_F(FirebaseAuthTest, TestSignInWithBadPasswordFails) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPasswordBad),
      "SignInWithEmailAndPassword", firebase::auth::kAuthErrorWrongPassword);
  EXPECT_FALSE(auth_->current_user().is_valid());
  SignOut();
  // Sign back in and delete the user.
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadPasswordFails_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPasswordBad),
                    "SignInWithEmailAndPassword_DEPRECATED",
                    firebase::auth::kAuthErrorWrongPassword);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  // Sign back in and delete the user.
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestCreateUserWithExistingEmailFails) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword 1");
  EXPECT_TRUE(auth_->current_user().is_valid());
  SignOut();
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPassword),
      "CreateUserWithEmailAndPassword 2",
      firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_FALSE(auth_->current_user().is_valid());
  SignOut();
  // Try again with a different password.
  WaitForCompletion(
      auth_->CreateUserWithEmailAndPassword(email.c_str(), kTestPasswordBad),
      "CreateUserWithEmailAndPassword 3",
      firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_FALSE(auth_->current_user().is_valid());
  SignOut();
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestCreateUserWithExistingEmailFails_DEPRECATED) {
  std::string email = GenerateEmailAddress();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED 1");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED 2",
                    firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  // Try again with a different password.
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPasswordBad),
                    "CreateUserWithEmailAndPassword_DEPRECATED 3",
                    firebase::auth::kAuthErrorEmailAlreadyInUse);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadCredentials) {
  // Get an anonymous user first.
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_TRUE(auth_->current_user().is_valid());
  // Hold on to the existing user, to make sure it is unchanged by bad signins.
  firebase::auth::User existing_user = auth_->current_user();
  // Test signing in with a variety of bad credentials.
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::FacebookAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential (Facebook)",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that failing to sign in with a credential doesn't modify the user.
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::TwitterAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential (Twitter)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GitHubAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential (GitHub)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential (Google 1)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, nullptr)),
                    "SignInWithCredential (Google 2)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
  WaitForCompletion(
      auth_->SignInWithCredential(firebase::auth::OAuthProvider::GetCredential(
          kTestIdProviderIdBad, kTestIdTokenBad, kTestAccessTokenBad)),
      "SignInWithCredential (OAuth)", firebase::auth::kAuthErrorFailure);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());

#if defined(__ANDROID__)
  // Test Play Games sign-in on Android only.
  WaitForCompletion(auth_->SignInWithCredential(
                        firebase::auth::PlayGamesAuthProvider::GetCredential(
                            kTestServerAuthCodeBad)),
                    "SignInWithCredential (Play Games)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(existing_user.is_valid());
  EXPECT_EQ(auth_->current_user().uid(), existing_user.uid());
#endif  // defined(__ANDROID__)
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSignInWithBadCredentials_DEPRECATED) {
  // Get an anonymous user first.
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(), "SignInAnonymously");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  // Hold on to the existing user, to make sure it is unchanged by bad signins.
  firebase::auth::User* existing_user = auth_->current_user_DEPRECATED();
  // Test signing in with a variety of bad credentials.
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::FacebookAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential_DEPRECATED (Facebook)",
                    firebase::auth::kAuthErrorInvalidCredential);
  // Ensure that failing to sign in with a credential doesn't modify the user.
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::TwitterAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential_DEPRECATED (Twitter)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::GitHubAuthProvider::GetCredential(
                            kTestAccessTokenBad)),
                    "SignInWithCredential_DEPRECATED (GitHub)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, kTestAccessTokenBad)),
                    "SignInWithCredential_DEPRECATED (Google 1)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::GoogleAuthProvider::GetCredential(
                            kTestIdTokenBad, nullptr)),
                    "SignInWithCredential_DEPRECATED (Google 2)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
  WaitForCompletion(
      auth_->SignInWithCredential_DEPRECATED(
          firebase::auth::OAuthProvider::GetCredential(
              kTestIdProviderIdBad, kTestIdTokenBad, kTestAccessTokenBad)),
      "SignInWithCredential (OAuth)", firebase::auth::kAuthErrorFailure);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);

#if defined(__ANDROID__)
  // Test Play Games sign-in on Android only.
  WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(
                        firebase::auth::PlayGamesAuthProvider::GetCredential(
                            kTestServerAuthCodeBad)),
                    "SignInWithCredential (Play Games)",
                    firebase::auth::kAuthErrorInvalidCredential);
  EXPECT_EQ(auth_->current_user_DEPRECATED(), existing_user);
#endif  // defined(__ANDROID__)
  DeleteUser_DEPRECATED();
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

TEST_F(FirebaseAuthTest, TestGameCenterSignIn_DEPRECATED) {
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
    WaitForCompletion(
        auth_->SignInWithCredential_DEPRECATED(*credential_future.result()),
        "SignInWithCredential_DEPRECATED (Game Center)");
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
  EXPECT_TRUE(auth_->current_user().is_valid());
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
  EXPECT_TRUE(auth_->current_user().is_valid());
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSendPasswordResetEmail_DEPRECATED) {
  // Test Auth::SendPasswordResetEmail().
  std::string email = GenerateEmailAddress();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  SignOut();
  // Send to correct email.
  WaitForCompletion(auth_->SendPasswordResetEmail(email.c_str()),
                    "SendPasswordResetEmail (good)");
  // Send to incorrect email.
  WaitForCompletion(auth_->SendPasswordResetEmail(kTestEmailBad),
                    "SendPasswordResetEmail (bad)",
                    firebase::auth::kAuthErrorUserNotFound);
  // Delete user now that we are done with it.
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
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
  firebase::Future<firebase::auth::AuthResult> auth_result =
      auth_->SignInWithEmailAndPassword(kCustomTestEmail, kCustomTestPassword);
  WaitForCompletion(auth_result, "SignInWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_EQ(auth_->current_user().email(), kCustomTestEmail);
}

TEST_F(FirebaseAuthTest, TestWithCustomEmailAndPassword_DEPRECATED) {
  if (strlen(kCustomTestEmail) == 0 || strlen(kCustomTestPassword) == 0) {
    LogInfo(
        "Skipping %s. To enable this test, set "
        "kCustomTestEmail and kCustomTestPassword in integration_test.cc.",
        test_info_->name());
    GTEST_SKIP();
    return;
  }
  firebase::Future<firebase::auth::User*> sign_in_user =
      auth_->SignInWithEmailAndPassword_DEPRECATED(kCustomTestEmail,
                                                   kCustomTestPassword);
  WaitForCompletion(sign_in_user, "SignInWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
}

TEST_F(FirebaseAuthTest, TestAuthPersistenceWithAnonymousSignin) {
  // Automated test is disabled on linux due to the need to unlock the keystore.
  SKIP_TEST_ON_LINUX;

  FLAKY_TEST_SECTION_BEGIN();

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(auth_->current_user().is_anonymous());
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_TRUE(auth_->current_user().is_anonymous());
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseAuthTest, TestAuthPersistenceWithAnonymousSignin_DEPRECATED) {
  // Automated test is disabled on linux due to the need to unlock the keystore.
  SKIP_TEST_ON_LINUX;

  FLAKY_TEST_SECTION_BEGIN();

  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(), "SignInAnonymously");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  EXPECT_TRUE(auth_->current_user_DEPRECATED()->is_anonymous());
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  EXPECT_TRUE(auth_->current_user_DEPRECATED()->is_anonymous());
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
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_FALSE(auth_->current_user().is_anonymous());
  firebase::auth::User user = auth_->current_user();
  std::string prev_provider_id = user.provider_id();
  // Save the old provider ID list so we can make sure it's the same once
  // it's loaded again.
  std::vector<std::string> prev_provider_data_ids;
  for (int i = 0; i < user.provider_data().size(); i++) {
    prev_provider_data_ids.push_back(user.provider_data()[i].provider_id());
  }
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_TRUE(auth_->current_user().is_valid());
  EXPECT_FALSE(auth_->current_user().is_anonymous());
  // Make sure the provider IDs are the same as they were before.
  EXPECT_EQ(auth_->current_user().provider_id(), prev_provider_id);
  user = auth_->current_user();
  std::vector<std::string> loaded_provider_data_ids;
  for (int i = 0; i < user.provider_data().size(); i++) {
    loaded_provider_data_ids.push_back(user.provider_data()[i].provider_id());
  }
  EXPECT_TRUE(loaded_provider_data_ids == prev_provider_data_ids);

  // Cleanup, ensure we are signed in as the user so we can delete it.
  WaitForCompletion(
      auth_->SignInWithEmailAndPassword(email.c_str(), kTestPassword),
      "SignInWithEmailAndPassword");
  EXPECT_TRUE(auth_->current_user().is_valid());
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseAuthTest, TestAuthPersistenceWithEmailSignin_DEPRECATED) {
  // Automated test is disabled on linux due to the need to unlock the keystore.
  SKIP_TEST_ON_LINUX;

  FLAKY_TEST_SECTION_BEGIN();

  std::string email = GenerateEmailAddress();
  WaitForCompletion(auth_->CreateUserWithEmailAndPassword_DEPRECATED(
                        email.c_str(), kTestPassword),
                    "CreateUserWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  EXPECT_FALSE(auth_->current_user_DEPRECATED()->is_anonymous());
  std::string prev_provider_id =
      auth_->current_user_DEPRECATED()->provider_id();
  // Save the old provider ID list so we can make sure it's the same once
  // it's loaded again.
  std::vector<std::string> prev_provider_data_ids;
  for (int i = 0;
       i < auth_->current_user_DEPRECATED()->provider_data_DEPRECATED().size();
       i++) {
    prev_provider_data_ids.push_back(auth_->current_user_DEPRECATED()
                                         ->provider_data_DEPRECATED()[i]
                                         ->provider_id());
  }
  Terminate();
  ProcessEvents(2000);
  Initialize();
  EXPECT_NE(auth_, nullptr);
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  EXPECT_FALSE(auth_->current_user_DEPRECATED()->is_anonymous());
  // Make sure the provider IDs are the same as they were before.
  EXPECT_EQ(auth_->current_user_DEPRECATED()->provider_id(), prev_provider_id);
  std::vector<std::string> loaded_provider_data_ids;
  for (int i = 0;
       i < auth_->current_user_DEPRECATED()->provider_data_DEPRECATED().size();
       i++) {
    loaded_provider_data_ids.push_back(auth_->current_user_DEPRECATED()
                                           ->provider_data_DEPRECATED()[i]
                                           ->provider_id());
  }
  EXPECT_TRUE(loaded_provider_data_ids == prev_provider_data_ids);

  // Cleanup, ensure we are signed in as the user so we can delete it.
  WaitForCompletion(auth_->SignInWithEmailAndPassword_DEPRECATED(email.c_str(),
                                                                 kTestPassword),
                    "SignInWithEmailAndPassword_DEPRECATED");
  EXPECT_NE(auth_->current_user_DEPRECATED(), nullptr);
  DeleteUser();

  FLAKY_TEST_SECTION_END();
}

class PhoneListener : public firebase::auth::PhoneAuthProvider::Listener {
 public:
  PhoneListener()
      : on_verification_complete_credential_count_(0),
        on_verification_complete_phone_auth_credential_count_(0),
        on_verification_failed_count_(0),
        on_code_sent_count_(0),
        on_code_auto_retrieval_time_out_count_(0) {}

  // Expect both OnVerificationCompleted methods to be called up
  // PhoneAuthProvider VerifyPhonenumber invokcations. One is the newer
  // method which accepts a PhoneAuthCredential object as parameter. The other
  // is now decprecated and accepts a Credential object.
  void OnVerificationCompleted(
      firebase::auth::PhoneAuthCredential phone_auth_credential) override {
    LogDebug(
        "PhoneListener: PhoneAuthCredential successful automatic "
        "verification.");
    on_verification_complete_phone_auth_credential_count_++;
    phone_auth_credential_ = phone_auth_credential;
  }

  void OnVerificationCompleted(firebase::auth::Credential credential) override {
    LogDebug("PhoneListener: Credential successful automatic verification.");
    on_verification_complete_credential_count_++;
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
    return on_verification_complete_phone_auth_credential_count() +
           on_verification_complete_credential_count();
  }
  // Tracks the number of callbacks made on the new OnVerificationCompleted
  // method which takes a PhoneAuthCredential as a parameter.
  int on_verification_complete_phone_auth_credential_count() const {
    return on_verification_complete_phone_auth_credential_count_;
  }
  // Tracks the number of callbacks made on the deprecated
  // OnVerificationCompleted method which takes a Credential as a parameter.
  int on_verification_complete_credential_count() const {
    return on_verification_complete_credential_count_;
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
    return on_verification_complete_credential_count() == 0 &&
           on_verification_complete_phone_auth_credential_count() == 0 &&
           on_verification_failed_count() == 0 && on_code_sent_count() == 0;
  }

  bool waiting_for_verification_id() {
    return on_verification_complete_count() == 0 &&
           on_verification_failed_count() == 0 &&
           on_code_auto_retrieval_time_out_count() == 0;
  }

  firebase::auth::Credential credential() { return credential_; }
  firebase::auth::PhoneAuthCredential phone_auth_credential() {
    return phone_auth_credential_;
  }

 private:
  std::string verification_id_;
  firebase::auth::PhoneAuthProvider::ForceResendingToken force_resending_token_;
  firebase::auth::Credential credential_;
  firebase::auth::PhoneAuthCredential phone_auth_credential_;
  int on_verification_complete_phone_auth_credential_count_;
  int on_verification_complete_credential_count_;
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
  const int random_phone_number_index =
      app_framework::GetCurrentTimeInMicroseconds() %
      kPhoneAuthTestNumPhoneNumbers;
  firebase::auth::PhoneAuthOptions phone_options;
  phone_options.phone_number =
      kPhoneAuthTestPhoneNumbers[random_phone_number_index];
  phone_options.timeout_milliseconds = kPhoneAuthTimeoutMs;
  phone_provider.VerifyPhoneNumber(phone_options, &listener);

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

  LogDebug("phone_auth_credential sms code: %s",
           listener.phone_auth_credential().sms_code().c_str());

  if (listener.on_verification_complete_count() > 0) {
    // Ensure both listener methods were invoked.
    EXPECT_EQ(listener.on_verification_complete_phone_auth_credential_count(),
              1);
    EXPECT_EQ(listener.on_verification_complete_credential_count(), 1);
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

TEST_F(FirebaseAuthTest, TestPhoneAuth_DEPRECATED) {
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
    WaitForCompletion(
        auth_->SignInWithCredential_DEPRECATED(
            listener.phone_auth_credential()),
        "SignInWithCredential_DEPRECATED(PhoneCredential) automatic");
  } else if (listener.on_verification_failed_count() > 0) {
    FAIL() << "Automatic verification failed.";
  } else {
    // Did not automatically verify, submit verification code manually.
    EXPECT_GT(listener.on_code_auto_retrieval_time_out_count(), 0);
    EXPECT_NE(listener.verification_id(), "");
    LogDebug("Signing in with verification code.");
    const firebase::auth::PhoneAuthCredential phone_credential =
        phone_provider.GetCredential(listener.verification_id().c_str(),
                                     kPhoneAuthTestVerificationCode);

    WaitForCompletion(auth_->SignInWithCredential_DEPRECATED(phone_credential),
                      "SignInWithCredential_DEPRECATED(PhoneCredential)");
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(auth_result_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulSignInFederatedProviderNoScopes_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                    provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(auth_result_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(
    FirebaseAuthTest,
    TestSuccessfulSignInFederatedProviderNoScopesNoCustomParameters_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                    provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(auth_result_future, "SignInWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSuccessfulSignInFederatedProvider_DEPRECATED) {
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
      auth_->SignInWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                    provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  WaitForCompletion(auth_result_future, "SignInWithProvider",
                    firebase::auth::kAuthErrorInvalidProviderId);
}

TEST_F(FirebaseAuthTest,
       TestSignInFederatedProviderBadProviderIdFails_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  firebase::auth::FederatedOAuthProviderData provider_data(
      /*provider=*/"MadeUpProvider",
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "5321"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(auth_result_future, "SignInWithProvider",
                        provider_id)) {
    WaitForCompletion(
        auth_result_future.result()->user.ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

// ReauthenticateWithProvider
TEST_F(FirebaseAuthTest, TestSuccessfulReauthenticateWithProvider_DEPRECATED) {
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
      auth_->SignInWithProvider_DEPRECATED(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                        provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider_DEPRECATED(
            &provider),
        "ReauthenticateWithProvider_DEPRECATED", provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(auth_result_future, "SignInWithProvider",
                        provider_id)) {
    WaitForCompletion(
        auth_result_future.result()->user.ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulReauthenticateWithProviderNoScopes_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                        provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider_DEPRECATED(
            &provider),
        "ReauthenticateWithProvider_DEPRECATED", provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(auth_result_future, "SignInWithProvider",
                        provider_id)) {
    WaitForCompletion(
        auth_result_future.result()->user.ReauthenticateWithProvider(&provider),
        "ReauthenticateWithProvider", provider_id);
  }
  DeleteUser();
}

TEST_F(
    FirebaseAuthTest,
    TestSuccessfulReauthenticateWithProviderNoScopesNoCustomParameters_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                        provider_id)) {
    WaitForCompletion(
        sign_in_future.result()->user->ReauthenticateWithProvider_DEPRECATED(
            &provider),
        "ReauthenticateWithProvider_DEPRECATED", provider_id);
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
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->SignInWithProvider(&provider);
  if (WaitForCompletion(auth_result_future, "SignInWithProvider",
                        provider_id)) {
    provider_data.provider_id = "MadeUpProvider";
    firebase::auth::FederatedOAuthProvider provider(provider_data);
    firebase::Future<firebase::auth::AuthResult> reauth_future =
        auth_->current_user().ReauthenticateWithProvider(&provider);
    WaitForCompletion(reauth_future, "ReauthenticateWithProvider",
                      firebase::auth::kAuthErrorInvalidProviderId);
  }
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestReauthenticateWithProviderBadProviderIdFails_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(provider_id);
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->SignInWithProvider_DEPRECATED(&provider);
  if (WaitForCompletion(sign_in_future, "SignInWithProvider_DEPRECATED",
                        provider_id)) {
    provider_data.provider_id = "MadeUpProvider";
    firebase::auth::FederatedOAuthProvider provider(provider_data);
    firebase::Future<firebase::auth::SignInResult> reauth_future =
        auth_->current_user_DEPRECATED()->ReauthenticateWithProvider_DEPRECATED(
            &provider);
    WaitForCompletion(reauth_future, "ReauthenticateWithProvider_DEPRECATED",
                      firebase::auth::kAuthErrorInvalidProviderId);
  }
  DeleteUser();
}

// LinkWithProvider
TEST_F(FirebaseAuthTest, TestSuccessfulLinkFederatedProviderNoScopes) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_TRUE(auth_->current_user().is_valid());
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->current_user().LinkWithProvider(&provider);
  WaitForCompletion(auth_result_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulLinkFederatedProviderNoScopes_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;
  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user_DEPRECATED()->LinkWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider_DEPRECATED", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestSuccessfulLinkFederatedProviderNoScopesNoCustomParameters) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_TRUE(auth_->current_user().is_valid());
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->current_user().LinkWithProvider(&provider);
  WaitForCompletion(auth_result_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(
    FirebaseAuthTest,
    TestSuccessfulLinkFederatedProviderNoScopesNoCustomParameters_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id, /*scopes=*/{}, /*custom_parameters=*/{});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user_DEPRECATED()->LinkWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider_DEPRECATED", provider_id);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestSuccessfulLinkFederatedProvider) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_TRUE(auth_->current_user().is_valid());
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id,
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->current_user().LinkWithProvider(&provider);
  WaitForCompletion(auth_result_future, "LinkWithProvider", provider_id);
  DeleteUser();
}

TEST_F(FirebaseAuthTest, TestSuccessfulLinkFederatedProvider_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  const std::string provider_id =
      firebase::auth::GoogleAuthProvider::kProviderId;
  firebase::auth::FederatedOAuthProviderData provider_data(
      provider_id,
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user_DEPRECATED()->LinkWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider_DEPRECATED", provider_id);
  DeleteUser_DEPRECATED();
}

TEST_F(FirebaseAuthTest, TestLinkFederatedProviderBadProviderIdFails) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");
  ASSERT_TRUE(auth_->current_user().is_valid());
  firebase::auth::FederatedOAuthProviderData provider_data(
      /*provider=*/"MadeUpProvider",
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::AuthResult> auth_result_future =
      auth_->current_user().LinkWithProvider(&provider);
  WaitForCompletion(auth_result_future, "LinkWithProvider",
                    firebase::auth::kAuthErrorInvalidProviderId);
  DeleteUser();
}

TEST_F(FirebaseAuthTest,
       TestLinkFederatedProviderBadProviderIdFails_DEPRECATED) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  WaitForCompletion(auth_->SignInAnonymously_DEPRECATED(),
                    "SignInAnonymously_DEPRECATED");
  ASSERT_NE(auth_->current_user_DEPRECATED(), nullptr);
  firebase::auth::FederatedOAuthProviderData provider_data(
      /*provider=*/"MadeUpProvider",
      /*scopes=*/{"https://www.googleapis.com/auth/fitness.activity.read"},
      /*custom_parameters=*/{{"req_id", "1234"}});
  firebase::auth::FederatedOAuthProvider provider(provider_data);
  firebase::Future<firebase::auth::SignInResult> sign_in_future =
      auth_->current_user_DEPRECATED()->LinkWithProvider_DEPRECATED(&provider);
  WaitForCompletion(sign_in_future, "LinkWithProvider_DEPRECATED",
                    firebase::auth::kAuthErrorInvalidProviderId);
  DeleteUser_DEPRECATED();
}

#endif  // defined(ENABLE_OAUTH_TESTS)

}  // namespace firebase_testapp_automated
