#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_SNAPSHOT_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_SNAPSHOT_IOS_H_

#include <memory>
#include <string>
#include <vector>

#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/map_field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/ios/firestore_ios.h"
#include "Firestore/core/src/api/document_snapshot.h"
#include "Firestore/core/src/model/field_value.h"

namespace firebase {
namespace firestore {

class Firestore;
class FirestoreInternal;

class DocumentSnapshotInternal {
 public:
  explicit DocumentSnapshotInternal(api::DocumentSnapshot&& snapshot);

  Firestore* firestore();
  FirestoreInternal* firestore_internal();
  const FirestoreInternal* firestore_internal() const;

  const std::string& id() const;
  bool exists() const;
  SnapshotMetadata metadata() const;

  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const;

  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const;

  const api::DocumentSnapshot& document_snapshot_core() const {
    return snapshot_;
  }

  DocumentReference reference() const;

 private:
  using ServerTimestampBehavior = DocumentSnapshot::ServerTimestampBehavior;

  FieldValue GetValue(const model::FieldPath& path,
                      ServerTimestampBehavior stb) const;

  // Note: these are member functions only because access to `api::Firestore`
  // is needed to create a `DocumentReferenceInternal`.
  FieldValue ConvertAnyValue(const model::FieldValue& input,
                             ServerTimestampBehavior stb) const;
  FieldValue ConvertObject(const model::FieldValue::Map& object,
                           ServerTimestampBehavior stb) const;
  FieldValue ConvertArray(const model::FieldValue::Array& array,
                          ServerTimestampBehavior stb) const;
  FieldValue ConvertReference(
      const model::FieldValue::Reference& reference) const;
  FieldValue ConvertServerTimestamp(
      const model::FieldValue::ServerTimestamp& server_timestamp,
      ServerTimestampBehavior stb) const;
  FieldValue ConvertScalar(const model::FieldValue& scalar,
                           ServerTimestampBehavior stb) const;

  api::DocumentSnapshot snapshot_;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_DOCUMENT_SNAPSHOT_IOS_H_
