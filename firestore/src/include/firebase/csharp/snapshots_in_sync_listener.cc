#include "firebase/csharp/snapshots_in_sync_listener.h"

#include "app/src/assert.h"

namespace firebase {
namespace firestore {
namespace csharp {

void SnapshotsInSyncListener::OnEvent(Error error) { callback_(callback_id_); }

/* static */
ListenerRegistration SnapshotsInSyncListener::AddListenerTo(
    Firestore* firestore, int32_t callback_id,
    SnapshotsInSyncCallback callback) {
  SnapshotsInSyncListener listener(callback_id, callback);

  return firestore->AddSnapshotsInSyncListener(
      [listener]() mutable { listener.OnEvent(Error::kErrorOk); });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
