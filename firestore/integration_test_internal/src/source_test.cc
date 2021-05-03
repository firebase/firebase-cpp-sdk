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

}  // namespace firestore
}  // namespace firebase