#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TYPE_MAPPING_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TYPE_MAPPING_H_

#include "app/src/include/firebase/internal/type_traits.h"

namespace firebase {
namespace firestore {

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
struct InternalTypeMap<void> {
  using type = void;
};

template <typename T>
using InternalType = typename InternalTypeMap<typename decay<T>::type>::type;

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_COMMON_TYPE_MAPPING_H_
