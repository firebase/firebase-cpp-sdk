#include "firebase/csharp/snapshots_in_sync_listener.h"

#include "app/src/callback.h"

namespace firebase {
namespace firestore {
namespace csharp {

namespace {

class ListenerCallback {
 public:
  ListenerCallback(SnapshotsInSyncCallback callback, int32_t callback_id)
      : callback_(callback), callback_id_(callback_id) {}

  static void Run(ListenerCallback* listener_callback) {
    listener_callback->Run();
  }

 private:
  void Run() { callback_(callback_id_); }

  SnapshotsInSyncCallback callback_ = nullptr;
  int32_t callback_id_ = -1;
};

}  // namespace

ListenerRegistration AddSnapshotsInSyncListener(
    Firestore* firestore, int32_t callback_id,
    SnapshotsInSyncCallback callback) {
  auto snapshots_in_sync_listener = [callback, callback_id] {
    ListenerCallback listener_callback(callback, callback_id);
    auto* callback = new callback::CallbackMoveValue1<ListenerCallback>(
        std::move(listener_callback), ListenerCallback::Run);
    callback::AddCallback(callback);
  };
  return firestore->AddSnapshotsInSyncListener(snapshots_in_sync_listener);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
