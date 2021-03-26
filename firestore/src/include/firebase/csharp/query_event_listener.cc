#include "firebase/csharp/query_event_listener.h"

#include <memory>
#include <string>
#include <utility>

#include "app/src/callback.h"
#include "firebase/firestore/query.h"

namespace firebase {
namespace firestore {
namespace csharp {

namespace {

class ListenerCallback {
 public:
  ListenerCallback(QueryEventListenerCallback callback, int32_t callback_id,
                   std::unique_ptr<QuerySnapshot> snapshot, Error error_code,
                   std::string error_message)
      : callback_(callback),
        callback_id_(callback_id),
        snapshot_(std::move(snapshot)),
        error_code_(error_code),
        error_message_(std::move(error_message)) {}

  static void Run(ListenerCallback* listener_callback) {
    listener_callback->Run();
  }

 private:
  void Run() {
    // Ownership of the QuerySnapshot pointer is passed to C#.
    callback_(callback_id_, snapshot_.release(), error_code_,
              error_message_.c_str());
  }

  QueryEventListenerCallback callback_ = nullptr;
  int32_t callback_id_ = -1;
  std::unique_ptr<QuerySnapshot> snapshot_;
  Error error_code_ = Error::kErrorUnknown;
  std::string error_message_;
};

}  // namespace

ListenerRegistration AddQuerySnapshotListener(
    Query* query, MetadataChanges metadata_changes, int32_t callback_id,
    QueryEventListenerCallback callback) {
  auto snapshot_listener = [callback, callback_id](
                               const QuerySnapshot& snapshot, Error error_code,
                               const std::string& error_message) {
    // NOLINTNEXTLINE(modernize-make-unique)
    std::unique_ptr<QuerySnapshot> snapshot_ptr(new QuerySnapshot(snapshot));
    ListenerCallback listener_callback(callback, callback_id,
                                       std::move(snapshot_ptr), error_code,
                                       error_message);
    auto* callback = new callback::CallbackMoveValue1<ListenerCallback>(
        std::move(listener_callback), ListenerCallback::Run);
    callback::AddCallback(callback);
  };
  return query->AddSnapshotListener(metadata_changes, snapshot_listener);
}

}  // namespace csharp
}  // namespace firestore
}  // namespace firebase
