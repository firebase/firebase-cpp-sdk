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

#include "database/src/desktop/core/repo.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <string>
#include <utility>

#include "app/src/callback.h"
#include "app/src/filesystem.h"
#include "app/src/function_registry.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/log.h"
#include "app/src/scheduler.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/connection/persistent_connection.h"
#include "database/src/desktop/core/constants.h"
#include "database/src/desktop/core/info_listen_provider.h"
#include "database/src/desktop/core/server_values.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/core/value_event_registration.h"
#include "database/src/desktop/core/web_socket_listen_provider.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/desktop/mutable_data_desktop.h"
#include "database/src/desktop/persistence/level_db_persistence_storage_engine.h"
#include "database/src/desktop/persistence/noop_persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/desktop/query_desktop.h"
#include "database/src/desktop/transaction_data.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/include/firebase/database/common.h"
#include "database/src/include/firebase/database/transaction.h"

namespace firebase {

using callback::NewCallback;

namespace database {
namespace internal {

static Mutex g_scheduler_mutex;  // NOLINT
static int g_scheduler_ref_count = 0;
scheduler::Scheduler* Repo::s_scheduler_;

namespace {
class RepoResponseBase : public connection::Response {
 public:
  RepoResponseBase(const Repo::ThisRef& repo, DatabaseInternal* db,
                   const Path& path, ResponseCallback callback)
      : connection::Response(callback),
        repo_ref_(repo),
        db_ref_(db ? db->this_ref() : DatabaseInternal::ThisRef(nullptr)),
        has_db_(db != nullptr),
        path_(path) {}

  Repo::ThisRef& repo_ref() { return repo_ref_; }
  const Path& path() const { return path_; }

 protected:
  bool IsDbValid() {
    if (!has_db_) return true;
    DatabaseInternal::ThisRefLock lock(&db_ref_);
    return lock.GetReference() != nullptr;
  }

  Repo::ThisRef repo_ref_;
  DatabaseInternal::ThisRef db_ref_;
  bool has_db_;
  Path path_;
  bool completed_ = false;
};

class TransactionResponse : public RepoResponseBase {
 public:
  TransactionResponse(const Repo::ThisRef& repo, DatabaseInternal* db,
                      const Path& path, std::vector<TransactionDataPtr> queue,
                      ResponseCallback callback)
      : RepoResponseBase(repo, db, path, callback), queue_(std::move(queue)) {}

  ~TransactionResponse() override { FailUncompletedTransactions(); }
  void MarkHandled() { completed_ = true; }

  void FailUncompletedTransactions() {
    if (completed_) return;
    completed_ = true;
    if (!IsDbValid()) return;
    for (auto& transaction : queue_) {
      if (transaction &&
          transaction->status != TransactionData::kStatusComplete &&
          transaction->ref_future) {
        transaction->status = TransactionData::kStatusComplete;
        transaction->ref_future->Complete(
            transaction->future_handle, kErrorUnknownError,
            "The database is being destroyed.");
      }
    }
  }

  std::vector<TransactionDataPtr>& queue() { return queue_; }

 private:
  std::vector<TransactionDataPtr> queue_;
};

class OnDisconnectResponse : public RepoResponseBase {
 public:
  OnDisconnectResponse(const Repo::ThisRef& repo_ref, DatabaseInternal* db,
                       const SafeFutureHandle<void>& handle,
                       ReferenceCountedFutureImpl* ref_future, const Path& path,
                       const Variant& data, ResponseCallback callback)
      : RepoResponseBase(repo_ref, db, path, callback),
        handle_(handle),
        ref_future_(ref_future),
        data_(data) {
    assert(ref_future != nullptr);
  }

  ~OnDisconnectResponse() override {
    if (!completed_ && IsDbValid() && ref_future_) {
      ref_future_->Complete(handle_, kErrorUnknownError,
                            "The database is being destroyed.");
    }
  }

  void MarkComplete() {
    completed_ = true;
    if (IsDbValid() && ref_future_) {
      if (!HasError()) {
        ref_future_->Complete(handle_, kErrorNone);
      } else {
        ref_future_->Complete(handle_, GetErrorCode(),
                              GetErrorMessage().c_str());
      }
    }
  }

  const Variant& data() const { return data_; }

 private:
  SafeFutureHandle<void> handle_;
  ReferenceCountedFutureImpl* ref_future_;
  Variant data_;
};

class SetValueResponse : public RepoResponseBase {
 public:
  SetValueResponse(const Repo::ThisRef& repo, DatabaseInternal* db,
                   const Path& path, WriteId write_id,
                   ReferenceCountedFutureImpl* api,
                   SafeFutureHandle<void> handle)
      : RepoResponseBase(
            repo, db, path,
            [](const connection::ResponsePtr& ptr) {
              auto* response = static_cast<SetValueResponse*>(ptr.get());
              Repo::ThisRefLock lock(&response->repo_ref());
              if (Repo* r = lock.GetReference()) {
                r->AckWriteAndRerunTransactions(response->write_id(),
                                                response->path(),
                                                response->GetErrorCode());
              }
              response->MarkComplete();
            }),
        write_id_(write_id),
        api_(api),
        handle_(handle) {}

