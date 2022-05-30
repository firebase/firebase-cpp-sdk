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

#include <future>
#include <string>
#include <vector>

#include "firebase/firestore.h"
#include "firebase_test_framework.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/bundle_builder.h"
#include "util/event_accumulator.h"
#include "util/future_test_util.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRBundlesTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/BundleTest.java
// The iOS test names start with the mandatory test prefix while Android test
// names do not. Here we use the Android test names.

namespace firebase {
namespace firestore {
namespace {

// Query names from the testing bundle in bundle_builder.cc
constexpr char kLimitQueryName[] = "limit";
constexpr char kLimitToLastQueryName[] = "limit-to-last";

MATCHER_P(
    InProgressWithLoadedDocuments,
    expected_documents,
    std::string("Is a valid InProgress update with documents_loaded() == ") +
        std::to_string(expected_documents)) {
  std::string state_string =
      arg.state() == LoadBundleTaskProgress::State::kInProgress
          ? "kInProgress"
          : (arg.state() == LoadBundleTaskProgress::State::kError ? "kError"
                                                                  : "kSuccess");
  *result_listener << "progress state() is " << state_string
                   << " documents_loaded() is: " << arg.documents_loaded()
                   << " total_documents() is: " << arg.total_documents()
                   << " bytes_loaded() is: " << arg.bytes_loaded()
                   << " total_bytes() is: " << arg.total_bytes();
  return arg.state() == LoadBundleTaskProgress::State::kInProgress &&
         arg.documents_loaded() == expected_documents &&
         arg.documents_loaded() <= arg.total_documents() &&
         arg.bytes_loaded() <= arg.total_bytes();
}

void VerifySuccessProgress(LoadBundleTaskProgress progress) {
  EXPECT_EQ(progress.state(), LoadBundleTaskProgress::State::kSuccess);
  EXPECT_EQ(progress.documents_loaded(), progress.total_documents());
  EXPECT_EQ(progress.bytes_loaded(), progress.total_bytes());
}

void VerifyErrorProgress(LoadBundleTaskProgress progress) {
  EXPECT_EQ(progress.state(), LoadBundleTaskProgress::State::kError);
  EXPECT_EQ(progress.documents_loaded(), 0);
  EXPECT_EQ(progress.bytes_loaded(), 0);
}

std::string CreateTestBundle(Firestore* db) {
  return CreateBundle(db->app()->options().project_id());
}

void SetPromiseValueWhenUpdateIsFinal(
    LoadBundleTaskProgress progress,
    std::promise<void>& final_update_received) {
  if (progress.state() == LoadBundleTaskProgress::State::kError ||
      progress.state() == LoadBundleTaskProgress::State::kSuccess) {
    final_update_received.set_value();
  }
}

class BundleTest : public FirestoreIntegrationTest {
 protected:
  void SetUp() override {
    FirestoreIntegrationTest::SetUp();
    // Clear the storage to avoid tests interfering with each other, since they
    // will be loading the same bundle file.
    ASSERT_THAT(TestFirestore()->ClearPersistence(), FutureSucceeds());
  }

  template <typename T>
  T AwaitResult(const Future<T>& future) {
    auto* ptr = Await(future);
    EXPECT_NE(ptr, nullptr);
    // Return default instance instead of crashing.
    if (ptr == nullptr) {
      return {};
    }

    return *ptr;
  }

  void VerifyQueryResults(Firestore* db) {
    {
      auto snapshot = AwaitResult(db->Collection("coll-1").Get(Source::kCache));
      EXPECT_THAT(
          QuerySnapshotToValues(snapshot),
          testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                             {"bar", FieldValue::Integer(1)}},
                               MapFieldValue{{"k", FieldValue::String("b")},
                                             {"bar", FieldValue::Integer(2)}}));
    }

    {
      Query limit = AwaitResult(db->NamedQuery(kLimitQueryName));
      auto limit_snapshot = AwaitResult(limit.Get(Source::kCache));
      EXPECT_THAT(
          QuerySnapshotToValues(limit_snapshot),
          testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("b")},
                                             {"bar", FieldValue::Integer(2)}}));
    }

    {
      Query limit_to_last = AwaitResult(db->NamedQuery(kLimitToLastQueryName));
      auto limit_to_last_snapshot =
          AwaitResult(limit_to_last.Get(Source::kCache));
      EXPECT_THAT(
          QuerySnapshotToValues(limit_to_last_snapshot),
          testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                             {"bar", FieldValue::Integer(1)}}));
    }
  }
};

TEST_F(BundleTest, CanLoadBundlesWithoutProgressUpdates) {
  Firestore* db = TestFirestore();
  auto bundle = CreateTestBundle(db);

  Future<LoadBundleTaskProgress> result = db->LoadBundle(bundle);

  VerifySuccessProgress(AwaitResult(result));
  VerifyQueryResults(db);
}

