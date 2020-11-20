#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/tests/firestore_integration_test.h"
#include "firestore/src/tests/util/event_accumulator.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#elif defined(FIRESTORE_STUB_BUILD)
#include "firestore/src/common/wrapper_assertions.h"
#include "firestore/src/stub/query_stub.h"
#endif  // defined(__ANDROID__)

#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"
#include "firebase/firestore/firestore_errors.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRQueryTests.mm
// and native Android client SDK test
//   firebase_firestore/tests/integration_tests/src/com/google/firebase/firestore/QueryTest.java
//
// Some test cases are moved to query_network_test.cc. Check that file for more
// details.

namespace firebase {
namespace firestore {
namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

std::vector<MapFieldValue> AllDocsExcept(
    const std::map<std::string, MapFieldValue>& docs,
    const std::vector<std::string>& excluded) {
  std::vector<MapFieldValue> expected_docs;
  for (const auto& e : docs) {
    if (std::find(excluded.begin(), excluded.end(), e.first) ==
        excluded.end()) {
      expected_docs.push_back(e.second);
    }
  }
  return expected_docs;
}

}  // namespace

#if !defined(FIRESTORE_STUB_BUILD)

TEST_F(FirestoreIntegrationTest, TestLimitQueries) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot = ReadDocuments(collection.Limit(2));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"k", FieldValue::String("a")}},
                                        {{"k", FieldValue::String("b")}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestLimitQueriesUsingDescendingSortOrder) {
  CollectionReference collection = Collection(
      {{"a",
        {{"k", FieldValue::String("a")}, {"sort", FieldValue::Integer(0)}}},
       {"b",
        {{"k", FieldValue::String("b")}, {"sort", FieldValue::Integer(1)}}},
       {"c",
        {{"k", FieldValue::String("c")}, {"sort", FieldValue::Integer(1)}}},
       {"d",
        {{"k", FieldValue::String("d")}, {"sort", FieldValue::Integer(2)}}}});
  QuerySnapshot snapshot = ReadDocuments(collection.Limit(2).OrderBy(
      FieldPath({"sort"}), Query::Direction::kDescending));
  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"k", FieldValue::String("d")}, {"sort", FieldValue::Integer(2)}},
           {{"k", FieldValue::String("c")}, {"sort", FieldValue::Integer(1)}}}),
      QuerySnapshotToValues(snapshot));
}

#if defined(__ANDROID__)
TEST_F(FirestoreIntegrationTest, TestLimitToLastMustAlsoHaveExplicitOrderBy) {
  CollectionReference collection = Collection();

  EXPECT_THROW(Await(collection.LimitToLast(2).Get()), FirestoreException);
}
#endif  // defined(__ANDROID__)

