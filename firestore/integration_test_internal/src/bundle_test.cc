#include <string>
#include <vector>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"
#include "util/bundle_builder.h"
#include "util/event_accumulator.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRBundlesTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/BundleTest.java
// The iOS test names start with the mandatory test prefix while Android test
// names do not. Here we use the Android test names.

namespace firebase {
namespace firestore {
namespace {

void VerifyProgress(LoadBundleTaskProgress progress, int expected_documents) {
  EXPECT_EQ(progress.state(), LoadBundleTaskProgress::State::kInProgress);
  EXPECT_EQ(progress.documents_loaded(), expected_documents);
  EXPECT_LE(progress.documents_loaded(), progress.total_documents());
  EXPECT_LE(progress.bytes_loaded(), progress.total_bytes());
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

class BundleTest : public FirestoreIntegrationTest {
 protected:
  void SetUp() override {
    Await(TestFirestore()->ClearPersistence());
  }

  void VerifyQueryResults(Firestore *db) {
    {
      auto snapshot = *Await(db->Collection("coll-1").Get(Source::kCache));
      EXPECT_THAT(QuerySnapshotToValues(snapshot),
                  testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                                     {"bar", FieldValue::Integer(1)}},
                                       MapFieldValue{{"k", FieldValue::String("b")},
                                                     {"bar", FieldValue::Integer(2)}}));
    }

    {
      Query limit = *Await(db->NamedQuery("limit"));
      auto limit_snapshot = *Await(limit.Get(Source::kCache));
      EXPECT_THAT(QuerySnapshotToValues(limit_snapshot),
                  testing::ElementsAre(
                      MapFieldValue{{"k", FieldValue::String("b")},
                                    {"bar", FieldValue::Integer(2)}}));
    }

    {
      Query limit_to_last = *Await(db->NamedQuery("limit-to-last"));
      auto limit_to_last_snapshot = *Await(limit_to_last.Get(Source::kCache));
      EXPECT_THAT(QuerySnapshotToValues(limit_to_last_snapshot),
                  testing::ElementsAre(
                      MapFieldValue{{"k", FieldValue::String("a")},
                                    {"bar", FieldValue::Integer(1)}}));
    }
  }
};

TEST_F(BundleTest, CanLoadBundlesWithProgressUpdates) {
  Firestore *db = TestFirestore();
  auto bundle = CreateBundle(db->app()->options().project_id());

  std::vector<LoadBundleTaskProgress> progresses;
  Future<LoadBundleTaskProgress> result = db->LoadBundle(bundle, [&progresses](const LoadBundleTaskProgress &progress) {
    progresses.push_back(progress);
  });

  auto final_progress = Await(result);

  EXPECT_EQ(progresses.size(), 4);
  VerifyProgress(progresses[0], 0);
  VerifyProgress(progresses[1], 1);
  VerifyProgress(progresses[2], 2);
  VerifySuccessProgress(progresses[3]);
  EXPECT_EQ(progresses[3], *final_progress);

  VerifyQueryResults(db);
}

TEST_F(BundleTest, LoadBundlesForASecondTimeSkips) {
  Firestore *db = TestFirestore();
  auto bundle = CreateBundle(db->app()->options().project_id());
  Await(db->LoadBundle(bundle));

  std::vector<LoadBundleTaskProgress> progresses;
  const LoadBundleTaskProgress *second_load =
      Await(db->LoadBundle(bundle,
                           [&progresses](const LoadBundleTaskProgress &progress) {
                             progresses.push_back(
                                 progress);
                           }));

  EXPECT_EQ(progresses.size(), 1);
  VerifySuccessProgress(progresses[0]);
  EXPECT_EQ(progresses[0], *second_load);

  VerifyQueryResults(db);
}

TEST_F(BundleTest, LoadBundleWithDocumentsAlreadyPulledFromBackend) {
  Firestore *db = TestFirestore();
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

  // The test bundle is holding ancient documents, so no events are generated as a result.
  // The case where a bundle has newer doc than cache can only be tested in spec tests.
  accumulator.AssertNoAdditionalEvents();

  auto bundle = CreateBundle(db->app()->options().project_id());
  VerifySuccessProgress(*Await(db->LoadBundle(bundle)));

  EXPECT_THAT(QuerySnapshotToValues(*Await(collection.Get(Source::kCache))),
              testing::ElementsAre(
                  MapFieldValue{{"bar", FieldValue::String("newValueA")}},
                  MapFieldValue{{"bar", FieldValue::String("newValueB")}}));

  {
    Query limit = *Await(db->NamedQuery("limit"));
    EXPECT_THAT(QuerySnapshotToValues(*Await(limit.Get(Source::kCache))),
                testing::ElementsAre(
                    MapFieldValue{{"bar", FieldValue::String("newValueB")}}));
  }

  {
    Query limit_to_last = *Await(db->NamedQuery("limit-to-last"));
    EXPECT_THAT(QuerySnapshotToValues(*Await(limit_to_last.Get(Source::kCache))),
                testing::ElementsAre(
                    MapFieldValue{{"bar", FieldValue::String("newValueA")}}));
  }
}

TEST_F(BundleTest, LoadedDocumentsShouldNotBeGarbageCollectedRightAway) {
  Firestore *db = TestFirestore();
  // This test really only makes sense with memory persistence, as disk persistence only ever
  // lazily deletes data
  auto new_settings = db->settings();
  new_settings.set_persistence_enabled(false);
  db->set_settings(new_settings);

  auto bundle = CreateBundle(db->app()->options().project_id());
  VerifySuccessProgress(*Await(db->LoadBundle(bundle)));

  // Read a different collection. This will trigger GC.
  Await(db->Collection("coll-other").Get());

  // Read the loaded documents, expecting them to exist in cache. With memory GC, the documents
  // would get GC-ed if we did not hold the document keys in an "umbrella" target. See
  // LocalStore for details.
  VerifyQueryResults(db);
}

TEST_F(BundleTest, LoadDocumentsFromOtherProjectsShouldFail) {
  Firestore *db = TestFirestore();
  auto bundle = CreateBundle("other-project");
  std::vector<LoadBundleTaskProgress> progresses;
  Future<LoadBundleTaskProgress> result = db->LoadBundle(bundle, [&progresses](const LoadBundleTaskProgress &progress) {
    progresses.push_back(progress);
  });
  Await(result);

  EXPECT_NE(result.error(), Error::kErrorOk);
  EXPECT_EQ(progresses.size(), 2);
  VerifyProgress(progresses[0], 0);
  VerifyErrorProgress(progresses[1]);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
