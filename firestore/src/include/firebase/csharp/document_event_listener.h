#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_DOCUMENT_EVENT_LISTENER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_DOCUMENT_EVENT_LISTENER_H_

#include "firebase/firestore/document_snapshot.h"
#include "firebase/firestore/listener_registration.h"
#include "firebase/firestore/metadata_changes.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {
namespace csharp {

// Add this to make this header compile when SWIG is not involved.
#ifndef SWIGSTDCALL
#if !defined(SWIG) && \
    (defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__))
#define SWIGSTDCALL __stdcall
#else
#define SWIGSTDCALL
#endif
#endif

// The callbacks that are used by the listener, that need to reach back to C#
// callbacks. The error_message pointer is only valid for the duration of the
// callback.
typedef void(SWIGSTDCALL* DocumentEventListenerCallback)(
    int callback_id, DocumentSnapshot* snapshot, Error error_code,
    const char* error_message);

// This method is a proxy to DocumentReference::AddSnapshotListener()
// that can be easily called from C#. It allows our C# wrapper to
// track user callbacks in a dictionary keyed off of a unique int
// for each user callback and then raise the correct one later.
ListenerRegistration AddDocumentSnapshotListener(
    DocumentReference* reference, MetadataChanges metadata_changes,
    int32_t callback_id, DocumentEventListenerCallback callback);

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_DOCUMENT_EVENT_LISTENER_H_
