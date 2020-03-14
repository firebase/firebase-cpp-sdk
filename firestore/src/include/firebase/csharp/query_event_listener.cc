#include "firebase/csharp/query_event_listener.h"

#include "app/src/assert.h"
#include "firebase/firestore/query.h"

namespace firebase {
namespace firestore {
namespace csharp {

::firebase::Mutex QueryEventListener::g_mutex;
QueryEventListenerCallback
    QueryEventListener::g_query_snapshot_event_listener_callback = nullptr;

void QueryEventListener::OnEvent(const QuerySnapshot& value, Error error) {
  MutexLock lock(g_mutex);
  if (g_query_snapshot_event_listener_callback) {
    firebase::callback::AddCallback(firebase::callback::NewCallback(
        QuerySnapshotEvent, callback_id_, value, error));
  }
}

/* static */
void QueryEventListener::SetCallback(QueryEventListenerCallback callback) {
  MutexLock lock(g_mutex);
  if (!callback) {
    g_query_snapshot_event_listener_callback = nullptr;
    return;
  }

  if (g_query_snapshot_event_listener_callback) {
    FIREBASE_ASSERT(g_query_snapshot_event_listener_callback == callback);
  } else {
    g_query_snapshot_event_listener_callback = callback;
  }
}

/* static */
ListenerRegistration QueryEventListener::AddListenerTo(
    int32_t callback_id, Query query, MetadataChanges metadata_changes) {
  QueryEventListener listener(callback_id);
  // TODO(zxu): For now, we call the one with lambda instead of EventListener
  // pointer so we do not have to manage the ownership of the listener. We
  // might want to extend the design to manage listeners and call the one with
  // EventListener parameter e.g. adding extra parameter in the API to specify
  // whether ownership is transferred.
  return query.AddSnapshotListener(
      metadata_changes,
      [listener](const QuerySnapshot& value, Error error) mutable {
        listener.OnEvent(value, error);
      });
}

/* static */
void QueryEventListener::QuerySnapshotEvent(int callback_id,
                                            QuerySnapshot value, Error error) {
  MutexLock lock(g_mutex);
  if (error != Ok || g_query_snapshot_event_listener_callback == nullptr) {
    return;
  }
  // The ownership is passed through the call to C# handler.
  QuerySnapshot* copy = new QuerySnapshot(value);
  g_query_snapshot_event_listener_callback(callback_id, copy);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
