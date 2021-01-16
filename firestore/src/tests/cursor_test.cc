#include <string>
#include <vector>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRCursorTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/CursorTest.java
// The iOS test names start with the mandatory test prefix while Android test
// names do not. Here we use the Android test names.

namespace firebase {
namespace firestore {

TEST_F(FirestoreIntegrationTest, CanPageThroughItems) {
  CollectionReference collection =
      Collection({{"a", {{"v", FieldValue::String("a")}}},
                  {"b", {{"v", FieldValue::String("b")}}},
                  {"c", {{"v", FieldValue::String("c")}}},
                  {"d", {{"v", FieldValue::String("d")}}},
                  {"e", {{"v", FieldValue::String("e")}}},
                  {"f", {{"v", FieldValue::String("f")}}}});
  QuerySnapshot snapshot = ReadDocuments(collection.Limit(2));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(snapshot));

  DocumentSnapshot last_doc = snapshot.documents()[1];
  snapshot = ReadDocuments(collection.Limit(3).StartAfter(last_doc));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("c")}},
                                        {{"v", FieldValue::String("d")}},
                                        {{"v", FieldValue::String("e")}}}),
            QuerySnapshotToValues(snapshot));

  last_doc = snapshot.documents()[2];
  snapshot = ReadDocuments(collection.Limit(1).StartAfter(last_doc));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("f")}}}),
            QuerySnapshotToValues(snapshot));

  last_doc = snapshot.documents()[0];
  snapshot = ReadDocuments(collection.Limit(3).StartAfter(last_doc));
  EXPECT_EQ(std::vector<MapFieldValue>{}, QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, CanBeCreatedFromDocuments) {
  CollectionReference collection = Collection(
      {{"a",
        {{"k", FieldValue::String("a")}, {"sort", FieldValue::Double(1.0)}}},
       {"b",
        {{"k", FieldValue::String("b")}, {"sort", FieldValue::Double(2.0)}}},
       {"c",
        {{"k", FieldValue::String("c")}, {"sort", FieldValue::Double(2.0)}}},
       {"d",
        {{"k", FieldValue::String("d")}, {"sort", FieldValue::Double(2.0)}}},
       {"e",
        {{"k", FieldValue::String("e")}, {"sort", FieldValue::Double(0.0)}}},
       // should not show up
       {"f",
        {{"k", FieldValue::String("f")},
         {"nosort", FieldValue::Double(1.0)}}}});
  Query query = collection.OrderBy("sort");
  DocumentSnapshot snapshot = ReadDocument(collection.Document("c"));

  EXPECT_TRUE(snapshot.exists());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"k", FieldValue::String("c")},
                                         {"sort", FieldValue::Double(2.0)}},
                                        {{"k", FieldValue::String("d")},
                                         {"sort", FieldValue::Double(2.0)}}}),
            QuerySnapshotToValues(ReadDocuments(query.StartAt(snapshot))));

  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"k", FieldValue::String("e")}, {"sort", FieldValue::Double(0.0)}},
           {{"k", FieldValue::String("a")}, {"sort", FieldValue::Double(1.0)}},
           {{"k", FieldValue::String("b")},
            {"sort", FieldValue::Double(2.0)}}}),
      QuerySnapshotToValues(ReadDocuments(query.EndBefore(snapshot))));
}

