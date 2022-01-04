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
  EXPECT_EQ(ptr->firestore(), nullptr);
  // Make sure to check both const and non-const overloads.
  EXPECT_EQ(static_cast<const T*>(ptr)->firestore(), nullptr);
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

// Checks that `operator==` and `operator!=` work correctly by comparing to
// a default-constructed instance.
template <typename T>
void ExpectEqualityToWork(const T& ref) {
  EXPECT_TRUE(ref == T());
  EXPECT_FALSE(ref != T());
}

// `ExpectAllMethodsAreNoOps` calls all the public API methods on the given
// `ptr` and checks that the calls don't crash and, where applicable, return
// value-initialized values.

void ExpectAllMethodsAreNoOps(CollectionReference* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  EXPECT_EQ(*ptr, CollectionReference());
  ExpectCopyableAndMoveable(ptr);
  ExpectEqualityToWork(ptr);

  ExpectAllMethodsAreNoOps(static_cast<Query*>(ptr));

  EXPECT_EQ(ptr->id(), "");
  EXPECT_EQ(ptr->path(), "");

  EXPECT_EQ(ptr->Document(), DocumentReference());
  EXPECT_EQ(ptr->Document("foo"), DocumentReference());
  EXPECT_EQ(ptr->Document(std::string("foo")), DocumentReference());

  EXPECT_EQ(ptr->Add(MapFieldValue()), FailedFuture<DocumentReference>());
}

void ExpectAllMethodsAreNoOps(DocumentChange* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  // TODO(b/137966104): implement == on `DocumentChange`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_EQ(ptr->type(), DocumentChange::Type());
  // TODO(b/137966104): implement == on `DocumentSnapshot`
  ptr->document();
  EXPECT_EQ(ptr->old_index(), 0);
  EXPECT_EQ(ptr->new_index(), 0);
}

void ExpectAllMethodsAreNoOps(DocumentReference* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);
  ExpectNullFirestore(ptr);

  EXPECT_EQ(ptr->ToString(), "DocumentReference(invalid)");

  EXPECT_EQ(ptr->id(), "");
  EXPECT_EQ(ptr->path(), "");

  EXPECT_EQ(ptr->Parent(), CollectionReference());
  EXPECT_EQ(ptr->Collection("foo"), CollectionReference());
  EXPECT_EQ(ptr->Collection(std::string("foo")), CollectionReference());

  EXPECT_EQ(ptr->Get(), FailedFuture<DocumentSnapshot>());

  EXPECT_EQ(ptr->Set(MapFieldValue()), FailedFuture<void>());

  EXPECT_EQ(ptr->Update(MapFieldValue()), FailedFuture<void>());
  EXPECT_EQ(ptr->Update(MapFieldPathValue()), FailedFuture<void>());

  EXPECT_EQ(ptr->Delete(), FailedFuture<void>());

  ptr->AddSnapshotListener(
      [](const DocumentSnapshot&, Error, const std::string&) {});
}

void ExpectAllMethodsAreNoOps(DocumentSnapshot* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  // TODO(b/137966104): implement == on `DocumentSnapshot`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_EQ(ptr->ToString(), "DocumentSnapshot(invalid)");

  EXPECT_EQ(ptr->id(), "");
  EXPECT_FALSE(ptr->exists());

  EXPECT_EQ(ptr->reference(), DocumentReference());
  ExpectEqualityToWork(ptr->metadata());

  EXPECT_EQ(ptr->GetData(), MapFieldValue());

  EXPECT_EQ(ptr->Get("foo"), FieldValue());
  EXPECT_EQ(ptr->Get(std::string("foo")), FieldValue());
  EXPECT_EQ(ptr->Get(FieldPath{"foo"}), FieldValue());
}

void ExpectAllMethodsAreNoOps(FieldValue* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  // FieldValue doesn't have a separate "invalid" type in its enum.
  EXPECT_TRUE(ptr->is_null());

  EXPECT_EQ(ptr->type(), FieldValue::Type());

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

  EXPECT_EQ(ptr->boolean_value(), false);
  EXPECT_EQ(ptr->integer_value(), 0);
  EXPECT_EQ(ptr->double_value(), 0);
  EXPECT_EQ(ptr->timestamp_value(), Timestamp());
  EXPECT_EQ(ptr->string_value(), "");
  EXPECT_EQ(ptr->blob_value(), nullptr);
  EXPECT_EQ(ptr->reference_value(), DocumentReference());
  EXPECT_EQ(ptr->geo_point_value(), GeoPoint());
  EXPECT_TRUE(ptr->array_value().empty());
  EXPECT_TRUE(ptr->map_value().empty());
}

void ExpectAllMethodsAreNoOps(ListenerRegistration* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  // `ListenerRegistration` isn't equality comparable.
  ExpectCopyableAndMoveable(ptr);

  ptr->Remove();
}

