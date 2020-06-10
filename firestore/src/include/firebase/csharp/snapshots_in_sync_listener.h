#ifndef FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_SNAPSHOTS_IN_SYNC_LISTENER_H_
#define FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_SNAPSHOTS_IN_SYNC_LISTENER_H_

#include <cstdint>

#include "app/src/callback.h"
#include "firebase/firestore.h"
#include "firebase/firestore/event_listener.h"
#include "firebase/firestore/listener_registration.h"
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
// callbacks.
typedef void(SWIGSTDCALL* SnapshotsInSyncCallback)(int callback_id);

// Provides a C++ implementation of the EventListener for
// ListenForSnapshotsInSync that can forward the calls back to the C# delegates.
class SnapshotsInSyncListener : public EventListener<void> {
 public:
  SnapshotsInSyncListener(int32_t callback_id,
                          SnapshotsInSyncCallback callback)
      : callback_id_(callback_id), callback_(callback) {}

  void OnEvent(Error error) override;

  // This method is a proxy to Firestore::AddSnapshotsInSyncListener()
  // that can be easily called from C#. It allows our C# wrapper to
  // track user callbacks in a dictionary keyed off of a unique int
  // for each user callback and then raise the correct one later.
  static ListenerRegistration AddListenerTo(Firestore* firestore,
                                            int32_t callback_id,
                                            SnapshotsInSyncCallback callback);

 private:
  int32_t callback_id_;
  SnapshotsInSyncCallback callback_;
};

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase

#endif  // FIREBASE_FIRESTORE_CLIENT_CPP_SRC_INCLUDE_FIREBASE_CSHARP_SNAPSHOTS_IN_SYNC_LISTENER_H_