// Two queries that mapped to the same target ID are referred to as "mirror
// queries". An example for a mirror query is a LimitToLast() query and a
// Limit() query that share the same backend Target ID. Since LimitToLast()
// queries are sent to the backend with a modified OrderBy() clause, they can
// map to the same target representation as Limit() query, even if both queries
// appear separate to the user.
TEST_F(FirestoreIntegrationTest,
       TestListenUnlistenRelistenSequenceOfMirrorQueries) {
  CollectionReference collection = Collection(
      {{"a",
        {{"k", FieldValue::String("a")}, {"sort", FieldValue::Integer(0)}}},
       {"b",
        {{"k", FieldValue::String("b")}, {"sort", FieldValue::Integer(1)}}},
       {"c",
        {{"k", FieldValue::String("c")}, {"sort", FieldValue::Integer(1)}}},
       {"d",
        {{"k", FieldValue::String("d")}, {"sort", FieldValue::Integer(2)}}}});

  // Set up `limit` query.
  Query limit =
      collection.Limit(2).OrderBy("sort", Query::Direction::kAscending);
  EventAccumulator<QuerySnapshot> limit_accumulator;
  ListenerRegistration limit_registration =
      limit_accumulator.listener()->AttachTo(&limit);

  // Set up mirroring `limitToLast` query.
  Query limit_to_last =
      collection.LimitToLast(2).OrderBy("sort", Query::Direction::kDescending);
  EventAccumulator<QuerySnapshot> limit_to_last_accumulator;
  ListenerRegistration limit_to_last_registration =
      limit_to_last_accumulator.listener()->AttachTo(&limit_to_last);

  // Verify both queries get expected result.
  QuerySnapshot snapshot = limit_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(0)}},
                           MapFieldValue{{"k", FieldValue::String("b")},
                                         {"sort", FieldValue::Integer(1)}}));
  snapshot = limit_to_last_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("b")},
                                         {"sort", FieldValue::Integer(1)}},
                           MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(0)}}));

  // Unlisten then re-listen to the `limit` query.
  limit_registration.Remove();
  limit_registration = limit_accumulator.listener()->AttachTo(&limit);

  // Verify `limit` query still works.
  snapshot = limit_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(0)}},
                           MapFieldValue{{"k", FieldValue::String("b")},
                                         {"sort", FieldValue::Integer(1)}}));

  // Add a document that would change the result set.
  Await(collection.Add(MapFieldValue{{"k", FieldValue::String("e")},
                                     {"sort", FieldValue::Integer(-1)}}));

  // Verify both queries get expected result.
  snapshot = limit_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("e")},
                                         {"sort", FieldValue::Integer(-1)}},
                           MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(0)}}));
  snapshot = limit_to_last_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(0)}},
                           MapFieldValue{{"k", FieldValue::String("e")},
                                         {"sort", FieldValue::Integer(-1)}}));

  // Unlisten to `LimitToLast`, update a doc, then relisten to `LimitToLast`
  limit_to_last_registration.Remove();
  Await(collection.Document("a").Update(MapFieldValue{
      {"k", FieldValue::String("a")}, {"sort", FieldValue::Integer(-2)}}));
  limit_to_last_registration =
      limit_to_last_accumulator.listener()->AttachTo(&limit_to_last);

  // Verify both queries get expected result.
  snapshot = limit_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(-2)}},
                           MapFieldValue{{"k", FieldValue::String("e")},
                                         {"sort", FieldValue::Integer(-1)}}));
  snapshot = limit_to_last_accumulator.Await();
  EXPECT_THAT(
      QuerySnapshotToValues(snapshot),
      testing::ElementsAre(MapFieldValue{{"k", FieldValue::String("e")},
                                         {"sort", FieldValue::Integer(-1)}},
                           MapFieldValue{{"k", FieldValue::String("a")},
                                         {"sort", FieldValue::Integer(-2)}}));
}

TEST_F(FirestoreIntegrationTest,
       TestKeyOrderIsDescendingForDescendingInequality) {
  CollectionReference collection =
      Collection({{"a", {{"foo", FieldValue::Integer(42)}}},
                  {"b", {{"foo", FieldValue::Double(42.0)}}},
                  {"c", {{"foo", FieldValue::Integer(42)}}},
                  {"d", {{"foo", FieldValue::Integer(21)}}},
                  {"e", {{"foo", FieldValue::Double(21.0)}}},
                  {"f", {{"foo", FieldValue::Integer(66)}}},
                  {"g", {{"foo", FieldValue::Double(66.0)}}}});
  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereGreaterThan("foo", FieldValue::Integer(21))
          .OrderBy(FieldPath({"foo"}), Query::Direction::kDescending));
  EXPECT_EQ(std::vector<std::string>({"g", "f", "c", "b", "a"}),
            QuerySnapshotToIds(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestUnaryFilterQueries) {
  CollectionReference collection = Collection(
      {{"a", {{"null", FieldValue::Null()}, {"nan", FieldValue::Double(NAN)}}},
       {"b", {{"null", FieldValue::Null()}, {"nan", FieldValue::Integer(0)}}},
       {"c",
        {{"null", FieldValue::Boolean(false)},
         {"nan", FieldValue::Double(NAN)}}}});
  QuerySnapshot snapshot =
      ReadDocuments(collection.WhereEqualTo("null", FieldValue::Null())
                        .WhereEqualTo("nan", FieldValue::Double(NAN)));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"null", FieldValue::Null()},
                                         {"nan", FieldValue::Double(NAN)}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestQueryWithFieldPaths) {
  CollectionReference collection =
      Collection({{"a", {{"a", FieldValue::Integer(1)}}},
                  {"b", {{"a", FieldValue::Integer(2)}}},
                  {"c", {{"a", FieldValue::Integer(3)}}}});
  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereLessThan(FieldPath({"a"}), FieldValue::Integer(3))
          .OrderBy(FieldPath({"a"}), Query::Direction::kDescending));
  EXPECT_EQ(std::vector<std::string>({"b", "a"}), QuerySnapshotToIds(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestFilterOnInfinity) {
  CollectionReference collection =
      Collection({{"a", {{"inf", FieldValue::Double(INFINITY)}}},
                  {"b", {{"inf", FieldValue::Double(-INFINITY)}}}});
  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereEqualTo("inf", FieldValue::Double(INFINITY)));
  EXPECT_EQ(
      std::vector<MapFieldValue>({{{"inf", FieldValue::Double(INFINITY)}}}),
      QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestWillNotGetMetadataOnlyUpdates) {
  CollectionReference collection =
      Collection({{"a", {{"v", FieldValue::String("a")}}},
                  {"b", {{"v", FieldValue::String("b")}}}});

  TestEventListener<QuerySnapshot> listener("no metadata-only update");
  ListenerRegistration registration = listener.AttachTo(&collection);
  Await(listener);
  EXPECT_EQ(1, listener.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener.last_result()));

  WriteDocument(collection.Document("a"), {{"v", FieldValue::String("a1")}});
  EXPECT_EQ(2, listener.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener.last_result()));

  registration.Remove();
}

