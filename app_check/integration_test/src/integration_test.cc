// Copyright 2022 Google Inc. All rights reserved.
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
#include <map>
#include <string>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/app_check.h"
#include "firebase/app_check/app_attest_provider.h"
#include "firebase/app_check/debug_provider.h"
#include "firebase/app_check/device_check_provider.h"
#include "firebase/app_check/play_integrity_provider.h"
#include "firebase/app_check/safety_net_provider.h"
#include "firebase/auth.h"
#include "firebase/database.h"
#include "firebase/internal/platform.h"
#include "firebase/util.h"
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

using app_framework::LogDebug;
using app_framework::LogError;
using app_framework::LogInfo;

using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

using testing::Pair;
using testing::UnorderedElementsAre;

const char kIntegrationTestRootPath[] = "integration_test_data";

class FirebaseAppCheckTest : public FirebaseTest {
 public:
  FirebaseAppCheckTest();
  ~FirebaseAppCheckTest() override;

  // Called after each test.
  void TearDown() override;

 protected:
  // Initialize App Check with the Debug provider.
  void InitializeAppCheckWithDebug();
  // Shut down App Check.
  void TerminateAppCheck();

  // Initialize Firebase App.
  void InitializeApp();
  // Shut down Firebase App.
  void TerminateApp();

  // Initialize Firebase Auth.
  void InitializeAuth();
  // Shut down Firebase Auth.
  void TerminateAuth();

  // Sign in an anonymous user.
  void SignIn();
  // Sign out the current user, if applicable.
  // If this is an anonymous user, deletes the user instead,
  // to avoid polluting the user list.
  void SignOut();

  // Initialize Firebase Database.
  void InitializeDatabase();
  // Shut down Firebase Database.
  void TerminateDatabase();

  // Initialize everything needed for Database tests.
  void InitializeAppAuthDatabase();

  firebase::database::DatabaseReference CreateWorkingPath(
      bool suppress_cleanup = false);

  firebase::App* app_;
  firebase::auth::Auth* auth_;

  bool initialized_;
  firebase::database::Database* database_;

  std::vector<firebase::database::DatabaseReference> cleanup_paths_;
};

// Initialization flow looks like this:
//  - For each test:
//    - Optionally initialize App Check.
//    - Initialize App, and any additional products.
//    - Run tests.
//    - TearDown: Shutdowns down everything automatically.

void FirebaseAppCheckTest::InitializeAppCheckWithDebug() {
  LogDebug("Initialize Firebase App Check with Debug Provider");

  firebase::app_check::AppCheck::SetAppCheckProviderFactory(
      firebase::app_check::DebugAppCheckProviderFactory::GetInstance());
}

void FirebaseAppCheckTest::TerminateAppCheck() {}

void FirebaseAppCheckTest::InitializeApp() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(__ANDROID__)
  app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                 app_framework::GetActivity());
#else
  app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  ASSERT_NE(app_, nullptr);

  firebase::SetLogLevel(firebase::kLogLevelVerbose);
}

void FirebaseAppCheckTest::TerminateApp() {
  if (app_) {
    LogDebug("Shutdown App.");
    delete app_;
    app_ = nullptr;
  }
}

FirebaseAppCheckTest::FirebaseAppCheckTest()
    : initialized_(false),
      app_(nullptr),
      auth_(nullptr),
      database_(nullptr),
      cleanup_paths_() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseAppCheckTest::~FirebaseAppCheckTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
}

void FirebaseAppCheckTest::TearDown() {
  // Teardown all the products
  TerminateDatabase();
  TerminateAuth();
  TerminateApp();
  TerminateAppCheck();
  FirebaseTest::TearDown();
}

void FirebaseAppCheckTest::InitializeAuth() {
  LogDebug("Initializing Auth.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, &auth_, [](::firebase::App* app, void* target) {
    LogDebug("Attempting to initialize Firebase Auth.");
    ::firebase::InitResult result;
    *reinterpret_cast<firebase::auth::Auth**>(target) =
        ::firebase::auth::Auth::GetAuth(app, &result);
    return result;
  });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeAuth");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Auth.");

  ASSERT_NE(auth_, nullptr);

  // Sign in anonymously.
  SignIn();
}

void FirebaseAppCheckTest::TerminateAuth() {
  if (auth_) {
    LogDebug("Signing out.");
    SignOut();
    LogDebug("Shutdown Auth.");
    delete auth_;
    auth_ = nullptr;
  }
}

void FirebaseAppCheckTest::InitializeDatabase() {
  LogDebug("Initializing Firebase Database.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      app_, &database_, [](::firebase::App* app, void* target) {
        LogDebug("Attempting to initialize Firebase Database.");
        ::firebase::InitResult result;
        *reinterpret_cast<firebase::database::Database**>(target) =
            firebase::database::Database::GetInstance(app, &result);
        return result;
      });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeDatabase");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Database.");

  initialized_ = true;
}

