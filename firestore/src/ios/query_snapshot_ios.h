#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_SNAPSHOT_IOS_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_SNAPSHOT_IOS_H_

#include <cstddef>
#include <vector>

#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/include/firebase/firestore/document_snapshot.h"
#include "firestore/src/include/firebase/firestore/metadata_changes.h"
#include "firestore/src/include/firebase/firestore/query.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/ios/firestore_ios.h"
#include "absl/types/optional.h"
#include "Firestore/core/src/api/query_snapshot.h"

namespace firebase {
namespace firestore {

class QuerySnapshotInternal {
 public:
  explicit QuerySnapshotInternal(api::QuerySnapshot&& snapshot);

  FirestoreInternal* firestore_internal();

  Query query() const;
  SnapshotMetadata metadata() const;
  std::size_t size() const;

  std::vector<DocumentChange> DocumentChanges(
      MetadataChanges metadata_changes) const;

  std::vector<DocumentSnapshot> documents() const;

 private:
  api::QuerySnapshot snapshot_;

  // Cache the results
  mutable absl::optional<std::vector<DocumentChange>> document_changes_;
  mutable absl::optional<std::vector<DocumentSnapshot>> documents_;
  mutable bool changes_include_metadata_ = false;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_IOS_QUERY_SNAPSHOT_IOS_H_
