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

#include <future>
#include <map>
#include <string>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRFirestoreSourceTests.mm
// and native Android client SDK test
//   firebase-firestore/src/androidTest/java/com/google/firebase/firestore/SourceTest.java

namespace firebase {
namespace firestore {

using SourceTest = FirestoreIntegrationTest;

TEST_F(SourceTest, GetDocumentWhileOnlineWithDefaultGetOptions) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Future<DocumentSnapshot> future = doc_ref.Get();
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());
}

TEST_F(SourceTest, GetCollectionWhileOnlineWithDefaultGetOptions) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Future<QuerySnapshot> future = col_ref.Get();
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 3);
  EXPECT_EQ(initial_docs, QuerySnapshotToMap(snapshot));
}

TEST_F(SourceTest, GetDocumentWhileOfflineWithDefaultGetOptions) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Await(doc_ref.Get());
  DisableNetwork();

  Future<DocumentSnapshot> future = doc_ref.Get();
  Await(future);
  DocumentSnapshot snapshot = *future.result();

  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());
}

TEST_F(SourceTest, GetCollectionWhileOfflineWithDefaultGetOptions) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Await(col_ref.Get());
  DisableNetwork();

  // Since we're offline, the returned futures won't complete.
  col_ref.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                               SetOptions().Merge());
  col_ref.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  col_ref.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  Future<QuerySnapshot> future = col_ref.Get();
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 4);
  std::map<std::string, MapFieldValue> new_data{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  EXPECT_EQ(new_data, QuerySnapshotToMap(snapshot));
}

TEST_F(SourceTest, GetDocumentWhileOnlineWithSourceEqualToCache) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Await(doc_ref.Get());
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);
  DocumentSnapshot snapshot = *future.result();

  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());
}

TEST_F(SourceTest, GetCollectionWhileOnlineWithSourceEqualToCache) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Await(col_ref.Get());
  Future<QuerySnapshot> future = col_ref.Get(Source::kCache);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 3);
  EXPECT_EQ(initial_docs, QuerySnapshotToMap(snapshot));
}

TEST_F(SourceTest, GetDocumentWhileOfflineWithSourceEqualToCache) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Await(doc_ref.Get());
  DisableNetwork();
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);
  DocumentSnapshot snapshot = *future.result();

  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());
}

TEST_F(SourceTest, GetCollectionWhileOfflineWithSourceEqualToCache) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Await(col_ref.Get());
  DisableNetwork();

  // Since we're offline, the returned futures won't complete.
  col_ref.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                               SetOptions().Merge());
  col_ref.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  col_ref.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  Future<QuerySnapshot> future = col_ref.Get(Source::kCache);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 4);
  std::map<std::string, MapFieldValue> new_data{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  EXPECT_EQ(new_data, QuerySnapshotToMap(snapshot));
}

TEST_F(SourceTest, GetDocumentWhileOnlineWithSourceEqualToServer) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Future<DocumentSnapshot> future = doc_ref.Get(Source::kServer);
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());
}

TEST_F(SourceTest, GetCollectionWhileOnlineWithSourceEqualToServer) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Future<QuerySnapshot> future = col_ref.Get(Source::kServer);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 3);
  EXPECT_EQ(initial_docs, QuerySnapshotToMap(snapshot));
}

