#include <utility>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#if defined(__ANDROID__)
#include "firestore/src/android/write_batch_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRWriteBatchTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/WriteBatchTest.java
// The test cases between the two native client SDK divert quite a lot. The port
// here is an effort to do a superset and cover both cases.

namespace firebase {
namespace firestore {

using WriteBatchCommonTest = testing::Test;

using WriteBatchTest = FirestoreIntegrationTest;

TEST_F(WriteBatchTest, TestSupportEmptyBatches) {
  Await(TestFirestore()->batch().Commit());
}

TEST_F(WriteBatchTest, TestSetDocuments) {
  DocumentReference doc = Document();
  Await(TestFirestore()
            ->batch()
            .Set(doc, MapFieldValue{{"a", FieldValue::String("b")}})
            .Set(doc, MapFieldValue{{"c", FieldValue::String("d")}})
            .Set(doc, MapFieldValue{{"foo", FieldValue::String("bar")}})
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  ASSERT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
}

TEST_F(WriteBatchTest, TestSetDocumentWithMerge) {
  DocumentReference doc = Document();
  Await(TestFirestore()
            ->batch()
            .Set(doc,
                 MapFieldValue{
                     {"a", FieldValue::String("b")},
                     {"nested",
                      FieldValue::Map({{"a", FieldValue::String("remove")}})}},
                 SetOptions::Merge())
            .Commit());
  Await(TestFirestore()
            ->batch()
            .Set(doc,
                 MapFieldValue{
                     {"c", FieldValue::String("d")},
                     {"ignore", FieldValue::Boolean(true)},
                     {"nested",
                      FieldValue::Map({{"c", FieldValue::String("d")}})}},
                 SetOptions::MergeFields({"c", "nested"}))
            .Commit());
  Await(TestFirestore()
            ->batch()
            .Set(doc,
                 MapFieldValue{
                     {"e", FieldValue::String("f")},
                     {"nested", FieldValue::Map(
                                    {{"e", FieldValue::String("f")},
                                     {"ignore", FieldValue::Boolean(true)}})}},
                 SetOptions::MergeFieldPaths({{"e"}, {"nested", "e"}}))
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  ASSERT_TRUE(snapshot.exists());
  EXPECT_THAT(
      snapshot.GetData(),
      testing::ContainerEq(MapFieldValue{
          {"a", FieldValue::String("b")},
          {"c", FieldValue::String("d")},
          {"e", FieldValue::String("f")},
          {"nested", FieldValue::Map({{"c", FieldValue::String("d")},
                                      {"e", FieldValue::String("f")}})}}));
}

TEST_F(WriteBatchTest, TestUpdateDocuments) {
  DocumentReference doc = Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldValue{{"baz", FieldValue::Integer(42)}})
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  ASSERT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(), testing::ContainerEq(MapFieldValue{
                                      {"foo", FieldValue::String("bar")},
                                      {"baz", FieldValue::Integer(42)}}));
}

TEST_F(WriteBatchTest, TestCannotUpdateNonexistentDocuments) {
  DocumentReference doc = Document();
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldValue{{"baz", FieldValue::Integer(42)}})
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_FALSE(snapshot.exists());
}

TEST_F(WriteBatchTest, TestDeleteDocuments) {
  DocumentReference doc = Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});
  DocumentSnapshot snapshot = ReadDocument(doc);

  EXPECT_TRUE(snapshot.exists());
  Await(TestFirestore()->batch().Delete(doc).Commit());
  snapshot = ReadDocument(doc);
  EXPECT_FALSE(snapshot.exists());
}

