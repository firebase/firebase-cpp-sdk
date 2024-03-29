// Copyright 2020 Google LLC
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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_PERSISTENCE_LEVEL_DB_PERSISTENCE_STORAGE_ENGINE_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_PERSISTENCE_LEVEL_DB_PERSISTENCE_STORAGE_ENGINE_H_

#include <memory>

#include "app/src/include/firebase/variant.h"
#include "app/src/logger.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/persistence/prune_forest.h"
#include "leveldb/db.h"

namespace firebase {
namespace database {
namespace internal {

class DatabaseInternal;

class LevelDbPersistenceStorageEngine : public PersistenceStorageEngine {
 public:
  explicit LevelDbPersistenceStorageEngine(LoggerBase* logger);

  ~LevelDbPersistenceStorageEngine() override;

  // Opening up the database may fail, so we have to initialize the database in
  // a separate step.
  bool Initialize(const std::string& level_db_path);

  // Write data to the local cache, overwriting the data at the given path.
  // Additionally, log that this write occurred so that when the database is
  // online again it can send updates.
  //
  // @param path The path for this write
  // @param data The data for this write
  // @param write_id The write id that was used for this write
  void SaveUserOverwrite(const Path& path, const Variant& data,
                         WriteId write_id) override;

  // Write data to the local cache, merging the data at the given path.
  // Additionally, log that this write occurred so that when the database is
  // online again it can send updates.
  //
  // @param path The path for this merge
  // @param children The children for this merge
  // @param write_id The write id that was used for this merge
  void SaveUserMerge(const Path& path, const CompoundWrite& children,
                     WriteId write_id) override;

  // Remove a write with the given write id.
  //
  // @param write_id The write id to remove.
  void RemoveUserWrite(WriteId write_id) override;

  // Return a std::vector of all writes that were persisted.
  //
  // @return The std::vector of writes.
  std::vector<UserWriteRecord> LoadUserWrites() override;

  // Removes all user writes.
  void RemoveAllUserWrites() override;

  // Loads all data at a path. It has no knowledge of whether the data is
  // "complete" or not.
  //
  // @param path The path at which to load the data.
  // @return The data that was loaded.
  Variant ServerCache(const Path& path) override;

  // Overwrite the server cache at the given path with the given data.
  //
  // @param path The path to update.
  // @param data The data to write to the cache.
  void OverwriteServerCache(const Path& path, const Variant& data) override;

  // Update the server cache at the given path with the given data, merging each
  // child into the cache.
  //
  // @param path The path to update.
  // @param data The data to merge into the cache.
  void MergeIntoServerCache(const Path& path, const Variant& data) override;

  // Update the server cache at the given path with the given children, merging
  // each one into the cache.
  //
  // @param path The path for this merge
  // @param children The children to update
  void MergeIntoServerCache(const Path& path,
                            const CompoundWrite& children) override;

  // Estimate the size of the Server Cache. This is not an exact byte count, of
  // the memory or disk space being used, just an estimate.
  //
  // @return The estimated server cache size.
  uint64_t ServerCacheEstimatedSizeInBytes() const override;

  // Write the tracked query to the cache.
  //
  // @param tracked_query the tracked query to persist.
  void SaveTrackedQuery(const TrackedQuery& tracked_query) override;

  // Delete the tracked query associated with he given QueryID
  //
  // @param query_id The query_id of the TrackedQuery to delete from
  // persistence.
  void DeleteTrackedQuery(QueryId query_id) override;

  // Return a std::vector of all tracked queries that were persisted.
  //
  // @return The std::vector of TrackedQueries.
  std::vector<TrackedQuery> LoadTrackedQueries() override;

  // Update the last_use time on all active tracked queries.
  //
  // @param last_use the new last_use time.
  void ResetPreviouslyActiveTrackedQueries(uint64_t last_use) override;

  // Persist the given set of tracked keys at associated with the TrackedQuery
  // with the given query_id.
  //
  // @param query_id The QueryId of the TrackedQuery.
  // @param keys The set of keys associated with the TrackedQuery to persist.
  void SaveTrackedQueryKeys(QueryId query_id,
                            const std::set<std::string>& keys) override;

  // Update the set of tracked query keys for a given TrackedQuery.
  //
  // @param query_id The QueryId of the TrackedQuery.
  // @param added The keys to add to the set of persisted keys.
  // @param removed The keys to remove from the set of persisted keys.
  void UpdateTrackedQueryKeys(QueryId query_id,
                              const std::set<std::string>& added,
                              const std::set<std::string>& removed) override;

  // Return a std::vector of all tracked queries keys that were persisted for
  // the tracked query associated with the given query_id.
  //
  // @param The set of tracked query keys.
  std::set<std::string> LoadTrackedQueryKeys(QueryId query_id) override;

  // Return a std::vector of all tracked query keys that were persisted for the
  // queries in the given set.
  //
  // @param The set of tracked query keys.
  std::set<std::string> LoadTrackedQueryKeys(
      const std::set<QueryId>& query_ids) override;

  // Remove unused items from the local cache based on the given prune forest.
  //
  // @param root The location from which pruning should begin.
  // @param prune_forest A tree of locations in the storage cache representing
  // which nodes should be removed.
  void PruneCache(const Path& root,
                  const PruneForestRef& prune_forest) override;

  // Begin a transaction. No other transactions can run until EndTransactions is
  // Begin a transaction. No other transactions can run until EndTransactions is
  // called.
  bool BeginTransaction() override;

  // End a transaction. This should be called after BeginTransaction has been
  // called, after the transaction is complete.
  void EndTransaction() override;

  // Declare that a transaction completed successfully.
  void SetTransactionSuccessful() override;

 private:
  void VerifyInsideTransaction();

  std::unique_ptr<leveldb::DB> database_;

  bool inside_transaction_;

  LoggerBase* logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_PERSISTENCE_LEVEL_DB_PERSISTENCE_STORAGE_ENGINE_H_
