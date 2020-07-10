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

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App, Firebase Auth, and Firebase Firestore.
  void Initialize();
  // Shut down Firebase Firestore, Firebase Auth, and Firebase App.
  void Terminate();
  // Sign in an anonymous user.
  void SignIn();

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

  bool initialized_ = false;
  firebase::auth::Auth* auth_ = nullptr;
  firebase::firestore::Firestore* firestore_ = nullptr;

  std::string collection_name_;
  std::vector<firebase::firestore::DocumentReference> cleanup_documents_;
};

FirebaseFirestoreBasicTest::FirebaseFirestoreBasicTest() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseFirestoreBasicTest::~FirebaseFirestoreBasicTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(auth_ == nullptr);
  assert(firestore_ == nullptr);
}

void FirebaseFirestoreBasicTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseFirestoreBasicTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    if (!cleanup_documents_.empty()) {
      LogDebug("Cleaning up documents.");
      std::vector<firebase::Future<void>> cleanups;
      cleanups.reserve(cleanup_documents_.size());
      for (int i = 0; i < cleanup_documents_.size(); ++i) {
        cleanups.push_back(cleanup_documents_[i].Delete());
      }
      for (int i = 0; i < cleanups.size(); ++i) {
        WaitForCompletion(cleanups[i], "FirebaseDatabaseTest::TearDown");
      }
      cleanup_documents_.clear();
    }
    Terminate();
  }
  FirebaseTest::TearDown();
}

void FirebaseFirestoreBasicTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Auth and Firebase Firestore.");

  // 0th element has a reference to this object, the rest have the initializer
  // targets.
  void* initialize_targets[] = {&auth_, &firestore_};

  const firebase::ModuleInitializer::InitializerFn initializers[] = {
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Firebase Auth.");
        ::firebase::InitResult result;
        *reinterpret_cast<::firebase::auth::Auth**>(targets[0]) =
            ::firebase::auth::Auth::GetAuth(app, &result);
        return result;
      },
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Firebase Firestore.");
        ::firebase::InitResult result;
        firebase::firestore::Firestore* firestore =
            firebase::firestore::Firestore::GetInstance(app, &result);
        *reinterpret_cast<::firebase::firestore::Firestore**>(targets[1]) =
            firestore;
        return result;
      }};

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, initialize_targets, initializers,
                         sizeof(initializers) / sizeof(initializers[0]));

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Auth and Firebase Firestore.");

  initialized_ = true;
}

void FirebaseFirestoreBasicTest::Terminate() {
  if (!initialized_) return;

  if (firestore_) {
    LogDebug("Shutdown the Firestore library.");
    delete firestore_;
    firestore_ = nullptr;
  }
  if (auth_) {
    LogDebug("Shutdown the Auth library.");
    delete auth_;
    auth_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseFirestoreBasicTest::SignIn() {
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
  SignIn();
  EXPECT_NE(auth_->current_user(), nullptr);
}

TEST_F(FirebaseFirestoreBasicTest, TestAppAndSettings) {
  EXPECT_EQ(firestore_->app(), app_);
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
  SKIP_TEST_IF_USING_STLPORT;  // STLPort uses EventListener<T> rather than
                               // std::function.
#if !defined(STLPORT)
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
              firebase::firestore::Error error) {
            EXPECT_EQ(error, firebase::firestore::kErrorOk);
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
#endif  // !defined(STLPORT)
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

TEST_F(FirebaseFirestoreBasicTest,
       TestInvalidatingReferencesWhenDeletingFirestore) {
  delete firestore_;
  firestore_ = nullptr;
  // TODO: Ensure existing Firestore objects are invalidated.
}

TEST_F(FirebaseFirestoreBasicTest, TestInvalidatingReferencesWhenDeletingApp) {
  delete app_;
  app_ = nullptr;
  // TODO: Ensure existing Firestore objects are invalidated.
}

// TODO: Add test for Auth signout while connected.

// TODO: Add additional comprehensive tests as needed.

}  // namespace firebase_testapp_automated
