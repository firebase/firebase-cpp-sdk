#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRFieldsTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/FieldsTest.java
// except we do not port the tests for legacy timestamp behavior. C++ SDK does
// not support the legacy timestamp behavior.

namespace firebase {
namespace firestore {

class FieldsTest : public FirestoreIntegrationTest {
 protected:
  /**
   * Creates test data with nested fields.
   */
  MapFieldValue NestedData(int number) {
    char buffer[32];
    MapFieldValue result;

    snprintf(buffer, sizeof(buffer), "room %d", number);
    result["name"] = FieldValue::String(buffer);

    MapFieldValue nested;
    nested["createdAt"] = FieldValue::Integer(number);
    MapFieldValue deep_nested;
    snprintf(buffer, sizeof(buffer), "deep-field-%d", number);
    deep_nested["field"] = FieldValue::String(buffer);
    nested["deep"] = FieldValue::Map(deep_nested);
    result["metadata"] = FieldValue::Map(nested);

    return result;
  }

  /**
   * Creates test data with special characters in field names. Datastore
   * currently prohibits mixing nested data with special characters so tests
   * that use this data must be separate.
   */
  MapFieldValue DottedData(int number) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "field %d", number);

    return {{"a", FieldValue::String(buffer)},
            {"b.dot", FieldValue::Integer(number)},
            {"c\\slash", FieldValue::Integer(number)}};
  }

  /**
   * Creates test data with Timestamp.
   */
  MapFieldValue DataWithTimestamp(Timestamp timestamp) {
    return {
        {"timestamp", FieldValue::Timestamp(timestamp)},
        {"nested",
         FieldValue::Map({{"timestamp2", FieldValue::Timestamp(timestamp)}})}};
  }
};

TEST_F(FieldsTest, TestNestedFieldsCanBeWrittenWithSet) {
  DocumentReference doc = Document();
  WriteDocument(doc, NestedData(1));
  EXPECT_THAT(ReadDocument(doc).GetData(), testing::ContainerEq(NestedData(1)));
}

TEST_F(FieldsTest, TestNestedFieldsCanBeReadDirectly) {
  DocumentReference doc = Document();
  WriteDocument(doc, NestedData(1));
  DocumentSnapshot snapshot = ReadDocument(doc);

  MapFieldValue expected = NestedData(1);
  EXPECT_EQ(expected["name"].string_value(),
            snapshot.Get("name").string_value());
  EXPECT_EQ(expected["metadata"].map_value(),
            snapshot.Get("metadata").map_value());
  EXPECT_EQ(expected["metadata"]
                .map_value()["deep"]
                .map_value()["field"]
                .string_value(),
            snapshot.Get("metadata.deep.field").string_value());
  EXPECT_FALSE(snapshot.Get("metadata.nofield").is_valid());
  EXPECT_FALSE(snapshot.Get("nometadata.nofield").is_valid());
}

TEST_F(FieldsTest, TestNestedFieldsCanBeReadDirectlyViaFieldPath) {
  DocumentReference doc = Document();
  WriteDocument(doc, NestedData(1));
  DocumentSnapshot snapshot = ReadDocument(doc);

  MapFieldValue expected = NestedData(1);
  EXPECT_EQ(expected["name"].string_value(),
            snapshot.Get(FieldPath{"name"}).string_value());
  EXPECT_EQ(expected["metadata"].map_value(),
            snapshot.Get(FieldPath{"metadata"}).map_value());
  EXPECT_EQ(
      expected["metadata"]
          .map_value()["deep"]
          .map_value()["field"]
          .string_value(),
      snapshot.Get(FieldPath{"metadata", "deep", "field"}).string_value());
  EXPECT_FALSE(snapshot.Get(FieldPath{"metadata", "nofield"}).is_valid());
  EXPECT_FALSE(snapshot.Get(FieldPath{"nometadata", "nofield"}).is_valid());
}

TEST_F(FieldsTest, TestNestedFieldsCanBeUpdated) {
  DocumentReference doc = Document();
  WriteDocument(doc, NestedData(1));
  UpdateDocument(
      doc, MapFieldValue{{"metadata.deep.field", FieldValue::Integer(100)},
                         {"metadata.added", FieldValue::Integer(200)}});
  EXPECT_THAT(ReadDocument(doc).GetData(),
              testing::ContainerEq(MapFieldValue(
                  {{"name", FieldValue::String("room 1")},
                   {"metadata",
                    FieldValue::Map(
                        {{"createdAt", FieldValue::Integer(1)},
                         {"deep", FieldValue::Map(
                                      {{"field", FieldValue::Integer(100)}})},
                         {"added", FieldValue::Integer(200)}})}})));
}