TEST_F(FirestoreIntegrationTest,
       TestCanListenForTheSameQueryWithDifferentOptions) {
  CollectionReference collection = Collection();
  WriteDocuments(collection, {{"a", {{"v", FieldValue::String("a")}}},
                              {"b", {{"v", FieldValue::String("b")}}}});

  // Add two listeners, one tracking metadata-change while the other not.
  TestEventListener<QuerySnapshot> listener("no metadata-only update");
  TestEventListener<QuerySnapshot> listener_full("include metadata update");

  ListenerRegistration registration_full =
      listener_full.AttachTo(&collection, MetadataChanges::kInclude);
  ListenerRegistration registration = listener.AttachTo(&collection);

  Await(listener);
  Await(listener_full, 2);  // Let's make sure both events triggered.

  EXPECT_EQ(1, listener.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener.last_result()));
  EXPECT_EQ(2, listener_full.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener_full.last_result(1)));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener_full.last_result()));
  EXPECT_TRUE(listener_full.last_result(1).metadata().is_from_cache());
  EXPECT_FALSE(listener_full.last_result().metadata().is_from_cache());

  // Change document to trigger the listeners.
  WriteDocument(collection.Document("a"), {{"v", FieldValue::String("a1")}});
  // Only one event without options
  EXPECT_EQ(2, listener.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener.last_result()));
  // Expect two events for the write, once from latency compensation and once
  // from the acknowledgement from the server.
  Await(listener_full, 4);  // Let's make sure both events triggered.
  EXPECT_EQ(4, listener_full.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener_full.last_result(1)));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b")}}}),
            QuerySnapshotToValues(listener_full.last_result()));
  EXPECT_TRUE(listener_full.last_result(1).metadata().has_pending_writes());
  EXPECT_FALSE(listener_full.last_result().metadata().has_pending_writes());

  // Change document again to trigger the listeners.
  WriteDocument(collection.Document("b"), {{"v", FieldValue::String("b1")}});
  // Only one event without options
  EXPECT_EQ(3, listener.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b1")}}}),
            QuerySnapshotToValues(listener.last_result()));
  // Expect two events for the write, once from latency compensation and once
  // from the acknowledgement from the server.
  Await(listener_full, 6);  // Let's make sure both events triggered.
  EXPECT_EQ(6, listener_full.event_count());
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b1")}}}),
            QuerySnapshotToValues(listener_full.last_result(1)));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"v", FieldValue::String("a1")}},
                                        {{"v", FieldValue::String("b1")}}}),
            QuerySnapshotToValues(listener_full.last_result()));
  EXPECT_TRUE(listener_full.last_result(1).metadata().has_pending_writes());
  EXPECT_FALSE(listener_full.last_result().metadata().has_pending_writes());

  // Unregister listeners.
  registration.Remove();
  registration_full.Remove();
}

