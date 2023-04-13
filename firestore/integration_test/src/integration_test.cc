/*
 * Copyright 2021 Google LLC
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

/*
   IMPORTANT: This file is used by both the regular and the internal Firestore
   integration tests, and needs to be present and identical in both.

   Please ensure that any changes to this file are reflected in both of its
   locations:

     - firestore/integration_test/src/integration_test.cc
     - firestore/integration_test_internal/src/integration_test.cc

   If you make any modifications to this file in one of the two locations,
   please copy the modified file into the other location before committing the
   change.
*/

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "app_framework.h"  // NOLINT
#include "firebase/auth.h"
#include "firebase/firestore.h"
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

using app_framework::GetCurrentTimeInMicroseconds;
using app_framework::LogDebug;
using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;
using testing::ElementsAre;
using testing::Pair;
using testing::ResultOf;
using testing::UnorderedElementsAre;

// Very basic first-level tests for Firestore. More comprehensive integration
// tests are contained in other source files.
class FirebaseFirestoreBasicTest : public FirebaseTest {
 public:
  FirebaseFirestoreBasicTest();
  ~FirebaseFirestoreBasicTest() override;

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

  // Initialize Firestore.
  void InitializeFirestore();
  // Shut down Firestore.
  void TerminateFirestore();

  // Create a custom-named collection to work with for this test.
  firebase::firestore::CollectionReference GetTestCollection();

  // Add the DocumentReference to the cleanup list. At TearDown, all these
  // documents will be deleted.
  firebase::firestore::DocumentReference Cleanup(
      const firebase::firestore::DocumentReference& doc) {
    if (find(cleanup_documents_.begin(), cleanup_documents_.end(), doc) ==
        cleanup_documents_.end()) {
      cleanup_documents_.push_back(doc);
    }
    // Pass through the DocumentReference to simplify test code.
    return doc;
  }

  firebase::firestore::DocumentReference Doc(const char* suffix = "");

  static firebase::App* shared_app_;
  static firebase::auth::Auth* shared_auth_;

  bool initialized_ = false;
  firebase::firestore::Firestore* firestore_ = nullptr;

  std::string collection_name_;
  std::vector<firebase::firestore::DocumentReference> cleanup_documents_;
};

// Initialization flow looks like this:
//  - Once, before any tests run:
//  -   SetUpTestSuite: Initialize App and Auth. Sign in.
//  - For each test:
//    - SetUp: Initialize Firestore.
//    - Run the test.
//    - TearDown: Shut down Firestore.
//  - Once, after all tests are finished:
//  -   TearDownTestSuite: Sign out. Shut down Auth and App.

firebase::App* FirebaseFirestoreBasicTest::shared_app_;
firebase::auth::Auth* FirebaseFirestoreBasicTest::shared_auth_;

void FirebaseFirestoreBasicTest::SetUpTestSuite() { InitializeAppAndAuth(); }

void FirebaseFirestoreBasicTest::InitializeAppAndAuth() {
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

void FirebaseFirestoreBasicTest::TearDownTestSuite() { TerminateAppAndAuth(); }

void FirebaseFirestoreBasicTest::TerminateAppAndAuth() {
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

FirebaseFirestoreBasicTest::FirebaseFirestoreBasicTest() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseFirestoreBasicTest::~FirebaseFirestoreBasicTest() {
  // Must be cleaned up on exit.
  assert(firestore_ == nullptr);
}

void FirebaseFirestoreBasicTest::SetUp() {
  FirebaseTest::SetUp();
  InitializeFirestore();
}

void FirebaseFirestoreBasicTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    if (!cleanup_documents_.empty() && firestore_ && shared_app_) {
      LogDebug("Cleaning up documents.");
      std::vector<firebase::Future<void>> cleanups;
      cleanups.reserve(cleanup_documents_.size());
      for (int i = 0; i < cleanup_documents_.size(); ++i) {
        cleanups.push_back(cleanup_documents_[i].Delete());
      }
      for (int i = 0; i < cleanups.size(); ++i) {
        WaitForCompletion(cleanups[i], "FirebaseFirestoreBasicTest::TearDown");
      }
      cleanup_documents_.clear();
    }
  }
  TerminateFirestore();
  FirebaseTest::TearDown();
}

void FirebaseFirestoreBasicTest::InitializeFirestore() {
  LogDebug("Initializing Firebase Firestore.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(
      shared_app_, &firestore_, [](::firebase::App* app, void* target) {
        LogDebug("Attempting to initialize Firebase Firestore.");
        ::firebase::InitResult result;
        *reinterpret_cast<firebase::firestore::Firestore**>(target) =
            firebase::firestore::Firestore::GetInstance(app, &result);
        return result;
      });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeFirestore");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Firestore.");

  initialized_ = true;
}

