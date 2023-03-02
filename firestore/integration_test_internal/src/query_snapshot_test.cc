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

TEST_F(QuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesWithLimitShouldBeEqual) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(2));

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);
  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
}

TEST_F(QuerySnapshotTest, IdenticalDefaultSnapshotShouldBeEqual) {
  QuerySnapshot snapshot1 = QuerySnapshot();
  QuerySnapshot snapshot2 = QuerySnapshot();

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot1 == snapshot2);
  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot1 != snapshot2);
}

TEST_F(QuerySnapshotTest, NonEquality) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(1));
  QuerySnapshot snapshot3 = ReadDocuments(collection);
  QuerySnapshot snapshot4 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kAscending));
  QuerySnapshot snapshot5 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kDescending));

  QuerySnapshot snapshot6 = QuerySnapshot();

  EXPECT_TRUE(snapshot1 == snapshot1);
  EXPECT_TRUE(snapshot2 == snapshot2);
  EXPECT_TRUE(snapshot3 == snapshot3);
  EXPECT_TRUE(snapshot4 == snapshot4);
  EXPECT_TRUE(snapshot5 == snapshot5);

  EXPECT_TRUE(snapshot1 != snapshot2);
  EXPECT_TRUE(snapshot1 != snapshot3);
  EXPECT_TRUE(snapshot1 != snapshot4);
  EXPECT_TRUE(snapshot1 != snapshot5);
  EXPECT_TRUE(snapshot1 != snapshot6);
  EXPECT_TRUE(snapshot2 != snapshot3);
  EXPECT_TRUE(snapshot2 != snapshot4);
  EXPECT_TRUE(snapshot2 != snapshot5);
  EXPECT_TRUE(snapshot2 != snapshot6);
  EXPECT_TRUE(snapshot3 != snapshot4);
  EXPECT_TRUE(snapshot3 != snapshot5);
  EXPECT_TRUE(snapshot3 != snapshot6);
  EXPECT_TRUE(snapshot4 != snapshot5);
  EXPECT_TRUE(snapshot4 != snapshot6);
  EXPECT_TRUE(snapshot5 != snapshot6);

  EXPECT_FALSE(snapshot1 != snapshot1);
  EXPECT_FALSE(snapshot2 != snapshot2);
  EXPECT_FALSE(snapshot3 != snapshot3);
  EXPECT_FALSE(snapshot4 != snapshot4);
  EXPECT_FALSE(snapshot5 != snapshot5);

  EXPECT_FALSE(snapshot1 == snapshot2);
  EXPECT_FALSE(snapshot1 == snapshot3);
  EXPECT_FALSE(snapshot1 == snapshot4);
  EXPECT_FALSE(snapshot1 == snapshot5);
  EXPECT_FALSE(snapshot1 == snapshot6);
  EXPECT_FALSE(snapshot2 == snapshot3);
  EXPECT_FALSE(snapshot2 == snapshot4);
  EXPECT_FALSE(snapshot2 == snapshot5);
  EXPECT_FALSE(snapshot2 == snapshot6);
  EXPECT_FALSE(snapshot3 == snapshot4);
  EXPECT_FALSE(snapshot3 == snapshot5);
  EXPECT_FALSE(snapshot3 == snapshot6);
  EXPECT_FALSE(snapshot4 == snapshot5);
  EXPECT_FALSE(snapshot4 == snapshot6);
  EXPECT_FALSE(snapshot5 == snapshot6);
}

TEST_F(QuerySnapshotTest,
       IdenticalSnapshotFromCollectionQueriesWithLimitShouldHaveSameHash) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(2));

  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot1));
  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot2));
}

TEST_F(QuerySnapshotTest, IdenticalDefaultSnapshotShouldHaveSameHash) {
  QuerySnapshot snapshot1 = QuerySnapshot();
  QuerySnapshot snapshot2 = QuerySnapshot();

  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot1));
  EXPECT_EQ(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot2));
}

TEST_F(QuerySnapshotTest, TestHashCodeNonEquality) {
  CollectionReference collection =
      Collection({{"a", {{"k", FieldValue::String("a")}}},
                  {"b", {{"k", FieldValue::String("b")}}},
                  {"c", {{"k", FieldValue::String("c")}}}});
  QuerySnapshot snapshot1 = ReadDocuments(collection.Limit(2));
  QuerySnapshot snapshot2 = ReadDocuments(collection.Limit(1));
  QuerySnapshot snapshot3 = ReadDocuments(collection);
  QuerySnapshot snapshot4 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kAscending));
  QuerySnapshot snapshot5 =
      ReadDocuments(collection.OrderBy("k", Query::Direction::kDescending));
  QuerySnapshot snapshot6 = QuerySnapshot();

  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot2));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot3));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot4));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot1), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot2), QuerySnapshotHash(snapshot3));
  EXPECT_NE(QuerySnapshotHash(snapshot2), QuerySnapshotHash(snapshot4));
  EXPECT_NE(QuerySnapshotHash(snapshot2), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot2), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot4));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot3), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot4), QuerySnapshotHash(snapshot5));
  EXPECT_NE(QuerySnapshotHash(snapshot4), QuerySnapshotHash(snapshot6));
  EXPECT_NE(QuerySnapshotHash(snapshot5), QuerySnapshotHash(snapshot6));
}

}  // namespace firestore
}  // namespace firebase
