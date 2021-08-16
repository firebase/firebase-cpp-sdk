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

#if defined(__ANDROID__)

using DocumentChangeTest = FirestoreIntegrationTest;

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
  EXPECT_EQ(changes.size(), 1);

  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kAdded);
  EXPECT_EQ(changes[0].document().id(), doc1.id());
  EXPECT_EQ(changes[0].old_index(), DocumentChange::npos);
  EXPECT_EQ(changes[0].new_index(), 0);

  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(2)}});
  Await(listener, 2);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  EXPECT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kAdded);
  EXPECT_EQ(changes[0].document().id(), doc2.id());
  EXPECT_EQ(changes[0].old_index(), DocumentChange::npos);
  EXPECT_EQ(changes[0].new_index(), 1);

  // Make doc2 ordered before doc1.
  WriteDocument(doc2, MapFieldValue{{"a", FieldValue::Integer(0)}});
  Await(listener, 3);
  snapshot = listener.last_result();

  changes = snapshot.DocumentChanges();
  EXPECT_EQ(changes.size(), 1);
  EXPECT_EQ(changes[0].type(), DocumentChange::Type::kModified);
  EXPECT_EQ(changes[0].document().id(), doc2.id());
  EXPECT_EQ(changes[0].old_index(), 1);
  EXPECT_EQ(changes[0].new_index(), 0);
}

#endif  // defined(__ANDROID__)

}  // namespace firestore
}  // namespace firebase
