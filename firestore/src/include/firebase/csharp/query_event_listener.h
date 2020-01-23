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
                                                      void* snapshot);

// Provide a C++ implementation of the EventListener for QuerySnapshot that
// can forward the calls back to the C# delegates.
class QueryEventListener : public EventListener<QuerySnapshot> {
 public:
  QueryEventListener(int32_t callback_id) : callback_id_(callback_id) {}

  ~QueryEventListener() override {}

  void OnEvent(const QuerySnapshot& value, Error error) override;

  static void SetCallback(QueryEventListenerCallback callback);

  static ListenerRegistration AddListenerTo(int32_t callback_id, Query query,
                                            MetadataChanges metadataChanges);

 private:
  static void QuerySnapshotEvent(int callback_id, QuerySnapshot value,
                                 Error error);

  int32_t callback_id_;

  // These static variables are named as global variable instead of private
  // class member.
  static Mutex g_mutex;
  static QueryEventListenerCallback g_query_snapshot_event_listener_callback;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_QUERY_EVENT_LISTENER_H_