TEST_F(BundleTest, CanLoadBundlesWithProgressUpdates) {
  Firestore* db = TestFirestore();
  auto bundle = CreateTestBundle(db);

  std::vector<LoadBundleTaskProgress> progresses;
  std::promise<void> final_update;
  Future<LoadBundleTaskProgress> result = db->LoadBundle(
      bundle,
      [&progresses, &final_update](const LoadBundleTaskProgress& progress) {
        progresses.push_back(progress);
        SetPromiseValueWhenUpdateIsFinal(progress, final_update);
      });

  auto final_progress = AwaitResult(result);

  // 4 progresses will be reported: initial, document 1, document 2, final
  // success.
  final_update.get_future().wait();
  ASSERT_EQ(progresses.size(), 4);
  EXPECT_THAT(progresses[0], InProgressWithLoadedDocuments(0));
  EXPECT_THAT(progresses[1], InProgressWithLoadedDocuments(1));
  EXPECT_THAT(progresses[2], InProgressWithLoadedDocuments(2));
  VerifySuccessProgress(progresses[3]);
  EXPECT_EQ(progresses[3], final_progress);

  VerifyQueryResults(db);
}

TEST_F(BundleTest, CanDeleteFirestoreFromProgressUpdate) {
  Firestore* db = TestFirestore();
  auto bundle = CreateTestBundle(db);

  std::promise<void> db_deleted;
  std::vector<LoadBundleTaskProgress> progresses;
  Future<LoadBundleTaskProgress> result =
      db->LoadBundle(bundle, [this, db, &progresses, &db_deleted](
                                 const LoadBundleTaskProgress& progress) {
        progresses.push_back(progress);
        // Delete firestore before the final progress.
        if (progresses.size() == 3) {
          // Save `db_deleted` to a local variable because this lambda gets
          // deleted by the call to `DeleteFirestore()` below, and therefore it
          // is undefined behavior to access any captured variables thereafter.
          auto& db_deleted_local = db_deleted;
          DeleteFirestore(db);
          db_deleted_local.set_value();
        }
      });

  // Wait for the notification that the instance is deleted before verifying.
  db_deleted.get_future().wait();

  // This future is not completed, and returns back a nullptr for result when it
  // times out.
  EXPECT_EQ(Await(result), nullptr);

  // 3 progresses will be reported: initial, document 1, document 2.
  // Final progress update is missing because Firestore is deleted before that.
  ASSERT_EQ(progresses.size(), 3);
  EXPECT_THAT(progresses[0], InProgressWithLoadedDocuments(0));
  EXPECT_THAT(progresses[1], InProgressWithLoadedDocuments(1));
  EXPECT_THAT(progresses[2], InProgressWithLoadedDocuments(2));
}

TEST_F(BundleTest, LoadBundlesForASecondTimeSkips) {
  // TODO(wuandy): This test fails on Windows CI, but
  // local run is fine. We need to figure out why and re-enable it.
  SKIP_TEST_ON_WINDOWS;

  Firestore* db = TestFirestore();
  auto bundle = CreateTestBundle(db);
  LoadBundleTaskProgress first_load = AwaitResult(db->LoadBundle(bundle));
  VerifySuccessProgress(first_load);

  std::vector<LoadBundleTaskProgress> progresses;
  std::promise<void> final_update;
  LoadBundleTaskProgress second_load = AwaitResult(db->LoadBundle(
      bundle,
      [&progresses, &final_update](const LoadBundleTaskProgress& progress) {
        progresses.push_back(progress);
        SetPromiseValueWhenUpdateIsFinal(progress, final_update);
      }));

  // There will be 4 progress updates if it does not skip loading.
  final_update.get_future().wait();
  ASSERT_EQ(progresses.size(), 1);
  VerifySuccessProgress(progresses[0]);
  EXPECT_EQ(progresses[0], second_load);

  VerifyQueryResults(db);
}

TEST_F(BundleTest, LoadInvalidBundlesShouldFail) {
  // TODO(wuandy): This test fails on Windows CI, but
  // local run is fine. We need to figure out why and re-enable it.
  SKIP_TEST_ON_WINDOWS;

  Firestore* db = TestFirestore();
  std::vector<std::string> invalid_bundles{
      "invalid bundle obviously", "\"(╯°□°）╯︵ ┻━┻\"",
      "\xc3\x28"  // random bytes
  };
  for (const auto& bundle : invalid_bundles) {
    std::vector<LoadBundleTaskProgress> progresses;
    std::promise<void> final_update;
    Future<LoadBundleTaskProgress> result = db->LoadBundle(
        bundle,
        [&progresses, &final_update](const LoadBundleTaskProgress& progress) {
          progresses.push_back(progress);
          SetPromiseValueWhenUpdateIsFinal(progress, final_update);
        });

    Await(result);
    EXPECT_NE(result.error(), Error::kErrorOk);

    final_update.get_future().wait();
    ASSERT_EQ(progresses.size(), 1);
    VerifyErrorProgress(progresses[0]);
  }
}