TEST_F(SourceTest, GetDocumentWhileOfflineWithSourceEqualToServer) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Await(doc_ref.Get());
  DisableNetwork();

  Future<DocumentSnapshot> future = doc_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetCollectionWhileOfflineWithSourceEqualToServer) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Await(col_ref.Get());
  DisableNetwork();

  Future<QuerySnapshot> future = col_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetDocumentWhileOfflineWithDifferentGetOptions) {
  MapFieldValue initial_data = {{"key", FieldValue::String("value")}};
  DocumentReference doc_ref = DocumentWithData(initial_data);

  Await(doc_ref.Get());
  DisableNetwork();

  // Create an initial listener for this query (to attempt to disrupt the gets
  // below) and wait for the listener to deliver its initial snapshot before
  // continuing.
  std::promise<Error> error_promise;
  std::future<Error> error_future = error_promise.get_future();

  doc_ref.AddSnapshotListener(
      [&error_promise](const DocumentSnapshot& snapshot, Error error_code,
                       const std::string& error_message) {
        error_promise.set_value(error_code);
      });
  // Note that future::get() will wait until the future has a valid result and
  // retrieve it. Calling wait() before get() is not needed.
  Error error_code = error_future.get();
  EXPECT_EQ(error_code, kErrorNone);

  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);
  DocumentSnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());

  future = doc_ref.Get();
  Await(future);
  snapshot = *future.result();
  EXPECT_TRUE(snapshot.exists());
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(initial_data, snapshot.GetData());

  future = doc_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetCollectionWhileOfflineWithDifferentGetOptions) {
  std::map<std::string, MapFieldValue> initial_docs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference col_ref = Collection(initial_docs);

  Await(col_ref.Get());
  DisableNetwork();

  // Since we're offline, the returned futures won't complete.
  col_ref.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                               SetOptions().Merge());
  col_ref.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  col_ref.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  // Create an initial listener for this query (to attempt to disrupt the gets
  // below) and wait for the listener to deliver its initial snapshot before
  // continuing.
  std::promise<Error> error_promise;
  std::future<Error> error_future = error_promise.get_future();

  col_ref.AddSnapshotListener(
      [&error_promise](const QuerySnapshot& snapshot, Error error_code,
                       const std::string& error_message) {
        error_promise.set_value(error_code);
      });
  // Note that future::get() will wait until the future has a valid result and
  // retrieve it. Calling wait() before get() is not needed.
  Error error_code = error_future.get();
  EXPECT_EQ(error_code, kErrorNone);

  Future<QuerySnapshot> future = col_ref.Get(Source::kCache);
  Await(future);
  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 4);
  std::map<std::string, MapFieldValue> new_data{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  EXPECT_EQ(new_data, QuerySnapshotToMap(snapshot));

  future = col_ref.Get();
  Await(future);
  snapshot = *future.result();
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_TRUE(snapshot.metadata().has_pending_writes());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 4);
  EXPECT_EQ(new_data, QuerySnapshotToMap(snapshot));

  future = col_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetNonExistingDocWhileOnlineWithDefaultGetOptions) {
  DocumentReference doc_ref = Document();

  Future<DocumentSnapshot> future = doc_ref.Get();
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.exists());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingCollectionWhileOnlineWithDefaultGetOptions) {
  CollectionReference col_ref = Collection();

  Future<QuerySnapshot> future = col_ref.Get();
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.empty());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 0);
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingDocWhileOfflineWithDefaultGetOptions) {
  DocumentReference doc_ref = Document();

  DisableNetwork();
  Future<DocumentSnapshot> future = doc_ref.Get();
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetDeletedDocWhileOfflineWithDefaultGetOptions) {
  GTEST_SKIP() << "b/112267729: We should raise a fromCache=true event with "
               << "a nonexistent snapshot, but because the default source goes "
               << "through a normal listener, we do not.";
  DocumentReference doc_ref = Document();
  Await(doc_ref.Delete());

  DisableNetwork();
  Future<DocumentSnapshot> future = doc_ref.Get();
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.exists());
  EXPECT_EQ(snapshot.GetData().size(), 0);
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingCollectionWhileOfflineWithDefaultGetOptions) {
  CollectionReference col_ref = Collection();

  DisableNetwork();
  Future<QuerySnapshot> future = col_ref.Get();
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.empty());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 0);
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingDocWhileOnlineWithSourceEqualToCache) {
  DocumentReference doc_ref = Document();

  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetNonExistingCollectionWhileOnlineWithSourceEqualToCache) {
  CollectionReference col_ref = Collection();

  Future<QuerySnapshot> future = col_ref.Get(Source::kCache);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.empty());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 0);
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingDocWhileOfflineWithSourceEqualToCache) {
  DocumentReference doc_ref = Document();

  DisableNetwork();
  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, GetDeletedDocWhileOfflineWithSourceEqualToCache) {
  DocumentReference doc_ref = Document();
  Await(doc_ref.Delete());

  DisableNetwork();
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kCache);
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.exists());
  EXPECT_EQ(snapshot.GetData().size(), 0);
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingCollectionWhileOfflineWithSourceEqualToCache) {
  CollectionReference col_ref = Collection();

  DisableNetwork();
  Future<QuerySnapshot> future = col_ref.Get(Source::kCache);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.empty());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 0);
  EXPECT_TRUE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingDocWhileOnlineWithSourceEqualToServer) {
  DocumentReference doc_ref = Document();

  Future<DocumentSnapshot> future = doc_ref.Get(Source::kServer);
  Await(future);

  DocumentSnapshot snapshot = *future.result();
  EXPECT_FALSE(snapshot.exists());
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingCollectionWhileOnlineWithSourceEqualToServer) {
  CollectionReference col_ref = Collection();

  Future<QuerySnapshot> future = col_ref.Get(Source::kServer);
  Await(future);

  QuerySnapshot snapshot = *future.result();
  EXPECT_TRUE(snapshot.empty());
  EXPECT_EQ(snapshot.DocumentChanges().size(), 0);
  EXPECT_FALSE(snapshot.metadata().is_from_cache());
  EXPECT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, GetNonExistingDocWhileOfflineWithSourceEqualToServer) {
  DocumentReference doc_ref = Document();

  DisableNetwork();
  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = doc_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest,
       GetNonExistingCollectionWhileOfflineWithSourceEqualToServer) {
  CollectionReference col_ref = Collection();

  DisableNetwork();
  Future<QuerySnapshot> future = col_ref.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

}  // namespace firestore
}  // namespace firebase