void FirebaseFirestoreBasicTest::TerminateFirestore() {
  if (!initialized_) return;

  if (firestore_) {
    LogDebug("Shutdown the Firestore library.");
    delete firestore_;
    firestore_ = nullptr;
  }
  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseFirestoreBasicTest::SignIn() {
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

void FirebaseFirestoreBasicTest::SignOut() {
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

firebase::firestore::CollectionReference
FirebaseFirestoreBasicTest::GetTestCollection() {
  if (collection_name_.empty()) {
    // Generate a collection for the test data based on the time in
    // milliseconds.
    int64_t time_in_microseconds = GetCurrentTimeInMicroseconds();

    char buffer[21] = {0};
    snprintf(buffer, sizeof(buffer), "test%lld",
             static_cast<long long>(time_in_microseconds));  // NOLINT
    collection_name_ = buffer;
  }
  return firestore_->Collection(collection_name_.c_str());
}

firebase::firestore::DocumentReference FirebaseFirestoreBasicTest::Doc(
    const char* suffix) {
  std::string path =
      std::string(
          ::testing::UnitTest::GetInstance()->current_test_info()->name()) +
      suffix;
  return Cleanup(GetTestCollection().Document(path));
}

// Test cases below.

TEST_F(FirebaseFirestoreBasicTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseFirestoreBasicTest, TestSignIn) {
  EXPECT_TRUE(shared_auth_->current_user().is_valid());
}

TEST_F(FirebaseFirestoreBasicTest, TestAppAndSettings) {
  EXPECT_EQ(firestore_->app(), shared_app_);
  firebase::firestore::Settings settings = firestore_->settings();
  firestore_->set_settings(settings);
  // No comparison operator in settings, so just assume it worked if we didn't
  // crash.
}

TEST_F(FirebaseFirestoreBasicTest, TestNonWrappedTypes) {
  const firebase::Timestamp timestamp{1, 2};
  EXPECT_EQ(timestamp.seconds(), 1);
  EXPECT_EQ(timestamp.nanoseconds(), 2);
  const firebase::firestore::SnapshotMetadata metadata{
      /*has_pending_writes*/ false, /*is_from_cache*/ true};
  EXPECT_FALSE(metadata.has_pending_writes());
  EXPECT_TRUE(metadata.is_from_cache());
  const firebase::firestore::GeoPoint point{1.23, 4.56};
  EXPECT_EQ(point.latitude(), 1.23);
  EXPECT_EQ(point.longitude(), 4.56);
}

TEST_F(FirebaseFirestoreBasicTest, TestCollection) {
  firebase::firestore::CollectionReference collection =
      firestore_->Collection("foo");
  EXPECT_EQ(collection.firestore(), firestore_);
  EXPECT_EQ(collection.id(), "foo");
  EXPECT_EQ(collection.Document("bar").path(), "foo/bar");
}

TEST_F(FirebaseFirestoreBasicTest, TestDocument) {
  firebase::firestore::DocumentReference document =
      firestore_->Document("foo/bar");
  EXPECT_EQ(document.firestore(), firestore_);
  EXPECT_EQ(document.path(), "foo/bar");
}

TEST_F(FirebaseFirestoreBasicTest, TestSetGet) {
  SignIn();

  firebase::firestore::DocumentReference document = Doc();

  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"str", firebase::firestore::FieldValue::String("foo")},
          {"int", firebase::firestore::FieldValue::Integer(123)}}),
      "document.Set");
  firebase::Future<firebase::firestore::DocumentSnapshot> future =
      document.Get();
  WaitForCompletion(future, "document.Get");
  EXPECT_NE(future.result(), nullptr);
  EXPECT_THAT(future.result()->GetData(),
              UnorderedElementsAre(
                  Pair("str", firebase::firestore::FieldValue::String("foo")),
                  Pair("int", firebase::firestore::FieldValue::Integer(123))));
}

TEST_F(FirebaseFirestoreBasicTest, TestSetUpdateGet) {
  SignIn();

  firebase::firestore::DocumentReference document = Doc();

  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"str", firebase::firestore::FieldValue::String("foo")},
          {"int", firebase::firestore::FieldValue::Integer(123)}}),
      "document.Set");
  WaitForCompletion(
      document.Update(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(321)}}),
      "document.Update");
  firebase::Future<firebase::firestore::DocumentSnapshot> future =
      document.Get();
  WaitForCompletion(future, "document.Get");
  EXPECT_NE(future.result(), nullptr);
  EXPECT_THAT(future.result()->GetData(),
              UnorderedElementsAre(
                  Pair("str", firebase::firestore::FieldValue::String("foo")),
                  Pair("int", firebase::firestore::FieldValue::Integer(321))));
}

