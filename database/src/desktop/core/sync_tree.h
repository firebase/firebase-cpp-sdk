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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_SYNC_TREE_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_SYNC_TREE_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/listen_provider.h"
#include "database/src/desktop/core/operation.h"
#include "database/src/desktop/core/sync_point.h"
#include "database/src/desktop/core/tag.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/core/write_tree.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/view/event.h"
#include "firebase/database/common.h"

namespace firebase::database::internal {

enum Persist {
  kDoNotPersist,
  kPersist,
};

class SyncTree {
 public:
  SyncTree(std::unique_ptr<WriteTree> pending_write_tree,
           std::unique_ptr<PersistenceManagerInterface> persistence_manager,
           std::unique_ptr<ListenProvider> listen_provider)
      : pending_write_tree_(std::move(pending_write_tree)),
        persistence_manager_(std::move(persistence_manager)),
        next_query_tag_(1L),
        listen_provider_(std::move(listen_provider)) {}

  virtual ~SyncTree() = default;

  virtual bool IsEmpty();
  virtual std::vector<Event> AckUserWrite(WriteId write_id, AckStatus revert,
                                          Persist persist,
                                          int64_t server_time_offset);
  virtual std::vector<Event> AddEventRegistration(
      std::unique_ptr<EventRegistration> event_registration);
  virtual std::vector<Event> ApplyTaggedListenComplete(const Tag& tag);
  virtual std::vector<Event> ApplyTaggedQueryOverwrite(const Path& path,
                                                       const Variant& snap,
                                                       const Tag& tag);
  virtual std::vector<Event> ApplyTaggedQueryMerge(
      const Path& path, const std::map<Path, Variant>& changed_children,
      const Tag& tag);
  virtual std::vector<Event> ApplyListenComplete(const Path& path);
  virtual std::vector<Event> ApplyServerMerge(
      const Path& path, const std::map<Path, Variant>& changed_children);
  virtual std::vector<Event> ApplyServerOverwrite(const Path& path,
                                                  const Variant& new_data);
  virtual std::vector<Event> ApplyUserMerge(
      const Path& path, const CompoundWrite& unresolved_children,
      const CompoundWrite& children, const WriteId write_id, Persist persist);
  virtual std::vector<Event> ApplyUserOverwrite(
      const Path& path, const Variant& unresolved_new_data,
      const Variant& new_data, WriteId write_id, OverwriteVisibility visibility,
      Persist persist);
  virtual std::vector<Event> RemoveAllWrites();
  virtual std::vector<Event> RemoveAllEventRegistrations(
      const QuerySpec& query_spec, Error error);
  virtual std::vector<Event> RemoveEventRegistration(
      const QuerySpec& query_spec, void* listener_ptr, Error cancelError);
  virtual std::optional<Variant> CalcCompleteEventCache(
      const Path& path, const std::vector<WriteId>& write_ids_to_exclude) const;
  virtual void SetKeepSynchronized(const QuerySpec& query_spec, bool keep);

 private:
  void SetupListener(const QuerySpec& query_spec);
  std::vector<Event> ApplyOperationHelper(const Operation& operation,
                                          Tree<SyncPoint>* sync_point_tree,
                                          const Variant* server_cache,
                                          WriteTreeRef* writes_cache);
  std::vector<Event> ApplyOperationDescendantsHelper(
      const Operation& operation, Tree<SyncPoint>* sync_point_tree,
      const Variant* server_cache, WriteTreeRef* writes_cache);
  std::vector<Event> ApplyOperationToSyncPoints(const Operation& operation);
  std::vector<Event> ApplyTaggedOperation(const QuerySpec& query_spec,
                                          const Operation& operation);
  void RemoveTags(const std::vector<QuerySpec>& queries);
  const QuerySpec* QuerySpecForTag(const Tag& tag);
  Tag TagForQuerySpec(const QuerySpec& query);
  Tag GetNextQueryTag();

  std::unique_ptr<WriteTree> pending_write_tree_;
  std::unique_ptr<PersistenceManagerInterface> persistence_manager_;
  Tree<SyncPoint> sync_point_tree_;
  std::map<uint64_t, QuerySpec> tag_to_query_spec_map_;
  std::map<QuerySpec, Tag> query_spec_to_tag_map_;
  int64_t next_query_tag_;
  std::set<QuerySpec> keep_synced_queries_;
  std::unique_ptr<ListenProvider> listen_provider_;
};

}  // namespace firebase::database::internal

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_SYNC_TREE_H_
