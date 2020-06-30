#include "firebase/csharp/document_event_listener.h"

#include "app/src/assert.h"
#include "firebase/firestore/document_reference.h"

namespace firebase {
namespace firestore {
namespace csharp {

void DocumentEventListener::OnEvent(const DocumentSnapshot& value,
                                    Error error) {
  // Ownership of this pointer is passed into the C# handler
  auto* copy = new DocumentSnapshot(value);

  callback_(callback_id_, copy, error);
}

/* static */
ListenerRegistration DocumentEventListener::AddListenerTo(
    DocumentReference* reference, MetadataChanges metadata_changes,
    int32_t callback_id, DocumentEventListenerCallback callback) {
  DocumentEventListener listener(callback_id, callback);

  return reference->AddSnapshotListener(
      metadata_changes,
      [listener](const DocumentSnapshot& value, Error error) mutable {
        listener.OnEvent(value, error);
      });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
