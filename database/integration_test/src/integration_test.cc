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
#include <map>
#include <string>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
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

using testing::ElementsAre;
using testing::Key;
using testing::Not;
using testing::Pair;
using testing::PrintToString;
using testing::UnorderedElementsAre;

const char kIntegrationTestRootPath[] = "integration_test_data";
const size_t kLargeWriteStringLength = 1024 * 1024;  // 1 Megabyte.

// Returns true if the given given timestamp is within 15 minutes of the
// expected timestamp. The value compared against must be a Variant of type
// int64 representing milliseconds since the epoch.
MATCHER_P(TimestampIsNear, expected,
          std::string("Timestamp ") + (negation ? "isn't " : "is ") + "near " +
              PrintToString(expected)) {
  if (!arg.is_int64()) {
    *result_listener << "Variant must be of type Int64, but was type "
                     << firebase::Variant::TypeName(arg.type());
    return false;
  }

  // As long as our timestamp is within 15 minutes, it's correct enough
  // for our purposes.
  const int64_t kAllowedTimeDifferenceMilliseconds = 1000L * 60L * 15L;

  int64_t time_difference = arg.int64_value() - expected;

  return time_difference <= kAllowedTimeDifferenceMilliseconds &&
         time_difference >= -kAllowedTimeDifferenceMilliseconds;
}

TEST(TimestampIsNear, Matcher) {
  int64_t kOneMinuteInMilliseconds = 1L * 60L * 1000L;
  int64_t kTenMinutesInMilliseconds = 10L * 60L * 1000L;
  int64_t kTwentyMinutesInMilliseconds = 20L * 60L * 1000L;

  int64_t base_time = 1234567890;
  firebase::Variant current_time = base_time;
  EXPECT_THAT(current_time, TimestampIsNear(base_time));

  int64_t one_minute_later = base_time + kOneMinuteInMilliseconds;
  EXPECT_THAT(current_time, TimestampIsNear(one_minute_later));
  int64_t one_minutes_earlier = base_time - kOneMinuteInMilliseconds;
  EXPECT_THAT(current_time, TimestampIsNear(one_minutes_earlier));

  int64_t ten_minutes_later = base_time + kTenMinutesInMilliseconds;
  EXPECT_THAT(current_time, TimestampIsNear(ten_minutes_later));
  int64_t ten_minutes_earlier = base_time - kTenMinutesInMilliseconds;
  EXPECT_THAT(current_time, TimestampIsNear(ten_minutes_earlier));

  int64_t twenty_minutes_later = base_time + kTwentyMinutesInMilliseconds;
  EXPECT_THAT(current_time, Not(TimestampIsNear(twenty_minutes_later)));
  int64_t twenty_minutes_earlier = base_time - kTwentyMinutesInMilliseconds;
  EXPECT_THAT(current_time, Not(TimestampIsNear(twenty_minutes_earlier)));

  // Wrong types.
  EXPECT_THAT(firebase::Variant::Null(), Not(TimestampIsNear(0)));
  EXPECT_THAT(firebase::Variant::False(), Not(TimestampIsNear(0)));
  EXPECT_THAT(firebase::Variant::EmptyString(), Not(TimestampIsNear(0)));
}

class FirebaseDatabaseTest : public FirebaseTest {
 public:
  FirebaseDatabaseTest();
  ~FirebaseDatabaseTest() override;

  // Called once before all tests.
  static void SetUpTestSuite();
  // Called once after all tests.
  static void TearDownTestSuite();

  // Called at the start of each test.
  void SetUp() override;
  // Called after each test.
  void TearDown() override;

 protected:
  // Initialize Firebase App and Firebase Auth.
  static void InitializeAppAndAuth();
  // Shut down Firebase App and Firebase Auth.
  static void TerminateAppAndAuth();

  // Sign in an anonymous user.
  static void SignIn();
  // Sign out the current user, if applicable.
  // If this is an anonymous user, deletes the user instead,
  // to avoid polluting the user list.
  static void SignOut();

  // Initialize Firebase Database.
  void InitializeDatabase();
  // Shut down Firebase Database.
  void TerminateDatabase();

  firebase::database::DatabaseReference CreateWorkingPath(
      bool suppress_cleanup = false);

  static firebase::App* shared_app_;
  static firebase::auth::Auth* shared_auth_;

  static bool first_time_;

  bool initialized_;
  firebase::database::Database* database_;

  std::vector<firebase::database::DatabaseReference> cleanup_paths_;
};

// Initialization flow looks like this:
//  - Once, before any tests run:
//  -   SetUpTestSuite: Initialize App and Auth. Sign in.
//  - For each test:
//    - SetUp: Initialize Database.
//    - Run the test.
//    - TearDown: Shut down Database.
//  - Once, after all tests are finished:
//  -   TearDownTestSuite: Sign out. Shut down Auth and App.

firebase::App* FirebaseDatabaseTest::shared_app_;
firebase::auth::Auth* FirebaseDatabaseTest::shared_auth_;

bool FirebaseDatabaseTest::first_time_ = true;

void FirebaseDatabaseTest::SetUpTestSuite() { InitializeAppAndAuth(); }

void FirebaseDatabaseTest::InitializeAppAndAuth() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(__ANDROID__)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  ASSERT_NE(shared_app_, nullptr);

  LogDebug("Initializing Auth.");

  // Initialize Firebase Auth.
  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(shared_app_, &shared_auth_,
                         [](::firebase::App* app, void* target) {
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

  ASSERT_NE(shared_auth_, nullptr);

  // Sign in anonymously.
  SignIn();
}

void FirebaseDatabaseTest::TearDownTestSuite() { TerminateAppAndAuth(); }

void FirebaseDatabaseTest::TerminateAppAndAuth() {
  if (shared_auth_) {
    LogDebug("Signing out.");
    SignOut();
    LogDebug("Shutdown Auth.");
    delete shared_auth_;
    shared_auth_ = nullptr;
  }
  if (shared_app_) {
    LogDebug("Shutdown App.");
    delete shared_app_;
    shared_app_ = nullptr;
  }
}

FirebaseDatabaseTest::FirebaseDatabaseTest()
    : initialized_(false), database_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseDatabaseTest::~FirebaseDatabaseTest() {
  // Must be cleaned up on exit.
  assert(database_ == nullptr);
}

void FirebaseDatabaseTest::SetUp() {
  FirebaseTest::SetUp();
  InitializeDatabase();
}

void FirebaseDatabaseTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    if (!cleanup_paths_.empty() && database_ && shared_app_) {
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
  }
  TerminateDatabase();
  FirebaseTest::TearDown();
}

