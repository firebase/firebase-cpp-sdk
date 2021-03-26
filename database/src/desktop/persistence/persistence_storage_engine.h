// Copyright 2018 Google LLC
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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_STORAGE_ENGINE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_STORAGE_ENGINE_H_

#include <cstdint>
#include <set>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"

namespace firebase {
namespace database {
namespace internal {

typedef int64_t WriteId;

// A pending write to the server.
struct UserWriteRecord {
  UserWriteRecord()
      : write_id(), path(), overwrite(), merge(), visible(), is_overwrite() {}

  UserWriteRecord(WriteId _write_id, const Path& _path,
                  const Variant& _overwrite, bool _visible)
      : write_id(_write_id),
        path(_path),
        overwrite(_overwrite),
        merge(),
        visible(_visible),
        is_overwrite(true) {}

  UserWriteRecord(WriteId _write_id, const Path& _path,
                  const CompoundWrite& _merge)
      : write_id(_write_id),
        path(_path),
        overwrite(),
        merge(_merge),
        visible(true),
        is_overwrite(false) {}

  // The unique write ID used to identify this write.
  WriteId write_id;

  // The location of this write.
  Path path;

  // The data to overwrite at the given location.
  Variant overwrite;
  // The data to merge at the given location.
  CompoundWrite merge;

  // If this database location is visible to the client.
  bool visible;

  // If this is an overwrite, use the overwrite Variant. Otherwise, use the
  // merge.
  bool is_overwrite;
};

inline bool operator==(const UserWriteRecord& lhs, const UserWriteRecord& rhs) {
  return lhs.write_id == rhs.write_id && lhs.path == rhs.path &&
         lhs.overwrite == rhs.overwrite && lhs.merge == rhs.merge &&
         lhs.visible == rhs.visible && lhs.is_overwrite == rhs.is_overwrite;
}

// This class provides an interface to a persistent cache. The persistence
// cache persists user writes, cached server data and the corresponding
// completeness tree. There exists one PersistentCache per repo.
//
// The PersistenceStorageEngine stores three kinds of data:
//   * Server Cache: The cached data from the server.
//   * Tracked Queries: Locations in the database that are being queried. This
//     also tracks whether the data at a given location is complete or filtered,
//     which helps when loading and pruning data.
//   * Tracked Query keys: Keys in tracked queries. For each query in
//     trackedQueries that is filtered , we'll track which keys are in the
//     query. This allows us to re-load only the keys of interest when restoring
//     the query, as well as prune data for keys that aren't tracked by any
//     query.
class PersistenceStorageEngine {
 public:
  virtual ~PersistenceStorageEngine() {}
  // Write data to the local cache, overwriting the data at the given path.
  // Additionally, log that this write occurred so that when the database is
  // online again it can send updates.
  //
  // @param path The path for this write
  // @param data The data for this write
  // @param write_id The write id that was used for this write
  virtual void SaveUserOverwrite(const Path& path, const Variant& data,
                                 WriteId write_id) = 0;

  // Write data to the local cache, merging the data at the given path.
  // Additionally, log that this write occurred so that when the database is
  // online again it can send updates.
  //
  // @param path The path for this merge
  // @param children The children for this merge
  // @param write_id The write id that was used for this merge
  virtual void SaveUserMerge(const Path& path, const CompoundWrite& children,
                             WriteId write_id) = 0;

  // Remove a write with the given write id.
  //
  // @param write_id The write id to remove.
  virtual void RemoveUserWrite(WriteId write_id) = 0;

  // Return a std::vector of all writes that were persisted.
  //
  // @return The std::vector of writes.
  virtual std::vector<UserWriteRecord> LoadUserWrites() = 0;

  // Removes all user writes.
  virtual void RemoveAllUserWrites() = 0;

  // Loads all data at a path. It has no knowledge of whether the data is
  // "complete" or not.
  //
  // @param path The path at which to load the data.
  // @return The data that was loaded.
  virtual Variant ServerCache(const Path& path) = 0;

  // Overwrite the server cache at the given path with the given data.
  //
  // @param path The path to update.
  // @param data The data to write to the cache.
  virtual void OverwriteServerCache(const Path& path, const Variant& data) = 0;

  // Update the server cache at the given path with the given data, merging each
  // child into the cache.
  //
  // @param path The path to update.
  // @param data The data to merge into the cache.
  virtual void MergeIntoServerCache(const Path& path, const Variant& data) = 0;

  // Update the server cache at the given path with the given children, merging
  // each one into the cache.
  //
  // @param path The path for this merge
  // @param children The children to update
  virtual void MergeIntoServerCache(const Path& path,
                                    const CompoundWrite& children) = 0;

  // Estimate the size of the Server Cache. This is not an exact byte count, of
  // the memory or disk space being used, just an estimate.
  //
  // @return The estimated server cache size.
  virtual uint64_t ServerCacheEstimatedSizeInBytes() const = 0;

  // Write the tracked query to the cache.
  //
  // @param tracked_query the tracked query to persist.
  virtual void SaveTrackedQuery(const TrackedQuery& tracked_query) = 0;

  // Delete the tracked query associated with he given QueryID
  //
  // @param query_id The query_id of the TrackedQuery to delete from
  // persistence.
  virtual void DeleteTrackedQuery(QueryId query_id) = 0;

  // Return a std::vector of all tracked queries that were persisted.
  //
  // @return The std::vector of TrackedQueries.
  virtual std::vector<TrackedQuery> LoadTrackedQueries() = 0;

  // Update the last_use time on all active tracked queries.
  //
  // @param last_use the new last_use time.
  virtual void ResetPreviouslyActiveTrackedQueries(uint64_t last_use) = 0;

  // Persist the given set of tracked keys at associated with the TrackedQuery
  // with the given query_id.
  //
  // @param query_id The QueryId of the TrackedQuery.
  // @param keys The set of keys associated with the TrackedQuery to persist.
  virtual void SaveTrackedQueryKeys(QueryId query_id,
                                    const std::set<std::string>& keys) = 0;

  // Update the set of tracked query keys for a given TrackedQuery.
  //
  // @param query_id The QueryId of the TrackedQuery.
  // @param added The keys to add to the set of persisted keys.
  // @param removed The keys to remove from the set of persisted keys.
  virtual void UpdateTrackedQueryKeys(QueryId query_id,
                                      const std::set<std::string>& added,
                                      const std::set<std::string>& removed) = 0;

  // Return a std::vector of all tracked queries keys that were persisted for
  // the tracked query associated with the given query_id.
  //
  // @param The set of tracked query keys.
  virtual std::set<std::string> LoadTrackedQueryKeys(QueryId query_id) = 0;

  // Return a std::vector of all tracked query keys that were persisted for the
  // queries in the given set.
  //
  // @param The set of tracked query keys.
  virtual std::set<std::string> LoadTrackedQueryKeys(
      const std::set<QueryId>& query_ids) = 0;

  // Remove unused items from the local cache based on the given prune forest.
  //
  // @param root The location from which pruning should begin.
  // @param prune_forest A tree of locations in the storage cache representing
  // which nodes should be removed.
  virtual void PruneCache(const Path& root,
                          const PruneForestRef& prune_forest) = 0;

  // Begin a transaction. No other transactions can run until EndTransactions is
  // called.
  virtual bool BeginTransaction() = 0;

  // End a transaction. This should be called after BeginTransaction has been
  // called, after the transaction is complete.
  virtual void EndTransaction() = 0;

  // Declare that a transaction completed successfully.
  virtual void SetTransactionSuccessful() = 0;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_STORAGE_ENGINE_H_