void FirebaseAppCheckTest::TerminateDatabase() {
  if (!initialized_) return;

  if (database_) {
    if (!cleanup_paths_.empty() && database_ && app_) {
      LogDebug("Cleaning up...");
      std::vector<firebase::Future<void>> cleanups;
      cleanups.reserve(cleanup_paths_.size());
      for (int i = 0; i < cleanup_paths_.size(); ++i) {
        cleanups.push_back(cleanup_paths_[i].RemoveValue());
      }
      for (int i = 0; i < cleanups.size(); ++i) {
        std::string cleanup_name = "Cleanup (" + cleanup_paths_[i].url() + ")";
        WaitForCompletion(cleanups[i], cleanup_name.c_str());
      }
      cleanup_paths_.clear();
    }

    LogDebug("Shutdown the Database library.");
    delete database_;
    database_ = nullptr;
  }
  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseAppCheckTest::InitializeAppAuthDatabase() {
  InitializeApp();
  InitializeAuth();
  InitializeDatabase();
}

void FirebaseAppCheckTest::SignIn() {
  if (auth_->current_user() != nullptr) {
    // Already signed in.
    return;
  }
  LogDebug("Signing in.");
  firebase::Future<firebase::auth::User*> sign_in_future =
      auth_->SignInAnonymously();
  WaitForCompletion(sign_in_future, "SignInAnonymously");
  if (sign_in_future.error() != 0) {
    FAIL() << "Ensure your application has the Anonymous sign-in provider "
              "enabled in Firebase Console.";
  }
  ProcessEvents(100);
}

void FirebaseAppCheckTest::SignOut() {
  if (auth_ == nullptr) {
    // Auth is not set up.
    return;
  }
  if (auth_->current_user() == nullptr) {
    // Already signed out.
    return;
  }
  if (auth_->current_user()->is_anonymous()) {
    // If signed in anonymously, delete the anonymous user.
    WaitForCompletion(auth_->current_user()->Delete(), "DeleteAnonymousUser");
    // If there was a problem deleting the user, try to sign out at least.
    if (auth_->current_user()) {
      auth_->SignOut();
    }
  } else {
    // If not signed in anonymously (e.g. if the tests were modified to sign in
    // as an actual user), just sign out normally.
    auth_->SignOut();

    // Wait for the sign-out to finish.
    while (auth_->current_user() != nullptr) {
      if (ProcessEvents(100)) break;
    }
  }
  EXPECT_EQ(auth_->current_user(), nullptr);
}

firebase::database::DatabaseReference FirebaseAppCheckTest::CreateWorkingPath(
    bool suppress_cleanup) {
  auto ref = database_->GetReference(kIntegrationTestRootPath).PushChild();
  if (!suppress_cleanup) {
    cleanup_paths_.push_back(ref);
  }
  return ref;
}

// Test cases below.
TEST_F(FirebaseAppCheckTest, TestInitializeAndTerminate) {
  InitializeAppCheckWithDebug();
  InitializeApp();
}

TEST_F(FirebaseAppCheckTest, TestSignIn) {
  InitializeAppCheckWithDebug();
  InitializeApp();
  InitializeAuth();
  EXPECT_NE(auth_->current_user(), nullptr);
}

TEST_F(FirebaseAppCheckTest, TestAppAttestProvider) {
  firebase::app_check::AppAttestProviderFactory* factory =
      firebase::app_check::AppAttestProviderFactory::GetInstance();
#if FIREBASE_PLATFORM_IOS
  EXPECT_NE(factory, nullptr);
  InitializeApp();
  firebase::app_check::AppCheckProvider* provider =
      factory->CreateProvider(app_);
  EXPECT_NE(provider, nullptr);
#else
  EXPECT_EQ(factory, nullptr);
#endif
}

TEST_F(FirebaseAppCheckTest, TestDeviceCheckProvider) {
  firebase::app_check::DeviceCheckProviderFactory* factory =
      firebase::app_check::DeviceCheckProviderFactory::GetInstance();
#if FIREBASE_PLATFORM_IOS
  EXPECT_NE(factory, nullptr);
  InitializeApp();
  firebase::app_check::AppCheckProvider* provider =
      factory->CreateProvider(app_);
  EXPECT_NE(provider, nullptr);
#else
  EXPECT_EQ(factory, nullptr);
#endif
}

TEST_F(FirebaseAppCheckTest, TestPlayIntegrityProvider) {
  firebase::app_check::PlayIntegrityProviderFactory* factory =
      firebase::app_check::PlayIntegrityProviderFactory::GetInstance();
#if FIREBASE_PLATFORM_ANDROID
  EXPECT_NE(factory, nullptr);
  InitializeApp();
  firebase::app_check::AppCheckProvider* provider =
      factory->CreateProvider(app_);
  EXPECT_NE(provider, nullptr);
#else
  EXPECT_EQ(factory, nullptr);
#endif
}

TEST_F(FirebaseAppCheckTest, TestSafetyNetProvider) {
  firebase::app_check::SafetyNetProviderFactory* factory =
      firebase::app_check::SafetyNetProviderFactory::GetInstance();
#if FIREBASE_PLATFORM_ANDROID
  EXPECT_NE(factory, nullptr);
  InitializeApp();
  firebase::app_check::AppCheckProvider* provider =
      factory->CreateProvider(app_);
  EXPECT_NE(provider, nullptr);
#else
  EXPECT_EQ(factory, nullptr);
#endif
}

TEST_F(FirebaseAppCheckTest, TestDatabaseFailure) {
  // Don't initialize App Check this time. Database should fail.
  InitializeAppAuthDatabase();
  firebase::database::DatabaseReference ref = CreateWorkingPath();
  const char* test_name = test_info_->name();
  firebase::Future<void> f = ref.Child(test_name).SetValue("test");
  // It is unclear if this should fail, or hang, so disabled for now.
  // WaitForCompletion(f, "SetString");
}

TEST_F(FirebaseAppCheckTest, TestDatabaseCreateWorkingPath) {
  InitializeAppCheckWithDebug();
  InitializeAppAuthDatabase();
  firebase::database::DatabaseReference working_path = CreateWorkingPath();
  LogInfo("Database URL: %s", working_path.url().c_str());
  EXPECT_TRUE(working_path.is_valid());
  EXPECT_FALSE(working_path.url().empty());
  EXPECT_EQ(working_path.url().find(database_->GetReference().url()), 0)
      << "Working path URL (" << working_path.url()
      << ") does not begin with root URL (" << database_->GetReference().url()
      << ")";
}

static const char kSimpleString[] = "Some simple string";

TEST_F(FirebaseAppCheckTest, TestDatabaseSetAndGet) {
  InitializeAppCheckWithDebug();
  InitializeAppAuthDatabase();

  const char* test_name = test_info_->name();
  firebase::database::DatabaseReference ref = CreateWorkingPath();

  {
    LogDebug("Setting value.");
    firebase::Future<void> f1 =
        ref.Child(test_name).Child("String").SetValue(kSimpleString);
    WaitForCompletion(f1, "SetSimpleString");
  }

  // Get the values that we just set, and confirm that they match what we
  // set them to.
  {
    LogDebug("Getting value.");
    firebase::Future<firebase::database::DataSnapshot> f1 =
        ref.Child(test_name).Child("String").GetValue();
    WaitForCompletion(f1, "GetSimpleString");

    EXPECT_EQ(f1.result()->value().AsString(), kSimpleString);
  }
}

TEST_F(FirebaseAppCheckTest, TestRunTransaction) {
  InitializeAppCheckWithDebug();
  InitializeAppAuthDatabase();

  const char* test_name = test_info_->name();

  firebase::database::DatabaseReference ref = CreateWorkingPath();

  // Test running a transaction. This will call RunTransaction and set
  // some values, including incrementing the player's score.
  firebase::Future<firebase::database::DataSnapshot> transaction_future;
  static const int kInitialScore = 500;
  // Set an initial score of 500 points.
  WaitForCompletion(
      ref.Child(test_name).Child("player_score").SetValue(kInitialScore),
      "SetInitialScoreValue");
  // The transaction will set the player's item and class, and increment
  // their score by 100 points.
  int score_delta = 100;
  transaction_future = ref.Child(test_name).RunTransaction(
      [](firebase::database::MutableData* data, void* score_delta_void) {
        LogDebug("  Transaction function executing.");
        data->Child("player_item").set_value("Fire sword");
        data->Child("player_class").set_value("Warrior");
        // Increment the current score by 100.
        int64_t score =
            data->Child("player_score").value().AsInt64().int64_value();
        data->Child("player_score")
            .set_value(score + *reinterpret_cast<int*>(score_delta_void));
        return firebase::database::kTransactionResultSuccess;
      },
      &score_delta);
  WaitForCompletion(transaction_future, "RunTransaction");

  // If the transaction succeeded, let's read back the values that were
  // written to confirm they match.
  if (transaction_future.error() == firebase::database::kErrorNone) {
    firebase::Future<firebase::database::DataSnapshot> read_future =
        ref.Child(test_name).GetValue();
    WaitForCompletion(read_future, "ReadTransactionResults");

    const firebase::database::DataSnapshot& read_result = *read_future.result();
    EXPECT_EQ(read_result.children_count(), 3);
    EXPECT_TRUE(read_result.HasChild("player_item"));
    EXPECT_EQ(read_result.Child("player_item").value(), "Fire sword");
    EXPECT_TRUE(read_result.HasChild("player_class"));
    EXPECT_EQ(read_result.Child("player_class").value(), "Warrior");
    EXPECT_TRUE(read_result.HasChild("player_score"));
    EXPECT_EQ(read_result.Child("player_score").value().AsInt64(),
              kInitialScore + score_delta);
    EXPECT_EQ(read_result.value(), transaction_future.result()->value());
  }
}

}  // namespace firebase_testapp_automated
