#include "firebase/csharp/snapshots_in_sync_listener.h"

namespace firebase {
namespace firestore {
namespace csharp {

ListenerRegistration AddSnapshotsInSyncListener(
    Firestore* firestore, int32_t callback_id,
    SnapshotsInSyncCallback callback) {
  return firestore->AddSnapshotsInSyncListener(
      [callback, callback_id]() { callback(callback_id); });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