TEST_F(FirestoreIntegrationTest, CanBeCreatedFromValues) {
  CollectionReference collection = Collection(
      {{"a",
        {{"k", FieldValue::String("a")}, {"sort", FieldValue::Double(1.0)}}},
       {"b",
        {{"k", FieldValue::String("b")}, {"sort", FieldValue::Double(2.0)}}},
       {"c",
        {{"k", FieldValue::String("c")}, {"sort", FieldValue::Double(2.0)}}},
       {"d",
        {{"k", FieldValue::String("d")}, {"sort", FieldValue::Double(2.0)}}},
       {"e",
        {{"k", FieldValue::String("e")}, {"sort", FieldValue::Double(0.0)}}},
       // should not show up
       {"f",
        {{"k", FieldValue::String("f")},
         {"nosort", FieldValue::Double(1.0)}}}});
  Query query = collection.OrderBy("sort");

  QuerySnapshot snapshot = ReadDocuments(
      query.StartAt(std::vector<FieldValue>({FieldValue::Double(2.0)})));
  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"k", FieldValue::String("b")}, {"sort", FieldValue::Double(2.0)}},
           {{"k", FieldValue::String("c")}, {"sort", FieldValue::Double(2.0)}},
           {{"k", FieldValue::String("d")},
            {"sort", FieldValue::Double(2.0)}}}),
      QuerySnapshotToValues(snapshot));

  snapshot = ReadDocuments(
      query.EndBefore(std::vector<FieldValue>({FieldValue::Double(2.0)})));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"k", FieldValue::String("e")},
                                         {"sort", FieldValue::Double(0.0)}},
                                        {{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Double(1.0)}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, CanBeCreatedUsingDocumentId) {
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"v", FieldValue::String("a")}}},
      {"b", {{"v", FieldValue::String("b")}}},
      {"c", {{"v", FieldValue::String("c")}}},
      {"d", {{"v", FieldValue::String("d")}}},
      {"e", {{"v", FieldValue::String("e")}}}};

  CollectionReference writer = TestFirestore("writer")
                                   ->Collection("parent-collection")
                                   .Document()
                                   .Collection("sub-collection");
  WriteDocuments(writer, docs);

  CollectionReference reader =
      TestFirestore("reader")->Collection(writer.path());
  QuerySnapshot snapshot = ReadDocuments(
      reader.OrderBy(FieldPath::DocumentId())
          .StartAt(std::vector<FieldValue>({FieldValue::String("b")}))
          .EndBefore(std::vector<FieldValue>({FieldValue::String("d")})));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("b")}},
                                        {{"v", FieldValue::String("c")}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, CanBeUsedWithReferenceValues) {
  Firestore* db = TestFirestore();
  std::map<std::string, MapFieldValue> docs = {
      {"a",
       {{"k", FieldValue::String("1a")},
        {"ref", FieldValue::Reference(db->Collection("1").Document("a"))}}},
      {"b",
       {{"k", FieldValue::String("1b")},
        {"ref", FieldValue::Reference(db->Collection("1").Document("b"))}}},
      {"c",
       {{"k", FieldValue::String("2a")},
        {"ref", FieldValue::Reference(db->Collection("2").Document("a"))}}},
      {"d",
       {{"k", FieldValue::String("2b")},
        {"ref", FieldValue::Reference(db->Collection("2").Document("b"))}}},
      {"e",
       {{"k", FieldValue::String("3a")},
        {"ref", FieldValue::Reference(db->Collection("3").Document("a"))}}}};

  CollectionReference collection = Collection(docs);

  QuerySnapshot snapshot = ReadDocuments(
      collection.OrderBy("ref")
          .StartAfter(std::vector<FieldValue>(
              {FieldValue::Reference(db->Collection("1").Document("a"))}))
          .EndAt(std::vector<FieldValue>(
              {FieldValue::Reference(db->Collection("2").Document("b"))})));

  std::vector<std::string> results;
  for (const DocumentSnapshot& doc : snapshot.documents()) {
    results.push_back(doc.Get("k").string_value());
  }
  EXPECT_EQ(std::vector<std::string>({"1b", "2a", "2b"}), results);
}

