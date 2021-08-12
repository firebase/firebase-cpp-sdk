// Copyright 2021 Google LLC

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

TEST_F(QuerySnapshotTest, TestCanQueryWithAndWithoutDocumentKey) {
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

}  // namespace firestore
}  // namespace firebase
