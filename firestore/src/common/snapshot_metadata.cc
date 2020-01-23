#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"

#include <ostream>

namespace firebase {
namespace firestore {

std::string SnapshotMetadata::ToString() const {
  return std::string("SnapshotMetadata{") +
         "has_pending_writes=" + (has_pending_writes() ? "true" : "false") +
         ", is_from_cache=" + (is_from_cache() ? "true" : "false") + '}';
}

std::ostream& operator<<(std::ostream& out, const SnapshotMetadata& metadata) {
  return out << metadata.ToString();
}

}  // namespace firestore
}  // namespace firebase
