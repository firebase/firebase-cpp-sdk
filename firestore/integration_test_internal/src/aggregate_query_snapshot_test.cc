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

#include "firestore_integration_test.h"

#include "firebase/firestore.h"

#include "gtest/gtest.h"

#if defined(__ANDROID__)
#include "firestore/src/android/aggregate_query_android.h"
#include "firestore/src/android/aggregate_query_snapshot_android.h"
#include "firestore/src/android/converter_android.h"
#include "firestore/src/jni/object.h"
#else
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#include "firestore/src/main/converter_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

class AggregateQuerySnapshotTest : public FirestoreIntegrationTest {
 protected:
  static AggregateQuerySnapshot TestAggregateQuerySnapshot(
      AggregateQuery aggregate_query, const int count);
};

#if defined(__ANDROID__)
AggregateQuerySnapshot AggregateQuerySnapshotTest::TestAggregateQuerySnapshot(
    firebase::firestore::AggregateQuery aggregate_query, const int count) {
  AggregateQueryInternal* internal = GetInternal(&aggregate_query);
  FirestoreInternal* firestoreInternal = internal->firestore_internal();
  jni::Env env = firestoreInternal->GetEnv();
  return AggregateQuerySnapshotInternal::Create(env, *internal, count);
}
#else
AggregateQuerySnapshot AggregateQuerySnapshotTest::TestAggregateQuerySnapshot(
    firebase::firestore::AggregateQuery aggregate_query, const int count) {
  api::AggregateQuery aggregateQuery =
      GetInternal(&aggregate_query)->aggregate_query_;
  return MakePublic(
      AggregateQuerySnapshotInternal(std::move(aggregateQuery), count));
}
#endif  // defined(__ANDROID__)

std::size_t AggregateQuerySnapshotHash(const AggregateQuerySnapshot& snapshot) {
  return snapshot.Hash();
}

