#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_

#include <string>

#include "firestore/src/android/firestore_android.h"
#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_reference.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/field_value.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

namespace firebase {
namespace firestore {

// This is the Android implementation of DocumentSnapshot.
class DocumentSnapshotInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  static void Initialize(jni::Loader& loader);

  ~DocumentSnapshotInternal() override {}

  /** Gets the Firestore instance associated with this document snapshot. */
  Firestore* firestore() const;

  /** Gets the document-id of this document. */
  const std::string& id() const;

  /** Gets the document location. */
  DocumentReference reference() const;

  /** Gets the metadata about this snapshot. */
  SnapshotMetadata metadata() const;

  /** Verifies the existence. */
  bool exists() const;

  /** Gets all data in the document as a map. */
  MapFieldValue GetData(DocumentSnapshot::ServerTimestampBehavior stb) const;

  /** Gets a specific field from the document. */
  FieldValue Get(const FieldPath& field,
                 DocumentSnapshot::ServerTimestampBehavior stb) const;

 private:
  mutable std::string cached_id_;
};

}  // namespace firestore
}  // namespace firebase
#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_DOCUMENT_SNAPSHOT_ANDROID_H_