TEST_F(FirestoreIntegrationTest, TestCanListenForQueryMetadataChanges) {
  CollectionReference collection =
      Collection({{"1",
                   {{"sort", FieldValue::Double(1.0)},
                    {"filter", FieldValue::Boolean(true)},
                    {"key", FieldValue::Integer(1)}}},
                  {"2",
                   {{"sort", FieldValue::Double(2.0)},
                    {"filter", FieldValue::Boolean(true)},
                    {"key", FieldValue::Integer(2)}}},
                  {"3",
                   {{"sort", FieldValue::Double(3.0)},
                    {"filter", FieldValue::Boolean(true)},
                    {"key", FieldValue::Integer(3)}}},
                  {"4",
                   {{"sort", FieldValue::Double(4.0)},
                    {"filter", FieldValue::Boolean(false)},
                    {"key", FieldValue::Integer(4)}}}});

  // The first query does not have any document cached.
  TestEventListener<QuerySnapshot> listener1("listener to the first query");
  Query collection_with_filter1 =
      collection.WhereLessThan("key", FieldValue::Integer(4));
  ListenerRegistration registration1 =
      listener1.AttachTo(&collection_with_filter1);
  Await(listener1);
  EXPECT_EQ(1, listener1.event_count());
  EXPECT_EQ(std::vector<std::string>({"1", "2", "3"}),
            QuerySnapshotToIds(listener1.last_result()));

  // The second query has document cached from the first query.
  TestEventListener<QuerySnapshot> listener2("listener to the second query");
  Query collection_with_filter2 =
      collection.WhereEqualTo("filter", FieldValue::Boolean(true));
  ListenerRegistration registration2 =
      listener2.AttachTo(&collection_with_filter2, MetadataChanges::kInclude);
  Await(listener2, 2);  // Let's make sure both events triggered.
  EXPECT_EQ(2, listener2.event_count());
  EXPECT_EQ(std::vector<std::string>({"1", "2", "3"}),
            QuerySnapshotToIds(listener2.last_result(1)));
  EXPECT_EQ(std::vector<std::string>({"1", "2", "3"}),
            QuerySnapshotToIds(listener2.last_result()));
  EXPECT_TRUE(listener2.last_result(1).metadata().is_from_cache());
  EXPECT_FALSE(listener2.last_result().metadata().is_from_cache());

  // Unregister listeners.
  registration1.Remove();
  registration2.Remove();
}

TEST_F(FirestoreIntegrationTest, TestCanExplicitlySortByDocumentId) {
  CollectionReference collection =
      Collection({{"a", {{"key", FieldValue::String("a")}}},
                  {"b", {{"key", FieldValue::String("b")}}},
                  {"c", {{"key", FieldValue::String("c")}}}});
  // Ideally this would be descending to validate it's different than
  // the default, but that requires an extra index
  QuerySnapshot snapshot =
      ReadDocuments(collection.OrderBy(FieldPath::DocumentId()));
  EXPECT_EQ(std::vector<std::string>({"a", "b", "c"}),
            QuerySnapshotToIds(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestCanQueryByDocumentId) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  // Query by Document Id.
  QuerySnapshot snapshot1 = ReadDocuments(collection.WhereEqualTo(
      FieldPath::DocumentId(), FieldValue::String("ab")));
  EXPECT_EQ(std::vector<std::string>({"ab"}), QuerySnapshotToIds(snapshot1));

  // Query by Document Ids.
  QuerySnapshot snapshot2 = ReadDocuments(
      collection
          .WhereGreaterThan(FieldPath::DocumentId(), FieldValue::String("aa"))
          .WhereLessThanOrEqualTo(FieldPath::DocumentId(),
                                  FieldValue::String("ba")));
  EXPECT_EQ(std::vector<std::string>({"ab", "ba"}),
            QuerySnapshotToIds(snapshot2));
}

TEST_F(FirestoreIntegrationTest, TestCanQueryByDocumentIdUsingRefs) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  // Query by Document Id.
  QuerySnapshot snapshot1 = ReadDocuments(collection.WhereEqualTo(
      FieldPath::DocumentId(),
      FieldValue::Reference(collection.Document("ab"))));
  EXPECT_EQ(std::vector<std::string>({"ab"}), QuerySnapshotToIds(snapshot1));

  // Query by Document Ids.
  QuerySnapshot snapshot2 = ReadDocuments(
      collection
          .WhereGreaterThan(FieldPath::DocumentId(),
                            FieldValue::Reference(collection.Document("aa")))
          .WhereLessThanOrEqualTo(
              FieldPath::DocumentId(),
              FieldValue::Reference(collection.Document("ba"))));
  EXPECT_EQ(std::vector<std::string>({"ab", "ba"}),
            QuerySnapshotToIds(snapshot2));
}

