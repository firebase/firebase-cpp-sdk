#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_

#include <cstdint>

#include "app/src/callback.h"
#include "app/src/mutex.h"
#include "firebase/firestore/event_listener.h"
#include "firebase/firestore/listener_registration.h"
#include "firebase/firestore/metadata_changes.h"
#include "firebase/firestore/query_snapshot.h"

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
// callbacks.
typedef void(SWIGSTDCALL* QueryEventListenerCallback)(int callback_id,
                                                      void* snapshot,
                                                      Error error);

// Provide a C++ implementation of the EventListener for QuerySnapshot that
// can forward the calls back to the C# delegates.
class QueryEventListener : public EventListener<QuerySnapshot> {
 public:
  explicit QueryEventListener(int32_t callback_id,
                              QueryEventListenerCallback callback)
      : callback_id_(callback_id), callback_(callback) {}

  void OnEvent(const QuerySnapshot& value, Error error) override;

  // This method is a proxy to Query::AddSnapshotsListener()
  // that can be easily called from C#. It allows our C# wrapper to
  // track user callbacks in a dictionary keyed off of a unique int
  // for each user callback and then raise the correct one later.
  static ListenerRegistration AddListenerTo(
      Query* query, MetadataChanges metadata_changes, int32_t callback_id,
      QueryEventListenerCallback callback);

 private:
  int32_t callback_id_;
  QueryEventListenerCallback callback_;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_
