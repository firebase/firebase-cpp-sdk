#include "firebase/csharp/document_event_listener.h"

#include "app/src/assert.h"
#include "firebase/firestore/document_reference.h"

namespace firebase {
namespace firestore {
namespace csharp {

::firebase::Mutex DocumentEventListener::g_mutex;
DocumentEventListenerCallback
    DocumentEventListener::g_document_snapshot_event_listener_callback =
        nullptr;

void DocumentEventListener::OnEvent(const DocumentSnapshot& value,
                                    Error error) {
  MutexLock lock(g_mutex);
  if (g_document_snapshot_event_listener_callback) {
    firebase::callback::AddCallback(firebase::callback::NewCallback(
        DocumentSnapshotEvent, callback_id_, value, error));
  }
}

/* static */
void DocumentEventListener::SetCallback(
    DocumentEventListenerCallback callback) {
  MutexLock lock(g_mutex);
  if (!callback) {
    g_document_snapshot_event_listener_callback = nullptr;
    return;
  }

  if (g_document_snapshot_event_listener_callback) {
    FIREBASE_ASSERT(g_document_snapshot_event_listener_callback == callback);
  } else {
    g_document_snapshot_event_listener_callback = callback;
  }
}

/* static */
ListenerRegistration DocumentEventListener::AddListenerTo(
    int32_t callback_id, DocumentReference reference,
    MetadataChanges metadata_changes) {
  DocumentEventListener listener(callback_id);
  // TODO(zxu): For now, we call the one with lambda instead of EventListener
  // pointer so we do not have to manage the ownership of the listener. We
  // might want to extend the design to manage listeners and call the one with
  // EventListener parameter e.g. adding extra parameter in the API to specify
  // whether ownership is transferred.
  return reference.AddSnapshotListener(
      metadata_changes,
      [listener](const DocumentSnapshot& value, Error error) mutable {
        listener.OnEvent(value, error);
      });
}

/* static */
void DocumentEventListener::DocumentSnapshotEvent(int callback_id,
                                                  DocumentSnapshot value,
                                                  Error error) {
  MutexLock lock(g_mutex);
  if (g_document_snapshot_event_listener_callback == nullptr) {
    return;
  }
  // The ownership is passed through the call to C# handler.
  DocumentSnapshot* copy = new DocumentSnapshot(value);
  g_document_snapshot_event_listener_callback(callback_id, copy, error);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
