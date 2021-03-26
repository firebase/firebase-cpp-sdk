#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_REFERENCE_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_REFERENCE_STUB_H_

#include <string>

#include "app/src/include/firebase/app.h"
#include "firestore/src/common/futures.h"
#include "firestore/src/include/firebase/firestore/collection_reference.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of DocumentReference.
class DocumentReferenceInternal {
 public:
  using ApiType = DocumentReference;
  FirestoreInternal* firestore_internal() { return nullptr; }
  Firestore* firestore() { return nullptr; }
  const std::string& id() { return id_; }
  const std::string& path() { return id_; }
  CollectionReference Parent() const { return CollectionReference{}; }
  CollectionReference Collection(const std::string& collection_path) {
    return CollectionReference{};
  }
  Future<DocumentSnapshot> Get(Source source) const {
    return FailedFuture<DocumentSnapshot>();
  }
  Future<void> Set(const MapFieldValue& data, const SetOptions& options) {
    return FailedFuture<void>();
  }
  Future<void> Update(const MapFieldValue& data) {
    return FailedFuture<void>();
  }
  Future<void> Update(const MapFieldPathValue& data) {
    return FailedFuture<void>();
  }
  Future<void> Delete() { return FailedFuture<void>(); }
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      EventListener<DocumentSnapshot>* listener) {
    return ListenerRegistration{};
  }

#if defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)
  ListenerRegistration AddSnapshotListener(
      MetadataChanges metadata_changes,
      std::function<void(const DocumentSnapshot&, Error)> callback) {
    return ListenerRegistration{};
  }
#endif  // defined(FIREBASE_USE_STD_FUNCTION) || defined(DOXYGEN)

 private:
  std::string id_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_REFERENCE_STUB_H_