TEST_F(FirestoreIntegrationTest, TestCanQueryWithAndWithoutDocumentKey) {
  CollectionReference collection = Collection();
  Await(collection.Add({}));
  QuerySnapshot snapshot1 = ReadDocuments(collection.OrderBy(
      FieldPath::DocumentId(), Query::Direction::kAscending));
  QuerySnapshot snapshot2 = ReadDocuments(collection);

  EXPECT_EQ(QuerySnapshotToValues(snapshot1), QuerySnapshotToValues(snapshot2));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotEqualFilters) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::String("98101")}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  // Search for zips not matching 98101.
  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereNotEqualTo("zip", FieldValue::Integer(98101)));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"c", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotEqualFiltersWithObject) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::String("98101")}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotEqualTo(
      "zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"h", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotEqualFiltersWithNull) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::String("98101")}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  // With Null.
  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotEqualTo(
      "zip", FieldValue::Map({{"code", FieldValue::Null()}})));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotEqualFiltersWithNan) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::String("98101")}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  QuerySnapshot snapshot =
      ReadDocuments(collection.WhereNotEqualTo("zip", FieldValue::Double(NAN)));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"a", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotEqualFiltersWithDocIds) {
  MapFieldValue doc_a = {{"key", FieldValue::String("aa")}};
  MapFieldValue doc_b = {{"key", FieldValue::String("ab")}};
  MapFieldValue doc_c = {{"key", FieldValue::String("ba")}};
  MapFieldValue doc_d = {{"key", FieldValue::String("bb")}};

  CollectionReference collection =
      Collection({{"aa", doc_a}, {"ab", doc_b}, {"ba", doc_c}, {"bb", doc_d}});

  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotEqualTo(
      FieldPath::DocumentId(), FieldValue::String("aa")));
  EXPECT_EQ(std::vector<MapFieldValue>({doc_b, doc_c, doc_d}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseArrayContainsFilters) {
  CollectionReference collection = Collection(
      {{"a", {{"array", FieldValue::Array({FieldValue::Integer(42)})}}},
       {"b",
        {{"array",
          FieldValue::Array({FieldValue::String("a"), FieldValue::Integer(42),
                             FieldValue::String("c")})}}},
       {"c",
        {{"array",
          FieldValue::Array(
              {FieldValue::Double(41.999), FieldValue::String("42"),
               FieldValue::Map(
                   {{"a", FieldValue::Array({FieldValue::Integer(42)})}})})}}},
       {"d",
        {{"array", FieldValue::Array({FieldValue::Integer(42)})},
         {"array2", FieldValue::Array({FieldValue::String("bingo")})}}}});
  // Search for 42
  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereArrayContains("array", FieldValue::Integer(42)));
  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"array", FieldValue::Array({FieldValue::Integer(42)})}},
           {{"array", FieldValue::Array({FieldValue::String("a"),
                                         FieldValue::Integer(42),
                                         FieldValue::String("c")})}},
           {{"array", FieldValue::Array({FieldValue::Integer(42)})},
            {"array2", FieldValue::Array({FieldValue::String("bingo")})}}}),
      QuerySnapshotToValues(snapshot));

  // NOTE: The backend doesn't currently support null, NaN, objects, or arrays,
  // so there isn't much of anything else interesting to test.
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseInFilters) {
  CollectionReference collection = Collection(
      {{"a", {{"zip", FieldValue::Integer(98101)}}},
       {"b", {{"zip", FieldValue::Integer(98102)}}},
       {"c", {{"zip", FieldValue::Integer(98103)}}},
       {"d", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
       {"e",
        {{"zip",
          FieldValue::Array(
              {FieldValue::String("98101"),
               FieldValue::Map({{"zip", FieldValue::Integer(98101)}})})}}},
       {"f", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
       {"g",
        {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                    FieldValue::Integer(98102)})}}}});
  // Search for zips matching 98101, 98103, or [98101, 98102].
  QuerySnapshot snapshot = ReadDocuments(collection.WhereIn(
      "zip", {FieldValue::Integer(98101), FieldValue::Integer(98103),
              FieldValue::Array(
                  {FieldValue::Integer(98101), FieldValue::Integer(98102)})}));
  EXPECT_EQ(std::vector<MapFieldValue>(
                {{{"zip", FieldValue::Integer(98101)}},
                 {{"zip", FieldValue::Integer(98103)}},
                 {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                             FieldValue::Integer(98102)})}}}),
            QuerySnapshotToValues(snapshot));

  // With objects.
  snapshot = ReadDocuments(collection.WhereIn(
      "zip", {FieldValue::Map({{"code", FieldValue::Integer(500)}})}));
  EXPECT_EQ(
      std::vector<MapFieldValue>(
          {{{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}}),
      QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseInFiltersWithDocIds) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  QuerySnapshot snapshot = ReadDocuments(
      collection.WhereIn(FieldPath::DocumentId(),
                         {FieldValue::String("aa"), FieldValue::String("ab")}));
  EXPECT_EQ(std::vector<MapFieldValue>({{{"key", FieldValue::String("aa")}},
                                        {{"key", FieldValue::String("ab")}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotInFilters) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::Integer(98103)}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  // Search for zips not matching 98101, 98103 or [98101, 98102].
  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotIn(
      "zip", {{FieldValue::Integer(98101), FieldValue::Integer(98103),
               FieldValue::Array({{FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)}})}}));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"c", "d", "f", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotInFiltersWithObject) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::Integer(98103)}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotIn(
      "zip", {{FieldValue::Map({{"code", FieldValue::Integer(500)}})}}));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"h", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotInFiltersWithNull) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::Integer(98103)}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  // With Null, this leads to no result.
  QuerySnapshot snapshot =
      ReadDocuments(collection.WhereNotIn("zip", {{FieldValue::Null()}}));
  EXPECT_THAT(QuerySnapshotToValues(snapshot), IsEmpty());
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotInFiltersWithNan) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::Integer(98103)}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  // With NAN.
  QuerySnapshot snapshot =
      ReadDocuments(collection.WhereNotIn("zip", {{FieldValue::Double(NAN)}}));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"a", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest,
       TestQueriesCanUseNotInFiltersWithNanAndNumber) {
  // These documents are ordered by value in "zip" since the NotEqual filter is
  // an inequality, which results in documents being sorted by value.
  std::map<std::string, MapFieldValue> docs = {
      {"a", {{"zip", FieldValue::Double(NAN)}}},
      {"b", {{"zip", FieldValue::Integer(91102)}}},
      {"c", {{"zip", FieldValue::Integer(98101)}}},
      {"d", {{"zip", FieldValue::Integer(98103)}}},
      {"e", {{"zip", FieldValue::Array({FieldValue::Integer(98101)})}}},
      {"f",
       {{"zip", FieldValue::Array({FieldValue::Integer(98101),
                                   FieldValue::Integer(98102)})}}},
      {"g",
       {{"zip", FieldValue::Array({FieldValue::String("98101"),
                                   FieldValue::Map({{"zip", FieldValue::Integer(
                                                                98101)}})})}}},
      {"h", {{"zip", FieldValue::Map({{"code", FieldValue::Integer(500)}})}}},
      {"i", {{"code", FieldValue::Integer(500)}}},
      {"j", MapFieldValue{{"zip", FieldValue::Null()}}},
  };
  CollectionReference collection = Collection(docs);

  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotIn(
      "zip", {{FieldValue::Double(NAN), FieldValue::Integer(98101)}}));
  EXPECT_THAT(QuerySnapshotToValues(snapshot),
              ElementsAreArray(AllDocsExcept(docs, {"a", "c", "i", "j"})));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseNotInFiltersWithDocIds) {
  MapFieldValue doc_a = {{"key", FieldValue::String("aa")}};
  MapFieldValue doc_b = {{"key", FieldValue::String("ab")}};
  MapFieldValue doc_c = {{"key", FieldValue::String("ba")}};
  MapFieldValue doc_d = {{"key", FieldValue::String("bb")}};

  CollectionReference collection =
      Collection({{"aa", doc_a}, {"ab", doc_b}, {"ba", doc_c}, {"bb", doc_d}});

  QuerySnapshot snapshot = ReadDocuments(collection.WhereNotIn(
      FieldPath::DocumentId(),
      {{FieldValue::String("aa"), FieldValue::String("ab")}}));
  EXPECT_EQ(std::vector<MapFieldValue>({doc_c, doc_d}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestQueriesCanUseArrayContainsAnyFilters) {
  CollectionReference collection = Collection(
      {{"a", {{"array", FieldValue::Array({FieldValue::Integer(42)})}}},
       {"b",
        {{"array",
          FieldValue::Array({FieldValue::String("a"), FieldValue::Integer(42),
                             FieldValue::String("c")})}}},
       {"c",
        {{"array",
          FieldValue::Array(
              {FieldValue::Double(41.999), FieldValue::String("42"),
               FieldValue::Map(
                   {{"a", FieldValue::Array({FieldValue::Integer(42)})}})})}}},
       {"d",
        {{"array", FieldValue::Array({FieldValue::Integer(42)})},
         {"array2", FieldValue::Array({FieldValue::String("bingo")})}}},
       {"e", {{"array", FieldValue::Array({FieldValue::Integer(43)})}}},
       {"f",
        {{"array", FieldValue::Array(
                       {FieldValue::Map({{"a", FieldValue::Integer(42)}})})}}},
       {"g", {{"array", FieldValue::Integer(42)}}}});

  // Search for "array" to contain [42, 43]
  QuerySnapshot snapshot = ReadDocuments(collection.WhereArrayContainsAny(
      "array", {FieldValue::Integer(42), FieldValue::Integer(43)}));
  EXPECT_EQ(std::vector<MapFieldValue>(
                {{{"array", FieldValue::Array({FieldValue::Integer(42)})}},
                 {{"array", FieldValue::Array({FieldValue::String("a"),
                                               FieldValue::Integer(42),
                                               FieldValue::String("c")})}},
                 {{"array", FieldValue::Array({FieldValue::Integer(42)})},
                  {"array2", FieldValue::Array({FieldValue::String("bingo")})}},
                 {{"array", FieldValue::Array({FieldValue::Integer(43)})}}}),
            QuerySnapshotToValues(snapshot));

  // With objects
  snapshot = ReadDocuments(collection.WhereArrayContainsAny(
      "array", {FieldValue::Map({{"a", FieldValue::Integer(42)}})}));
  EXPECT_EQ(std::vector<MapFieldValue>(
                {{{"array", FieldValue::Array({FieldValue::Map(
                                {{"a", FieldValue::Integer(42)}})})}}}),
            QuerySnapshotToValues(snapshot));
}

TEST_F(FirestoreIntegrationTest, TestCollectionGroupQueries) {
  Firestore* db = TestFirestore();
  // Use .Document() to get a random collection group name to use but ensure it
  // starts with 'b' for predictable ordering.
  std::string collection_group = "b" + db->Collection("foo").Document().id();

  std::string doc_paths[] = {
      "abc/123/" + collection_group + "/cg-doc1",
      "abc/123/" + collection_group + "/cg-doc2",
      collection_group + "/cg-doc3",
      collection_group + "/cg-doc4",
      "def/456/" + collection_group + "/cg-doc5",
      collection_group + "/virtual-doc/nested-coll/not-cg-doc",
      "x" + collection_group + "/not-cg-doc",
      collection_group + "x/not-cg-doc",
      "abc/123/" + collection_group + "x/not-cg-doc",
      "abc/123/x" + collection_group + "/not-cg-doc",
      "abc/" + collection_group,
  };

  WriteBatch batch = db->batch();
  for (const auto& doc_path : doc_paths) {
    batch.Set(db->Document(doc_path), {{"x", FieldValue::Integer(1)}});
  }
  Await(batch.Commit());

  QuerySnapshot query_snapshot =
      ReadDocuments(db->CollectionGroup(collection_group));
  EXPECT_EQ(std::vector<std::string>(
                {"cg-doc1", "cg-doc2", "cg-doc3", "cg-doc4", "cg-doc5"}),
            QuerySnapshotToIds(query_snapshot));
}

TEST_F(FirestoreIntegrationTest,
       TestCollectionGroupQueriesWithStartAtEndAtWithArbitraryDocumentIds) {
  Firestore* db = TestFirestore();
  // Use .Document() to get a random collection group name to use but ensure it
  // starts with 'b' for predictable ordering.
  std::string collection_group = "b" + db->Collection("foo").Document().id();

  std::string doc_paths[] = {
      "a/a/" + collection_group + "/cg-doc1",
      "a/b/a/b/" + collection_group + "/cg-doc2",
      "a/b/" + collection_group + "/cg-doc3",
      "a/b/c/d/" + collection_group + "/cg-doc4",
      "a/c/" + collection_group + "/cg-doc5",
      collection_group + "/cg-doc6",
      "a/b/nope/nope",
  };

  WriteBatch batch = db->batch();
  for (const auto& doc_path : doc_paths) {
    batch.Set(db->Document(doc_path), {{"x", FieldValue::Integer(1)}});
  }
  Await(batch.Commit());

  QuerySnapshot query_snapshot =
      ReadDocuments(db->CollectionGroup(collection_group)
                        .OrderBy(FieldPath::DocumentId())
                        .StartAt({FieldValue::String("a/b")})
                        .EndAt({FieldValue::String("a/b0")}));
  EXPECT_EQ(std::vector<std::string>({"cg-doc2", "cg-doc3", "cg-doc4"}),
            QuerySnapshotToIds(query_snapshot));
}

TEST_F(FirestoreIntegrationTest,
       TestCollectionGroupQueriesWithWhereFiltersOnArbitraryDocumentIds) {
  Firestore* db = TestFirestore();
  // Use .Document() to get a random collection group name to use but ensure it
  // starts with 'b' for predictable ordering.
  std::string collection_group = "b" + db->Collection("foo").Document().id();

  std::string doc_paths[] = {
      "a/a/" + collection_group + "/cg-doc1",
      "a/b/a/b/" + collection_group + "/cg-doc2",
      "a/b/" + collection_group + "/cg-doc3",
      "a/b/c/d/" + collection_group + "/cg-doc4",
      "a/c/" + collection_group + "/cg-doc5",
      collection_group + "/cg-doc6",
      "a/b/nope/nope",
  };

  WriteBatch batch = db->batch();
  for (const auto& doc_path : doc_paths) {
    batch.Set(db->Document(doc_path), {{"x", FieldValue::Integer(1)}});
  }
  Await(batch.Commit());

  QuerySnapshot query_snapshot =
      ReadDocuments(db->CollectionGroup(collection_group)
                        .WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                                   FieldValue::String("a/b"))
                        .WhereLessThanOrEqualTo(FieldPath::DocumentId(),
                                                FieldValue::String("a/b0")));
  EXPECT_EQ(std::vector<std::string>({"cg-doc2", "cg-doc3", "cg-doc4"}),
            QuerySnapshotToIds(query_snapshot));

  query_snapshot = ReadDocuments(
      db->CollectionGroup(collection_group)
          .WhereGreaterThan(FieldPath::DocumentId(), FieldValue::String("a/b"))
          .WhereLessThan(
              FieldPath::DocumentId(),
              FieldValue::String("a/b/" + collection_group + "/cg-doc3")));
  EXPECT_EQ(std::vector<std::string>({"cg-doc2"}),
            QuerySnapshotToIds(query_snapshot));
}

#endif  // !defined(FIRESTORE_STUB_BUILD)

#if defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)
TEST(QueryTest, Construction) {
  testutil::AssertWrapperConstructionContract<Query>();
}

TEST(QueryTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<Query>();
}
#endif  // defined(__ANDROID__) || defined(FIRESTORE_STUB_BUILD)

}  // namespace firestore
}  // namespace firebase