TEST_F(FirebaseFirestoreBasicTest, TestSetDelete) {
  SignIn();

  firebase::firestore::DocumentReference document = Doc();

  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"str", firebase::firestore::FieldValue::String("bar")},
          {"int", firebase::firestore::FieldValue::Integer(456)}}),
      "document.Set");

  WaitForCompletion(document.Delete(), "document.Delete");
  firebase::Future<firebase::firestore::DocumentSnapshot> future =
      document.Get();
  WaitForCompletion(future, "document.Get");
  EXPECT_NE(future.result(), nullptr);
  EXPECT_FALSE(future.result()->exists());

  // TODO: Test error cases (deleting invalid path, etc.)
}

TEST_F(FirebaseFirestoreBasicTest, TestDocumentListener) {
  SignIn();

  firebase::firestore::DocumentReference document = Doc();

  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"val", firebase::firestore::FieldValue::String("start")}}),
      "document.Set 0");

  std::vector<firebase::firestore::MapFieldValue> document_snapshots;
  firebase::firestore::ListenerRegistration registration =
      document.AddSnapshotListener(
          [&](const firebase::firestore::DocumentSnapshot& result,
              firebase::firestore::Error error_code,
              const std::string& error_message) {
            EXPECT_EQ(error_code, firebase::firestore::kErrorOk);
            EXPECT_EQ(error_message, "");
            document_snapshots.push_back(result.GetData());
          });

  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"val", firebase::firestore::FieldValue::String("update")}}),
      "document.Set 1");
  registration.Remove();
  WaitForCompletion(
      document.Set(firebase::firestore::MapFieldValue{
          {"val", firebase::firestore::FieldValue::String("final")}}),
      "document.Set 2");
  EXPECT_THAT(
      document_snapshots,
      ElementsAre(
          firebase::firestore::MapFieldValue{
              {"val", firebase::firestore::FieldValue::String("start")}},
          firebase::firestore::MapFieldValue{
              {"val", firebase::firestore::FieldValue::String("update")}}));
}

TEST_F(FirebaseFirestoreBasicTest, TestBatchWrite) {
  SignIn();

  firebase::firestore::DocumentReference document1 = Doc("1");
  firebase::firestore::DocumentReference document2 = Doc("2");

  firebase::firestore::WriteBatch batch = firestore_->batch();
  batch.Set(document1,
            firebase::firestore::MapFieldValue{
                {"str", firebase::firestore::FieldValue::String("first")}});
  batch.Set(document2,
            firebase::firestore::MapFieldValue{
                {"int", firebase::firestore::FieldValue::Integer(2222)}});
  WaitForCompletion(batch.Commit(), "batch.Commit");

  // Confirm the updated docs are correct.
  auto future1 = Doc("1").Get();
  WaitForCompletion(future1, "document.Get 1");
  EXPECT_THAT(future1.result()->GetData(),
              ElementsAre(Pair(
                  "str", firebase::firestore::FieldValue::String("first"))));

  auto future2 = Doc("2").Get();
  WaitForCompletion(future2, "document.Get 2");
  EXPECT_THAT(
      future2.result()->GetData(),
      ElementsAre(Pair("int", firebase::firestore::FieldValue::Integer(2222))));
}

TEST_F(FirebaseFirestoreBasicTest, TestRunTransaction) {
  SignIn();

  WaitForCompletion(
      Doc("1").Set(firebase::firestore::MapFieldValue{
          {"str", firebase::firestore::FieldValue::String("foo")}}),
      "document.Set 1");
  WaitForCompletion(
      Doc("2").Set(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(123)}}),
      "document.Set 2");
  WaitForCompletion(
      Doc("3").Set(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(678)}}),
      "document.Set 3");
  // Make sure there's no doc 4.
  WaitForCompletion(Doc("4").Delete(), "document.Delete 4");

  auto collection = GetTestCollection();

  auto transaction_future = firestore_->RunTransaction(
      [collection, this](firebase::firestore::Transaction& transaction,
                         std::string&) -> firebase::firestore::Error {
        // Set a default error to ensure that the error is filled in by Get().
        firebase::firestore::Error geterr =
            static_cast<firebase::firestore::Error>(-1);
        std::string getmsg = "[[uninitialized message]]";
        int64_t prev_int = transaction.Get(Doc("2"), &geterr, &getmsg)
                               .Get("int")
                               .integer_value();
        EXPECT_EQ(geterr, firebase::firestore::kErrorOk) << getmsg;

        // Update 1, increment 2, delete 3, add 4.
        transaction.Update(
            Doc("1"),
            firebase::firestore::MapFieldValue{
                {"int", firebase::firestore::FieldValue::Integer(456)}});
        LogDebug("Previous value: %lld", prev_int);
        transaction.Update(Doc("2"),
                           firebase::firestore::MapFieldValue{
                               {"int", firebase::firestore::FieldValue::Integer(
                                           prev_int + 100)}});
        transaction.Delete(Doc("3"));
        transaction.Set(
            Doc("4"),
            firebase::firestore::MapFieldValue{
                {"int", firebase::firestore::FieldValue::Integer(789)}});
        return firebase::firestore::kErrorOk;
      });

  WaitForCompletion(transaction_future, "firestore.RunTransaction");

  (void)Doc("4");  // Add new doc to cleanup list

  // Confirm the updated docs are correct.
  // First doc had an additional field added.
  auto future1 = Doc("1").Get();
  WaitForCompletion(future1, "document.Get 1");
  EXPECT_THAT(future1.result()->GetData(),
              UnorderedElementsAre(
                  Pair("str", firebase::firestore::FieldValue::String("foo")),
                  Pair("int", firebase::firestore::FieldValue::Integer(456))));

  // Second doc was incremented by 100.
  auto future2 = Doc("2").Get();
  WaitForCompletion(future2, "document.Get 2");
  EXPECT_THAT(
      future2.result()->GetData(),
      ElementsAre(Pair("int", firebase::firestore::FieldValue::Integer(223))));

  // Third doc was deleted.
  auto future3 = Doc("3").Get();
  WaitForCompletion(future3, "document.Get 3");
  EXPECT_FALSE(future3.result()->exists());

  // Fourth doc was newly added.
  auto future4 = Doc("4").Get();
  WaitForCompletion(future4, "document.Get 4");
  EXPECT_THAT(
      future4.result()->GetData(),
      ElementsAre(Pair("int", firebase::firestore::FieldValue::Integer(789))));
}

