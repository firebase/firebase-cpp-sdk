/*
 * Copyright 2020 Google LLC
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

#ifndef FIREBASE_FIRESTORE_SRC_COMMON_TYPE_MAPPING_H_
#define FIREBASE_FIRESTORE_SRC_COMMON_TYPE_MAPPING_H_

#include "app/src/include/firebase/internal/type_traits.h"

namespace firebase {
namespace firestore {

class AggregateQuery;
class AggregateQueryInternal;
class AggregateQuerySnapshot;
class AggregateQuerySnapshotInternal;
class CollectionReference;
class CollectionReferenceInternal;
class DocumentChange;
class DocumentChangeInternal;
class DocumentReference;
class DocumentReferenceInternal;
class DocumentSnapshot;
class DocumentSnapshotInternal;
class FieldValue;
class FieldValueInternal;
class Firestore;
class FirestoreInternal;
class ListenerRegistration;
class ListenerRegistrationInternal;
class LoadBundleTaskProgress;
class LoadBundleTaskProgressInternal;
class Query;
class QueryInternal;
class QuerySnapshot;
class QuerySnapshotInternal;
class Transaction;
class TransactionInternal;
class WriteBatch;
class WriteBatchInternal;

// `InternalType<T>` is the internal type corresponding to a public type T.
// For example, `InternalType<Firestore>` is `FirestoreInternal`. Several other
// useful mappings are included, such as `void` for `void`.

template <typename T>
struct InternalTypeMap {};

template <>
struct InternalTypeMap<AggregateQuery> {
  using type = AggregateQueryInternal;
};
template <>
struct InternalTypeMap<AggregateQuerySnapshot> {
  using type = AggregateQuerySnapshotInternal;
};
template <>
struct InternalTypeMap<CollectionReference> {
  using type = CollectionReferenceInternal;
};
template <>
struct InternalTypeMap<DocumentChange> {
  using type = DocumentChangeInternal;
};
template <>
struct InternalTypeMap<DocumentReference> {
  using type = DocumentReferenceInternal;
};
template <>
struct InternalTypeMap<DocumentSnapshot> {
  using type = DocumentSnapshotInternal;
};
template <>
struct InternalTypeMap<FieldValue> {
  using type = FieldValueInternal;
};
template <>
struct InternalTypeMap<Firestore> {
  using type = FirestoreInternal;
};
template <>
struct InternalTypeMap<ListenerRegistration> {
  using type = ListenerRegistrationInternal;
};
template <>
struct InternalTypeMap<Query> {
  using type = QueryInternal;
};
template <>
struct InternalTypeMap<QuerySnapshot> {
  using type = QuerySnapshotInternal;
};
template <>
struct InternalTypeMap<Transaction> {
  using type = TransactionInternal;
};
template <>
struct InternalTypeMap<WriteBatch> {
  using type = WriteBatchInternal;
};
template <>
struct InternalTypeMap<LoadBundleTaskProgress> {
  using type = LoadBundleTaskProgressInternal;
};
template <>
struct InternalTypeMap<void> {
  using type = void;
};

template <typename T>
using InternalType = typename InternalTypeMap<typename decay<T>::type>::type;

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_SRC_COMMON_TYPE_MAPPING_H_