namespace {

TEST_F(AggregateQuerySnapshotTest, DefaultConstructorReturnsInvalidObject) {
  AggregateQuerySnapshot snapshot;
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_FALSE(snapshot.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       CopyConstructorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  AggregateQuerySnapshot copied_snapshot(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(copied_snapshot.count(), 0);
  EXPECT_EQ(copied_snapshot.query(), AggregateQuery());
  EXPECT_FALSE(copied_snapshot.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       CopyConstructorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 5);

  EXPECT_EQ(snapshot.count(), 5);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  AggregateQuerySnapshot copied_snapshot(snapshot);

  EXPECT_EQ(snapshot.count(), 5);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  EXPECT_EQ(copied_snapshot.count(), 5);
  EXPECT_EQ(copied_snapshot.query(), aggregate_query);
  EXPECT_TRUE(copied_snapshot.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    DefaultObjectCopyAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();
  const AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 7);

  AggregateQuerySnapshot snapshot_copy_dest;

  EXPECT_EQ(snapshot_copy_dest.count(), 0);
  EXPECT_EQ(snapshot_copy_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_copy_dest.is_valid());

  snapshot_copy_dest = snapshot;

  EXPECT_EQ(snapshot.count(), 7);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  EXPECT_EQ(snapshot_copy_dest.count(), 7);
  EXPECT_EQ(snapshot_copy_dest.query(), aggregate_query);
  EXPECT_TRUE(snapshot_copy_dest.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    DefaultObjectCopyAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  const AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  AggregateQuerySnapshot snapshot_copy_dest;

  EXPECT_EQ(snapshot_copy_dest.count(), 0);
  EXPECT_EQ(snapshot_copy_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_copy_dest.is_valid());

  snapshot_copy_dest = snapshot;

  EXPECT_EQ(snapshot_copy_dest.count(), 0);
  EXPECT_EQ(snapshot_copy_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_copy_dest.is_valid());

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    ValidObjectCopyAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot_copy_dest =
      TestAggregateQuerySnapshot(aggregate_query, 7);

  EXPECT_EQ(snapshot_copy_dest.count(), 7);
  EXPECT_EQ(snapshot_copy_dest.query(), aggregate_query);
  EXPECT_TRUE(snapshot_copy_dest.is_valid());

  snapshot_copy_dest = snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot_copy_dest.count(), 0);
  EXPECT_EQ(snapshot_copy_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_copy_dest.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    ValidObjectCopyAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query1 =
      TestFirestore()->Collection("foo").Limit(10).Count();
  const AggregateQuery aggregate_query2 =
      TestFirestore()->Collection("bar").Limit(20).Count();

  const AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query1, 1);

  EXPECT_EQ(snapshot.count(), 1);
  EXPECT_EQ(snapshot.query(), aggregate_query1);
  EXPECT_TRUE(snapshot.is_valid());

  AggregateQuerySnapshot snapshot_copy_dest =
      TestAggregateQuerySnapshot(aggregate_query2, 2);

  EXPECT_EQ(snapshot_copy_dest.count(), 2);
  EXPECT_EQ(snapshot_copy_dest.query(), aggregate_query2);
  EXPECT_TRUE(snapshot_copy_dest.is_valid());

  snapshot_copy_dest = snapshot;

  EXPECT_EQ(snapshot.count(), 1);
  EXPECT_EQ(snapshot.query(), aggregate_query1);
  EXPECT_TRUE(snapshot.is_valid());

  EXPECT_EQ(snapshot_copy_dest.count(), 1);
  EXPECT_EQ(snapshot_copy_dest.query(), aggregate_query1);
  EXPECT_TRUE(snapshot_copy_dest.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       CopyAssignmentAppliedSelfReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 7);

  EXPECT_EQ(snapshot.count(), 7);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  snapshot = snapshot;

  EXPECT_EQ(snapshot.count(), 7);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       MoveConstructorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 11);

  EXPECT_EQ(snapshot.count(), 11);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  AggregateQuerySnapshot moved_snapshot_dest(std::move(snapshot));

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(moved_snapshot_dest.count(), 11);
  EXPECT_EQ(moved_snapshot_dest.query(), aggregate_query);
  EXPECT_TRUE(moved_snapshot_dest.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       MoveConstructorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  AggregateQuerySnapshot moved_snapshot_dest(std::move(snapshot));

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    DefaultObjectMoveAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 3);

  EXPECT_EQ(snapshot.count(), 3);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  AggregateQuerySnapshot snapshot_move_dest = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot_move_dest.count(), 3);
  EXPECT_EQ(snapshot_move_dest.query(), aggregate_query);
  EXPECT_TRUE(snapshot_move_dest.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    DefaultObjectMoveAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  AggregateQuerySnapshot snapshot_move_dest = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot_move_dest.count(), 0);
  EXPECT_EQ(snapshot_move_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_move_dest.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    ValidObjectMoveAssignmentOperatorAppliedToValidObjectReturnsEqualObject) {
  const AggregateQuery aggregate_query1 =
      TestFirestore()->Collection("foo").Limit(10).Count();
  const AggregateQuery aggregate_query2 =
      TestFirestore()->Collection("bar").Limit(20).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query1, 3);

  EXPECT_EQ(snapshot.count(), 3);
  EXPECT_EQ(snapshot.query(), aggregate_query1);
  EXPECT_TRUE(snapshot.is_valid());

  AggregateQuerySnapshot snapshot_move_dest =
      TestAggregateQuerySnapshot(aggregate_query2, 6);

  EXPECT_EQ(snapshot_move_dest.count(), 6);
  EXPECT_EQ(snapshot_move_dest.query(), aggregate_query2);
  EXPECT_TRUE(snapshot_move_dest.is_valid());

  snapshot_move_dest = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot_move_dest.count(), 3);
  EXPECT_EQ(snapshot_move_dest.query(), aggregate_query1);
  EXPECT_TRUE(snapshot_move_dest.is_valid());
}

