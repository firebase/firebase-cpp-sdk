#include "app/src/include/firebase/internal/common.h"
#include "firebase/firestore.h"
#include "firestore/src/common/futures.h"
#include "firestore_integration_test.h"

namespace firebase {
namespace firestore {
namespace {

void ExpectAllMethodsAreNoOps(Query* ptr);

// Checks that methods accessing the associated Firestore instance don't crash
// and return null.
template <typename T>
void ExpectNullFirestore(T* ptr) {
  EXPECT_TRUE(ptr->firestore() == nullptr);
  // Make sure to check both const and non-const overloads.
  EXPECT_TRUE(static_cast<const T*>(ptr)->firestore() == nullptr);
}

// Checks that the given object can be copied from, and the resulting copy can
// be moved.
template <typename T>
void ExpectCopyableAndMoveable(T* ptr) {
  // Copy constructor
  T copy = *ptr;
  // Move constructor
  T moved = std::move(copy);

  // Copy assignment operator
  copy = *ptr;
  // Move assignment operator
  moved = std::move(copy);
}

// Checks that `operator==` and `operator!=` work correctly by comparing to
// a default-constructed instance.
template <typename T>
void ExpectEqualityToWork(T* ptr) {
  EXPECT_TRUE(*ptr == T());
  EXPECT_FALSE(*ptr != T());
}

// `ExpectAllMethodsAreNoOps` calls all the public API methods on the given
// `ptr` and checks that the calls don't crash and, where applicable, return
// value-initialized values.

void ExpectAllMethodsAreNoOps(CollectionReference* ptr) {
  EXPECT_TRUE(*ptr == CollectionReference());
  ExpectCopyableAndMoveable(ptr);
  ExpectEqualityToWork(ptr);

  ExpectAllMethodsAreNoOps(static_cast<Query*>(ptr));

  EXPECT_TRUE(ptr->id() == "");
  EXPECT_TRUE(ptr->path() == "");

  EXPECT_TRUE(ptr->Document() == DocumentReference());
  EXPECT_TRUE(ptr->Document("foo") == DocumentReference());
  EXPECT_TRUE(ptr->Document(std::string("foo")) == DocumentReference());

  EXPECT_TRUE(ptr->Add(MapFieldValue()) == FailedFuture<DocumentReference>());
}

void ExpectAllMethodsAreNoOps(DocumentChange* ptr) {
  // TODO(b/137966104): implement == on `DocumentChange`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_TRUE(ptr->type() == DocumentChange::Type());
  // TODO(b/137966104): implement == on `DocumentSnapshot`
  ptr->document();
  EXPECT_TRUE(ptr->old_index() == 0);
  EXPECT_TRUE(ptr->new_index() == 0);
}

void ExpectAllMethodsAreNoOps(DocumentReference* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);
  ExpectNullFirestore(ptr);

  EXPECT_TRUE(ptr->ToString() == "DocumentReference(invalid)");

  EXPECT_TRUE(ptr->id() == "");
  EXPECT_TRUE(ptr->path() == "");

  EXPECT_TRUE(ptr->Parent() == CollectionReference());
  EXPECT_TRUE(ptr->Collection("foo") == CollectionReference());
  EXPECT_TRUE(ptr->Collection(std::string("foo")) == CollectionReference());

  EXPECT_TRUE(ptr->Get() == FailedFuture<DocumentSnapshot>());

  EXPECT_TRUE(ptr->Set(MapFieldValue()) == FailedFuture<void>());

  EXPECT_TRUE(ptr->Update(MapFieldValue()) == FailedFuture<void>());
  EXPECT_TRUE(ptr->Update(MapFieldPathValue()) == FailedFuture<void>());

