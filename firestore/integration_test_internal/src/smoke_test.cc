#include <map>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FSTSmokeTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/SmokeTest.java

namespace firebase {
namespace firestore {

using TypeTest = FirestoreIntegrationTest;

TEST_F(TypeTest, TestCanWriteASingleDocument) {
  const MapFieldValue test_data{
      {"name", FieldValue::String("Patryk")},
      {"message", FieldValue::String("We are actually writing data!")}};
  CollectionReference collection = Collection();
  Await(collection.Add(test_data));
}

TEST_F(TypeTest, TestCanReadAWrittenDocument) {
  const MapFieldValue test_data{{"foo", FieldValue::String("bar")}};
  CollectionReference collection = Collection();

  DocumentReference new_reference = *Await(collection.Add(test_data));
  DocumentSnapshot result = *Await(new_reference.Get());
  EXPECT_THAT(
      result.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
}

TEST_F(TypeTest, TestObservesExistingDocument) {
  const MapFieldValue test_data{{"foo", FieldValue::String("bar")}};
  DocumentReference writer_reference =
      TestFirestore("writer")->Collection("collection").Document();
  DocumentReference reader_reference = TestFirestore("reader")
                                           ->Collection("collection")
                                           .Document(writer_reference.id());
  Await(writer_reference.Set(test_data));

  EventAccumulator<DocumentSnapshot> accumulator;
  ListenerRegistration registration = accumulator.listener()->AttachTo(
      &reader_reference, MetadataChanges::kInclude);

  DocumentSnapshot doc = accumulator.Await();
  EXPECT_THAT(
      doc.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
  registration.Remove();
}

TEST_F(TypeTest, TestObservesNewDocument) {
  CollectionReference collection = Collection();
  DocumentReference writer_reference = collection.Document();
  DocumentReference reader_reference =
      collection.Document(writer_reference.id());

  EventAccumulator<DocumentSnapshot> accumulator;
  ListenerRegistration registration = accumulator.listener()->AttachTo(
      &reader_reference, MetadataChanges::kInclude);

  DocumentSnapshot doc = accumulator.Await();
  EXPECT_FALSE(doc.exists());

  const MapFieldValue test_data{{"foo", FieldValue::String("bar")}};
  Await(writer_reference.Set(test_data));

  doc = accumulator.Await();
  EXPECT_THAT(
      doc.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
  EXPECT_TRUE(doc.metadata().has_pending_writes());

  doc = accumulator.Await();
  EXPECT_THAT(
      doc.GetData(),
      testing::ContainerEq(MapFieldValue{{"foo", FieldValue::String("bar")}}));
  EXPECT_FALSE(doc.metadata().has_pending_writes());

  registration.Remove();
}

TEST_F(TypeTest, TestWillFireValueEventsForEmptyCollections) {
  CollectionReference collection = Collection();
  EventAccumulator<QuerySnapshot> accumulator;
  ListenerRegistration registration =
      accumulator.listener()->AttachTo(&collection, MetadataChanges::kInclude);

  QuerySnapshot query_snapshot = accumulator.Await();
  EXPECT_EQ(0, query_snapshot.size());
  EXPECT_TRUE(query_snapshot.empty());

  registration.Remove();
}

TEST_F(TypeTest, TestGetCollectionQuery) {
  const std::map<std::string, MapFieldValue> test_data{
      {"1",
       {{"name", FieldValue::String("Patryk")},
        {"message", FieldValue::String("Real data, yo!")}}},
      {"2",
       {{"name", FieldValue::String("Gil")},
        {"message", FieldValue::String("Yep!")}}},
      {"3",
       {{"name", FieldValue::String("Jonny")},
        {"message", FieldValue::String("Back to work!")}}}};
  CollectionReference collection = Collection(test_data);
  QuerySnapshot result = *Await(collection.Get());
  EXPECT_FALSE(result.empty());
  EXPECT_THAT(
      QuerySnapshotToValues(result),
      testing::ElementsAre(
          MapFieldValue{{"name", FieldValue::String("Patryk")},
                        {"message", FieldValue::String("Real data, yo!")}},
          MapFieldValue{{"name", FieldValue::String("Gil")},
                        {"message", FieldValue::String("Yep!")}},
          MapFieldValue{{"name", FieldValue::String("Jonny")},
                        {"message", FieldValue::String("Back to work!")}}));
}

// TODO(klimt): This test is disabled because we can't create compound indexes
// programmatically.
TEST_F(TypeTest, DISABLED_TestQueryByFieldAndUseOrderBy) {
  const std::map<std::string, MapFieldValue> test_data{
      {"1",
       {{"sort", FieldValue::Double(1.0)},
        {"filter", FieldValue::Boolean(true)},
        {"key", FieldValue::String("1")}}},
      {"2",
       {{"sort", FieldValue::Double(2.0)},
        {"filter", FieldValue::Boolean(true)},
        {"key", FieldValue::String("2")}}},
      {"3",
       {{"sort", FieldValue::Double(2.0)},
        {"filter", FieldValue::Boolean(true)},
        {"key", FieldValue::String("3")}}},
      {"4",
       {{"sort", FieldValue::Double(3.0)},
        {"filter", FieldValue::Boolean(false)},
        {"key", FieldValue::String("4")}}}};
  CollectionReference collection = Collection(test_data);
  Query query = collection.WhereEqualTo("filter", FieldValue::Boolean(true))
                    .OrderBy("sort", Query::Direction::kDescending);
  QuerySnapshot result = *Await(query.Get());
  EXPECT_THAT(
      QuerySnapshotToValues(result),
      testing::ElementsAre(MapFieldValue{{"sort", FieldValue::Double(2.0)},
                                         {"filter", FieldValue::Boolean(true)},
                                         {"key", FieldValue::String("2")}},
                           MapFieldValue{{"sort", FieldValue::Double(2.0)},
                                         {"filter", FieldValue::Boolean(true)},
                                         {"key", FieldValue::String("3")}},
                           MapFieldValue{{"sort", FieldValue::Double(1.0)},
                                         {"filter", FieldValue::Boolean(true)},
                                         {"key", FieldValue::String("1")}}));
}

}  // namespace firestore
}  // namespace firebase
