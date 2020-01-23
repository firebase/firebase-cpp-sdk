#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_SNAPSHOT_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_SNAPSHOT_STUB_H_

#include <map>
#include <string>

#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

// This is the stub implementation of DocumentSnapshot.
class DocumentSnapshotInternal {
 public:
  using ApiType = DocumentSnapshot;
  FirestoreInternal* firestore_internal() const { return nullptr; }
  Firestore* firestore() const { return nullptr; }
  const std::string& id() const { return id_; }
  DocumentReference reference() const { return DocumentReference{}; }
  SnapshotMetadata metadata() const { return SnapshotMetadata(false, false); }
  bool exists() const { return false; }
  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const {
    return MapFieldValue{};
  }
  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const {
    return FieldValue{};
  }

 private:
  std::string id_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_DOCUMENT_SNAPSHOT_STUB_H_