TEST_F(WriteBatchTest, TestBatchesCommitAtomicallyRaisingCorrectEvents) {
  CollectionReference collection = Collection();
  DocumentReference doc_a = collection.Document("a");
  DocumentReference doc_b = collection.Document("b");
  EventAccumulator<QuerySnapshot> accumulator;
  accumulator.listener()->AttachTo(&collection, MetadataChanges::kInclude);
  QuerySnapshot initial_snapshot = accumulator.Await();
  EXPECT_EQ(0, initial_snapshot.size());

  // Atomically write two documents.
  Await(TestFirestore()
            ->batch()
            .Set(doc_a, MapFieldValue{{"a", FieldValue::Integer(1)}})
            .Set(doc_b, MapFieldValue{{"b", FieldValue::Integer(2)}})
            .Commit());

  QuerySnapshot local_snapshot = accumulator.Await();
  EXPECT_TRUE(local_snapshot.metadata().has_pending_writes());
  EXPECT_THAT(
      QuerySnapshotToValues(local_snapshot),
      testing::ElementsAre(MapFieldValue{{"a", FieldValue::Integer(1)}},
                           MapFieldValue{{"b", FieldValue::Integer(2)}}));

  QuerySnapshot server_snapshot = accumulator.Await();
  EXPECT_FALSE(server_snapshot.metadata().has_pending_writes());
  EXPECT_THAT(
      QuerySnapshotToValues(server_snapshot),
      testing::ElementsAre(MapFieldValue{{"a", FieldValue::Integer(1)}},
                           MapFieldValue{{"b", FieldValue::Integer(2)}}));
}

TEST_F(WriteBatchTest, TestBatchesFailAtomicallyRaisingCorrectEvents) {
  CollectionReference collection = Collection();
  DocumentReference doc_a = collection.Document("a");
  DocumentReference doc_b = collection.Document("b");
  EventAccumulator<QuerySnapshot> accumulator;
  accumulator.listener()->AttachTo(&collection, MetadataChanges::kInclude);
  QuerySnapshot initial_snapshot = accumulator.Await();
  EXPECT_EQ(0, initial_snapshot.size());

  // Atomically write 1 document and update a nonexistent document.
  Future<void> future =
      TestFirestore()
          ->batch()
          .Set(doc_a, MapFieldValue{{"a", FieldValue::Integer(1)}})
          .Update(doc_b, MapFieldValue{{"b", FieldValue::Integer(2)}})
          .Commit();
  Await(future);
  EXPECT_EQ(FutureStatus::kFutureStatusComplete, future.status());
  EXPECT_EQ(Error::kErrorNotFound, future.error());

  // Local event with the set document.
  QuerySnapshot local_snapshot = accumulator.Await();
  EXPECT_TRUE(local_snapshot.metadata().has_pending_writes());
  EXPECT_THAT(
      QuerySnapshotToValues(local_snapshot),
      testing::ElementsAre(MapFieldValue{{"a", FieldValue::Integer(1)}}));

  // Server event with the set reverted
  QuerySnapshot server_snapshot = accumulator.Await();
  EXPECT_FALSE(server_snapshot.metadata().has_pending_writes());
  EXPECT_EQ(0, server_snapshot.size());
}

TEST_F(WriteBatchTest, TestWriteTheSameServerTimestampAcrossWrites) {
  CollectionReference collection = Collection();
  DocumentReference doc_a = collection.Document("a");
  DocumentReference doc_b = collection.Document("b");
  EventAccumulator<QuerySnapshot> accumulator;
  accumulator.listener()->AttachTo(&collection, MetadataChanges::kInclude);
  QuerySnapshot initial_snapshot = accumulator.Await();
  EXPECT_EQ(0, initial_snapshot.size());

  // Atomically write two documents with server timestamps.
  Await(TestFirestore()
            ->batch()
            .Set(doc_a, MapFieldValue{{"when", FieldValue::ServerTimestamp()}})
            .Set(doc_b, MapFieldValue{{"when", FieldValue::ServerTimestamp()}})
            .Commit());

  QuerySnapshot local_snapshot = accumulator.Await();
  EXPECT_TRUE(local_snapshot.metadata().has_pending_writes());
  EXPECT_THAT(
      QuerySnapshotToValues(local_snapshot),
      testing::ElementsAre(MapFieldValue{{"when", FieldValue::Null()}},
                           MapFieldValue{{"when", FieldValue::Null()}}));

  QuerySnapshot server_snapshot = accumulator.AwaitRemoteEvent();
  EXPECT_FALSE(server_snapshot.metadata().has_pending_writes());
  EXPECT_EQ(2, server_snapshot.size());
  const FieldValue when = server_snapshot.documents()[0].Get("when");
  EXPECT_EQ(FieldValue::Type::kTimestamp, when.type());
  EXPECT_THAT(QuerySnapshotToValues(server_snapshot),
              testing::ElementsAre(MapFieldValue{{"when", when}},
                                   MapFieldValue{{"when", when}}));
}