// TODO: Add test for failing transaction.

TEST_F(FirebaseFirestoreBasicTest, TestQuery) {
  SignIn();

  firebase::firestore::CollectionReference collection = GetTestCollection();
  // { "int" : 99, "int" : 100, "int" : 101, "int": 102, "str": "hello" }
  // Query for int > 100 should return only the 101 and 102 entries.
  WaitForCompletion(Doc("1").Set(firebase::firestore::MapFieldValue{
                        {"int", firebase::firestore::FieldValue::Integer(99)}}),
                    "document.Set 1");
  WaitForCompletion(
      Doc("2").Set(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(100)}}),
      "document.Set 2");
  WaitForCompletion(
      Doc("3").Set(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(101)}}),
      "document.Set 3");
  WaitForCompletion(
      Doc("4").Set(firebase::firestore::MapFieldValue{
          {"int", firebase::firestore::FieldValue::Integer(102)}}),
      "document.Set 4");
  WaitForCompletion(
      Doc("5").Set(firebase::firestore::MapFieldValue{
          {"str", firebase::firestore::FieldValue::String("hello")}}),
      "document.Set 5");

  firebase::firestore::Query query = collection.WhereGreaterThan(
      "int", firebase::firestore::FieldValue::Integer(100));
  auto query_future = query.Get();
  WaitForCompletion(query_future, "query.Get");
  EXPECT_NE(query_future.result(), nullptr);
  auto DocumentSnapshot_GetData =
      [](const firebase::firestore::DocumentSnapshot& ds) {
        return ds.GetData();
      };
  EXPECT_THAT(
      query_future.result()->documents(),
      UnorderedElementsAre(
          ResultOf(DocumentSnapshot_GetData,
                   ElementsAre(Pair(
                       "int", firebase::firestore::FieldValue::Integer(102)))),
          ResultOf(
              DocumentSnapshot_GetData,
              ElementsAre(Pair(
                  "int", firebase::firestore::FieldValue::Integer(101))))));
}

TEST_F(FirebaseFirestoreBasicTest, TestDocumentChangeNpos) {
  // This test may seem pointless, but it exists to avoid the long-standing
  // latent bug that `npos` was not defined on non-Android platforms and
  // would therefore fail to link if used.
  EXPECT_EQ(firebase::firestore::DocumentChange::npos,
            static_cast<std::size_t>(-1));
}

TEST_F(FirebaseFirestoreBasicTest,
       TestInvalidatingReferencesWhenDeletingFirestore) {
  delete firestore_;
  firestore_ = nullptr;
  // TODO: Ensure existing Firestore objects are invalidated.
}

TEST_F(FirebaseFirestoreBasicTest, TestInvalidatingReferencesWhenDeletingApp) {
  delete shared_app_;
  shared_app_ = nullptr;
  // TODO: Ensure existing Firestore objects are invalidated.

  // Fully shut down App and Auth so they can be reinitialized.
  TerminateAppAndAuth();
  // Reinitialize App and Auth.
  InitializeAppAndAuth();
}

// TODO: Add test for Auth signout while connected.

// TODO: Add additional comprehensive tests as needed.

}  // namespace firebase_testapp_automated