void ExpectAllMethodsAreNoOps(Query* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);
  ExpectNullFirestore(ptr);

  EXPECT_EQ(ptr->WhereEqualTo("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereEqualTo(FieldPath{"foo"}, FieldValue()), Query());

  EXPECT_EQ(ptr->WhereLessThan("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereLessThan(FieldPath{"foo"}, FieldValue()), Query());

  EXPECT_EQ(ptr->WhereLessThanOrEqualTo("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereLessThanOrEqualTo(FieldPath{"foo"}, FieldValue()),
            Query());

  EXPECT_EQ(ptr->WhereGreaterThan("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereGreaterThan(FieldPath{"foo"}, FieldValue()), Query());

  EXPECT_EQ(ptr->WhereGreaterThanOrEqualTo("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereGreaterThanOrEqualTo(FieldPath{"foo"}, FieldValue()),
            Query());

  EXPECT_EQ(ptr->WhereArrayContains("foo", FieldValue()), Query());
  EXPECT_EQ(ptr->WhereArrayContains(FieldPath{"foo"}, FieldValue()), Query());

  EXPECT_EQ(ptr->OrderBy("foo"), Query());
  EXPECT_EQ(ptr->OrderBy(FieldPath{"foo"}), Query());

  EXPECT_EQ(ptr->Limit(123), Query());

  EXPECT_EQ(ptr->StartAt(DocumentSnapshot()), Query());
  EXPECT_EQ(ptr->StartAt(std::vector<FieldValue>()), Query());

  EXPECT_EQ(ptr->StartAfter(DocumentSnapshot()), Query());
  EXPECT_EQ(ptr->StartAfter(std::vector<FieldValue>()), Query());

  EXPECT_EQ(ptr->EndBefore(DocumentSnapshot()), Query());
  EXPECT_EQ(ptr->EndBefore(std::vector<FieldValue>()), Query());

  EXPECT_EQ(ptr->EndAt(DocumentSnapshot()), Query());
  EXPECT_EQ(ptr->EndAt(std::vector<FieldValue>()), Query());

  EXPECT_EQ(ptr->Get(), FailedFuture<QuerySnapshot>());

  EXPECT_EQ(ptr->Get(), FailedFuture<QuerySnapshot>());

  ptr->AddSnapshotListener(
      [](const QuerySnapshot&, Error, const std::string&) {});
}

void ExpectAllMethodsAreNoOps(QuerySnapshot* ptr) {
  EXPECT_FALSE(ptr->is_valid());

  // TODO(b/137966104): implement == on `QuerySnapshot`
  // ExpectEqualityToWork(ptr);
  ExpectCopyableAndMoveable(ptr);

  EXPECT_EQ(ptr->query(), Query());

  ExpectEqualityToWork(ptr->metadata());

  EXPECT_TRUE(ptr->DocumentChanges().empty());
  EXPECT_TRUE(ptr->documents().empty());
  EXPECT_TRUE(ptr->empty());
  EXPECT_EQ(ptr->size(), 0);
}

void ExpectAllMethodsAreNoOps(WriteBatch* ptr, const DocumentReference& doc) {
  EXPECT_FALSE(ptr->is_valid());

  // `WriteBatch` isn't equality comparable.
  ExpectCopyableAndMoveable(ptr);

  ptr->Set(doc, MapFieldValue());

  ptr->Update(doc, MapFieldValue());
  ptr->Update(doc, MapFieldPathValue());

  ptr->Delete(doc);

  EXPECT_EQ(ptr->Commit(), FailedFuture<void>());
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
  EXPECT_EQ(str_value.string_value(), "bar");

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
  EXPECT_EQ(snap.size(), 1);

  DeleteFirestore(col.firestore());
  SCOPED_TRACE("QuerySnapshot.AfterCleanup");
  ExpectAllMethodsAreNoOps(&snap);
}

// Note: `Transaction` is uncopyable and not default constructible, and storing
// a pointer to a `Transaction` is not valid in general, because the object will
// be destroyed as soon as the transaction is finished. Thus, there is no valid
// case where a user could be accessing a "blank" transaction.

TEST_F(CleanupTest, WriteBatchIsBlankAfterCleanup) {
  // Need a valid `DocumentReference` to avoid `WriteBatch` methods throwing.
  DocumentReference doc = Document();

  {
    WriteBatch default_constructed;
    SCOPED_TRACE("WriteBatch.DefaultConstructed");
    ExpectAllMethodsAreNoOps(&default_constructed, doc);
  }

  Firestore* db = TestFirestore();
  WriteBatch batch = db->batch();
  DeleteFirestore(db);
  SCOPED_TRACE("WriteBatch.AfterCleanup");
  ExpectAllMethodsAreNoOps(&batch, doc);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
