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
#include "firestore_integration_test.h"

#if defined(__ANDROID__)
#include "firestore/src/android/document_snapshot_android.h"
#include "firestore/src/common/wrapper_assertions.h"
#endif  // defined(__ANDROID__)

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using DocumentSnapshotTest = FirestoreIntegrationTest;

std::size_t DocumentSnapshotHash(const DocumentSnapshot& snapshot) {
  return snapshot.Hash();
}

#if defined(__ANDROID__)

TEST_F(DocumentSnapshotTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentSnapshot,
                                              DocumentSnapshotInternal>();
}

TEST_F(DocumentSnapshotTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentSnapshot,
                                            DocumentSnapshotInternal>();
}

#endif

TEST_F(DocumentSnapshotTest, Equality) {
  DocumentReference doc1 = Collection("col1").Document();
  DocumentReference doc2 = Collection("col1").Document();
  DocumentSnapshot doc1_snap1 = ReadDocument(doc1);
  DocumentSnapshot doc1_snap2 = ReadDocument(doc1);
  DocumentSnapshot doc2_snap1 = ReadDocument(doc2);
  DocumentSnapshot snap3 = DocumentSnapshot();
  DocumentSnapshot snap4 = DocumentSnapshot();

  EXPECT_TRUE(doc1_snap1 == doc1_snap1);
  EXPECT_TRUE(doc1_snap1 == doc1_snap2);
  EXPECT_TRUE(doc1_snap1 != doc2_snap1);
  EXPECT_TRUE(doc1_snap1 != snap3);
  EXPECT_TRUE(snap3 == snap4);

  EXPECT_FALSE(doc1_snap1 != doc1_snap1);
  EXPECT_FALSE(doc1_snap1 != doc1_snap2);
  EXPECT_FALSE(doc1_snap1 == doc2_snap1);
  EXPECT_FALSE(doc1_snap1 == snap3);
  EXPECT_FALSE(snap3 != snap4);
}

TEST_F(DocumentSnapshotTest, TestHashCode) {
  DocumentReference doc1 = Collection("col1").Document();
  DocumentReference doc2 = Collection("col1").Document();
  DocumentSnapshot doc1_snap1 = ReadDocument(doc1);
  DocumentSnapshot doc1_snap2 = ReadDocument(doc1);
  DocumentSnapshot doc2_snap1 = ReadDocument(doc2);
  DocumentSnapshot snap3 = DocumentSnapshot();
  DocumentSnapshot snap4 = DocumentSnapshot();

  EXPECT_EQ(DocumentSnapshotHash(doc1_snap1), DocumentSnapshotHash(doc1_snap1));
  EXPECT_EQ(DocumentSnapshotHash(doc1_snap1), DocumentSnapshotHash(doc1_snap2));
  EXPECT_NE(DocumentSnapshotHash(doc1_snap1), DocumentSnapshotHash(doc2_snap1));
  EXPECT_NE(DocumentSnapshotHash(doc1_snap1), DocumentSnapshotHash(snap3));
  EXPECT_EQ(DocumentSnapshotHash(snap3), DocumentSnapshotHash(snap4));
}

}  // namespace firestore
}  // namespace firebase