TEST_F(BundleTest, LoadBundleWithDocumentsAlreadyPulledFromBackend) {
  // TODO(wuandy, b/189477267): This test fails on Windows CI, but
  // local run is fine. We need to figure out why and re-enable it.
  SKIP_TEST_ON_WINDOWS;

  Firestore* db = TestFirestore();
  auto collection = db->Collection("coll-1");
  WriteDocuments(collection,
                 {
                     {"a", {{"bar", FieldValue::String("newValueA")}}},
                     {"b", {{"bar", FieldValue::String("newValueB")}}},
                 });
  EventAccumulator<QuerySnapshot> accumulator;
  ListenerRegistration limit_registration =
      accumulator.listener()->AttachTo(&collection);
  accumulator.AwaitRemoteEvent();

  // The test bundle is holding ancient documents, so no events are generated as
  // a result. The case where a bundle has newer doc than cache can only be
  // tested in spec tests.
  accumulator.FailOnNextEvent();

  auto bundle = CreateTestBundle(db);
  VerifySuccessProgress(AwaitResult(db->LoadBundle(bundle)));

  EXPECT_THAT(QuerySnapshotToValues(*Await(collection.Get(Source::kCache))),
              testing::ElementsAre(
                  MapFieldValue{{"bar", FieldValue::String("newValueA")}},
                  MapFieldValue{{"bar", FieldValue::String("newValueB")}}));

  {
    Query limit = AwaitResult(db->NamedQuery(kLimitQueryName));
    EXPECT_THAT(QuerySnapshotToValues(AwaitResult(limit.Get(Source::kCache))),
                testing::ElementsAre(
                    MapFieldValue{{"bar", FieldValue::String("newValueB")}}));
  }

  {
    Query limit_to_last = AwaitResult(db->NamedQuery(kLimitToLastQueryName));
    EXPECT_THAT(
        QuerySnapshotToValues(AwaitResult(limit_to_last.Get(Source::kCache))),
        testing::ElementsAre(
            MapFieldValue{{"bar", FieldValue::String("newValueA")}}));
  }
}

TEST_F(BundleTest, LoadedDocumentsShouldNotBeGarbageCollectedRightAway) {
  // TODO(wuandy, b/189477267): This test fails on Windows CI, but
  // local run is fine. We need to figure out why and re-enable it.
  SKIP_TEST_ON_WINDOWS;

  Firestore* db = TestFirestore();
  // This test really only makes sense with memory persistence, as disk
  // persistence only ever lazily deletes data.
  auto new_settings = db->settings();
  new_settings.set_persistence_enabled(false);
  db->set_settings(new_settings);

  auto bundle = CreateTestBundle(db);
  VerifySuccessProgress(AwaitResult(db->LoadBundle(bundle)));

  // Read a different collection. This will trigger GC.
  Await(db->Collection("coll-other").Get());

  // Read the loaded documents, expecting them to exist in cache. With memory
  // GC, the documents would get GC-ed if we did not hold the document keys in
  // an "umbrella" target. See LocalStore for details.
  VerifyQueryResults(db);
}

TEST_F(BundleTest, LoadDocumentsFromOtherProjectsShouldFail) {
  Firestore* db = TestFirestore();
  auto bundle = CreateBundle("other-project");
  std::vector<LoadBundleTaskProgress> progresses;
  std::promise<void> final_update;
  Future<LoadBundleTaskProgress> result = db->LoadBundle(
      bundle,
      [&progresses, &final_update](const LoadBundleTaskProgress& progress) {
        progresses.push_back(progress);
        SetPromiseValueWhenUpdateIsFinal(progress, final_update);
      });
  Await(result);
  EXPECT_NE(result.error(), Error::kErrorOk);

  final_update.get_future().wait();
  ASSERT_EQ(progresses.size(), 2);
  EXPECT_THAT(progresses[0], InProgressWithLoadedDocuments(0));
  VerifyErrorProgress(progresses[1]);
}

TEST_F(BundleTest, GetInvalidNamedQuery) {
  Firestore* db = TestFirestore();
  {
    auto future = db->NamedQuery("DOES_NOT_EXIST");
    Await(future);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
    EXPECT_EQ(future.error(), Error::kErrorNotFound);
  }
  {
    auto future = db->NamedQuery("");
    Await(future);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
    EXPECT_EQ(future.error(), Error::kErrorNotFound);
  }
  {
    auto future = db->NamedQuery("\xc3\x28");
    Await(future);
    EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
    EXPECT_EQ(future.error(), Error::kErrorNotFound);
  }
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
