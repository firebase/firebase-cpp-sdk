/*
 * Copyright 2021 Google LLC
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

#include <utility>

#include "firebase/firestore.h"
#include "firestore/src/common/wrapper_assertions.h"
#include "firestore_integration_test.h"
#if defined(__ANDROID__)
#include "firestore/src/android/query_snapshot_android.h"
#endif  // defined(__ANDROID__)

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using QuerySnapshotTest = FirestoreIntegrationTest;

std::size_t QuerySnapshotHash(const QuerySnapshot& snapshot) {
  return snapshot.Hash();
}

#if defined(__ANDROID__)

TEST_F(QuerySnapshotTest, Construction) {
  testutil::AssertWrapperConstructionContract<QuerySnapshot,
                                              QuerySnapshotInternal>();
}

TEST_F(QuerySnapshotTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<QuerySnapshot,
                                            QuerySnapshotInternal>();
}

#endif  // defined(__ANDROID__)

TEST_F(QuerySnapshotTest, Equality) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot3 = ReadDocuments(collection.Limit(1));
  QuerySnapshot snapshot4 = ReadDocuments(collection);
  QuerySnapshot snapshot5 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kAscending));
  QuerySnapshot snapshot6 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kDescending));

  QuerySnapshot snapshot7 = QuerySnapshot();
  QuerySnapshot snapshot8 = QuerySnapshot();

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);
  EXPECT_TRUE(snapshot1 != snapshot3);
  EXPECT_TRUE(snapshot1 != snapshot4);
  EXPECT_TRUE(snapshot1 != snapshot5);
  EXPECT_TRUE(snapshot1 != snapshot6);
  EXPECT_TRUE(snapshot3 != snapshot4);
  EXPECT_TRUE(snapshot3 != snapshot5);
  EXPECT_TRUE(snapshot3 != snapshot6);
  EXPECT_TRUE(snapshot5 != snapshot6);
  EXPECT_TRUE(snapshot1 != snapshot7);
  EXPECT_TRUE(snapshot2 != snapshot7);
  EXPECT_TRUE(snapshot3 != snapshot7);
  EXPECT_TRUE(snapshot4 != snapshot7);
  EXPECT_TRUE(snapshot5 != snapshot7);
  EXPECT_TRUE(snapshot6 != snapshot7);
  EXPECT_TRUE(snapshot7 == snapshot8);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
  EXPECT_FALSE(snapshot1 == snapshot3);
  EXPECT_FALSE(snapshot1 == snapshot4);
  EXPECT_FALSE(snapshot1 == snapshot5);
  EXPECT_FALSE(snapshot1 == snapshot6);
  EXPECT_FALSE(snapshot3 == snapshot4);
  EXPECT_FALSE(snapshot3 == snapshot5);
  EXPECT_FALSE(snapshot3 == snapshot6);
  EXPECT_FALSE(snapshot5 == snapshot6);
  EXPECT_FALSE(snapshot1 == snapshot7);
  EXPECT_FALSE(snapshot2 == snapshot7);
  EXPECT_FALSE(snapshot3 == snapshot7);
  EXPECT_FALSE(snapshot4 == snapshot7);
  EXPECT_FALSE(snapshot5 == snapshot7);
  EXPECT_FALSE(snapshot6 == snapshot7);
  EXPECT_FALSE(snapshot7 != snapshot8);
}

TEST_F(QuerySnapshotTest, TestHashCode) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot3 = ReadDocuments(collection.Limit(1));
  QuerySnapshot snapshot4 = ReadDocuments(collection);
  QuerySnapshot snapshot5 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kAscending));
  QuerySnapshot snapshot6 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kDescending));

  QuerySnapshot snapshot7 = QuerySnapshot();
  QuerySnapshot snapshot8 = QuerySnapshot();

  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot1));
  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot2));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot3));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot4));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot4));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot5), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot7));
  EXPECT_NE(QuerySnapshotHash(snapshot2), QuerySnapshotHash(snapshot7));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot7));
  EXPECT_NE(QuerySnapshotHash(snapshot4), QuerySnapshotHash(snapshot7));
  EXPECT_NE(QuerySnapshotHash(snapshot5), QuerySnapshotHash(snapshot7));
  EXPECT_NE(QuerySnapshotHash(snapshot6), QuerySnapshotHash(snapshot7));
  EXPECT_EQ(QuerySnapshotHash(snapshot7), QuerySnapshotHash(snapshot8));
}

}  // namespace firestore
}  // namespace firebase
