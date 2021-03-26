#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_COLLECTION_REFERENCE_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_COLLECTION_REFERENCE_STUB_H_

#include <string>

#include "app/src/include/firebase/app.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/stub/firestore_stub.h"
#include "firestore/src/stub/query_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of CollectionReference.
class CollectionReferenceInternal : public QueryInternal {
 public:
  using ApiType = CollectionReference;
  FirestoreInternal* firestore_internal() { return nullptr; }
  const std::string& id() { return id_; }
  const std::string& path() { return id_; }
  DocumentReference Parent() const { return DocumentReference{}; }
  DocumentReference Document() const { return DocumentReference{}; }
  DocumentReference Document(const std::string& document_path) const {
    return DocumentReference{};
  }
  Future<DocumentReference> Add(const MapFieldValue& data) {
    return FailedFuture<DocumentReference>();
  }

 private:
  std::string id_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_COLLECTION_REFERENCE_STUB_H_
