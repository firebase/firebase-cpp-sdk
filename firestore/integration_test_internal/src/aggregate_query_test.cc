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

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

#include "gtest/gtest.h"

#if defined(__ANDROID__)
#include "firestore/src/android/converter_android.h"
#else
#include "firestore/src/main/converter_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

size_t AggregateQueryHash(const AggregateQuery& aggregate_query) {
  return aggregate_query.Hash();
}

namespace {

using AggregateQueryTest = FirestoreIntegrationTest;

TEST_F(AggregateQueryTest, DefaultConstructor) {
  AggregateQuery aggregate_query;
  EXPECT_EQ(aggregate_query.query(), Query());
}

TEST_F(AggregateQueryTest, CopyConstructor) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  AggregateQuery copied_aggregate_query(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_EQ(copied_aggregate_query.query(), query);
}

TEST_F(AggregateQueryTest, CopyAssignmentOperator) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  AggregateQuery copied_aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_EQ(copied_aggregate_query.query(), query);
}

TEST_F(AggregateQueryTest, MoveConstructor) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery aggregate_query = query.Count();

  AggregateQuery moved_snapshot_dest(std::move(aggregate_query));

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_EQ(moved_snapshot_dest.query(), query);
}

TEST_F(AggregateQueryTest, MoveAssignmentOperator) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery aggregate_query = query.Count();

  AggregateQuery snapshot_move_dest = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_EQ(snapshot_move_dest.query(), query);
}

TEST_F(AggregateQueryTest, TestHashCode) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}}});
  Query query1 =
      collection.Limit(2).OrderBy("sort", Query::Direction::kAscending);
  Query query2 =
      collection.Limit(2).OrderBy("sort", Query::Direction::kDescending);
  EXPECT_NE(AggregateQueryHash(query1.Count()),
            AggregateQueryHash(query2.Count()));
  EXPECT_EQ(AggregateQueryHash(query1.Count()),
            AggregateQueryHash(query1.Count()));
}

}
}  // namespace firestore
}  // namespace firebase
