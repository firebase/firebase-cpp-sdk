#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_CHANGE_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_CHANGE_STUB_H_

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of DocumentChange.
class DocumentChangeInternal {
 public:
  using ApiType = DocumentChange;
  DocumentChangeInternal() {}
  FirestoreInternal* firestore_internal() const { return nullptr; }
  DocumentChange::Type type() const { return DocumentChange::Type::kModified; }
  DocumentSnapshot document() const { return DocumentSnapshot{}; }
  std::size_t old_index() const { return 1; }
  std::size_t new_index() const { return 2; }
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_CHANGE_STUB_H_
