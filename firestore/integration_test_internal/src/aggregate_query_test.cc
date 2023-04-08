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

namespace firebase {
namespace firestore {

size_t AggregateQueryHash(const AggregateQuery& aggregate_query) {
  return aggregate_query.Hash();
}

namespace {

using AggregateQueryTest = FirestoreIntegrationTest;

TEST_F(AggregateQueryTest, DefaultConstructorReturnsInvalidObject) {
  AggregateQuery aggregate_query;
  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest,
       CopyConstructorAppliedToValidObjectReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), query);
  EXPECT_TRUE(copied_aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest, CopyConstructorAppliedToDefaultReturnsEqualObject) {
  const AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), Query());
  EXPECT_FALSE(copied_aggregate_query.is_valid());
}

TEST_F(
    AggregateQueryTest,
    DefaultObjectCopyAssignmentOperatorAppliedToValidObjectOperatorReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query;

  EXPECT_EQ(copied_aggregate_query.query(), Query());
  EXPECT_FALSE(copied_aggregate_query.is_valid());

  copied_aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), query);
  EXPECT_TRUE(copied_aggregate_query.is_valid());
}

TEST_F(
    AggregateQueryTest,
    DefaultObjectCopyAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query;

  EXPECT_EQ(copied_aggregate_query.query(), Query());
  EXPECT_FALSE(copied_aggregate_query.is_valid());

  copied_aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), Query());
  EXPECT_FALSE(copied_aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest,
       ValidObjectCopyAssignmentAppliedToValidObjectReturnsEqualObject) {
  const Query query1 = TestFirestore()->Collection("foo").Limit(10);
  const Query query2 = TestFirestore()->Collection("bar").Limit(20);
  const AggregateQuery aggregate_query = query1.Count();

  EXPECT_EQ(aggregate_query.query(), query1);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query = query2.Count();

  EXPECT_EQ(copied_aggregate_query.query(), query2);
  EXPECT_TRUE(copied_aggregate_query.is_valid());

  copied_aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), query1);
  EXPECT_TRUE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), query1);
  EXPECT_TRUE(copied_aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest,
       ValidObjectCopyAssignmentAppliedToDefaultReturnsEqualObject) {
  const AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  const Query query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery copied_aggregate_query = query.Count();

  EXPECT_EQ(copied_aggregate_query.query(), query);
  EXPECT_TRUE(copied_aggregate_query.is_valid());

  copied_aggregate_query = aggregate_query;

  EXPECT_EQ(copied_aggregate_query.query(), Query());
  EXPECT_FALSE(copied_aggregate_query.is_valid());

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest, CopyAssignmentAppliedSelfReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest,
       CopyAssignmentAppliedToValidObjectReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery copied_aggregate_query = aggregate_query;

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  EXPECT_EQ(copied_aggregate_query.query(), query);
  EXPECT_TRUE(copied_aggregate_query.is_valid());
}

TEST_F(AggregateQueryTest,
       MoveConstructorAppliedToValidObjectReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery moved_snapshot_dest(std::move(aggregate_query));

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(moved_snapshot_dest.query(), query);
  EXPECT_TRUE(moved_snapshot_dest.is_valid());
}

TEST_F(AggregateQueryTest,
       MoveConstructorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  AggregateQuery moved_snapshot_dest(std::move(aggregate_query));

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(moved_snapshot_dest.query(), Query());
  EXPECT_FALSE(moved_snapshot_dest.is_valid());
}

TEST_F(
    AggregateQueryTest,
    DefaultObjectMoveAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery snapshot_move_dest = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(snapshot_move_dest.query(), query);
  EXPECT_TRUE(snapshot_move_dest.is_valid());
}

TEST_F(
    AggregateQueryTest,
    DefaultObjectMoveAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  AggregateQuery snapshot_move_dest = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(snapshot_move_dest.query(), Query());
  EXPECT_FALSE(snapshot_move_dest.is_valid());
}

TEST_F(
    AggregateQueryTest,
    ValidObjectMoveAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const Query query1 = TestFirestore()->Collection("foo").Limit(10);
  const Query query2 = TestFirestore()->Collection("bar").Limit(20);
  AggregateQuery aggregate_query = query1.Count();

  EXPECT_EQ(aggregate_query.query(), query1);
  EXPECT_TRUE(aggregate_query.is_valid());

  AggregateQuery snapshot_move_dest = query2.Count();

  EXPECT_EQ(snapshot_move_dest.query(), query2);
  EXPECT_TRUE(snapshot_move_dest.is_valid());

  snapshot_move_dest = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(snapshot_move_dest.query(), query1);
  EXPECT_TRUE(snapshot_move_dest.is_valid());
}

TEST_F(AggregateQueryTest,
       MoveAssignmentOperatorAppliedToSelfReturnsEqualObject) {
  const Query query = TestFirestore()->Collection("foo").Limit(10);

  AggregateQuery aggregate_query = query.Count();

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());

  aggregate_query = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), query);
  EXPECT_TRUE(aggregate_query.is_valid());
}

TEST_F(
    AggregateQueryTest,
    ValidObjectMoveAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuery aggregate_query;

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  const Query query = TestFirestore()->Collection("foo").Limit(10);
  AggregateQuery snapshot_move_dest = query.Count();

  EXPECT_EQ(snapshot_move_dest.query(), query);
  EXPECT_TRUE(snapshot_move_dest.is_valid());

  snapshot_move_dest = std::move(aggregate_query);

  EXPECT_EQ(aggregate_query.query(), Query());
  EXPECT_FALSE(aggregate_query.is_valid());

  EXPECT_EQ(snapshot_move_dest.query(), Query());
  EXPECT_FALSE(snapshot_move_dest.is_valid());
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

}  // namespace
}  // namespace firestore
}  // namespace firebase
