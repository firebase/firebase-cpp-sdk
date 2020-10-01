#include "firebase/csharp/query_event_listener.h"

#include "firebase/firestore/query.h"

namespace firebase {
namespace firestore {
namespace csharp {

ListenerRegistration AddQuerySnapshotListener(
    Query* query, MetadataChanges metadata_changes, int32_t callback_id,
    QueryEventListenerCallback callback) {
  return query->AddSnapshotListener(
      metadata_changes,
      [callback, callback_id](const QuerySnapshot& value, Error error_code,
                              const std::string& error_message) {
        // Ownership of the QuerySnapshot pointer is passed to C#.
        callback(callback_id, new QuerySnapshot(value), error_code,
                 error_message.c_str());
      });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
