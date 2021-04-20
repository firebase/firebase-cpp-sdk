#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_WRITE_BATCH_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_WRITE_BATCH_STUB_H_

#include "firestore/src/common/futures.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/write_batch.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of WriteBatch.
class WriteBatchInternal {
 public:
  using ApiType = WriteBatch;
  WriteBatchInternal() {}
  FirestoreInternal* firestore_internal() const { return nullptr; }

  void Set(const DocumentReference& document, const MapFieldValue& data,
           const SetOptions& options) {}

  void Update(const DocumentReference& document, const MapFieldValue& data) {}

  void Update(const DocumentReference& document,
              const MapFieldPathValue& data) {}

  void Delete(const DocumentReference& document) {}

  Future<void> Commit() { return FailedFuture<void>(); }
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_WRITE_BATCH_STUB_H_