void FirebaseDatabaseTest::InitializeDatabase() {
  LogDebug("Initializing Firebase Database.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      shared_app_, &database_, [](::firebase::App* app, void* target) {
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

  // The first time we initialize database, enable persistence on mobile.
  // We need to do this here because on iOS you can only enable persistence
  // before ANY FIRDatabase instances are used.
  if (first_time_) {
    database_->set_persistence_enabled(true);
    first_time_ = false;
  }

  initialized_ = true;
}

void FirebaseDatabaseTest::TerminateDatabase() {
  if (!initialized_) return;

  if (database_) {
    LogDebug("Shutdown the Database library.");
    delete database_;
    database_ = nullptr;
  }
  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseDatabaseTest::SignIn() {
  if (shared_auth_->current_user().is_valid()) {
    // Already signed in.
    return;
  }
  LogDebug("Signing in.");
  firebase::Future<firebase::auth::AuthResult> sign_in_future =
      shared_auth_->SignInAnonymously();
  WaitForCompletion(sign_in_future, "SignInAnonymously");
  if (sign_in_future.error() != 0) {
    FAIL() << "Ensure your application has the Anonymous sign-in provider "
              "enabled in Firebase Console.";
  }
  ProcessEvents(100);
}

void FirebaseDatabaseTest::SignOut() {
  if (shared_auth_ == nullptr) {
    // Auth is not set up.
    return;
  }
  if (!shared_auth_->current_user().is_valid()) {
    // Already signed out.
    return;
  }
  if (shared_auth_->current_user().is_anonymous()) {
    // If signed in anonymously, delete the anonymous user.
    WaitForCompletion(shared_auth_->current_user().Delete(),
                      "DeleteAnonymousUser");
  } else {
    // If not signed in anonymously (e.g. if the tests were modified to sign in
    // as an actual user), just sign out normally.
    shared_auth_->SignOut();

    // Wait for the sign-out to finish.
    while (shared_auth_->current_user().is_valid()) {
      if (ProcessEvents(100)) break;
    }
  }
  EXPECT_FALSE(shared_auth_->current_user().is_valid());
}

firebase::database::DatabaseReference FirebaseDatabaseTest::CreateWorkingPath(
    bool suppress_cleanup) {
  auto ref = database_->GetReference(kIntegrationTestRootPath).PushChild();
  if (!suppress_cleanup) {
    cleanup_paths_.push_back(ref);
  }
  return ref;
}

// Test cases below.
TEST_F(FirebaseDatabaseTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseDatabaseTest, TestSignIn) {
  EXPECT_TRUE(shared_auth_->current_user().is_valid());
}

TEST_F(FirebaseDatabaseTest, TestCreateWorkingPath) {
  // This test is flaky on Android emulator for unknown reasons.
  SKIP_TEST_ON_ANDROID_EMULATOR;

  SignIn();
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
static const int kSimpleInt = 2;
static const int kSimplePriority = 100;
static const double kSimpleDouble = 3.4;
static const bool kSimpleBool = true;
static const double kLongDouble = 0.123456789876543;

TEST_F(FirebaseDatabaseTest, TestSetAndGetSimpleValues) {
  const char* test_name = test_info_->name();
  SignIn();
  firebase::database::DatabaseReference ref = CreateWorkingPath();

  {
    LogDebug("Setting values.");
    firebase::Future<void> f1 =
        ref.Child(test_name).Child("String").SetValue(kSimpleString);
    firebase::Future<void> f2 =
        ref.Child(test_name).Child("Int").SetValue(kSimpleInt);
    firebase::Future<void> f3 =
        ref.Child(test_name).Child("Double").SetValue(kSimpleDouble);
    firebase::Future<void> f4 =
        ref.Child(test_name).Child("Bool").SetValue(kSimpleBool);
    firebase::Future<void> f5 =
        ref.Child(test_name)
            .Child("Timestamp")
            .SetValue(firebase::database::ServerTimestamp());
    firebase::Future<void> f6 =
        ref.Child(test_name)
            .Child("IntAndPriority")
            .SetValueAndPriority(kSimpleInt, kSimplePriority);
    firebase::Future<void> f7 =
        ref.Child(test_name).Child("LongDouble").SetValue(kLongDouble);
    WaitForCompletion(f1, "SetSimpleString");
    WaitForCompletion(f2, "SetSimpleInt");
    WaitForCompletion(f3, "SetSimpleDouble");
    WaitForCompletion(f4, "SetSimpleBool");
    WaitForCompletion(f5, "SetSimpleTimestamp");
    WaitForCompletion(f6, "SetSimpleIntAndPriority");
    WaitForCompletion(f7, "SetLongDouble");
  }

  // Get the values that we just set, and confirm that they match what we
  // set them to.
  {
    LogDebug("Getting values.");
    firebase::Future<firebase::database::DataSnapshot> f1 =
        ref.Child(test_name).Child("String").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f2 =
        ref.Child(test_name).Child("Int").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f3 =
        ref.Child(test_name).Child("Double").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f4 =
        ref.Child(test_name).Child("Bool").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f5 =
        ref.Child(test_name).Child("Timestamp").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f6 =
        ref.Child(test_name).Child("IntAndPriority").GetValue();
    firebase::Future<firebase::database::DataSnapshot> f7 =
        ref.Child(test_name).Child("LongDouble").GetValue();
    WaitForCompletion(f1, "GetSimpleString");
    WaitForCompletion(f2, "GetSimpleInt");
    WaitForCompletion(f3, "GetSimpleDouble");
    WaitForCompletion(f4, "GetSimpleBool");
    WaitForCompletion(f5, "GetSimpleTimestamp");
    WaitForCompletion(f6, "GetSimpleIntAndPriority");
    WaitForCompletion(f7, "GetLongDouble");

    // Get the current time to compare to the Timestamp.
    int64_t current_time_milliseconds =
        static_cast<int64_t>(time(nullptr)) * 1000L;

    EXPECT_EQ(f1.result()->value().AsString(), kSimpleString);
    EXPECT_EQ(f2.result()->value().AsInt64(), kSimpleInt);
    EXPECT_EQ(f3.result()->value().AsDouble(), kSimpleDouble);
    EXPECT_EQ(f4.result()->value().AsBool(), kSimpleBool);
    EXPECT_THAT(f5.result()->value().AsInt64(),
                TimestampIsNear(current_time_milliseconds));
    EXPECT_EQ(f6.result()->value().AsInt64(), kSimpleInt);
    EXPECT_EQ(f6.result()->priority().AsInt64(), kSimplePriority);
    EXPECT_EQ(f7.result()->value().AsDouble(), kLongDouble);
  }
}

// A ValueListener that expects a specific value to be set.
class ExpectValueListener : public firebase::database::ValueListener {
 public:
  explicit ExpectValueListener(const firebase::Variant& value_to_expect)
      : value_to_expect_(value_to_expect),
        value_changed_(false),
        got_expected_value_(false) {}
  void OnValueChanged(
      const firebase::database::DataSnapshot& snapshot) override {
    value_changed_ = true;
    if (snapshot.value().AsString() == value_to_expect_.AsString()) {
      got_expected_value_ = true;
    } else {
      LogError("ExpectValueListener did not receive the expected result.");
    }
  }
  void OnCancelled(const firebase::database::Error& error_code,
                   const char* error_message) override {
    LogError("ExpectValueListener canceled: %d: %s", error_code, error_message);
    value_changed_ = true;
    got_expected_value_ = false;
  }

  bool WaitForExpectedValue() {
    const int kWaitForValueSeconds = 10;

    for (int i = 0; i < kWaitForValueSeconds; i++) {
      ProcessEvents(1000);
      if (value_changed_) {
        return got_expected_value_;
      }
    }
    LogError("ExpectValueListener timed out.");
    return got_expected_value_;
  }

 private:
  firebase::Variant value_to_expect_;
  bool value_changed_;
  bool got_expected_value_;
};

TEST_F(FirebaseDatabaseTest, TestLargeWrite) {
  const char* test_name = test_info_->name();
  SignIn();
  firebase::database::DatabaseReference ref = CreateWorkingPath();

  LogDebug("Setting value.");
  std::string large_string;
  large_string.reserve(kLargeWriteStringLength);
  for (uint32_t i = 0; i < kLargeWriteStringLength; i++) {
    large_string.push_back('1');
  }

  // Setup a listener to ensure the value changes properly.
  ExpectValueListener listener(large_string);
  ref.Child(test_name).Child("LargeString").AddValueListener(&listener);

  // Set the value.
  firebase::Future<void> f1 = ref.Child(test_name)
                                  .Child("LargeString")
                                  .SetValue(std::string(large_string));
  WaitForCompletion(f1, "SetLargeString");

  LogDebug("Listening for value to change as expected");
  ASSERT_TRUE(listener.WaitForExpectedValue());
  ref.Child(test_name).Child("LargeString").RemoveValueListener(&listener);
}

TEST_F(FirebaseDatabaseTest, TestReadingFromPersistanceWhileOffline) {
  const char* test_name = test_info_->name();

  SignIn();
  // database_->set_persistence_enabled(true); // Already set in Initialize().

  firebase::database::DatabaseReference ref = CreateWorkingPath(true);
  std::string working_url = ref.url();

  // Write a value that we can test for.
  const char* kPersistenceString = "Persistence Test!";
  WaitForCompletion(ref.Child(test_name).SetValue(kPersistenceString),
                    "SetValue (online)");

  // Shut down the realtime database and restart it, to make sure that
  // persistence persists across database object instances.
  delete database_;
  database_ = ::firebase::database::Database::GetInstance(shared_app_);

  // Offline mode.  If persistence works, we should still be able to fetch
  // our value even though we're offline.
  database_->GoOffline();
  ref = database_->GetReferenceFromUrl(working_url.c_str());

  {
    // Check that we can get the offline value via ValueListener.
    ExpectValueListener listener(kPersistenceString);
    ref.Child(test_name).AddValueListener(&listener);
    ASSERT_TRUE(listener.WaitForExpectedValue());
    ref.Child(test_name).RemoveValueListener(&listener);
  }

  // Check that we can get the offline value via GetValue().
  firebase::Future<firebase::database::DataSnapshot> offline_value =
      ref.Child(test_name).GetValue();
  WaitForCompletion(offline_value, "GetValue (offline)");
  EXPECT_EQ(offline_value.result()->value(), kPersistenceString);

  // Go back online.
  database_->GoOnline();
  SignIn();

  // Check the online value via GetValue().
  firebase::Future<firebase::database::DataSnapshot> online_value =
      ref.Child(test_name).GetValue();
  WaitForCompletion(online_value, "GetValue (online)");
  EXPECT_EQ(online_value.result()->value(), kPersistenceString);
  // Clean up manually.
  WaitForCompletion(ref.RemoveValue(), "RemoveValue");
}

TEST_F(FirebaseDatabaseTest, TestRunTransaction) {
  const char* test_name = test_info_->name();

  SignIn();
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

TEST_F(FirebaseDatabaseTest, TestUpdateChildren) {
  const char* test_name = test_info_->name();

  SignIn();

  firebase::database::DatabaseReference ref = CreateWorkingPath();
  WaitForCompletion(
      ref.Child(test_name).SetValue(std::map<std::string, std::string>{
          {"hello", "world"}, {"apples", "oranges"}, {"puppies", "kittens"}}),
      "SetValue");
  firebase::Future<firebase::database::DataSnapshot> read_future =
      ref.Child(test_name).GetValue();
  WaitForCompletion(read_future, "GetValue 1");
  EXPECT_THAT(read_future.result()->value().map(),
              testing::UnorderedElementsAre(Pair("hello", "world"),
                                            Pair("apples", "oranges"),
                                            Pair("puppies", "kittens")));
  firebase::Future<void> update_future = ref.Child(test_name).UpdateChildren(
      {{"hello", "galaxy"},
       {"pears", "grapes"},
       {"bunnies", "birbs"},
       {"timestamp", firebase::database::ServerTimestamp()}});
  WaitForCompletion(update_future, "UpdateChildren");
  read_future = ref.Child(test_name).GetValue();
  WaitForCompletion(read_future, "GetValue 2");
  int64_t current_time_milliseconds =
      static_cast<int64_t>(time(nullptr)) * 1000L;
  EXPECT_THAT(
      read_future.result()->value().map(),
      UnorderedElementsAre(
          Pair("apples", "oranges"), Pair("puppies", "kittens"),
          Pair("hello", "galaxy"), Pair("pears", "grapes"),
          Pair("bunnies", "birbs"),
          Pair("timestamp", TimestampIsNear(current_time_milliseconds))));
}

// TODO(drsanta): Disabled test due to an assertion in Firebase Android SDK.
// The issue should should be fixed in the next Android SDK release after
// 19.15.0.
#if !defined(ANDROID)
TEST_F(FirebaseDatabaseTest, TestQueryFiltering) {
  const char* test_name = test_info_->name();

  // Set up an initial corpus of data that we can then query filter.
  // This test exercises the following methods on Query:
  // OrderByChild, OrderByKey, OrderByPriority, OrderByValue
  // StartAt, EndAt, EqualTo
  // LimitToFirst, LimitToLast
  firebase::Variant initial_data = std::map<std::string, firebase::Variant>{
      {"apple", 1},
      {"banana", "two"},
      {"durian",
       std::map<std::string, firebase::Variant>{{"subkey", 3},
                                                {"value", "delicious"}}},
      {"fig", 3.3},
      {"cranberry",
       std::map<std::string, firebase::Variant>{{"subkey", 500}, {"value", 9}}},
      {"eggplant",
       std::map<std::string, firebase::Variant>{{"subkey", 100},
                                                {"value", "purple"}}},
      {"gooseberry", "honk"}};

  SignIn();

  firebase::database::DatabaseReference ref = CreateWorkingPath();
  // On mobile, keep this path synchronized to work around persistence issues.
#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  ref.SetKeepSynchronized(true);
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  WaitForCompletion(ref.Child(test_name).SetValue(initial_data), "SetValue");
  WaitForCompletion(ref.Child(test_name).Child("cranberry").SetPriority(100),
                    "SetPriority 1");
  WaitForCompletion(ref.Child(test_name).Child("banana").SetPriority(-100),
                    "SetPriority 2");
  WaitForCompletion(ref.Child(test_name).Child("eggplant").SetPriority(200),
                    "SetPriority 3");
  WaitForCompletion(ref.Child(test_name).Child("gooseberry").SetPriority(300),
                    "SetPriority 4");
  {
    firebase::Future<firebase::database::DataSnapshot> initial_read =
        ref.Child(test_name).GetValue();
    WaitForCompletion(initial_read, "GetValue (initial)");
    EXPECT_EQ(initial_read.result()->value(), initial_data);
    EXPECT_THAT(
        initial_read.result()->value().map(),
        UnorderedElementsAre(Key("apple"), Key("banana"), Key("cranberry"),
                             Key("durian"), Key("eggplant"), Key("fig"),
                             Key("gooseberry")));
  }

  // First, try ordering by key. Use LimitToFirst/Last to make sure we get the
  // first few or last values (even though std::map will always be sorted by
  // key).
  {
    firebase::Future<firebase::database::DataSnapshot> order_by_key =
        ref.Child(test_name).OrderByKey().LimitToFirst(3).GetValue();
    WaitForCompletion(order_by_key, "GetValue (OrderByKey)");
    EXPECT_THAT(
        order_by_key.result()->value().map(),
        UnorderedElementsAre(Key("apple"), Key("banana"), Key("cranberry")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> order_by_child =
        ref.Child(test_name).OrderByChild("subkey").LimitToLast(3).GetValue();
    WaitForCompletion(order_by_child, "GetValue (OrderByChild)");
    EXPECT_THAT(
        order_by_child.result()->value().map(),
        UnorderedElementsAre(Key("cranberry"), Key("durian"), Key("eggplant")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> order_by_priority =
        ref.Child(test_name).OrderByPriority().LimitToLast(3).GetValue();
    WaitForCompletion(order_by_priority, "GetValue (OrderByPriority)");
    EXPECT_THAT(order_by_priority.result()->value().map(),
                UnorderedElementsAre(Key("cranberry"), Key("eggplant"),
                                     Key("gooseberry")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> order_by_value =
        ref.Child(test_name).OrderByValue().LimitToFirst(3).GetValue();
    WaitForCompletion(order_by_value, "GetValue (OrderByValue)");
    EXPECT_THAT(
        order_by_value.result()->value().map(),
        UnorderedElementsAre(Key("apple"), Key("fig"), Key("gooseberry")));
  }

  // Try StartAt, EndAt, EqualTo.
  {
    firebase::Future<firebase::database::DataSnapshot> start_at =
        ref.Child(test_name).OrderByKey().StartAt("d").GetValue();
    WaitForCompletion(start_at, "GetValue (StartAt)");
    EXPECT_THAT(start_at.result()->value().map(),
                UnorderedElementsAre(Key("durian"), Key("eggplant"), Key("fig"),
                                     Key("gooseberry")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> end_at =
        ref.Child(test_name).OrderByKey().EndAt("f").GetValue();
    WaitForCompletion(end_at, "GetValue (EndAt)");
    EXPECT_THAT(
        end_at.result()->value().map(),
        UnorderedElementsAre(Key("apple"), Key("banana"), Key("cranberry"),
                             Key("durian"), Key("eggplant")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> start_and_end_at =
        ref.Child(test_name).OrderByKey().StartAt("c").EndAt("f").GetValue();
    WaitForCompletion(start_and_end_at, "GetValue (StartAndEndAt)");
    EXPECT_THAT(
        start_and_end_at.result()->value().map(),
        UnorderedElementsAre(Key("cranberry"), Key("durian"), Key("eggplant")));
  }
  {
    firebase::Future<firebase::database::DataSnapshot> equal_to =
        ref.Child(test_name).OrderByKey().EqualTo("fig").GetValue();
    WaitForCompletion(equal_to, "GetValue (EqualTo)");
    EXPECT_THAT(equal_to.result()->value().map(),
                UnorderedElementsAre(Key("fig")));
  }
}
#endif  // !defined(ANDROID)

// A simple ValueListener that logs the values it sees.
class LoggingValueListener : public firebase::database::ValueListener {
 public:
  LoggingValueListener() : got_error_(false) {}
  void OnValueChanged(
      const firebase::database::DataSnapshot& snapshot) override {
    LogDebug("  ValueListener.OnValueChanged(%s)",
             FirebaseTest::VariantToString(snapshot.value()).c_str());
    last_seen_value_ = snapshot.value();
    seen_values_.push_back(snapshot.value());
  }
  void OnCancelled(const firebase::database::Error& error_code,
                   const char* error_message) override {
    LogError("ValueListener got error: %d: %s", error_code, error_message);
    got_error_ = true;
  }
  const firebase::Variant& last_seen_value() { return last_seen_value_; }
  bool has_seen_value(const firebase::Variant& value) {
    return std::find(seen_values_.begin(), seen_values_.end(), value) !=
           seen_values_.end();
  }
  size_t num_seen_values() { return seen_values_.size(); }

  bool got_error() { return got_error_; }

 private:
  firebase::Variant last_seen_value_;
  std::vector<firebase::Variant> seen_values_;
  bool got_error_;
};

TEST_F(FirebaseDatabaseTest, TestAddAndRemoveListenerRace) {
  SKIP_TEST_ON_MOBILE;
  const char* test_name = test_info_->name();

  SignIn();

  firebase::database::DatabaseReference ref = CreateWorkingPath();
  WaitForCompletion(ref.Child(test_name).SetValue(0), "SetValue");

  const int kTestIterations = 100;

  // Ensure adding, removing and deleting a listener in rapid succession is safe
  // from race conditions.
  for (int i = 0; i < kTestIterations; i++) {
    LoggingValueListener* listener = new LoggingValueListener();
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    delete listener;
  }

  // Ensure adding, removing and deleting the same listener twice in rapid
  // succession is safe from race conditions.
  for (int i = 0; i < kTestIterations; i++) {
    LoggingValueListener* listener = new LoggingValueListener();
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    delete listener;
  }

  // Ensure adding, removing and deleting the same listener twice in rapid
  // succession is safe from race conditions.
  for (int i = 0; i < kTestIterations; i++) {
    LoggingValueListener* listener = new LoggingValueListener();
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    delete listener;
  }

  // Ensure removing a listener too many times is benign.
  for (int i = 0; i < kTestIterations; i++) {
    LoggingValueListener* listener = new LoggingValueListener();
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).AddValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    ref.Child(test_name).RemoveValueListener(listener);
    delete listener;
  }

  // Ensure adding, removing and deleting difference listeners in rapid
  // succession is safe from race conditions.
  for (int i = 0; i < kTestIterations; i++) {
    LoggingValueListener* listener1 = new LoggingValueListener();
    LoggingValueListener* listener2 = new LoggingValueListener();
    ref.Child(test_name).AddValueListener(listener1);
    ref.Child(test_name).AddValueListener(listener2);
    ref.Child(test_name).RemoveValueListener(listener1);
    ref.Child(test_name).RemoveValueListener(listener2);
    delete listener1;
    delete listener2;
  }
}

TEST_F(FirebaseDatabaseTest, TestValueListener) {
  const char* test_name = test_info_->name();

  SignIn();

  firebase::database::DatabaseReference ref = CreateWorkingPath();
  WaitForCompletion(ref.Child(test_name).SetValue(0), "SetValue");
  LoggingValueListener* listener = new LoggingValueListener();

  // Attach the listener, then set 3 values, which will trigger the
  // listener.
  ref.Child(test_name).AddValueListener(listener);

  // The listener's OnChanged callback is triggered once when the listener is
  // attached and again every time the data, including children, changes.
  // Wait for here for a moment for the initial values to be received.
  ProcessEvents(1000);

  WaitForCompletion(ref.Child(test_name).SetValue(1), "SetValue 1");
  WaitForCompletion(ref.Child(test_name).SetValue(2), "SetValue 2");
  WaitForCompletion(ref.Child(test_name).SetValue(3), "SetValue 3");

  // Wait a moment for the value listener to be triggered.
  ProcessEvents(1000);

  ref.Child(test_name).RemoveValueListener(listener);
  // Ensure that the listener is not triggered once removed.
  WaitForCompletion(ref.Child(test_name).SetValue(4), "SetValue 4");

  // Wait a moment to ensure the listener is not triggered.
  ProcessEvents(1000);

  EXPECT_FALSE(listener->got_error());
  // Ensure that the listener was only triggered 4 times, with the values
  // 0 (the initial value), 1, 2, and 3. The value 4 should not be present.
  EXPECT_EQ(listener->num_seen_values(), 4);
  EXPECT_TRUE(listener->has_seen_value(0));
  EXPECT_TRUE(listener->has_seen_value(1));
  EXPECT_TRUE(listener->has_seen_value(2));
  EXPECT_TRUE(listener->has_seen_value(3));
  EXPECT_FALSE(listener->has_seen_value(4));

  delete listener;
}

// An simple ChildListener class that simply logs the events it sees.
class LoggingChildListener : public firebase::database::ChildListener {
 public:
  LoggingChildListener() : got_error_(false) {}

  void OnChildAdded(const firebase::database::DataSnapshot& snapshot,
                    const char* previous_sibling) override {
    LogDebug("  ChildListener.OnChildAdded(%s)", snapshot.key());
    events_.push_back(std::string("added ") + snapshot.key());
  }
  void OnChildChanged(const firebase::database::DataSnapshot& snapshot,
                      const char* previous_sibling) override {
    LogDebug("  ChildListener.OnChildChanged(%s)", snapshot.key());
    events_.push_back(std::string("changed ") + snapshot.key());
  }
  void OnChildMoved(const firebase::database::DataSnapshot& snapshot,
                    const char* previous_sibling) override {
    LogDebug("  ChildListener.OnChildMoved(%s)", snapshot.key());
    events_.push_back(std::string("moved ") + snapshot.key());
  }
  void OnChildRemoved(
      const firebase::database::DataSnapshot& snapshot) override {
    LogDebug("  ChildListener.OnChildRemoved(%s)", snapshot.key());
    events_.push_back(std::string("removed ") + snapshot.key());
  }
  void OnCancelled(const firebase::database::Error& error_code,
                   const char* error_message) override {
    LogError("ChildListener got error: %d: %s", error_code, error_message);
    got_error_ = true;
  }

  const std::vector<std::string>& events() { return events_; }

  // Get the total number of Child events this listener saw.
  size_t total_events() { return events_.size(); }

  // Get the number of times this event was seen.
  int num_events(const std::string& event) {
    int count = 0;
    for (int i = 0; i < events_.size(); i++) {
      if (events_[i] == event) {
        count++;
      }
    }
    return count;
  }
  bool got_error() { return got_error_; }

 public:
  // Vector of strings defining the events we saw, in order.
  std::vector<std::string> events_;
  bool got_error_;
};

TEST_F(FirebaseDatabaseTest, TestChildListener) {
  const char* test_name = test_info_->name();

  SignIn();

  firebase::database::DatabaseReference ref = CreateWorkingPath();

  LoggingChildListener* listener = new LoggingChildListener();
  ref.Child(test_name)
      .OrderByChild("entity_type")
      .EqualTo("enemy")
      .AddChildListener(listener);

  // The listener's OnChanged callback is triggered once when the listener is
  // attached and again every time the data, including children, changes.
  // Wait for here for a moment for the initial values to be received.
  ProcessEvents(1000);

  EXPECT_FALSE(listener->got_error());

  // Perform a series of operations that we will then verify with the
  // ChildListener's event log.
  std::map<std::string, std::string> params;
  params["entity_name"] = "cobra";
  params["entity_type"] = "enemy";
  WaitForCompletion(
      ref.Child(test_name).Child("0").SetValueAndPriority(params, 0),
      "SetEntity0");  // added 0
  params["entity_name"] = "warrior";
  params["entity_type"] = "hero";
  WaitForCompletion(
      ref.Child(test_name).Child("1").SetValueAndPriority(params, 10),
      "SetEntity1");  // no event
  params["entity_name"] = "wizard";
  params["entity_type"] = "hero";
  WaitForCompletion(
      ref.Child(test_name).Child("2").SetValueAndPriority(params, 20),
      "SetEntity2");  // no event
  params["entity_name"] = "rat";
  params["entity_type"] = "enemy";
  WaitForCompletion(
      ref.Child(test_name).Child("3").SetValueAndPriority(params, 30),
      "SetEntity3");  // added 3
  params["entity_name"] = "thief";
  params["entity_type"] = "enemy";
  WaitForCompletion(
      ref.Child(test_name).Child("4").SetValueAndPriority(params, 40),
      "SetEntity4");  // added 4
  params["entity_name"] = "paladin";
  params["entity_type"] = "hero";
  WaitForCompletion(
      ref.Child(test_name).Child("5").SetValueAndPriority(params, 50),
      "SetEntity5");  // no event
  params["entity_name"] = "ghost";
  params["entity_type"] = "enemy";
  WaitForCompletion(
      ref.Child(test_name).Child("6").SetValueAndPriority(params, 60),
      "SetEntity6");  // added 6
  params["entity_name"] = "dragon";
  params["entity_type"] = "enemy";
  WaitForCompletion(
      ref.Child(test_name).Child("7").SetValueAndPriority(params, 70),
      "SetEntity7");  // added 7
  // Now the thief becomes a hero!
  WaitForCompletion(
      ref.Child(test_name).Child("4").Child("entity_type").SetValue("hero"),
      "SetEntity4Type");
  // Now the dragon becomes a super-dragon!
  WaitForCompletion(ref.Child(test_name)
                        .Child("7")
                        .Child("entity_name")
                        .SetValue("super-dragon"),
                    "SetEntity7Name");  // changed 7
  // Now the super-dragon becomes an mega-dragon!
  WaitForCompletion(ref.Child(test_name)
                        .Child("7")
                        .Child("entity_name")
                        .SetValue("mega-dragon"),
                    "SetEntity7NameAgain");  // changed 7
  // And now we change a hero entity, which the Query ignores.
  WaitForCompletion(ref.Child(test_name)
                        .Child("2")
                        .Child("entity_name")
                        .SetValue("super-wizard"),
                    "SetEntity2Value");  // no event
  // Now poof, the mega-dragon is gone.
  WaitForCompletion(ref.Child(test_name).Child("7").RemoveValue(),
                    "RemoveEntity7");  // removed 7

  // Wait a few seconds for the child listener to be triggered.
  ProcessEvents(1000);
  // Unregister the listener, so it stops triggering.
  ref.Child(test_name)
      .OrderByChild("entity_type")
      .EqualTo("enemy")
      .RemoveChildListener(listener);
  // Wait a few seconds for the child listener to finish up.
  ProcessEvents(1000);

  // Make one more change, to ensure the listener has been removed.
  WaitForCompletion(ref.Child(test_name).Child("6").SetPriority(0),
                    "SetEntity6Priority");
  // We are expecting to have the following events:
  EXPECT_THAT(listener->events(),
              ElementsAre("added 0", "added 3", "added 4", "added 6", "added 7",
                          "removed 4", "changed 7", "changed 7", "removed 7"));
  delete listener;
}

TEST_F(FirebaseDatabaseTest, TestOnDisconnect) {
  const char* test_name = test_info_->name();

  SignIn();
  firebase::database::DatabaseReference ref = CreateWorkingPath();
  std::string saved_url = ref.url();

  // Set up some ondisconnect handlers to set several values.
  WaitForCompletion(
      ref.Child(test_name).Child("SetValueTo1").OnDisconnect()->SetValue(1),
      "OnDisconnectSetValue1");
  WaitForCompletion(ref.Child(test_name)
                        .Child("SetValue2Priority3")
                        .OnDisconnect()
                        ->SetValueAndPriority(2, 3),
                    "OnDisconnect (SetValue2Priority3)");
  WaitForCompletion(ref.Child(test_name)
                        .Child("SetValueButThenCancel")
                        .OnDisconnect()
                        ->SetValue("Going to cancel this"),
                    "OnDisconnect (SetValueToCancel)");
  WaitForCompletion(ref.Child(test_name)
                        .Child("SetValueButThenCancel")
                        .OnDisconnect()
                        ->Cancel(),
                    "OnDisconnect (Cancel)");
  // Set a value that we will then remove on disconnect.
  WaitForCompletion(
      ref.Child(test_name).Child("RemoveValue").SetValue("Will be removed"),
      "SetValue (RemoveValue)");
  WaitForCompletion(
      ref.Child(test_name).Child("RemoveValue").OnDisconnect()->RemoveValue(),
      "OnDisconnect (RemoveValue)");
  // Set up a map to pass to OnDisconnect()->UpdateChildren().
  std::map<std::string, int> children;
  children.insert(std::make_pair("one", 1));
  children.insert(std::make_pair("two", 2));
  children.insert(std::make_pair("three", 3));
  WaitForCompletion(ref.Child(test_name)
                        .Child("UpdateChildren")
                        .OnDisconnect()
                        ->UpdateChildren(children),
                    "OnDisconnect (UpdateChildren)");

  // Set up a listener to wait for the ondisconnect action to occur.
  ExpectValueListener* listener = new ExpectValueListener(1);
  ref.Child(test_name).Child("SetValueTo1").AddValueListener(listener);
  LogDebug("Disconnecting...");
  database_->GoOffline();

  listener->WaitForExpectedValue();
  ref.Child(test_name).Child("SetValueTo1").RemoveValueListener(listener);

  // Let go of the reference we already had.
  ref = firebase::database::DatabaseReference();

  delete listener;
  listener = nullptr;

  LogDebug("Reconnecting...");
  database_->GoOnline();

  /// Check that the DisconnectionHandler actions were performed.
  /// Get a brand new reference to the location to be sure.
  ref = database_->GetReferenceFromUrl(saved_url.c_str());
  firebase::Future<firebase::database::DataSnapshot> future =
      ref.Child(test_name).GetValue();
  WaitForCompletion(future, "GetValue (OnDisconnectChanges)");
  const firebase::database::DataSnapshot& result = *future.result();
  EXPECT_TRUE(result.HasChild("SetValueTo1"));
  EXPECT_EQ(result.Child("SetValueTo1").value(), 1);
  EXPECT_TRUE(result.HasChild("SetValue2Priority3"));
  EXPECT_EQ(result.Child("SetValue2Priority3").value(), 2);
  EXPECT_EQ(result.Child("SetValue2Priority3").priority().AsInt64(), 3);
  EXPECT_FALSE(result.HasChild("RemoveValue"));
  EXPECT_FALSE(result.HasChild("SetValueButThenCancel"));
  EXPECT_TRUE(result.HasChild("UpdateChildren"));
  EXPECT_THAT(
      result.Child("UpdateChildren").value().map(),
      UnorderedElementsAre(Pair("one", 1), Pair("two", 2), Pair("three", 3)));
}

TEST_F(FirebaseDatabaseTest, TestInvalidatingReferencesWhenDeletingDatabase) {
  SignIn();

  // Create a file so we can get its metadata and check that it's properly
  // invalidated.
  firebase::database::DatabaseReference ref = CreateWorkingPath();
  cleanup_paths_.erase(std::find(cleanup_paths_.begin(), cleanup_paths_.end(),
                                 ref));  // We'll remove this path manually.
  firebase::database::Query query = ref.LimitToFirst(10);
  firebase::Future<void> set_future =
      ref.Child("Invalidating").SetValue(kSimpleString);
  WaitForCompletion(set_future, "SetValue");
  firebase::Future<firebase::database::DataSnapshot> get_future =
      ref.Child("Invalidating").GetValue();
  WaitForCompletion(get_future, "GetValue");
  firebase::database::DataSnapshot snapshot = *get_future.result();
  firebase::Future<void> delete_future =
      ref.Child("Invalidating").RemoveValue();
  WaitForCompletion(delete_future, "RemoveValue");

  EXPECT_TRUE(ref.is_valid());
  EXPECT_TRUE(query.is_valid());
  EXPECT_TRUE(snapshot.is_valid());
  EXPECT_NE(set_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_NE(get_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_NE(delete_future.status(), firebase::kFutureStatusInvalid);

  delete database_;
  database_ = nullptr;

  EXPECT_FALSE(ref.is_valid());
  EXPECT_FALSE(query.is_valid());
  EXPECT_FALSE(snapshot.is_valid());
  EXPECT_EQ(set_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(get_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(delete_future.status(), firebase::kFutureStatusInvalid);
}

TEST_F(FirebaseDatabaseTest, TestInvalidatingReferencesWhenDeletingApp) {
  SignIn();

  // Create a file so we can get its metadata and check that it's properly
  // invalidated.
  firebase::database::DatabaseReference ref = CreateWorkingPath(true);
  firebase::database::Query query = ref.LimitToFirst(10);
  firebase::Future<void> set_future =
      ref.Child("Invalidating").SetValue(kSimpleString);
  WaitForCompletion(set_future, "SetValue");
  firebase::Future<firebase::database::DataSnapshot> get_future =
      ref.Child("Invalidating").GetValue();
  WaitForCompletion(get_future, "GetValue");
  firebase::database::DataSnapshot snapshot = *get_future.result();
  firebase::Future<void> delete_future =
      ref.Child("Invalidating").SetValue(firebase::Variant::Null());
  WaitForCompletion(delete_future, "DeleteValue");

  EXPECT_TRUE(ref.is_valid());
  EXPECT_TRUE(query.is_valid());
  EXPECT_TRUE(snapshot.is_valid());
  EXPECT_NE(set_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_NE(get_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_NE(delete_future.status(), firebase::kFutureStatusInvalid);

  // Deleting App should invalidate all the objects and Futures, same as
  // deleting Database.
  delete shared_app_;
  shared_app_ = nullptr;

  EXPECT_FALSE(ref.is_valid());
  EXPECT_FALSE(query.is_valid());
  EXPECT_FALSE(snapshot.is_valid());
  EXPECT_EQ(set_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(get_future.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(delete_future.status(), firebase::kFutureStatusInvalid);

  // Fully shut down App and Auth so they can be reinitialized.
  TerminateAppAndAuth();
  // Reinitialize App and Auth.
  InitializeAppAndAuth();
}

TEST_F(FirebaseDatabaseTest, TestInfoConnected) {
  SignIn();

  // The entire test can be a bit flaky on mobile, as the iOS and
  // Android SDKs' .info/connected is not quite perfect.
#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  FLAKY_TEST_SECTION_BEGIN();
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

  firebase::database::DatabaseReference ref = CreateWorkingPath();
  // Force getting a value so that we are connected to the database.
  WaitForCompletion(ref.GetValue(), "GetValue 1 [ignored]");

  firebase::database::DatabaseReference info =
      database_->GetReference(".info").Child("connected");
  {
    auto connected = info.GetValue();
    WaitForCompletion(connected, "GetValue 2");
    EXPECT_EQ(connected.result()->value(), true);
  }
  LogDebug("Disconnecting...");
  database_->GoOffline();
  // Pause a moment to give the SDK time to realize we are disconnected.
  ProcessEvents(2000);
  {
    auto disconnected = info.GetValue();
    WaitForCompletion(disconnected, "GetValue 3");
    EXPECT_EQ(disconnected.result()->value(), false);
  }
  LogDebug("Reconnecting...");
  database_->GoOnline();
  // Pause a moment to give the SDK time to realize we are reconnected.
  ProcessEvents(5000);
  // Force getting a value so that we reconnect to the database.
  WaitForCompletion(ref.GetValue(), "GetValue 4 [ignored]");
  // Pause a moment to give the SDK time to realize we are reconnected.
#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // This is extra brittle on mobile, so give the SDK an EXTRA opportunity
  // to notice we are reconnected.
  ProcessEvents(2000);
  WaitForCompletion(ref.GetValue(), "GetValue 4B [ignored]");
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Pause another moment to REALLY give the SDK time to realize we are
  // reconnected.
  ProcessEvents(5000);
  {
    auto reconnected = info.GetValue();
    WaitForCompletion(reconnected, "GetValue 5");
    EXPECT_EQ(reconnected.result()->value(), true);
  }

#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  FLAKY_TEST_SECTION_END();
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
}

TEST_F(FirebaseDatabaseTest, TestGetReferenceWillNullArgument) {
  EXPECT_FALSE(database_->GetReference(nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestGetReferenceFromUrlWithNullArgument) {
  EXPECT_FALSE(database_->GetReferenceFromUrl(nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestDatabaseReferenceChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  EXPECT_FALSE(ref.Child(nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestDataSnapshotChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  firebase::Future<firebase::database::DataSnapshot> future = ref.GetValue();
  WaitForCompletion(future, "ref.GetValue()");
  const firebase::database::DataSnapshot* snapshot = future.result();
  EXPECT_FALSE(snapshot->Child(nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestDataSnapshotHasChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  firebase::Future<firebase::database::DataSnapshot> future = ref.GetValue();
  WaitForCompletion(future, "ref.GetValue()");
  const firebase::database::DataSnapshot* snapshot = future.result();
  EXPECT_FALSE(snapshot->HasChild(nullptr));
}

TEST_F(FirebaseDatabaseTest, TestMutableDataChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  firebase::Future<firebase::database::DataSnapshot> transaction_future;
  transaction_future = ref.RunTransaction(
      [](firebase::database::MutableData* data, void*) {
        // This is the best way we have to check validity of MutableData as we
        // don't currently expose an is_valid method.
        EXPECT_EQ(data->Child(nullptr).value(), firebase::Variant::Null());
        return firebase::database::kTransactionResultSuccess;
      },
      nullptr);
  WaitForCompletion(transaction_future, "RunTransaction");
}

TEST_F(FirebaseDatabaseTest, TestMutableDataHasChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  firebase::Future<firebase::database::DataSnapshot> transaction_future;
  transaction_future = ref.RunTransaction(
      [](firebase::database::MutableData* data, void*) {
        EXPECT_FALSE(data->HasChild(nullptr));
        return firebase::database::kTransactionResultSuccess;
      },
      nullptr);
  WaitForCompletion(transaction_future, "RunTransaction");
}

TEST_F(FirebaseDatabaseTest, TestQueryOrderByChildWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  EXPECT_FALSE(ref.OrderByChild(nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestQueryStartAtWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  EXPECT_FALSE(
      ref.StartAt(firebase::Variant("SomeString"), nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestQueryEndAtWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  EXPECT_FALSE(ref.EndAt(firebase::Variant("SomeString"), nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestQueryEqualToWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  EXPECT_FALSE(
      ref.EqualTo(firebase::Variant("SomeString"), nullptr).is_valid());
}

TEST_F(FirebaseDatabaseTest, TestValueListenerWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  ref.AddValueListener(nullptr);
}

TEST_F(FirebaseDatabaseTest, TestChildListenerWithNullArgument) {
  firebase::database::DatabaseReference ref =
      database_->GetReference("Nothing/will/be/uploaded/here");
  ref.AddChildListener(nullptr);
}

}  // namespace firebase_testapp_automated
