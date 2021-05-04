#include <future>

#include "firebase/firestore.h"
#include "firestore_integration_test.h"

// These test cases are in sync with native iOS client SDK test
//   Firestore/Example/Tests/Integration/API/FIRFirestoreSourceTests.mm
// and native Android client SDK test
//   firebase-firestore/src/androidTest/java/com/google/firebase/firestore/SourceTest.java

namespace firebase {
namespace firestore {

// class SourceTest : public FirestoreIntegrationTest {
//  public:
//   DocumentReference WriteDocumentWithInitData(const MapFieldValue& data)
//   const {
//     DocumentReference docRef = Document();
//     Future<void> future = docRef.Set(data);
//     Await(future);
//     FailIfUnsuccessful("WriteDocumentWithInitData", future);
//     return docRef;
//   }
// };

using SourceTest = FirestoreIntegrationTest;

TEST_F(SourceTest, TestGetDocumentWhileOnlineWithDefaultGetOptions) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  Future<DocumentSnapshot> future = docRef.Get();
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_TRUE(snapshot.exists());
  ASSERT_FALSE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());
}

TEST_F(SourceTest, TestGetCollectionWhileOnlineWithDefaultGetOptions) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  Future<QuerySnapshot> future = colRef.Get();
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_FALSE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(3, querySnapshot.DocumentChanges().size());
  ASSERT_EQ(initialDocs, QuerySnapshotToMap(querySnapshot));
}

TEST_F(SourceTest, TestGetDocumentWhileOfflineWithDefaultGetOptions) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  WaitFor(docRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  Future<DocumentSnapshot> future = docRef.Get();
  WaitFor(future);
  DocumentSnapshot snapshot = *future.result();

  ASSERT_TRUE(snapshot.exists());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());
}

TEST_F(SourceTest, TestGetCollectionWhileOfflineWithDefaultGetOptions) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  WaitFor(colRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  // Since we're offline, the returned futures won't complete.
  colRef.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                              SetOptions().Merge());
  colRef.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  colRef.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  Future<QuerySnapshot> future = colRef.Get();
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_TRUE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(4, querySnapshot.DocumentChanges().size());
  std::map<std::string, MapFieldValue> newData{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  ASSERT_EQ(newData, QuerySnapshotToMap(querySnapshot));
}

TEST_F(SourceTest, TestGetDocumentWhileOnlineWithSourceEqualToCache) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  WaitFor(docRef.Get());
  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  WaitFor(future);
  DocumentSnapshot snapshot = *future.result();

  ASSERT_TRUE(snapshot.exists());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());
}

TEST_F(SourceTest, TestGetCollectionWhileOnlineWithSourceEqualToCache) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  WaitFor(colRef.Get());
  Future<QuerySnapshot> future = colRef.Get(Source::kCache);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(3, querySnapshot.DocumentChanges().size());
  ASSERT_EQ(initialDocs, QuerySnapshotToMap(querySnapshot));
}

TEST_F(SourceTest, TestGetDocumentWhileOfflineWithSourceEqualToCache) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  WaitFor(docRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());
  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  WaitFor(future);
  DocumentSnapshot snapshot = *future.result();

  ASSERT_TRUE(snapshot.exists());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());
}

TEST_F(SourceTest, TestGetCollectionWhileOfflineWithSourceEqualToCache) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  WaitFor(colRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  // Since we're offline, the returned futures won't complete.
  colRef.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                              SetOptions().Merge());
  colRef.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  colRef.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  Future<QuerySnapshot> future = colRef.Get(Source::kCache);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_TRUE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(4, querySnapshot.DocumentChanges().size());
  std::map<std::string, MapFieldValue> newData{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  ASSERT_EQ(newData, QuerySnapshotToMap(querySnapshot));
}

TEST_F(SourceTest, TestGetDocumentWhileOnlineWithSourceEqualToServer) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  Future<DocumentSnapshot> future = docRef.Get(Source::kServer);
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_TRUE(snapshot.exists());
  ASSERT_FALSE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());
}