  EXPECT_TRUE(ptr->Delete() == FailedFuture<void>());

#if defined(FIREBASE_USE_STD_FUNCTION)
  ptr->AddSnapshotListener(
      [](const DocumentSnapshot&, Error, const std::string&) {});
#else
  ptr->AddSnapshotListener(nullptr);
#endif
}

void ExpectAllMethodsAreNoOps(DocumentSnapshot* ptr) {
  // TODO(b/137966104): implement == on `DocumentSnapshot`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_TRUE(ptr->ToString() == "DocumentSnapshot(invalid)");

  EXPECT_TRUE(ptr->id() == "");
  EXPECT_FALSE(ptr->exists());

  EXPECT_TRUE(ptr->reference() == DocumentReference());
  // TODO(b/137966104): implement == on `SnapshotMetadata`
  ptr->metadata();

  EXPECT_TRUE(ptr->GetData() == MapFieldValue());

  EXPECT_TRUE(ptr->Get("foo") == FieldValue());
  EXPECT_TRUE(ptr->Get(std::string("foo")) == FieldValue());
  EXPECT_TRUE(ptr->Get(FieldPath{"foo"}) == FieldValue());
}

void ExpectAllMethodsAreNoOps(FieldValue* ptr) {
  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_FALSE(ptr->is_valid());
  // FieldValue doesn't have a separate "invalid" type in its enum.
  EXPECT_TRUE(ptr->is_null());

  EXPECT_TRUE(ptr->type() == FieldValue::Type());

  EXPECT_FALSE(ptr->is_boolean());
  EXPECT_FALSE(ptr->is_integer());
  EXPECT_FALSE(ptr->is_double());
  EXPECT_FALSE(ptr->is_timestamp());
  EXPECT_FALSE(ptr->is_string());
  EXPECT_FALSE(ptr->is_blob());
  EXPECT_FALSE(ptr->is_reference());
  EXPECT_FALSE(ptr->is_geo_point());
  EXPECT_FALSE(ptr->is_array());
  EXPECT_FALSE(ptr->is_map());

  EXPECT_TRUE(ptr->boolean_value() == false);
  EXPECT_TRUE(ptr->integer_value() == 0);
  EXPECT_TRUE(ptr->double_value() == 0);
  EXPECT_TRUE(ptr->timestamp_value() == Timestamp());
  EXPECT_TRUE(ptr->string_value() == "");
  EXPECT_TRUE(ptr->blob_value() == nullptr);
  EXPECT_TRUE(ptr->reference_value() == DocumentReference());
  EXPECT_TRUE(ptr->geo_point_value() == GeoPoint());
  EXPECT_TRUE(ptr->array_value().empty());
  EXPECT_TRUE(ptr->map_value().empty());
}

void ExpectAllMethodsAreNoOps(ListenerRegistration* ptr) {
  // `ListenerRegistration` isn't equality comparable.
  ExpectCopyableAndMoveable(ptr);

  ptr->Remove();
}

void ExpectAllMethodsAreNoOps(Query* ptr) {
  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);
  ExpectNullFirestore(ptr);

