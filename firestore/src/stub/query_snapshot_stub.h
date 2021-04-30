#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_QUERY_SNAPSHOT_STUB_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_QUERY_SNAPSHOT_STUB_H_

#include <cstddef>
#include <vector>

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/include/firebase/firestore/metadata_changes.h"
#include "firestore/src/include/firebase/firestore/query.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/stub/firestore_stub.h"

namespace firebase {
namespace firestore {

class QuerySnapshotInternal {
 public:
  using ApiType = QuerySnapshot;
  FirestoreInternal* firestore_internal() { return nullptr; }
  Query query() const { return Query{}; }
  SnapshotMetadata metadata() const {
    return SnapshotMetadata{/*has_pending_writes=*/false,
                            /*is_from_cache=*/false};
  }
  std::vector<DocumentChange> DocumentChanges(
      MetadataChanges metadata_changes) const {
    return {};
  }
  std::vector<DocumentSnapshot> documents() const { return {}; }
  std::size_t size() const { return 0; }
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_STUB_QUERY_SNAPSHOT_STUB_H_