TEST_F(WriteBatchTest, TestCanWriteTheSameDocumentMultipleTimes) {
  DocumentReference doc = Document();
  EventAccumulator<DocumentSnapshot> accumulator;
  accumulator.listener()->AttachTo(&doc, MetadataChanges::kInclude);
  DocumentSnapshot initial_snapshot = accumulator.Await();
  EXPECT_FALSE(initial_snapshot.exists());

  Await(TestFirestore()
            ->batch()
            .Delete(doc)
            .Set(doc, MapFieldValue{{"a", FieldValue::Integer(1)},
                                    {"b", FieldValue::Integer(1)},
                                    {"when", FieldValue::String("when")}})
            .Update(doc, MapFieldValue{{"b", FieldValue::Integer(2)},
                                       {"when", FieldValue::ServerTimestamp()}})
            .Commit());
  DocumentSnapshot local_snapshot = accumulator.Await();
  EXPECT_TRUE(local_snapshot.metadata().has_pending_writes());
  EXPECT_THAT(local_snapshot.GetData(), testing::ContainerEq(MapFieldValue{
                                            {"a", FieldValue::Integer(1)},
                                            {"b", FieldValue::Integer(2)},
                                            {"when", FieldValue::Null()}}));

  DocumentSnapshot server_snapshot = accumulator.Await();
  EXPECT_FALSE(server_snapshot.metadata().has_pending_writes());
  const FieldValue when = server_snapshot.Get("when");
  EXPECT_EQ(FieldValue::Type::kTimestamp, when.type());
  EXPECT_THAT(server_snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{{"a", FieldValue::Integer(1)},
                                                 {"b", FieldValue::Integer(2)},
                                                 {"when", when}}));
}

TEST_F(WriteBatchTest, TestUpdateFieldsWithDots) {
  DocumentReference doc = Document();
  WriteDocument(doc, MapFieldValue{{"a.b", FieldValue::String("old")},
                                   {"c.d", FieldValue::String("old")}});
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldPathValue{{FieldPath{"a.b"},
                                            FieldValue::String("new")}})
            .Commit());
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldPathValue{{FieldPath{"c.d"},
                                            FieldValue::String("new")}})
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  ASSERT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(), testing::ContainerEq(MapFieldValue{
                                      {"a.b", FieldValue::String("new")},
                                      {"c.d", FieldValue::String("new")}}));
}

TEST_F(WriteBatchTest, TestUpdateNestedFields) {
  DocumentReference doc = Document();
  WriteDocument(
      doc, MapFieldValue{
               {"a", FieldValue::Map({{"b", FieldValue::String("old")}})},
               {"c", FieldValue::Map({{"d", FieldValue::String("old")}})},
               {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}});
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldValue({{"a.b", FieldValue::String("new")}}))
            .Commit());
  Await(TestFirestore()
            ->batch()
            .Update(doc, MapFieldPathValue({{FieldPath{"c", "d"},
                                             FieldValue::String("new")}}))
            .Commit());
  DocumentSnapshot snapshot = ReadDocument(doc);
  ASSERT_TRUE(snapshot.exists());
  EXPECT_THAT(snapshot.GetData(),
              testing::ContainerEq(MapFieldValue{
                  {"a", FieldValue::Map({{"b", FieldValue::String("new")}})},
                  {"c", FieldValue::Map({{"d", FieldValue::String("new")}})},
                  {"e", FieldValue::Map({{"f", FieldValue::String("old")}})}}));
}

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

TEST_F(WriteBatchCommonTest, Construction) {
  testutil::AssertWrapperConstructionContract<WriteBatch>();
}

TEST_F(WriteBatchCommonTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<WriteBatch>();
}

#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