  EXPECT_TRUE(ptr->WhereEqualTo("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereEqualTo(FieldPath{"foo"}, FieldValue()) == Query());

  EXPECT_TRUE(ptr->WhereLessThan("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereLessThan(FieldPath{"foo"}, FieldValue()) == Query());

  EXPECT_TRUE(ptr->WhereLessThanOrEqualTo("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereLessThanOrEqualTo(FieldPath{"foo"}, FieldValue()) ==
              Query());

  EXPECT_TRUE(ptr->WhereGreaterThan("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereGreaterThan(FieldPath{"foo"}, FieldValue()) == Query());

  EXPECT_TRUE(ptr->WhereGreaterThanOrEqualTo("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereGreaterThanOrEqualTo(FieldPath{"foo"}, FieldValue()) ==
              Query());

  EXPECT_TRUE(ptr->WhereArrayContains("foo", FieldValue()) == Query());
  EXPECT_TRUE(ptr->WhereArrayContains(FieldPath{"foo"}, FieldValue()) ==
              Query());

  EXPECT_TRUE(ptr->OrderBy("foo") == Query());
  EXPECT_TRUE(ptr->OrderBy(FieldPath{"foo"}) == Query());

  EXPECT_TRUE(ptr->Limit(123) == Query());

  EXPECT_TRUE(ptr->StartAt(DocumentSnapshot()) == Query());
  EXPECT_TRUE(ptr->StartAt(std::vector<FieldValue>()) == Query());

  EXPECT_TRUE(ptr->StartAfter(DocumentSnapshot()) == Query());
  EXPECT_TRUE(ptr->StartAfter(std::vector<FieldValue>()) == Query());

  EXPECT_TRUE(ptr->EndBefore(DocumentSnapshot()) == Query());
  EXPECT_TRUE(ptr->EndBefore(std::vector<FieldValue>()) == Query());

  EXPECT_TRUE(ptr->EndAt(DocumentSnapshot()) == Query());
  EXPECT_TRUE(ptr->EndAt(std::vector<FieldValue>()) == Query());

  EXPECT_TRUE(ptr->Get() == FailedFuture<QuerySnapshot>());

  EXPECT_TRUE(ptr->Get() == FailedFuture<QuerySnapshot>());

#if defined(FIREBASE_USE_STD_FUNCTION)
  ptr->AddSnapshotListener(
      [](const QuerySnapshot&, Error, const std::string&) {});
#else
  ptr->AddSnapshotListener(nullptr);
#endif
}

void ExpectAllMethodsAreNoOps(QuerySnapshot* ptr) {
  // TODO(b/137966104): implement == on `QuerySnapshot`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_TRUE(ptr->query() == Query());

  // TODO(b/137966104): implement == on `SnapshotMetadata`
  ptr->metadata();

  EXPECT_TRUE(ptr->DocumentChanges().empty());
  EXPECT_TRUE(ptr->documents().empty());
  EXPECT_TRUE(ptr->empty());
  EXPECT_TRUE(ptr->size() == 0);
}

void ExpectAllMethodsAreNoOps(WriteBatch* ptr) {
  // `WriteBatch` isn't equality comparable.
  ExpectCopyableAndMoveable(ptr);

  ptr->Set(DocumentReference(), MapFieldValue());

  ptr->Update(DocumentReference(), MapFieldValue());
  ptr->Update(DocumentReference(), MapFieldPathValue());

  ptr->Delete(DocumentReference());

  EXPECT_TRUE(ptr->Commit() == FailedFuture<void>());
}

using CleanupTest = FirestoreIntegrationTest;

TEST_F(CleanupTest, CollectionReferenceIsBlankAfterCleanup) {
  {
    CollectionReference default_constructed;
    SCOPED_TRACE("CollectionReference.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  CollectionReference col = Collection();
  DeleteFirestore(col.firestore());
  SCOPED_TRACE("CollectionReference.AfterCleanup");
  ExpectAllMethodsAreNoOps(&col);
}

TEST_F(CleanupTest, DocumentChangeIsBlankAfterCleanup) {
  {
    DocumentChange default_constructed;
    SCOPED_TRACE("DocumentChange.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  CollectionReference col = Collection("col");
  DocumentReference doc = col.Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});

  QuerySnapshot snap = ReadDocuments(col);
  auto changes = snap.DocumentChanges();
  ASSERT_EQ(changes.size(), 1);
  DocumentChange& change = changes.front();

  DeleteFirestore(col.firestore());
  SCOPED_TRACE("DocumentChange.AfterCleanup");
  ExpectAllMethodsAreNoOps(&change);
}

TEST_F(CleanupTest, DocumentReferenceIsBlankAfterCleanup) {
  {
    DocumentReference default_constructed;
    SCOPED_TRACE("DocumentReference.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  DocumentReference doc = Document();
  DeleteFirestore(doc.firestore());
  SCOPED_TRACE("DocumentReference.AfterCleanup");
  ExpectAllMethodsAreNoOps(&doc);
}

TEST_F(CleanupTest, DocumentSnapshotIsBlankAfterCleanup) {
  {
    DocumentSnapshot default_constructed;
    SCOPED_TRACE("DocumentSnapshot.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  DocumentReference doc = Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});
  DocumentSnapshot snap = ReadDocument(doc);

  DeleteFirestore(doc.firestore());
  SCOPED_TRACE("DocumentSnapshot.AfterCleanup");
  ExpectAllMethodsAreNoOps(&snap);
}

TEST_F(CleanupTest, FieldValueIsBlankAfterCleanup) {
  {
    FieldValue default_constructed;
    SCOPED_TRACE("FieldValue.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  DocumentReference doc = Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")},
                                   {"ref", FieldValue::Reference(doc)}});
  DocumentSnapshot snap = ReadDocument(doc);

  FieldValue str_value = snap.Get("foo");
  EXPECT_TRUE(str_value.is_valid());
  EXPECT_TRUE(str_value.is_string());

  FieldValue ref_value = snap.Get("ref");
  EXPECT_TRUE(ref_value.is_valid());
  EXPECT_TRUE(ref_value.is_reference());

  DeleteFirestore(doc.firestore());
  // `FieldValue`s are not cleaned up, because they are owned by the user and
  // stay valid after Firestore has shut down.
  EXPECT_TRUE(str_value.is_valid());
  EXPECT_TRUE(str_value.is_string());
  EXPECT_TRUE(str_value.string_value() == "bar");

  // However, need to make sure that in a reference value, the reference was
  // cleaned up.
  EXPECT_TRUE(ref_value.is_valid());
  EXPECT_TRUE(ref_value.is_reference());
  DocumentReference ref_after_cleanup = ref_value.reference_value();
  SCOPED_TRACE("FieldValue.AfterCleanup");
  ExpectAllMethodsAreNoOps(&ref_after_cleanup);
}

// Note: `Firestore` is not default-constructible, and it is deleted immediately
// after cleanup. Thus, there is no case where a user could be accessing
// a "blank" Firestore instance.

#if defined(FIREBASE_USE_STD_FUNCTION)
TEST_F(CleanupTest, ListenerRegistrationIsBlankAfterCleanup) {
  {
    ListenerRegistration default_constructed;
    SCOPED_TRACE("ListenerRegistration.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  DocumentReference doc = Document();
  ListenerRegistration reg = doc.AddSnapshotListener(
      [](const DocumentSnapshot&, Error, const std::string&) {});
  DeleteFirestore(doc.firestore());
  SCOPED_TRACE("ListenerRegistration.AfterCleanup");
  ExpectAllMethodsAreNoOps(&reg);
}
#endif

// Note: `Query` cleanup is tested as part of `CollectionReference` cleanup
// (`CollectionReference` is derived from `Query`).

TEST_F(CleanupTest, QuerySnapshotIsBlankAfterCleanup) {
  {
    QuerySnapshot default_constructed;
    SCOPED_TRACE("QuerySnapshot.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  CollectionReference col = Collection("col");
  DocumentReference doc = col.Document();
  WriteDocument(doc, MapFieldValue{{"foo", FieldValue::String("bar")}});

  QuerySnapshot snap = ReadDocuments(col);
  EXPECT_TRUE(snap.size() == 1);

  DeleteFirestore(col.firestore());
  SCOPED_TRACE("QuerySnapshot.AfterCleanup");
  ExpectAllMethodsAreNoOps(&snap);
}

// Note: `Transaction` is uncopyable and not default constructible, and storing
// a pointer to a `Transaction` is not valid in general, because the object will
// be destroyed as soon as the transaction is finished. Thus, there is no valid
// case where a user could be accessing a "blank" transaction.

TEST_F(CleanupTest, WriteBatchIsBlankAfterCleanup) {
  {
    WriteBatch default_constructed;
    SCOPED_TRACE("WriteBatch.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed);
  }

  Firestore* db = TestFirestore();
  WriteBatch batch = db->batch();
  DeleteFirestore(db);
  SCOPED_TRACE("WriteBatch.AfterCleanup");
  ExpectAllMethodsAreNoOps(&batch);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
