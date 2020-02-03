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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_TREE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_TREE_H_

#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/optional.h"
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

namespace firebase {
namespace database {
namespace internal {

enum Persist {
  kDoNotPersist,
  kPersist,
};

class SyncTree {
 public:
  SyncTree(UniquePtr<WriteTree> pending_write_tree,
           UniquePtr<PersistenceManagerInterface> persistence_manager,
           UniquePtr<ListenProvider> listen_provider)
      : pending_write_tree_(std::move(pending_write_tree)),
        persistence_manager_(std::move(persistence_manager)),
        next_query_tag_(1L),
        listen_provider_(std::move(listen_provider)) {}

  virtual ~SyncTree() {}

  // Returns true if the SyncTree does not contain any SyncPoints.
  virtual bool IsEmpty();

  // Acknowlege that a write was received by the server, and whether it was
  // accepted or rejected (meaning that is should be reverted) and generate any
  // necessary events that result from the change to the sync tree.
  virtual std::vector<Event> AckUserWrite(WriteId write_id, AckStatus revert,
                                          Persist persist,
                                          int64_t server_time_offset);

  // Add an event registration to the sync tree. The listener attached to that
  // registration will now receive the appropriate events when changes are made
  // to the sync tree. Also generate any initial that need to be sent to the
  // Listener based on the data already cached.
  virtual std::vector<Event> AddEventRegistration(
      UniquePtr<EventRegistration> event_registration);

  // Listening is now complete at the location in the QuerySpec associated with
  // the given tag (which can be for a number of reasons, like losing read
  // permission at that location), and generate any necessary events that result
  // from the change to the sync tree.
  virtual std::vector<Event> ApplyTaggedListenComplete(const Tag& tag);

  // Apply an overwrite from the server to the given path, and generate any
  // necessary events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyTaggedQueryOverwrite(const Path& path,
                                                       const Variant& snap,
                                                       const Tag& tag);

  // Apply a merge from the server to the given path, and generate any necessary
  // events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyTaggedQueryMerge(
      const Path& path, const std::map<Path, Variant>& changed_children,
      const Tag& tag);

  // Listening is now complete at the given location (which can be for a number
  // of reasons, like losing read permission at that location), and generate any
  // necessary events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyListenComplete(const Path& path);

  // Apply a merge from the server to the given path, and generate any necessary
  // events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyServerMerge(
      const Path& path, const std::map<Path, Variant>& changed_children);

  // Apply an overwrite from the server to the given path, and generate any
  // necessary events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyServerOverwrite(const Path& path,
                                                  const Variant& new_data);

  // Apply a merge from the user to the given path, and generate any necessary
  // events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyUserMerge(
      const Path& path, const CompoundWrite& unresolved_children,
      const CompoundWrite& children, const WriteId write_id, Persist persist);

  // Apply an overwrite from the user to the given path, and generate any
  // necessary events that result from the change to the sync tree.
  virtual std::vector<Event> ApplyUserOverwrite(
      const Path& path, const Variant& unresolved_new_data,
      const Variant& new_data, WriteId write_id, OverwriteVisibility visibility,
      Persist persist);

  // Remove all pending writes to the server, and generate any necessary revert
  // events that result from the change to the sync tree.
  virtual std::vector<Event> RemoveAllWrites();

  // Remove all event registrations, and generate any necessary cancel events
  // that result from the change to the sync tree.
  virtual std::vector<Event> RemoveAllEventRegistrations(
      const QuerySpec& query_spec, Error error);

  // Remove the event registration corresponding to the given query_spec and
  // listener pointer, and generate any necessary cancel events that result from
  // the change to the sync tree.
  virtual std::vector<Event> RemoveEventRegistration(
      const QuerySpec& query_spec, void* listener_ptr, Error cancelError);

  // Calculate the complete local cache at the given path, ignoring the writes
  // with given WriteIds.
  virtual Optional<Variant> CalcCompleteEventCache(
      const Path& path, const std::vector<WriteId>& write_ids_to_exclude) const;

  // Determine whether to keep the data at the location given by the QuerySpec
  // loaded locally, even though we don't have a listener on it listening for
  // evennts.
  virtual void SetKeepSynchronized(const QuerySpec& query_spec, bool keep);

 private:
  // For a given new listen, manage the de-duplication of outstanding
  // subscriptions.
  void SetupListener(const QuerySpec& query_spec, const View* view);

  // Recursive helper for ApplyOperationToSyncPoints
  std::vector<Event> ApplyOperationHelper(const Operation& operation,
                                          Tree<SyncPoint>* sync_point_tree,
                                          const Variant* server_cache,
                                          WriteTreeRef* writes_cache);

  // Recursive helper for ApplyOperationToSyncPoints
  std::vector<Event> ApplyOperationDescendantsHelper(
      const Operation& operation, Tree<SyncPoint>* sync_point_tree,
      const Variant* server_cache, WriteTreeRef* writes_cache);

  // Apply the operation to all applicable SyncPoints.
  std::vector<Event> ApplyOperationToSyncPoints(const Operation& operation);

  // This is a thin wrapper around SyncPoint::ApplyOperation, which is used by
  // the various functions above that take Tag arguments. It ensures that there
  // is a SyncPoint at the appropriate location in the database, and grabs the
  // appropriate cache data to pass to ApplyOperation.
  std::vector<Event> ApplyTaggedOperation(const QuerySpec& query_spec,
                                          const Operation& operation);

  // Remove the tags associated with the given QuerySpecs.
  void RemoveTags(const std::vector<QuerySpec>& queries);

  // Get the QuerySpec for the given tag, if one exists.
  const QuerySpec* QuerySpecForTag(const Tag& tag);

  // Return the tag associated with the given query.
  Tag TagForQuerySpec(const QuerySpec& query);

  // Accessor for query tags.
  Tag GetNextQueryTag();

  // A tree of all pending user writes (user-initiated set()'s, transaction()'s,
  // update()'s, etc.).
  UniquePtr<WriteTree> pending_write_tree_;

  // The persistence manager, which is used to interact with the persisted data
  // on disk (both reads and writes).
  UniquePtr<PersistenceManagerInterface> persistence_manager_;

  // A tree that contains the SyncPoints for each location being watched in the
  // database.
  Tree<SyncPoint> sync_point_tree_;

  // Maps that associate Tags with QuerySpecs and vice versa. Used when sending
  // data to and receiving data from the server to disambiguate what QuerySpec
  // data at a location should be applied to.
  std::map<uint64_t, QuerySpec> tag_to_query_spec_map_;
  std::map<QuerySpec, Tag> query_spec_to_tag_map_;

  // Static tracker for next query tag.
  int64_t next_query_tag_;

  // Locations that are being kept synchronized without use of a listener (i.e.
  // through Query::SetKeepSynchronized).
  std::set<QuerySpec> keep_synced_queries_;

  // The listen provider manages which locations we are watching for changes on.
  // When we start watching a new location in the database, we notify the
  // ListenProvider to get updates from the server. And when we stop watching a
  // location the ListenProvider must be notified to stop getting updates on
  // that location.
  UniquePtr<ListenProvider> listen_provider_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_SYNC_TREE_H_