TEST_F(SourceTest, TestGetCollectionWhileOnlineWithSourceEqualToServer) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  Future<QuerySnapshot> future = colRef.Get(Source::kServer);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_FALSE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(3, querySnapshot.DocumentChanges().size());
  ASSERT_EQ(initialDocs, QuerySnapshotToMap(querySnapshot));
}

TEST_F(SourceTest, TestGetDocumentWhileOfflineWithSourceEqualToServer) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  WaitFor(docRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  Future<DocumentSnapshot> future = docRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetCollectionWhileOfflineWithSourceEqualToServer) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  WaitFor(colRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  Future<QuerySnapshot> future = colRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetDocumentWhileOfflineWithDifferentGetOptions) {
  MapFieldValue initialData = {{"key", FieldValue::String("value")}};
  DocumentReference docRef = DocumentWithData(initialData);

  WaitFor(docRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  // Create an initial listener for this query (to attempt to disrupt the
  // below) and wait for the listener to deliver its initial snapshot before
  // continuing.
  std::promise<Error> errorPromise;
  std::future<Error> errorFuture = errorPromise.get_future();

  docRef.AddSnapshotListener([&errorPromise](const DocumentSnapshot& snapshot,
                                             Error error_code,
                                             const std::string& error_message) {
    errorPromise.set_value(error_code);
  });
  errorFuture.wait();

  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  WaitFor(future);
  DocumentSnapshot snapshot = *future.result();
  ASSERT_TRUE(snapshot.exists());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());

  future = docRef.Get();
  WaitFor(future);
  snapshot = *future.result();
  ASSERT_TRUE(snapshot.exists());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
  ASSERT_EQ(initialData, snapshot.GetData());

  future = docRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetCollectionWhileOfflineWithDifferentGetOptions) {
  const std::map<std::string, MapFieldValue> initialDocs = {
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2", {{"key2", FieldValue::String("value2")}}},
      {"doc3", {{"key3", FieldValue::String("value3")}}}};
  CollectionReference colRef = Collection(initialDocs);

  WaitFor(colRef.Get());
  WaitFor(TestFirestore()->DisableNetwork());

  // Since we're offline, the returned futures won't complete.
  colRef.Document("doc2").Set({{"key2b", FieldValue::String("value2b")}},
                              SetOptions().Merge());
  colRef.Document("doc3").Set({{"key3b", FieldValue::String("value3b")}});
  colRef.Document("doc4").Set({{"key4", FieldValue::String("value4")}});

  // Create an initial listener for this query (to attempt to disrupt the
  // below) and wait for the listener to deliver its initial snapshot before
  // continuing.
  std::promise<Error> errorPromise;
  std::future<Error> errorFuture = errorPromise.get_future();

  colRef.AddSnapshotListener([&errorPromise](const QuerySnapshot& snapshot,
                                             Error error_code,
                                             const std::string& error_message) {
    errorPromise.set_value(error_code);
  });
  errorFuture.wait();

  Future<QuerySnapshot> future = colRef.Get(Source::kCache);
  WaitFor(future);
  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_TRUE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(4, querySnapshot.DocumentChanges().size());
  std::map<std::string, MapFieldValue> newData{
      {"doc1", {{"key1", FieldValue::String("value1")}}},
      {"doc2",
       {{"key2", FieldValue::String("value2")},
        {"key2b", FieldValue::String("value2b")}}},
      {"doc3", {{"key3b", FieldValue::String("value3b")}}},
      {"doc4", {{"key4", FieldValue::String("value4")}}},
  };
  ASSERT_EQ(newData, QuerySnapshotToMap(querySnapshot));

  future = colRef.Get();
  WaitFor(future);
  querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_TRUE(querySnapshot.metadata().has_pending_writes());
  ASSERT_EQ(4, querySnapshot.DocumentChanges().size());
  ASSERT_EQ(newData, QuerySnapshotToMap(querySnapshot));

  future = colRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOnlineWithDefaultGetOptions) {
  DocumentReference docRef = Document();

  Future<DocumentSnapshot> future = docRef.Get();
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_FALSE(snapshot.exists());
  ASSERT_FALSE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOnlineWithDefaultGetOptions) {
  CollectionReference colRef = Collection();

  Future<QuerySnapshot> future = colRef.Get();
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.empty());
  ASSERT_EQ(0, querySnapshot.DocumentChanges().size());
  ASSERT_FALSE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOfflineWithDefaultGetOptions) {
  DocumentReference docRef = Document();

  WaitFor(TestFirestore()->DisableNetwork());
  Future<DocumentSnapshot> future = docRef.Get();
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetDeletedDocWhileOfflineWithDefaultGetOptions) {
  DocumentReference docRef = Document();
  WaitFor(docRef.Delete());

  WaitFor(TestFirestore()->DisableNetwork());
  Future<DocumentSnapshot> future = docRef.Get();
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_FALSE(snapshot.exists());
  ASSERT_EQ(0, snapshot.GetData().size());
  // TODO(ehsann): is_from_cache() is false. why?
  // ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOfflineWithDefaultGetOptions) {
  CollectionReference colRef = Collection();

  WaitFor(TestFirestore()->DisableNetwork());
  Future<QuerySnapshot> future = colRef.Get();
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.empty());
  ASSERT_EQ(0, querySnapshot.DocumentChanges().size());
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOnlineWithSourceEqualToCache) {
  DocumentReference docRef = Document();

  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOnlineWithSourceEqualToCache) {
  CollectionReference colRef = Collection();

  Future<QuerySnapshot> future = colRef.Get(Source::kCache);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.empty());
  ASSERT_EQ(0, querySnapshot.DocumentChanges().size());
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOfflineWithSourceEqualToCache) {
  DocumentReference docRef = Document();

  WaitFor(TestFirestore()->DisableNetwork());
  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest, TestGetDeletedDocWhileOfflineWithSourceEqualToCache) {
  DocumentReference docRef = Document();
  WaitFor(docRef.Delete());

  WaitFor(TestFirestore()->DisableNetwork());
  Future<DocumentSnapshot> future = docRef.Get(Source::kCache);
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_FALSE(snapshot.exists());
  ASSERT_EQ(0, snapshot.GetData().size());
  ASSERT_TRUE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOfflineWithSourceEqualToCache) {
  CollectionReference colRef = Collection();

  WaitFor(TestFirestore()->DisableNetwork());
  Future<QuerySnapshot> future = colRef.Get(Source::kCache);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.empty());
  ASSERT_EQ(0, querySnapshot.DocumentChanges().size());
  ASSERT_TRUE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOnlineWithSourceEqualToServer) {
  DocumentReference docRef = Document();

  Future<DocumentSnapshot> future = docRef.Get(Source::kServer);
  WaitFor(future);

  DocumentSnapshot snapshot = *future.result();
  ASSERT_FALSE(snapshot.exists());
  ASSERT_FALSE(snapshot.metadata().is_from_cache());
  ASSERT_FALSE(snapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOnlineWithSourceEqualToServer) {
  CollectionReference colRef = Collection();

  Future<QuerySnapshot> future = colRef.Get(Source::kServer);
  WaitFor(future);

  QuerySnapshot querySnapshot = *future.result();
  ASSERT_TRUE(querySnapshot.empty());
  ASSERT_EQ(0, querySnapshot.DocumentChanges().size());
  ASSERT_FALSE(querySnapshot.metadata().is_from_cache());
  ASSERT_FALSE(querySnapshot.metadata().has_pending_writes());
}

TEST_F(SourceTest, TestGetNonExistingDocWhileOfflineWithSourceEqualToServer) {
  DocumentReference docRef = Document();

  WaitFor(TestFirestore()->DisableNetwork());
  // Attempt to get doc. This will fail since there's nothing in cache.
  Future<DocumentSnapshot> future = docRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

TEST_F(SourceTest,
       TestGetNonExistingCollectionWhileOfflineWithSourceEqualToServer) {
  CollectionReference colRef = Collection();

  WaitFor(TestFirestore()->DisableNetwork());
  Future<QuerySnapshot> future = colRef.Get(Source::kServer);
  Await(future);
  EXPECT_EQ(future.status(), FutureStatus::kFutureStatusComplete);
  EXPECT_EQ(future.error(), Error::kErrorUnavailable);
}

}  // namespace firestore
}  // namespace firebase