TEST_F(FirestoreIntegrationTest, CanBeUsedInDescendingQueries) {
  CollectionReference collection = Collection(
      {{"a",
        {{"k", FieldValue::String("a")}, {"sort", FieldValue::Double(1.0)}}},
       {"b",
        {{"k", FieldValue::String("b")}, {"sort", FieldValue::Double(2.0)}}},
       {"c",
        {{"k", FieldValue::String("c")}, {"sort", FieldValue::Double(2.0)}}},
       {"d",
        {{"k", FieldValue::String("d")}, {"sort", FieldValue::Double(3.0)}}},
       {"e",
        {{"k", FieldValue::String("e")}, {"sort", FieldValue::Double(0.0)}}},
       // should not show up
       {"f",
        {{"k", FieldValue::String("f")},
         {"nosort", FieldValue::Double(1.0)}}}});
  Query query =
      collection.OrderBy("sort", Query::Direction::kDescending)
          .OrderBy(FieldPath::DocumentId(), Query::Direction::kDescending);

  QuerySnapshot snapshot = ReadDocuments(
      query.StartAt(std::vector<FieldValue>({FieldValue::Double(2.0)})));
  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"k", FieldValue::String("c")}, {"sort", FieldValue::Double(2.0)}},
           {{"k", FieldValue::String("b")}, {"sort", FieldValue::Double(2.0)}},
           {{"k", FieldValue::String("a")}, {"sort", FieldValue::Double(1.0)}},
           {{"k", FieldValue::String("e")},
            {"sort", FieldValue::Double(0.0)}}}),
      QuerySnapshotToValues(snapshot));

  snapshot = ReadDocuments(
      query.EndBefore(std::vector<FieldValue>({FieldValue::Double(2.0)})));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"k", FieldValue::String("d")},
                                         {"sort", FieldValue::Double(3.0)}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TimestampsCanBePassedToQueriesAsLimits) {
  CollectionReference collection =
      Collection({{"a", {{"timestamp", FieldValue::Timestamp({100, 2000})}}},
                  {"b", {{"timestamp", FieldValue::Timestamp({100, 5000})}}},
                  {"c", {{"timestamp", FieldValue::Timestamp({100, 3000})}}},
                  {"d", {{"timestamp", FieldValue::Timestamp({100, 1000})}}},
                  // Number of nanoseconds deliberately repeated.
                  {"e", {{"timestamp", FieldValue::Timestamp({100, 5000})}}},
                  {"f", {{"timestamp", FieldValue::Timestamp({100, 4000})}}}});
  QuerySnapshot snapshot = ReadDocuments(
      collection.OrderBy("timestamp")
          .StartAfter(
              std::vector<FieldValue>({FieldValue::Timestamp({100, 2000})}))
          .EndAt(
              std::vector<FieldValue>({FieldValue::Timestamp({100, 5000})})));
  EXPECT_EQ(std::vector<std::string>({"c", "f", "b", "e"}),
            QuerySnapshotToIds(snapshot));
}

TEST_F(FirestoreIntegrationTest, TimestampsCanBePassedToQueriesInWhereClause) {
  CollectionReference collection =
      Collection({{"a", {{"timestamp", FieldValue::Timestamp({100, 7000})}}},
                  {"b", {{"timestamp", FieldValue::Timestamp({100, 4000})}}},
                  {"c", {{"timestamp", FieldValue::Timestamp({100, 8000})}}},
                  {"d", {{"timestamp", FieldValue::Timestamp({100, 5000})}}},
                  {"e", {{"timestamp", FieldValue::Timestamp({100, 6000})}}}});

  QuerySnapshot snapshot = ReadDocuments(
      collection
          .WhereGreaterThanOrEqualTo("timestamp",
                                     FieldValue::Timestamp({100, 5000}))
          .WhereLessThan("timestamp", FieldValue::Timestamp({100, 8000})));
  EXPECT_EQ(std::vector<std::string>({"d", "e", "a"}),
            QuerySnapshotToIds(snapshot));
}

TEST_F(FirestoreIntegrationTest, TimestampsAreTruncatedToMicroseconds) {
  const FieldValue nanos = FieldValue::Timestamp({0, 123456789});
  const FieldValue micros = FieldValue::Timestamp({0, 123456000});
  const FieldValue millis = FieldValue::Timestamp({0, 123000000});
  CollectionReference collection = Collection({{"a", {{"timestamp", nanos}}}});

  QuerySnapshot snapshot =
      ReadDocuments(collection.WhereEqualTo("timestamp", nanos));
  EXPECT_EQ(1, QuerySnapshotToValues(snapshot).size());

  // Because Timestamp should have been truncated to microseconds, the
  // microsecond timestamp should be considered equal to the nanosecond one.
  snapshot = ReadDocuments(collection.WhereEqualTo("timestamp", micros));
  EXPECT_EQ(1, QuerySnapshotToValues(snapshot).size());

  // The truncation is just to the microseconds, however, so the millisecond
  // timestamp should be treated as different and thus the query should return
  // no results.
  snapshot = ReadDocuments(collection.WhereEqualTo("timestamp", millis));
  EXPECT_TRUE(QuerySnapshotToValues(snapshot).empty());
}

}  // namespace firestore
}  // namespace firebase