TEST_F(FieldsTest, TestNestedFieldsCanBeUsedInQueryFilters) {
  CollectionReference collection = Collection(
      {{"1", NestedData(300)}, {"2", NestedData(100)}, {"3", NestedData(200)}});
  QuerySnapshot snapshot = ReadDocuments(collection.WhereGreaterThanOrEqualTo(
      "metadata.createdAt", FieldValue::Integer(200)));
  // inequality adds implicit sort on field
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              testing::ElementsAre(NestedData(200), NestedData(300)));
}

TEST_F(FieldsTest, TestNestedFieldsCanBeUsedInOrderBy) {
  CollectionReference collection = Collection(
      {{"1", NestedData(300)}, {"2", NestedData(100)}, {"3", NestedData(200)}});
  QuerySnapshot snapshot =
      ReadDocuments(collection.OrderBy("metadata.createdAt"));
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(NestedData(100), NestedData(200), NestedData(300)));
}

TEST_F(FieldsTest, TestFieldsWithSpecialCharsCanBeWrittenWithSet) {
  DocumentReference doc = Document();
  WriteDocument(doc, DottedData(1));
  EXPECT_EQ(DottedData(1), ReadDocument(doc).GetData());
}

TEST_F(FieldsTest, TestFieldsWithSpecialCharsCanBeReadDirectly) {
  DocumentReference doc = Document();
  WriteDocument(doc, DottedData(1));
  DocumentSnapshot snapshot = ReadDocument(doc);

  MapFieldValue expected = DottedData(1);
  EXPECT_EQ(expected["a"].string_value(), snapshot.Get("a").string_value());
  EXPECT_EQ(expected["b.dot"].integer_value(),
            snapshot.GetData()["b.dot"].integer_value());
  EXPECT_EQ(expected["c\\slash"].integer_value(),
            snapshot.GetData()["c\\slash"].integer_value());
}

TEST_F(FieldsTest, TestFieldsWithSpecialCharsCanBeUpdated) {
  DocumentReference doc = Document();
  WriteDocument(doc, DottedData(1));
  UpdateDocument(doc, MapFieldPathValue{
                          {FieldPath{"b.dot"}, FieldValue::Integer(100)},
                          {FieldPath{"c\\slash"}, FieldValue::Integer(200)}});
  DocumentSnapshot snapshot = ReadDocument(doc);
  EXPECT_THAT(ReadDocument(doc).GetData(),
              testing::ContainerEq(
                  MapFieldValue({{"a", FieldValue::String("field 1")},
                                 {"b.dot", FieldValue::Integer(100)},
                                 {"c\\slash", FieldValue::Integer(200)}})));
}

TEST_F(FieldsTest, TestFieldsWithSpecialCharsCanBeUsedInQueryFilters) {
  CollectionReference collection = Collection(
      {{"1", DottedData(300)}, {"2", DottedData(100)}, {"3", DottedData(200)}});
  QuerySnapshot snapshot = ReadDocuments(collection.WhereGreaterThanOrEqualTo(
      FieldPath{"b.dot"}, FieldValue::Integer(200)));
  // inequality adds implicit sort on field
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              testing::ElementsAre(DottedData(200), DottedData(300)));
}

TEST_F(FieldsTest, TestFieldsWithSpecialCharsCanBeUsedInOrderBy) {
  CollectionReference collection = Collection(
      {{"1", DottedData(300)}, {"2", DottedData(100)}, {"3", DottedData(200)}});
  QuerySnapshot snapshot =
      ReadDocuments(collection.OrderBy(FieldPath{"b.dot"}));
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(DottedData(100), DottedData(200), DottedData(300)));
}

TEST_F(FieldsTest, TestTimestampsInSnapshots) {
  Timestamp original_timestamp{100, 123456789};
  // Timestamps are currently truncated to microseconds after being written to
  // the database.
  Timestamp truncated_timestamp{100, 123456000};

  DocumentReference doc = Document();
  WriteDocument(doc, DataWithTimestamp(original_timestamp));
  DocumentSnapshot snapshot = ReadDocument(doc);
  MapFieldValue data = snapshot.GetData();

  Timestamp timestamp_from_snapshot =
      snapshot.Get("timestamp").timestamp_value();
  Timestamp timestamp_from_data = data["timestamp"].timestamp_value();
  EXPECT_EQ(truncated_timestamp, timestamp_from_data);
  EXPECT_EQ(timestamp_from_snapshot, timestamp_from_data);

  timestamp_from_snapshot = snapshot.Get("nested.timestamp2").timestamp_value();
  timestamp_from_data =
      data["nested"].map_value()["timestamp2"].timestamp_value();
  EXPECT_EQ(truncated_timestamp, timestamp_from_data);
  EXPECT_EQ(timestamp_from_snapshot, timestamp_from_data);
}

}  // namespace firestore
}  // namespace firebase
