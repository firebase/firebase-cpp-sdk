#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_

#include "firebase/firestore/listener_registration.h"
#include "firebase/firestore/query_snapshot.h"
#include "firebase/firestore/firestore_errors.h"

namespace firebase {
namespace firestore {
namespace csharp {

// Add this to make this header compile when SWIG is not involved.
#ifndef SWIGSTDCALL
#if defined(_WIN32) || defined(__WIN32__) || defined(__CYGWIN__)
#define SWIGSTDCALL __stdcall
#else
#define SWIGSTDCALL
#endif
#endif

// The callbacks that are used by the listener, that need to reach back to C#
// callbacks. The error_message pointer is only valid for the duration of the
// callback.
typedef void(SWIGSTDCALL* QueryEventListenerCallback)(
    int32_t callback_id, QuerySnapshot* snapshot, Error error_code,
    const char* error_message);

// This method is a proxy to Query::AddSnapshotsListener()
// that can be easily called from C#. It allows our C# wrapper to
// track user callbacks in a dictionary keyed off of a unique int
// for each user callback and then raise the correct one later.
ListenerRegistration AddQuerySnapshotListener(
    Query* query, MetadataChanges metadata_changes, int32_t callback_id,
    QueryEventListenerCallback callback);

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_
