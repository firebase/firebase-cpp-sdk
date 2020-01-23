#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_TRANSACTION_STUB_H__
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_TRANSACTION_STUB_H__

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/transaction.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of WriteBatch.
class TransactionInternal {
 public:
  using ApiType = Transaction;
  TransactionInternal() {}
  FirestoreInternal* firestore_internal() const { return nullptr; }

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options) {}

  void Update(const DocumentReference& document, const MapFieldValue& data) {}

  void Update(const DocumentReference& document,
              const MapFieldPathValue& data) {}

  void Delete(const DocumentReference& document) {}

  DocumentSnapshot Get(const DocumentReference& document, Error* error_code,
                       std::string* error_message) {
    return DocumentSnapshot{};
  }
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_TRANSACTION_STUB_H__