  ~SetValueResponse() override {
    if (!completed_ && IsDbValid() && api_) {
      api_->Complete(handle_, kErrorUnknownError,
                     "The database is being destroyed.");
    }
  }

  void MarkComplete() {
    completed_ = true;
    if (IsDbValid() && api_) {
      api_->Complete(handle_, GetErrorCode(),
                     ::firebase::database::GetErrorMessage(GetErrorCode()));
    }
  }

  WriteId write_id() const { return write_id_; }

 private:
  WriteId write_id_;
  ReferenceCountedFutureImpl* api_;
  SafeFutureHandle<void> handle_;
};

class NoopListener : public ValueListener {
 public:
  ~NoopListener() override = default;
  void OnValueChanged(const DataSnapshot&) override {}
  void OnCancelled(const Error&, const char*) override {}
};

struct FutureToComplete {
  TransactionDataPtr transaction;
  Error abort_reason;
  Variant node;
};
}  // namespace

Repo::Repo(App* app, DatabaseInternal* database, const char* url,
           Logger* logger, bool persistence_enabled)
    : database_(database),
      persistence_enabled_(persistence_enabled),
      safe_this_(this),
      logger_(logger) {
  {
    MutexLock lock(g_scheduler_mutex);
    g_scheduler_ref_count++;
    if (s_scheduler_ == nullptr) s_scheduler_ = new scheduler::Scheduler();
  }

  ParseUrl parser;
  if (!url || parser.Parse(url) != ParseUrl::kParseOk) {
    logger_->LogError("Database Url is not valid: %s", url ? url : "(null)");
  } else {
    host_info_ = connection::HostInfo(parser.hostname.c_str(),
                                      parser.ns.c_str(), parser.secure);
    url_ = host_info_.ToString();
  }

  connection_ = std::make_unique<connection::PersistentConnection>(
      app, host_info_, this, s_scheduler_, logger_);
  s_scheduler_->Schedule(NewCallback(
      [](ThisRef ref) {
        ThisRefLock lock(&ref);
        if (Repo* r = lock.GetReference()) r->DeferredInitialization();
      },
      safe_this_));

  if (!url_.empty()) connection_->ScheduleInitialize();
}

Repo::~Repo() {
  safe_this_.ClearReference();
  if (database_ && database_->GetApp()) {
    database_->GetApp()->function_registry()->CallFunction(
        ::firebase::internal::FnAppCheckRemoveListener, database_->GetApp(),
        reinterpret_cast<void*>(OnAppCheckTokenChanged), this);
  }
  connection_.reset();
  {
    MutexLock lock(g_scheduler_mutex);
    if (g_scheduler_ref_count && --g_scheduler_ref_count == 0) {
      delete s_scheduler_;
      s_scheduler_ = nullptr;
    }
  }
}

void Repo::AddEventCallback(
    std::unique_ptr<EventRegistration> event_registration) {
  SyncTree* tree =
      StringStartsWith(event_registration->query_spec().path.str(), kDotInfo)
          ? info_sync_tree_.get()
          : server_sync_tree_.get();
  if (tree) PostEvents(tree->AddEventRegistration(std::move(event_registration)));
}

void Repo::RemoveEventCallback(void* listener_ptr,
                               const QuerySpec& query_spec) {
  SyncTree* tree = StringStartsWith(query_spec.path.str(), kDotInfo)
                       ? info_sync_tree_.get()
                       : server_sync_tree_.get();
  if (tree) {
    PostEvents(
        tree->RemoveEventRegistration(query_spec, listener_ptr, kErrorNone));
  }
}

void Repo::OnDisconnectSetValue(const SafeFutureHandle<void>& handle,
                                ReferenceCountedFutureImpl* ref_future,
                                const Path& path, const Variant& data) {
  connection::ResponsePtr response = std::make_shared<OnDisconnectResponse>(
      safe_this_, database_, handle, ref_future, path, data,
      [](const connection::ResponsePtr& ptr) {
        auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
        if (!resp->HasError()) {
          ThisRefLock lock(&resp->repo_ref());
          if (Repo* r = lock.GetReference()) {
            r->on_disconnect_.Remember(resp->path(), resp->data());
          }
        }
        resp->MarkComplete();
      });

  Repo::scheduler().Schedule(NewCallback(
      [](ThisRef ref, connection::ResponsePtr ptr) {
        ThisRefLock lock(&ref);
        if (Repo* r = lock.GetReference(); r && r->connection()) {
          auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
          r->connection()->OnDisconnectPut(resp->path(), resp->data(), ptr);
        }
      },
      safe_this_, response));
}

void Repo::OnDisconnectCancel(const SafeFutureHandle<void>& handle,
                              ReferenceCountedFutureImpl* ref_future,
                              const Path& path) {
  connection::ResponsePtr response = std::make_shared<OnDisconnectResponse>(
      safe_this_, database_, handle, ref_future, path, Variant::Null(),
      [](const connection::ResponsePtr& ptr) {
        auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
        if (!resp->HasError()) {
          ThisRefLock lock(&resp->repo_ref());
          if (Repo* r = lock.GetReference()) {
            r->on_disconnect_.Forget(resp->path());
          }
        }
        resp->MarkComplete();
      });

  Repo::scheduler().Schedule(NewCallback(
      [](ThisRef ref, connection::ResponsePtr ptr) {
        ThisRefLock lock(&ref);
        if (Repo* r = lock.GetReference(); r && r->connection()) {
          auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
          r->connection()->OnDisconnectCancel(resp->path(), ptr);
        }
      },
      safe_this_, response));
}

void Repo::OnDisconnectUpdate(const SafeFutureHandle<void>& handle,
                              ReferenceCountedFutureImpl* ref_future,
                              const Path& path, const Variant& data) {
  connection::ResponsePtr response = std::make_shared<OnDisconnectResponse>(
      safe_this_, database_, handle, ref_future, path, data,
      [](const connection::ResponsePtr& ptr) {
        auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
        if (!resp->HasError()) {
          ThisRefLock lock(&resp->repo_ref());
          if (Repo* r = lock.GetReference(); r && resp->data().is_map()) {
            for (const auto& [key, val] : resp->data().map()) {
              r->on_disconnect_.Remember(
                  resp->path().GetChild(key.AsString().string_value()), val);
            }
          }
        }
        resp->MarkComplete();
      });

  Repo::scheduler().Schedule(NewCallback(
      [](ThisRef ref, connection::ResponsePtr ptr) {
        ThisRefLock lock(&ref);
        if (Repo* r = lock.GetReference(); r && r->connection()) {
          auto* resp = static_cast<OnDisconnectResponse*>(ptr.get());
          r->connection()->OnDisconnectMerge(resp->path(), resp->data(), ptr);
        }
      },
      safe_this_, response));
}

void Repo::PurgeOutstandingWrites() {
  if (server_sync_tree_) PostEvents(server_sync_tree_->RemoveAllWrites());
  AbortTransactions(Path(), kErrorWriteCanceled);
  if (connection_) connection_->PurgeOutstandingWrites();
}

void Repo::SetValue(const Path& path, const Variant& new_data_unresolved,
                    ReferenceCountedFutureImpl* api,
                    SafeFutureHandle<void> handle) {
  if (!server_sync_tree_ || !connection_) {
    if (api) {
      api->Complete(handle, kErrorUnknownError,
                    GetErrorMessage(kErrorUnknownError));
    }
    return;
  }
  Variant server_values = GenerateServerValues(server_time_offset_);
  Variant new_data =
      ResolveDeferredValueSnapshot(new_data_unresolved, server_values);
  WriteId write_id = GetNextWriteId();
  PostEvents(server_sync_tree_->ApplyUserOverwrite(path, new_data_unresolved,
                                                   new_data, write_id,
                                                   kOverwriteVisible, kPersist));
  connection_->Put(path, new_data_unresolved,
                   std::make_shared<SetValueResponse>(
                       safe_this_, database_, path, write_id, api, handle));
  RerunTransactions(AbortTransactions(path, kErrorOverriddenBySet));
}

void Repo::UpdateChildren(const Path& path, const Variant& data,
                          ReferenceCountedFutureImpl* api,
                          SafeFutureHandle<void> handle) {
  CompoundWrite updates = CompoundWrite::FromVariantMerge(data);
  if (updates.IsEmpty()) {
    api->Complete(handle, kErrorNone, "");
    return;
  }
  if (!server_sync_tree_ || !connection_) {
    if (api) {
      api->Complete(handle, kErrorUnknownError,
                    GetErrorMessage(kErrorUnknownError));
    }
    return;
  }
  Variant server_values = GenerateServerValues(server_time_offset_);
  CompoundWrite resolved = ResolveDeferredValueMerge(updates, server_values);
  WriteId write_id = GetNextWriteId();
  PostEvents(server_sync_tree_->ApplyUserMerge(path, updates, resolved,
                                               write_id, kPersist));
  connection_->Merge(path, data,
                     std::make_shared<SetValueResponse>(
                         safe_this_, database_, path, write_id, api, handle));
  updates.write_tree().CallOnEach(
      Path(), [this, &path](const Path& rel_path, const Variant&) {
        RerunTransactions(AbortTransactions(path.GetChild(rel_path),
                                            kErrorOverriddenBySet));
      });
}

void Repo::AckWriteAndRerunTransactions(WriteId write_id, const Path& path,
                                        Error error) {
  if (error == kErrorWriteCanceled) return;
  AckStatus ack_status = (error == kErrorNone) ? kAckConfirm : kAckRevert;
  std::vector<Event> events = server_sync_tree_->AckUserWrite(
      write_id, ack_status, kPersist, server_time_offset_);
  if (!events.empty()) RerunTransactions(path);
  PostEvents(events);
}

static std::unique_ptr<PersistenceManagerInterface> CreatePersistenceManager(
    const char* app_data_path, LoggerBase* logger) {
  static const uint64_t kDefaultCacheSize = 10 * 1024 * 1024;
  auto engine = std::make_unique<LevelDbPersistenceStorageEngine>(logger);
  if (!engine->Initialize(app_data_path)) {
    logger->LogError("Could not initialize persistence");
    return nullptr;
  }
  auto tracked_query_manager =
      std::make_unique<TrackedQueryManager>(engine.get(), logger);
  return std::make_unique<PersistenceManager>(
      std::move(engine), std::move(tracked_query_manager),
      std::make_unique<LRUCachePolicy>(kDefaultCacheSize), logger);
}

void Repo::DeferredInitialization() {
  if (database_ && database_->GetApp()) {
    database_->GetApp()->function_registry()->CallFunction(
        ::firebase::internal::FnAppCheckAddListener, database_->GetApp(),
        reinterpret_cast<void*>(OnAppCheckTokenChanged), this);
  }

  {
    std::unique_ptr<PersistenceManagerInterface> persistence_manager;
    if (persistence_enabled_) {
      const char* package_name =
          (database_ && database_->GetApp())
              ? database_->GetApp()->options().package_name()
              : nullptr;
      auto slash_pos = url_.find("//");
      if (!package_name) {
        logger_->LogError("Could not initialize persistence: No package_name.");
      } else if (url_.empty() || slash_pos == std::string::npos) {
        logger_->LogError("Could not initialize persistence: No database url.");
      } else {
        std::string database_path =
            std::string(package_name) + "/" + url_.substr(slash_pos + 2);
        std::string err;
        std::string app_data_path =
            AppDataDir(database_path.c_str(), true, &err);
        if (app_data_path.empty()) {
          logger_->LogError(
              "Could not initialize persistence: App data directory error: %s",
              err.c_str());
        } else {
          logger_->LogDebug("app_data_path: %s", app_data_path.c_str());
          persistence_manager =
              CreatePersistenceManager(app_data_path.c_str(), logger_);
        }
      }
    }
    if (!persistence_manager) {
      persistence_manager = std::make_unique<NoopPersistenceManager>();
    }
    auto listen_provider = std::make_unique<WebSocketListenProvider>(
        this, connection_.get(), logger_);
    auto* listen_provider_ptr = listen_provider.get();
    server_sync_tree_ = std::make_unique<SyncTree>(
        std::make_unique<WriteTree>(), std::move(persistence_manager),
        std::move(listen_provider));
    listen_provider_ptr->set_sync_tree(server_sync_tree_.get());
  }

  {
    auto listen_provider =
        std::make_unique<InfoListenProvider>(this, &info_data_);
    auto* listen_provider_ptr = listen_provider.get();
    info_sync_tree_ = std::make_unique<SyncTree>(
        std::make_unique<WriteTree>(),
        std::make_unique<NoopPersistenceManager>(),
        std::move(listen_provider));
    listen_provider_ptr->set_sync_tree(info_sync_tree_.get());
  }

  UpdateInfo(kDotInfoAuthenticated, false);
  UpdateInfo(kDotInfoConnected, false);
}

void Repo::PostEvents(const std::vector<Event>& events) {
  for (const Event& event : events) {
    if (event.type != kEventTypeError) {
      event.event_registration->SafelyFireEvent(event);
    } else {
      event.event_registration->SafelyFireCancelEvent(event.error);
    }
  }
}

void Repo::OnConnect() {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  OnServerInfoUpdate(kDotInfoConnected, true);
}

void Repo::OnDisconnect() {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  OnServerInfoUpdate(kDotInfoConnected, false);
  RunOnDisconnectEvents();
}

void Repo::RunOnDisconnectEvents() {
  Variant server_values = GenerateServerValues(server_time_offset_);
  SparseSnapshotTree resolved_tree =
      ResolveDeferredValueTree(on_disconnect_, server_values);
  std::vector<Event> events;
  resolved_tree.ForEachTree(
      Path(), [this, &events](const Path& prefix_path, const Variant& node) {
        Extend(&events,
               server_sync_tree_->ApplyServerOverwrite(prefix_path, node));
        RerunTransactions(
            AbortTransactions(prefix_path, kErrorOverriddenBySet));
      });
  on_disconnect_.Clear();
  PostEvents(events);
}

void Repo::OnAuthStatus(bool auth_ok) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  OnServerInfoUpdate(kDotInfoAuthenticated, auth_ok);
}

void Repo::OnServerInfoUpdate(const std::string& key, const Variant& value) {
  UpdateInfo(key, value);
}

void Repo::OnServerInfoUpdate(const std::map<Variant, Variant>& updates) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  for (const auto& [key, val] : updates) {
    UpdateInfo(key.AsString().mutable_string(), val);
  }
}

void Repo::OnDataUpdate(const Path& path, const Variant& data, bool is_merge,
                        const Tag& tag) {
  SAFE_REFERENCE_RETURN_VOID_IF_INVALID(ThisRefLock, lock, safe_this_);
  std::vector<Event> events;
  if (tag.has_value()) {
    events = is_merge ? server_sync_tree_->ApplyTaggedQueryMerge(
                            path, VariantToPathMap(data), tag)
                      : server_sync_tree_->ApplyTaggedQueryOverwrite(path, data,
                                                                     tag);
  } else {
    events = is_merge ? server_sync_tree_->ApplyServerMerge(
                            path, VariantToPathMap(data))
                      : server_sync_tree_->ApplyServerOverwrite(path, data);
  }
  if (!events.empty()) RerunTransactions(path);
  PostEvents(events);
}

void Repo::SetKeepSynchronized(const QuerySpec& query_spec,
                               bool keep_synchronized) {
  if (server_sync_tree_) {
    server_sync_tree_->SetKeepSynchronized(query_spec, keep_synchronized);
  }
}

void Repo::StartTransaction(const Path& path,
                            DoTransactionWithContext transaction_function,
                            void* context, void (*delete_context)(void*),
                            bool trigger_local_events,
                            ReferenceCountedFutureImpl* api,
                            SafeFutureHandle<DataSnapshot> handle) {
  if (!server_sync_tree_ || !connection_) {
    if (delete_context) delete_context(context);
    if (api) {
      api->Complete(handle, kErrorUnknownError,
                    GetErrorMessage(kErrorUnknownError));
    }
    return;
  }
  auto listener = std::make_unique<NoopListener>();
  NoopListener* listener_ptr = listener.get();
  QuerySpec query_spec(path);
  AddEventCallback(std::make_unique<ValueEventRegistration>(
      database_, listener_ptr, query_spec));

  auto transaction_data = std::make_shared<TransactionData>(
      handle, api, query_spec.path, transaction_function, context,
      delete_context, trigger_local_events, std::move(listener));

  Variant current_state = GetLatestState(path);
  transaction_data->current_input_snapshot = current_state;
  auto* mutable_data_impl = new MutableDataInternal(database_, current_state);
  MutableData mutable_current(mutable_data_impl);

  if (transaction_function(&mutable_current, context) !=
      kTransactionResultSuccess) {
    transaction_data->current_output_snapshot_raw = Variant::Null();
    transaction_data->current_output_snapshot_resolved = Variant::Null();
    transaction_data->status = TransactionData::kStatusNeedsAbort;
    transaction_data->ref_future->Complete(transaction_data->future_handle,
                                           kErrorWriteCanceled);
    RemoveEventCallback(listener_ptr, query_spec);
    return;
  }

  transaction_data->status = TransactionData::kStatusRun;
  auto* queue_node = transaction_queue_tree_.GetOrMakeSubtree(path);
  if (!queue_node->value().has_value()) {
    queue_node->set_value(std::vector<TransactionDataPtr>());
  }
  queue_node->value()->push_back(transaction_data);

  Variant server_values = GenerateServerValues(server_time_offset_);
  const Variant* new_node_unresolved = mutable_data_impl->GetNode();
  Variant new_node_resolved =
      ResolveDeferredValueSnapshot(*new_node_unresolved, server_values);

  transaction_data->current_output_snapshot_raw = *new_node_unresolved;
  transaction_data->current_output_snapshot_resolved = new_node_resolved;
  transaction_data->current_write_id = GetNextWriteId();

  PostEvents(server_sync_tree_->ApplyUserOverwrite(
      path, *new_node_unresolved, new_node_resolved,
      transaction_data->current_write_id,
      trigger_local_events ? kOverwriteVisible : kOverwriteInvisible,
      kDoNotPersist));
  SendAllReadyTransactions();
}

void Repo::SendAllReadyTransactions() {
  PruneCompletedTransactions(&transaction_queue_tree_);
  SendReadyTransactions(&transaction_queue_tree_);
}

void Repo::SendReadyTransactions(Tree<std::vector<TransactionDataPtr>>* node) {
  if (node->value().has_value()) {
    auto queue = BuildTransactionQueue(node);
    assert(!queue.empty());
    bool all_run = std::all_of(
        queue.begin(), queue.end(), [](const TransactionDataPtr& tx) {
          return tx->status == TransactionData::kStatusRun;
        });
    if (all_run) SendTransactionQueue(queue, node->GetPath());
  } else {
    for (auto& child : node->children()) {
      SendReadyTransactions(&child.second);
    }
  }
}

Path Repo::AbortTransactions(const Path& path, Error reason) {
  Path affected_path = GetAncestorTransactionNode(path)->GetPath();
  auto* transaction_node = transaction_queue_tree_.GetOrMakeSubtree(path);
  transaction_node->CallOnEachAncestor(
      [this, reason](Tree<std::vector<TransactionDataPtr>>* tree) {
        AbortTransactionsAtNode(tree, reason);
        return false;
      });
  AbortTransactionsAtNode(transaction_node, reason);
  transaction_node->CallOnEachDescendant(
      [this, reason](Tree<std::vector<TransactionDataPtr>>* tree) {
        AbortTransactionsAtNode(tree, reason);
      });
  PruneCompletedTransactions(&transaction_queue_tree_);
  return affected_path;
}

void Repo::AbortTransactionsAtNode(Tree<std::vector<TransactionDataPtr>>* node,
                                   Error reason) {
  std::optional<std::vector<TransactionDataPtr>>& queue = node->value();
  if (!queue.has_value()) return;

  std::vector<Event> events;
  std::vector<FutureToComplete> futures_to_complete;
  Error abort_error = (reason == kErrorOverriddenBySet) ? kErrorOverriddenBySet
                                                        : kErrorWriteCanceled;
  FIREBASE_DEV_ASSERT_MESSAGE(
      reason == kErrorOverriddenBySet || reason == kErrorWriteCanceled,
      "Unknown transaction abort reason");

  int last_sent = -1;
  for (int i = 0; i < static_cast<int>(queue->size()); ++i) {
    const TransactionDataPtr& transaction = (*queue)[i];
    if (transaction->status == TransactionData::kStatusSentNeedsAbort) {
      FIREBASE_DEV_ASSERT_MESSAGE(
          last_sent == i - 1,
          "All sent items should be at beginning of queue.");
      last_sent = i;
    } else if (transaction->status == TransactionData::kStatusSent) {
      FIREBASE_DEV_ASSERT_MESSAGE(
          last_sent == i - 1,
          "All sent items should be at beginning of queue.");
      last_sent = i;
      transaction->status = TransactionData::kStatusSentNeedsAbort;
      transaction->abort_reason = abort_error;
    } else {
      FIREBASE_DEV_ASSERT_MESSAGE(
          transaction->status == TransactionData::kStatusRun,
          "Unexpected transaction status in abort");
      RemoveEventCallback(transaction->outstanding_listener.get(),
                          QuerySpec(transaction->path));
      if (reason == kErrorOverriddenBySet) {
        Extend(&events, server_sync_tree_->AckUserWrite(
                            transaction->current_write_id, kAckRevert,
                            kDoNotPersist, server_time_offset_));
      }
      futures_to_complete.push_back(
          {transaction, abort_error, Variant::Null()});
    }
  }

  if (last_sent == -1) {
    node->value().reset();
  } else {
    queue->erase(queue->begin() + last_sent + 1, queue->end());
  }

  PostEvents(events);
  for (auto& ftc : futures_to_complete) {
    ftc.transaction->ref_future->Complete(ftc.transaction->future_handle,
                                          ftc.abort_reason,
                                          GetErrorMessage(ftc.abort_reason));
  }
}

Path Repo::RerunTransactions(const Path& changed_path) {
  auto* root_most_transaction_node = GetAncestorTransactionNode(changed_path);
  Path path = root_most_transaction_node->GetPath();
  RerunTransactionQueue(BuildTransactionQueue(root_most_transaction_node),
                        path);
  return path;
}

void Repo::SendTransactionQueue(const std::vector<TransactionDataPtr>& queue,
                                const Path& path) {
  assert(!queue.empty());
  logger_->LogDebug("SendTransactionQueue @ %s (# of transaction : %d)",
                    path.c_str(), static_cast<int>(queue.size()));

  std::vector<WriteId> sets_to_ignore;
  sets_to_ignore.reserve(queue.size());
  for (const TransactionDataPtr& transaction : queue) {
    sets_to_ignore.push_back(transaction->current_write_id);
  }

  Variant latest_state = GetLatestState(path, sets_to_ignore);
  if (HasVector(latest_state)) ConvertVectorToMap(&latest_state);
  Variant snap_to_send = latest_state;
  std::string hash;
  GetHash(latest_state, &hash);

  for (const TransactionDataPtr& transaction : queue) {
    FIREBASE_DEV_ASSERT_MESSAGE(
        transaction->status == TransactionData::kStatusRun,
        "Cannot send a transaction that is not running!");
    transaction->status = TransactionData::kStatusSent;
    ++transaction->retry_count;
    auto relative_path = Path::GetRelative(path, transaction->path);
    FIREBASE_DEV_ASSERT(relative_path.has_value());
    SetVariantAtPath(&snap_to_send, *relative_path,
                     transaction->current_output_snapshot_raw);
  }
  if (HasVector(snap_to_send)) ConvertVectorToMap(&snap_to_send);

  connection::ResponsePtr response = std::make_shared<TransactionResponse>(
      safe_this_, database_, path, queue,
      [](const connection::ResponsePtr& ptr) {
        auto* resp = static_cast<TransactionResponse*>(ptr.get());
        FIREBASE_DEV_ASSERT(resp);
        ThisRefLock lock(&resp->repo_ref());
        if (Repo* r = lock.GetReference()) {
          resp->MarkHandled();
          r->HandleTransactionResponse(ptr);
        } else {
          resp->FailUncompletedTransactions();
        }
      });

  connection_->CompareAndPut(path, snap_to_send, hash, response);
}

void Repo::HandleTransactionResponse(const connection::ResponsePtr& ptr) {
  auto* response = static_cast<TransactionResponse*>(ptr.get());
  assert(response);
  const Path& path = response->path();

  if (!response->HasError()) {
    std::vector<Event> events;
    std::vector<FutureToComplete> futures_to_complete;
    futures_to_complete.reserve(response->queue().size());

    for (auto& transaction : response->queue()) {
      transaction->status = TransactionData::kStatusComplete;
      Extend(&events, server_sync_tree_->AckUserWrite(
                          transaction->current_write_id, kAckConfirm,
                          kDoNotPersist, server_time_offset_));
      futures_to_complete.push_back(
          {transaction, kErrorNone,
           transaction->current_output_snapshot_resolved});
    }

    PruneCompletedTransactions(transaction_queue_tree_.GetChild(path));
    SendAllReadyTransactions();
    PostEvents(events);

    for (auto& ftc : futures_to_complete) {
      DataSnapshot snapshot(new DataSnapshotInternal(
          database_, ftc.node, QuerySpec(ftc.transaction->path)));
      ftc.transaction->ref_future->CompleteWithResult(
          ftc.transaction->future_handle, kErrorNone, snapshot);
      RemoveEventCallback(ftc.transaction->outstanding_listener.get(),
                          QuerySpec(ftc.transaction->path));
    }
  } else {
    if (response->GetErrorCode() == kErrorDataStale) {
      for (auto& tx : response->queue()) {
        tx->status = (tx->status == TransactionData::kStatusSentNeedsAbort)
                         ? TransactionData::kStatusNeedsAbort
                         : TransactionData::kStatusRun;
      }
    } else {
      for (auto& tx : response->queue()) {
        tx->status = TransactionData::kStatusNeedsAbort;
        tx->abort_reason = response->GetErrorCode();
      }
    }
    RerunTransactions(response->path());
  }
}

void Repo::RerunTransactionQueue(const std::vector<TransactionDataPtr>& queue,
                                 const Path& path) {
  logger_->LogDebug("RerunTransactionQueue @ %s (# of transaction : %d)",
                    path.c_str(), static_cast<int>(queue.size()));
  if (queue.empty()) return;

  std::vector<FutureToComplete> futures_to_complete;
  std::vector<WriteId> sets_to_ignore;
  sets_to_ignore.reserve(queue.size());
  for (const TransactionDataPtr& transaction : queue) {
    sets_to_ignore.push_back(transaction->current_write_id);
  }

  for (const TransactionDataPtr& transaction : queue) {
    auto relative_path = Path::GetRelative(path, transaction->path);
    assert(relative_path.has_value());
    (void)relative_path;

    bool abort_transaction = false;
    Error abort_reason = kErrorNone;
    std::vector<Event> events;

    if (transaction->status == TransactionData::kStatusNeedsAbort) {
      abort_transaction = true;
      abort_reason = transaction->abort_reason;
      if (abort_reason != kErrorWriteCanceled) {
        Extend(&events, server_sync_tree_->AckUserWrite(
                            transaction->current_write_id, kAckRevert,
                            kDoNotPersist, server_time_offset_));
      }
    } else if (transaction->status == TransactionData::kStatusRun) {
      if (transaction->retry_count >= TransactionData::kTransactionMaxRetries) {
        abort_transaction = true;
        abort_reason = kErrorMaxRetries;
        Extend(&events, server_sync_tree_->AckUserWrite(
                            transaction->current_write_id, kAckRevert,
                            kDoNotPersist, server_time_offset_));
      } else {
        Variant current_input =
            GetLatestState(transaction->path, sets_to_ignore);
        if (HasVector(current_input)) ConvertVectorToMap(&current_input);
        transaction->current_input_snapshot = current_input;

        auto* mutable_data_impl =
            new MutableDataInternal(database_, current_input);
        MutableData mutable_data(mutable_data_impl);
        if (transaction->transaction_function(&mutable_data,
                                              transaction->context) ==
            kTransactionResultSuccess) {
          WriteId old_write_id = transaction->current_write_id;
          Variant server_values = GenerateServerValues(server_time_offset_);
          Variant* new_data_node = mutable_data_impl->GetNode();
          Variant new_node_resolved =
              ResolveDeferredValueSnapshot(*new_data_node, server_values);

          transaction->current_output_snapshot_raw = *new_data_node;
          transaction->current_output_snapshot_resolved = new_node_resolved;
          transaction->current_write_id = GetNextWriteId();

          sets_to_ignore.push_back(old_write_id);
          Extend(&events,
                 server_sync_tree_->ApplyUserOverwrite(
                     transaction->path, *new_data_node, new_node_resolved,
                     transaction->current_write_id,
                     transaction->trigger_local_events ? kOverwriteVisible
                                                       : kOverwriteInvisible,
                     kDoNotPersist));
          Extend(&events, server_sync_tree_->AckUserWrite(
                              old_write_id, kAckRevert, kDoNotPersist,
                              server_time_offset_));
        } else {
          abort_transaction = true;
          abort_reason = kErrorWriteCanceled;
          Extend(&events, server_sync_tree_->AckUserWrite(
                              transaction->current_write_id, kAckRevert,
                              kDoNotPersist, server_time_offset_));
        }
      }
    }

    PostEvents(events);

    if (abort_transaction) {
      transaction->status = TransactionData::kStatusComplete;
      futures_to_complete.push_back(
          {transaction, abort_reason, transaction->current_input_snapshot});
      Repo::scheduler().Schedule(NewCallback(
          [](ThisRef ref, TransactionDataPtr tx) {
            ThisRefLock lock(&ref);
            if (Repo* r = lock.GetReference()) {
              r->RemoveEventCallback(tx->outstanding_listener.get(),
                                     QuerySpec(tx->path));
            }
          },
          safe_this_, transaction));
    }
  }
  PruneCompletedTransactions(&transaction_queue_tree_);

  for (auto& ftc : futures_to_complete) {
    DataSnapshot snapshot(new DataSnapshotInternal(
        database_, ftc.node, QuerySpec(ftc.transaction->path)));
    ftc.transaction->ref_future->CompleteWithResult(
        ftc.transaction->future_handle, ftc.abort_reason, snapshot);
  }
  SendAllReadyTransactions();
}

Variant Repo::GetLatestState(const Path& path,
                             std::vector<WriteId> sets_to_ignore) {
  std::optional<Variant> state =
      server_sync_tree_->CalcCompleteEventCache(path, sets_to_ignore);
  return state.has_value() ? *state : Variant::Null();
}

void Repo::PruneCompletedTransactions(
    Tree<std::vector<TransactionDataPtr>>* node) {
  if (!node) return;
  if (auto& queue = node->value(); queue.has_value()) {
    queue->erase(std::remove_if(queue->begin(), queue->end(),
                                [](const TransactionDataPtr& tx) {
                                  return tx->status ==
                                         TransactionData::kStatusComplete;
                                }),
                 queue->end());
    if (queue->empty()) node->value().reset();
  }
  for (auto it = node->children().begin(); it != node->children().end();) {
    PruneCompletedTransactions(&it->second);
    it = it->second.IsEmpty() ? node->children().erase(it) : std::next(it);
  }
}

Tree<std::vector<TransactionDataPtr>>* Repo::GetAncestorTransactionNode(
    Path path) {
  auto* node = &transaction_queue_tree_;
  while (!path.empty() && !node->value().has_value()) {
    node = node->GetOrMakeSubtree(path.FrontDirectory());
    path = path.PopFrontDirectory();
  }
  return node;
}

std::vector<TransactionDataPtr> Repo::BuildTransactionQueue(
    Tree<std::vector<TransactionDataPtr>>* transaction_node) {
  std::vector<TransactionDataPtr> queue;
  AggregateTransactionQueues(&queue, transaction_node);
  std::sort(queue.begin(), queue.end(),
            [](const TransactionDataPtr& a, const TransactionDataPtr& b) {
              return *a < *b;
            });
  return queue;
}

void Repo::AggregateTransactionQueues(
    std::vector<TransactionDataPtr>* queue,
    Tree<std::vector<TransactionDataPtr>>* node) {
  if (node->value().has_value()) Extend(queue, *node->value());
  for (auto& [key, subtree] : node->children()) {
    AggregateTransactionQueues(queue, &subtree);
  }
}

void Repo::UpdateInfo(const std::string& key, const Variant& value) {
  if (key == kDotInfoServerTimeOffset) {
    server_time_offset_ = value.AsInt64().int64_value();
  }
  Path path = Path(kDotInfo).GetChild(key);
  VariantUpdateChild(&info_data_, path, value);
  PostEvents(info_sync_tree_->ApplyServerOverwrite(path, value));
}

void Repo::OnAppCheckTokenChanged(const std::string& token, void* context) {
  if (auto* repo = static_cast<Repo*>(context)) {
    ThisRefLock lock(&repo->this_ref());
    if (Repo* r = lock.GetReference()) r->connection_->RefreshAppCheckToken(token);
  }
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
