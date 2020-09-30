#include "firebase/csharp/document_event_listener.h"

#include "firebase/firestore/document_reference.h"

namespace firebase {
namespace firestore {
namespace csharp {

ListenerRegistration AddDocumentSnapshotListener(
    DocumentReference* reference, MetadataChanges metadata_changes,
    int32_t callback_id, DocumentEventListenerCallback callback) {
  return reference->AddSnapshotListener(
      metadata_changes,
      [callback, callback_id](const DocumentSnapshot& value, Error error_code,
                              const std::string& error_message) {
        // Ownership of the DocumentSnapshot pointer is passed to C#.
        callback(callback_id, new DocumentSnapshot(value), error_code,
                 error_message.c_str());
      });
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
