// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_REPO_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_REPO_H_

#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/safe_reference.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/event_registration.h"
#include "database/src/desktop/core/sparse_snapshot_tree.h"
#include "database/src/desktop/core/sync_tree.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/transaction_data.h"
#include "database/src/desktop/view/event.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/transaction.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;
class EventRegistration;

class Repo : public connection::PersistentConnectionEventHandler {
 public:
  typedef firebase::internal::SafeReference<Repo> ThisRef;
  typedef firebase::internal::SafeReferenceLock<Repo> ThisRefLock;

  Repo(App* app, DatabaseInternal* database, const char* url, Logger* logger,
       bool persistence_enabled);

  ~Repo() override;

  connection::PersistentConnection* connection() { return connection_.get(); }

  void AddEventCallback(UniquePtr<EventRegistration> event_registration);

  void RemoveEventCallback(void* listener_ptr, const QuerySpec& query_spec);

  void OnDisconnectSetValue(const SafeFutureHandle<void>& handle,
                            ReferenceCountedFutureImpl* ref_future,
                            const Path& path, const Variant& data);

  void OnDisconnectCancel(const SafeFutureHandle<void>& handle,
                          ReferenceCountedFutureImpl* ref_future,
                          const Path& path);

  void OnDisconnectUpdate(const SafeFutureHandle<void>& handle,
                          ReferenceCountedFutureImpl* ref_future,
                          const Path& path, const Variant& data);

  void PurgeOutstandingWrites();

  void SetValue(const Path& path, const Variant& data,
                ReferenceCountedFutureImpl* api, SafeFutureHandle<void> handle);

  void UpdateChildren(const Path& path, const Variant& data,
                      ReferenceCountedFutureImpl* api,
                      SafeFutureHandle<void> handle);

  void AckWriteAndRerunTransactions(WriteId write_id, const Path& path,
                                    Error error);

  void PostEvents(const std::vector<Event>& events);

  void SetKeepSynchronized(const QuerySpec& query_spec, bool keep_synchronized);

  void StartTransaction(const Path& path,
                        DoTransactionWithContext transaction_function,
                        void* context, void (*delete_context)(void*),
                        bool trigger_local_events,
                        ReferenceCountedFutureImpl* api,
                        SafeFutureHandle<DataSnapshot> handle);

  SyncTree* server_sync_tree() { return server_sync_tree_.get(); }

  void OnConnect() override;

  void OnDisconnect() override;

  void OnAuthStatus(bool auth_ok) override;

  void OnServerInfoUpdate(const std::string& key, const Variant& value);

  void OnServerInfoUpdate(const std::map<Variant, Variant>& updates) override;

  void OnDataUpdate(const Path& path, const Variant& payload_data,
                    bool is_merge, const Tag& tag) override;

  const std::string& url() const { return url_; }

  static scheduler::Scheduler& scheduler() { return *s_scheduler_; }

  ThisRef& this_ref() { return safe_this_; }

 private:
  WriteId GetNextWriteId();

  Path AbortTransactions(const Path& path, Error reason);

  void AbortTransactionsAtNode(Tree<std::vector<TransactionDataPtr>>* node,
                               Error reason);

  // Defers any initialization that is potentially expensive (e.g. disk access)
  // and must be run on the run loop
  void DeferredInitialization();

  Path RerunTransactions(const Path& changed_path);

  void SendAllReadyTransactions();

  void SendReadyTransactions(Tree<std::vector<TransactionDataPtr>>* node);

  void SendTransactionQueue(const std::vector<TransactionDataPtr>& queue,
                            const Path& path);

  void HandleTransactionResponse(const connection::ResponsePtr& ptr);

  void RerunTransactionQueue(const std::vector<TransactionDataPtr>& queue,
                             const Path& path);

  void PruneCompletedTransactions(Tree<std::vector<TransactionDataPtr>>* node);

  Tree<std::vector<TransactionDataPtr>>* GetAncestorTransactionNode(Path path);

  std::vector<TransactionDataPtr> BuildTransactionQueue(
      Tree<std::vector<TransactionDataPtr>>* transactions);

  void AggregateTransactionQueues(std::vector<TransactionDataPtr>* queue,
                                  Tree<std::vector<TransactionDataPtr>>* node);

  Variant GetLatestState(const Path& path) {
    return GetLatestState(path, std::vector<WriteId>());
  }

  Variant GetLatestState(const Path& path, std::vector<WriteId> sets_to_ignore);

  void RunOnDisconnectEvents();

  void UpdateInfo(const std::string& key, const Variant& value);

  DatabaseInternal* database_;

  SparseSnapshotTree on_disconnect_;

  // The scheduler shared with every DatabaseInternal. It is designed to
  // out-live any class which is using it, so that it is safe to use even in
  // destructor.
  static scheduler::Scheduler* s_scheduler_;

  // Caches information about the connection to the host.
  connection::HostInfo host_info_;

  // The database URL. A cached version of host_info_.ToString().
  std::string url_;

  bool persistence_enabled_;

  // Firebase websocket connection with wire protocol support
  UniquePtr<connection::PersistentConnection> connection_;

  UniquePtr<SyncTree> info_sync_tree_;

  UniquePtr<SyncTree> server_sync_tree_;

  Variant info_data_;

  int64_t server_time_offset_;

  WriteId next_write_id_;

  Tree<std::vector<TransactionDataPtr>> transaction_queue_tree_;

  // Safe reference to this.  Set in constructor and cleared in destructor
  // Should be safe to be copied to any thread.
  ThisRef safe_this_;

  Logger* logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_REPO_H_
