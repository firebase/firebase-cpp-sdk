/*
 * Copyright 2023 Google LLC
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

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "firebase/firestore.h"
#include "firebase/firestore/field_value.h"
#include "firebase/firestore/map_field_value.h"
#include "firestore/src/common/macros.h"
#include "firestore_integration_test.h"
#include "util/event_accumulator.h"

#if defined(__ANDROID__)
#include "firestore/src/android/query_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#endif  // defined(__ANDROID__)

#include "Firestore/core/src/util/firestore_exceptions.h"
#include "firebase/firestore/firestore_errors.h"
#include "firebase_test_framework.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

using AggregateCountTest = FirestoreIntegrationTest;

}  // namespace

TEST_F(AggregateCountTest, TestKeyOrderIsDescendingForDescendingInequality) {
  CollectionReference collection =
      Collection({{"a", {{"foo", FieldValue::Integer(42)}}},
                  {"b", {{"foo", FieldValue::Double(42.0)}}},
                  {"c", {{"foo", FieldValue::Integer(42)}}},
                  {"d", {{"foo", FieldValue::Integer(21)}}},
                  {"e", {{"foo", FieldValue::Double(21.0)}}},
                  {"f", {{"foo", FieldValue::Integer(66)}}},
                  {"g", {{"foo", FieldValue::Double(66.0)}}}});
  const AggregateQuery aggregate_query =
      collection.WhereGreaterThan("foo", FieldValue::Integer(21))
          .OrderBy(FieldPath({"foo"}), Query::Direction::kDescending)
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(5, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestUnaryFilterQueries) {
  CollectionReference collection = Collection(
      {{"a", {{"null", FieldValue::Null()}, {"nan", FieldValue::Double(NAN)}}},
       {"b", {{"null", FieldValue::Null()}, {"nan", FieldValue::Integer(0)}}},
       {"c",
        {{"null", FieldValue::Boolean(false)},
         {"nan", FieldValue::Double(NAN)}}}});

  const AggregateQuery aggregate_query =
      collection.WhereEqualTo("null", FieldValue::Null())
          .WhereEqualTo("nan", FieldValue::Double(NAN))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(1, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueryWithFieldPaths) {
  CollectionReference collection =
      Collection({{"a", {{"a", FieldValue::Integer(1)}}},
                  {"b", {{"a", FieldValue::Integer(2)}}},
                  {"c", {{"a", FieldValue::Integer(3)}}}});
  const AggregateQuery aggregate_query =
      collection.WhereLessThan(FieldPath({"a"}), FieldValue::Integer(3))
          .OrderBy(FieldPath({"a"}), Query::Direction::kDescending)
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(2, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestFilterOnInfinity) {
  CollectionReference collection =
      Collection({{"a", {{"inf", FieldValue::Double(INFINITY)}}},
                  {"b", {{"inf", FieldValue::Double(-INFINITY)}}}});

  const AggregateQuery aggregate_query =
      collection.WhereEqualTo("inf", FieldValue::Double(INFINITY)).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(1, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestCanQueryByDocumentId) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  // Query by Document Id.
  const AggregateQuery aggregate_query1 =
      collection.WhereEqualTo(FieldPath::DocumentId(), FieldValue::String("ab"))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot1 =
      ReadAggregate(aggregate_query1);
  EXPECT_EQ(1, aggregate_snapshot1.count());
  EXPECT_EQ(aggregate_query1, aggregate_snapshot1.query());

  // Query by Document Ids.
  const AggregateQuery aggregate_query2 =
      collection
          .WhereGreaterThan(FieldPath::DocumentId(), FieldValue::String("aa"))
          .WhereLessThanOrEqualTo(FieldPath::DocumentId(),
                                  FieldValue::String("ba"))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot2 =
      ReadAggregate(aggregate_query2);
  EXPECT_EQ(2, aggregate_snapshot2.count());
  EXPECT_EQ(aggregate_query2, aggregate_snapshot2.query());
}

TEST_F(AggregateCountTest, TestCanQueryByDocumentIdUsingRefs) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  // Query by Document Id.
  const AggregateQuery aggregate_query1 =
      collection
          .WhereEqualTo(FieldPath::DocumentId(),
                        FieldValue::Reference(collection.Document("ab")))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot1 =
      ReadAggregate(aggregate_query1);
  EXPECT_EQ(1, aggregate_snapshot1.count());
  EXPECT_EQ(aggregate_query1, aggregate_snapshot1.query());

  // Query by Document Ids.
  const AggregateQuery aggregate_query2 =
      collection
          .WhereGreaterThan(FieldPath::DocumentId(),
                            FieldValue::Reference(collection.Document("aa")))
          .WhereLessThanOrEqualTo(
              FieldPath::DocumentId(),
              FieldValue::Reference(collection.Document("ba")))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot2 =
      ReadAggregate(aggregate_query2);
  EXPECT_EQ(2, aggregate_snapshot2.count());
  EXPECT_EQ(aggregate_query2, aggregate_snapshot2.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotEqualFilters) {
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
  const AggregateQuery aggregate_query =
      collection.WhereNotEqualTo("zip", FieldValue::Integer(98101)).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotEqualFiltersWithObject) {
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

  const AggregateQuery aggregate_query =
      collection
          .WhereNotEqualTo(
              "zip", FieldValue::Map({{"code", FieldValue::Integer(500)}}))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotEqualFiltersWithNull) {
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
  const AggregateQuery aggregate_query =
      collection
          .WhereNotEqualTo("zip",
                           FieldValue::Map({{"code", FieldValue::Null()}}))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(8, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotEqualFiltersWithNan) {
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

  const AggregateQuery aggregate_query =
      collection.WhereNotEqualTo("zip", FieldValue::Double(NAN)).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotEqualFiltersWithDocIds) {
  MapFieldValue doc_a = {{"key", FieldValue::String("aa")}};
  MapFieldValue doc_b = {{"key", FieldValue::String("ab")}};
  MapFieldValue doc_c = {{"key", FieldValue::String("ba")}};
  MapFieldValue doc_d = {{"key", FieldValue::String("bb")}};

  CollectionReference collection =
      Collection({{"aa", doc_a}, {"ab", doc_b}, {"ba", doc_c}, {"bb", doc_d}});

  const AggregateQuery aggregate_query =
      collection
          .WhereNotEqualTo(FieldPath::DocumentId(), FieldValue::String("aa"))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(3, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseArrayContainsFilters) {
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
  const AggregateQuery aggregate_query =
      collection.WhereArrayContains("array", FieldValue::Integer(42)).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(3, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());

  // NOTE: The backend doesn't currently support null, NaN, objects, or arrays,
  // so there isn't much of anything else interesting to test.
}

TEST_F(AggregateCountTest, TestQueriesCanUseInFilters) {
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
  const AggregateQuery aggregate_query1 =
      collection
          .WhereIn("zip",
                   {FieldValue::Integer(98101), FieldValue::Integer(98103),
                    FieldValue::Array({FieldValue::Integer(98101),
                                       FieldValue::Integer(98102)})})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot1 =
      ReadAggregate(aggregate_query1);
  EXPECT_EQ(3, aggregate_snapshot1.count());
  EXPECT_EQ(aggregate_query1, aggregate_snapshot1.query());

  // With objects.
  const AggregateQuery aggregate_query2 =
      collection
          .WhereIn("zip",
                   {FieldValue::Map({{"code", FieldValue::Integer(500)}})})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot2 =
      ReadAggregate(aggregate_query2);
  EXPECT_EQ(1, aggregate_snapshot2.count());
  EXPECT_EQ(aggregate_query2, aggregate_snapshot2.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseInFiltersWithDocIds) {
  CollectionReference collection =
      Collection({{"aa", {{"key", FieldValue::String("aa")}}},
                  {"ab", {{"key", FieldValue::String("ab")}}},
                  {"ba", {{"key", FieldValue::String("ba")}}},
                  {"bb", {{"key", FieldValue::String("bb")}}}});

  const AggregateQuery aggregate_query =
      collection
          .WhereIn(FieldPath::DocumentId(),
                   {FieldValue::String("aa"), FieldValue::String("ab")})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(2, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFilters) {
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
  const AggregateQuery aggregate_query =
      collection
          .WhereNotIn("zip",
                      {{FieldValue::Integer(98101), FieldValue::Integer(98103),
                        FieldValue::Array({{FieldValue::Integer(98101),
                                            FieldValue::Integer(98102)}})}})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(5, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFiltersWithObject) {
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

  const AggregateQuery aggregate_query =
      collection
          .WhereNotIn("zip",
                      {{FieldValue::Map({{"code", FieldValue::Integer(500)}})}})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFiltersWithNull) {
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
  const AggregateQuery aggregate_query =
      collection.WhereNotIn("zip", {{FieldValue::Null()}}).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(0, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFiltersWithNan) {
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
  const AggregateQuery aggregate_query =
      collection.WhereNotIn("zip", {{FieldValue::Double(NAN)}}).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  // TODO(b/272502845): NaN Handling
  // EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(8, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFiltersWithNanAndNumber) {
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

  const AggregateQuery aggregate_query =
      collection
          .WhereNotIn("zip",
                      {{FieldValue::Double(NAN), FieldValue::Integer(98101)}})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  // TODO(b/272502845): NaN Handling
  // EXPECT_EQ(6, aggregate_snapshot.count());
  EXPECT_EQ(7, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseNotInFiltersWithDocIds) {
  MapFieldValue doc_a = {{"key", FieldValue::String("aa")}};
  MapFieldValue doc_b = {{"key", FieldValue::String("ab")}};
  MapFieldValue doc_c = {{"key", FieldValue::String("ba")}};
  MapFieldValue doc_d = {{"key", FieldValue::String("bb")}};

  CollectionReference collection =
      Collection({{"aa", doc_a}, {"ab", doc_b}, {"ba", doc_c}, {"bb", doc_d}});

  const AggregateQuery aggregate_query =
      collection
          .WhereNotIn(FieldPath::DocumentId(),
                      {{FieldValue::String("aa"), FieldValue::String("ab")}})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(2, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest, TestQueriesCanUseArrayContainsAnyFilters) {
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
  const AggregateQuery aggregate_query1 =
      collection
          .WhereArrayContainsAny(
              "array", {FieldValue::Integer(42), FieldValue::Integer(43)})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot1 =
      ReadAggregate(aggregate_query1);
  EXPECT_EQ(4, aggregate_snapshot1.count());
  EXPECT_EQ(aggregate_query1, aggregate_snapshot1.query());

  // With objects
  const AggregateQuery aggregate_query2 =
      collection
          .WhereArrayContainsAny(
              "array", {FieldValue::Map({{"a", FieldValue::Integer(42)}})})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot2 =
      ReadAggregate(aggregate_query2);
  EXPECT_EQ(1, aggregate_snapshot2.count());
  EXPECT_EQ(aggregate_query2, aggregate_snapshot2.query());
}

TEST_F(AggregateCountTest, TestCollectionGroupQueries) {
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

  const AggregateQuery aggregate_query =
      db->CollectionGroup(collection_group).Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(5, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest,
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

  const AggregateQuery aggregate_query =
      db->CollectionGroup(collection_group)
          .OrderBy(FieldPath::DocumentId())
          .StartAt({FieldValue::String("a/b")})
          .EndAt({FieldValue::String("a/b0")})
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot =
      ReadAggregate(aggregate_query);
  EXPECT_EQ(3, aggregate_snapshot.count());
  EXPECT_EQ(aggregate_query, aggregate_snapshot.query());
}

TEST_F(AggregateCountTest,
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

  const AggregateQuery aggregate_query1 =
      db->CollectionGroup(collection_group)
          .WhereGreaterThanOrEqualTo(FieldPath::DocumentId(),
                                     FieldValue::String("a/b"))
          .WhereLessThanOrEqualTo(FieldPath::DocumentId(),
                                  FieldValue::String("a/b0"))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot1 =
      ReadAggregate(aggregate_query1);
  EXPECT_EQ(3, aggregate_snapshot1.count());
  EXPECT_EQ(aggregate_query1, aggregate_snapshot1.query());

  const AggregateQuery aggregate_query2 =
      db->CollectionGroup(collection_group)
          .WhereGreaterThan(FieldPath::DocumentId(), FieldValue::String("a/b"))
          .WhereLessThan(
              FieldPath::DocumentId(),
              FieldValue::String("a/b/" + collection_group + "/cg-doc3"))
          .Count();
  const AggregateQuerySnapshot aggregate_snapshot2 =
      ReadAggregate(aggregate_query2);
  EXPECT_EQ(1, aggregate_snapshot2.count());
  EXPECT_EQ(aggregate_query2, aggregate_snapshot2.query());
}

#if defined(__ANDROID__)
TEST(QueryTestAndroidStub, Construction) {
  testutil::AssertWrapperConstructionContract<Query>();
}

TEST(QueryTestAndroidStub, Assignment) {
  testutil::AssertWrapperAssignmentContract<Query>();
}
#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
