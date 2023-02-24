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
#include "firestore/src/main/aggregate_query_snapshot_main.h"
#include "firestore/src/main/converter_main.h"
#endif  // defined(__ANDROID__)

namespace firebase {
namespace firestore {

class AggregateQuerySnapshotTest : public FirestoreIntegrationTest {
 protected:
  static AggregateQuerySnapshot TestAggregateQuerySnapshot(
      AggregateQuery aggregate_query, const int count) {
    api::AggregateQuery aggregateQuery =
        GetInternal(&aggregate_query)->aggregate_query_;
    return MakePublic(
        AggregateQuerySnapshotInternal(std::move(aggregateQuery), count));
  }
};

std::size_t AggregateQuerySnapshotHash(const AggregateQuerySnapshot& snapshot) {
  return snapshot.Hash();
}

namespace {

TEST_F(AggregateQuerySnapshotTest, DefaultConstructor) {
  AggregateQuerySnapshot snapshot;
  EXPECT_EQ(snapshot.query(), AggregateQuery());
  EXPECT_EQ(snapshot.count(), 0);
}

TEST_F(AggregateQuerySnapshotTest, CopyConstructor) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  static const int COUNT = 5;
  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, COUNT);

  AggregateQuerySnapshot copied_snapshot(snapshot);

  EXPECT_EQ(snapshot.count(), COUNT);
  EXPECT_EQ(snapshot.query(), aggregate_query);

  EXPECT_EQ(copied_snapshot.count(), COUNT);
  EXPECT_EQ(copied_snapshot.query(), aggregate_query);
}

TEST_F(AggregateQuerySnapshotTest, CopyAssignmentOperator) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  static const int COUNT = 7;
  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, COUNT);

  AggregateQuerySnapshot snapshot_copy_dest = snapshot;

  EXPECT_EQ(snapshot.count(), COUNT);
  EXPECT_EQ(snapshot.query(), aggregate_query);

  EXPECT_EQ(snapshot_copy_dest.count(), COUNT);
  EXPECT_EQ(snapshot_copy_dest.query(), aggregate_query);
}

TEST_F(AggregateQuerySnapshotTest, MoveConstructor) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  static const int COUNT = 8;
  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, COUNT);

  AggregateQuerySnapshot moved_snapshot_dest(std::move(snapshot));

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());

  EXPECT_EQ(moved_snapshot_dest.count(), COUNT);
  EXPECT_EQ(moved_snapshot_dest.query(), aggregate_query);
}

TEST_F(AggregateQuerySnapshotTest, MoveAssignmentOperator) {
  const Query& query = TestFirestore()->Collection("foo").Limit(10);
  const AggregateQuery& aggregate_query = query.Count();

  static const int COUNT = 3;
  AggregateQuerySnapshot snapshot =
      TestAggregateQuerySnapshot(aggregate_query, COUNT);

  AggregateQuerySnapshot snapshot_move_dest = std::move(snapshot);

  EXPECT_EQ(snapshot.count(), 0);
  EXPECT_EQ(snapshot.query(), AggregateQuery());

  EXPECT_EQ(snapshot_move_dest.count(), COUNT);
  EXPECT_EQ(snapshot_move_dest.query(), aggregate_query);
}

TEST_F(AggregateQuerySnapshotTest, Equality1) {
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

TEST_F(AggregateQuerySnapshotTest, Equality2) {
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

TEST_F(AggregateQuerySnapshotTest, Equality3) {
  AggregateQuerySnapshot snapshot1 = AggregateQuerySnapshot();
  AggregateQuerySnapshot snapshot2 = AggregateQuerySnapshot();

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
  AggregateQuerySnapshot snapshot5 = AggregateQuerySnapshot();

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

TEST_F(AggregateQuerySnapshotTest, TestHashCodeEquals1) {
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

TEST_F(AggregateQuerySnapshotTest, TestHashCodeEquals2) {
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

}
}  // namespace firestore
}  // namespace firebase
