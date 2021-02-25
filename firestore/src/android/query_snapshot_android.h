#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_QUERY_SNAPSHOT_ANDROID_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_QUERY_SNAPSHOT_ANDROID_H_

#include <cstddef>
#include <vector>

#include "firestore/src/android/wrapper.h"
#include "firestore/src/include/firebase/firestore/document_change.h"
#include "firestore/src/include/firebase/firestore/metadata_changes.h"
#include "firestore/src/include/firebase/firestore/query.h"
#include "firestore/src/include/firebase/firestore/query_snapshot.h"
#include "firestore/src/include/firebase/firestore/snapshot_metadata.h"
#include "firestore/src/jni/jni_fwd.h"

namespace firebase {
namespace firestore {

class QuerySnapshotInternal : public Wrapper {
 public:
  using Wrapper::Wrapper;

  static void Initialize(jni::Loader& loader);

  /**
   * @brief The query from which you get this QuerySnapshot.
   */
  Query query() const;

  /**
   * @brief Metadata about this snapshot, concerning its source and if it has
   * local modifications.
   *
   * @return The metadata for this document snapshot.
   */
  SnapshotMetadata metadata() const;

  /**
   * @brief The list of documents that changed since the last snapshot. If it's
   * the first snapshot, all documents will be in the list as added changes.
   *
   * Documents with changes only to their metadata will not be included.
   *
   * @param[in] metadata_changes Indicates whether metadata-only changes (i.e.
   * only Query.getMetadata() changed) should be included.
   *
   * @return The list of document changes since the last snapshot.
   */
  std::vector<DocumentChange> DocumentChanges(
      MetadataChanges metadata_changes) const;

  /**
   * @brief The list of documents in this QuerySnapshot in order of the query.
   *
   * @return The list of documents.
   */
  std::vector<DocumentSnapshot> documents() const;

  /**
   * @brief Check the size of the QuerySnapshot.
   *
   * @return The number of documents in the QuerySnapshot.
   */
  std::size_t size() const;
};

}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_ANDROID_QUERY_SNAPSHOT_ANDROID_H_
