#include "firebase/csharp/query_event_listener.h"

#include "app/src/assert.h"
#include "firebase/firestore/query.h"

namespace firebase {
namespace firestore {
namespace csharp {

void QueryEventListener::OnEvent(const QuerySnapshot& value, Error error) {
  // Ownership of this pointer is passed into the C# handler
  auto* copy = new QuerySnapshot(value);

  callback_(callback_id_, copy, error);
}

/* static */
ListenerRegistration QueryEventListener::AddListenerTo(
    Query* query, MetadataChanges metadata_changes, int32_t callback_id,
    QueryEventListenerCallback callback) {
  QueryEventListener listener(callback_id, callback);

  return query->AddSnapshotListener(
      metadata_changes,
      [listener](const QuerySnapshot& value, Error error) mutable {
        listener.OnEvent(value, error);
      });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
