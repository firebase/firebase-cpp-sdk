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

using AggregateQuerySnapshotTest = FirestoreIntegrationTest;

std::size_t AggregateQuerySnapshotHash(const AggregateQuerySnapshot& snapshot) {
  return snapshot.Hash();
}

TEST_F(AggregateQuerySnapshotTest, Equality) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(
      collection.WhereEqualTo("k", FieldValue::String("d")).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot3 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot4 = ReadAggregate(collection.Limit(3).Count());
  AggregateQuerySnapshot snapshot5 = ReadAggregate(collection.Count());
  AggregateQuerySnapshot snapshot6 = ReadAggregate(collection.Count());

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 != snapshot2);
  EXPECT_TRUE(snapshot1 != snapshot3);
  EXPECT_TRUE(snapshot1 != snapshot4);
  EXPECT_TRUE(snapshot1 != snapshot5);
  EXPECT_TRUE(snapshot1 != snapshot6);
  EXPECT_TRUE(snapshot2 == snapshot2);
  EXPECT_TRUE(snapshot2 == snapshot3);
  EXPECT_TRUE(snapshot2 != snapshot4);
  EXPECT_TRUE(snapshot2 != snapshot5);
  EXPECT_TRUE(snapshot2 != snapshot6);
  EXPECT_TRUE(snapshot4 == snapshot4);
  EXPECT_TRUE(snapshot4 != snapshot5);
  EXPECT_TRUE(snapshot4 != snapshot6);
  EXPECT_TRUE(snapshot5 == snapshot5);
  EXPECT_TRUE(snapshot5 == snapshot6);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 == snapshot2);
  EXPECT_FALSE(snapshot1 == snapshot3);
  EXPECT_FALSE(snapshot1 == snapshot4);
  EXPECT_FALSE(snapshot1 == snapshot5);
  EXPECT_FALSE(snapshot1 == snapshot6);
  EXPECT_FALSE(snapshot2 != snapshot2);
  EXPECT_FALSE(snapshot2 != snapshot3);
  EXPECT_FALSE(snapshot2 == snapshot4);
  EXPECT_FALSE(snapshot2 == snapshot5);
  EXPECT_FALSE(snapshot2 == snapshot6);
  EXPECT_FALSE(snapshot4 != snapshot4);
  EXPECT_FALSE(snapshot4 == snapshot5);
  EXPECT_FALSE(snapshot4 == snapshot6);
  EXPECT_FALSE(snapshot5 != snapshot5);
  EXPECT_FALSE(snapshot5 != snapshot6);
}

TEST_F(AggregateQuerySnapshotTest, TestHashCode) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  AggregateQuerySnapshot snapshot1 = ReadAggregate(
      collection.WhereEqualTo("k", FieldValue::String("d")).Count());
  AggregateQuerySnapshot snapshot2 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot3 = ReadAggregate(collection.Limit(1).Count());
  AggregateQuerySnapshot snapshot4 = ReadAggregate(collection.Limit(3).Count());
  AggregateQuerySnapshot snapshot5 = ReadAggregate(collection.Count());
  AggregateQuerySnapshot snapshot6 = ReadAggregate(collection.Count());

  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot1));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot2));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot3));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot5));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot1),
            AggregateQuerySnapshotHash(snapshot6));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot2));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot3));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot5));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot2),
            AggregateQuerySnapshotHash(snapshot6));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot4),
            AggregateQuerySnapshotHash(snapshot4));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot4),
            AggregateQuerySnapshotHash(snapshot5));
  EXPECT_NE(AggregateQuerySnapshotHash(snapshot4),
            AggregateQuerySnapshotHash(snapshot6));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot5),
            AggregateQuerySnapshotHash(snapshot5));
  EXPECT_EQ(AggregateQuerySnapshotHash(snapshot5),
            AggregateQuerySnapshotHash(snapshot6));
}

}  // namespace firestore
}  // namespace firebase