TEST_F(
    AggregateQuerySnapshotTest,
    ValidObjectMoveAssignmentOperatorAppliedToDefaultObjectReturnsEqualObject) {
  AggregateQuerySnapshot snapshot;

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot_move_dest =
      TestAggregateQuerySnapshot(aggregate_query, 99);

  EXPECT_EQ(snapshot_move_dest.count(), 99);
  EXPECT_EQ(snapshot_move_dest.query(), aggregate_query);
  EXPECT_TRUE(snapshot_move_dest.is_valid());

  snapshot_move_dest = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_FALSE(snapshot.is_valid());

  EXPECT_EQ(snapshot_move_dest.count(), 0);
  EXPECT_EQ(snapshot_move_dest.query(), AggregateQuery());
  EXPECT_FALSE(snapshot_move_dest.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       MoveAssignmentOperatorAppliedToSelfReturnsEqualObject) {
  const AggregateQuery aggregate_query =
      TestFirestore()->Collection("foo").Limit(10).Count();

  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, 99);

  EXPECT_EQ(snapshot.count(), 99);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());

  snapshot = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 99);
  EXPECT_EQ(snapshot.query(), aggregate_query);
  EXPECT_TRUE(snapshot.is_valid());
}

TEST_F(AggregateQuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesWithLimitShouldBeEqual) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
}

TEST_F(AggregateQuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesShouldBeEqual) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(collection.Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Count());

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
}

TEST_F(AggregateQuerySnapshotTest,
       IdenticalDefaultAggregateSnapshotShouldBeEqual) {
  AggregateQuerySnapshot snapshot1;
  AggregateQuerySnapshot snapshot2;

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
}

TEST_F(AggregateQuerySnapshotTest, NonEquality) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(
      collection.WhereEqualTo("k", FieldValue::String("d")).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot3 = ReadAggregate(collection.Limit(3).Count());
  AggregateQuerySnapshot snapshot4 = ReadAggregate(collection.Count());
  AggregateQuerySnapshot snapshot5;

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot2 == snapshot2);
  EXPECT_TRUE(snapshot3 == snapshot3);
  EXPECT_TRUE(snapshot4 == snapshot4);
  EXPECT_TRUE(snapshot5 == snapshot5);

  EXPECT_TRUE(snapshot1 != snapshot2);
  EXPECT_TRUE(snapshot1 != snapshot3);
  EXPECT_TRUE(snapshot1 != snapshot4);
  EXPECT_TRUE(snapshot1 != snapshot5);
  EXPECT_TRUE(snapshot2 != snapshot3);
  EXPECT_TRUE(snapshot2 != snapshot4);
  EXPECT_TRUE(snapshot2 != snapshot5);
  EXPECT_TRUE(snapshot3 != snapshot4);
  EXPECT_TRUE(snapshot3 != snapshot5);
  EXPECT_TRUE(snapshot4 != snapshot5);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot2 != snapshot2);
  EXPECT_FALSE(snapshot3 != snapshot3);
  EXPECT_FALSE(snapshot4 != snapshot4);
  EXPECT_FALSE(snapshot5 != snapshot5);

  EXPECT_FALSE(snapshot1 == snapshot2);
  EXPECT_FALSE(snapshot1 == snapshot3);
  EXPECT_FALSE(snapshot1 == snapshot4);
  EXPECT_FALSE(snapshot1 == snapshot5);
  EXPECT_FALSE(snapshot2 == snapshot3);
  EXPECT_FALSE(snapshot2 == snapshot4);
  EXPECT_FALSE(snapshot2 == snapshot5);
  EXPECT_FALSE(snapshot3 == snapshot4);
  EXPECT_FALSE(snapshot3 == snapshot5);
  EXPECT_FALSE(snapshot4 == snapshot5);
}

TEST_F(AggregateQuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesWithLimitShouldHaveSameHash) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());

  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot1));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot2));
}

TEST_F(AggregateQuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesShouldHaveSameHash) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(collection.Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Count());

  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot1));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot2));
}

TEST_F(AggregateQuerySnapshotTest, TestHashCode) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(
      collection.WhereEqualTo("k", FieldValue::String("d")).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot3 = ReadAggregate(collection.Limit(3).Count());
  AggregateQuerySnapshot snapshot4 = ReadAggregate(collection.Count());

  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot1));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot2));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot3));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot2));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot3));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot3),
            AggregateQuerySnapshotHash(snapshot3));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot3),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot4),
            AggregateQuerySnapshotHash(snapshot4));
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
