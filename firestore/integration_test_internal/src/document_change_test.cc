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

#if defined(__ANDROID__)
#include "firestore/src/android/document_change_android.h"
#endif  // defined(__ANDROID__)

#include "firebase/firestore.h"
#include "firestore/src/common/wrapper_assertions.h"
#include "firestore_integration_test.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {

using DocumentChangeTest = FirestoreIntegrationTest;

std::size_t DocumentChangeHash(const DocumentChange& change) {
  return change.Hash();
}

#if defined(__ANDROID__)

TEST_F(DocumentChangeTest, Construction) {
  testutil::AssertWrapperConstructionContract<DocumentChange,
                                              DocumentChangeInternal>();
}

TEST_F(DocumentChangeTest, Assignment) {
  testutil::AssertWrapperAssignmentContract<DocumentChange,
                                            DocumentChangeInternal>();
}

TEST_F(DocumentChangeTest, TestDocumentChanges) {
  CollectionReference collection = Collection();
  Query query = collection.OrderBy("a");

  DocumentReference doc1 = collection.Document("1");
  DocumentReference doc2 = collection.Document("2");

  TestEventListener<QuerySnapshot> listener("TestDocumentChanges");
  ListenerRegistration registration = listener.AttachTo(&query);
  Await(listener);
  QuerySnapshot snapshot = listener.last_result();
  EXPECT_EQ(snapshot.size(), 0);

  WriteDocument(doc1, MapFieldValue{{"a", FieldValue::Integer(1)}});
  Await(listener);
  snapshot = listener.last_result();

  std::vector<DocumentChange> changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);

  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kAdded);
  EXPECT_EQ(changes[0].document().id(), doc1.id());
  EXPECT_EQ(changes[0].old_index(), DocumentChange::npos);
  EXPECT_EQ(changes[0].new_index(), 0);

  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(2)}});
  Await(listener, 2);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kAdded);
  EXPECT_EQ(changes[0].document().id(), doc2.id());
  EXPECT_EQ(changes[0].old_index(), DocumentChange::npos);
  EXPECT_EQ(changes[0].new_index(), 1);

  // Make doc2 ordered before doc1.
  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(0)}});
  Await(listener, 3);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kModified);
  EXPECT_EQ(changes[0].document().id(), doc2.id());
  EXPECT_EQ(changes[0].old_index(), 1);
  EXPECT_EQ(changes[0].new_index(), 0);
}

#endif  // defined(__ANDROID__)

TEST_F(DocumentChangeTest, EqualityAndHashCode) {
  DocumentChange invalid_change_1 = DocumentChange();
  DocumentChange invalid_change_2 = DocumentChange();

  EXPECT_TRUE(invalid_change_1 == invalid_change_2);
  EXPECT_FALSE(invalid_change_1 != invalid_change_2);
  EXPECT_EQ(DocumentChangeHash(invalid_change_1),
            DocumentChangeHash(invalid_change_2));

  CollectionReference collection = Collection();
  Query query = collection.OrderBy("a");

  DocumentReference doc1 = collection.Document("1");
  DocumentReference doc2 = collection.Document("2");

  TestEventListener<QuerySnapshot> listener("TestDocumentChanges");
  ListenerRegistration registration = listener.AttachTo(&query);
  Await(listener);
  QuerySnapshot snapshot = listener.last_result();
  EXPECT_EQ(snapshot.size(), 0);

  WriteDocument(doc1, MapFieldValue{{"a", FieldValue::Integer(1)}});
  Await(listener);
  snapshot = listener.last_result();

  std::vector<DocumentChange> changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  // First change: Added doc1.
  auto change1 = changes[0];
  EXPECT_TRUE(change1 == change1);
  EXPECT_TRUE(change1 != invalid_change_1);
  EXPECT_FALSE(change1 != change1);
  EXPECT_FALSE(change1 == invalid_change_1);
  EXPECT_EQ(DocumentChangeHash(change1), DocumentChangeHash(change1));
  EXPECT_NE(DocumentChangeHash(change1), DocumentChangeHash(invalid_change_1));

  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(2)}});
  Await(listener, 2);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  // Second change: Added doc2.
  auto change2 = changes[0];
  EXPECT_TRUE(change2 != change1);
  EXPECT_TRUE(change2 != invalid_change_1);
  EXPECT_FALSE(change2 == change1);
  EXPECT_FALSE(change2 == invalid_change_1);
  EXPECT_NE(DocumentChangeHash(change2), DocumentChangeHash(change1));
  EXPECT_NE(DocumentChangeHash(change2), DocumentChangeHash(invalid_change_1));

  // Make doc2 ordered before doc1.
  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(0)}});
  Await(listener, 3);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  // Third change: Modified doc2.
  auto change3 = changes[0];
  EXPECT_TRUE(change3 != change1);
  EXPECT_TRUE(change3 != change2);
  EXPECT_TRUE(change3 != invalid_change_1);
  EXPECT_FALSE(change3 == change1);
  EXPECT_FALSE(change3 == change2);
  EXPECT_FALSE(change3 == invalid_change_1);
  EXPECT_NE(DocumentChangeHash(change3), DocumentChangeHash(change1));
  EXPECT_NE(DocumentChangeHash(change3), DocumentChangeHash(change2));
  EXPECT_NE(DocumentChangeHash(change3), DocumentChangeHash(invalid_change_1));
}

}  // namespace firestore
}  // namespace firebase
