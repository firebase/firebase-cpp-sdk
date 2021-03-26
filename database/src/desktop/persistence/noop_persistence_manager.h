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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_MANAGER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_MANAGER_H_

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "app/memory/unique_ptr.h"
#include "app/src/mutex.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/cache_policy.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/persistence/persistence_manager_interface.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class NoopPersistenceManager : public PersistenceManagerInterface {
 public:
  NoopPersistenceManager();

  // Persist a user write to the storage engine.
  //
  // @param path The path for this write.
  // @param variant The variant for this write.
  // @param write_id The write id that was used for this write.
  void SaveUserOverwrite(const Path& path, const Variant& variant,
                         WriteId write_id) override;

  // Persist a user merge to the storage engine.
  //
  // @param path The path for this merge.
  // @param children The children for this merge.
  // @param write_id The write id that was used for this merge.
  void SaveUserMerge(const Path& path, const CompoundWrite& children,
                     WriteId write_id) override;

  // Remove the user write with the given write id.
  //
  // @param write_id The write id to remove.
  void RemoveUserWrite(WriteId write_id) override;

  // Remove all user writes.
  void RemoveAllUserWrites() override;

  // Applies the write to the storage engine so that it can be persisted.
  void ApplyUserWriteToServerCache(const Path& path,
                                   const Variant& variant) override;

  // Applies the merge to the storage engine so that it can be persisted.
  void ApplyUserWriteToServerCache(const Path& path,
                                   const CompoundWrite& merge) override;

  // Return a list of all writes that were persisted
  //
  // @return The list of writes
  std::vector<UserWriteRecord> LoadUserWrites() override;

  // Returns any cached variant or children as a CacheNode. The query is *not*
  // used to filter the variant but rather to determine if it can be considered
  // complete.
  //
  // @param query The query at the path
  // @return The cached variant or an empty CacheNode if no cache is available
  CacheNode ServerCache(const QuerySpec& query) override;

  // Overwrite the server cache at the location given by the given QuerySpec.
  void UpdateServerCache(const QuerySpec& query,
                         const Variant& variant) override;

  // Merge the given write into theserver cache at the location given by the
  // given QuerySpec.
  void UpdateServerCache(const Path& path,
                         const CompoundWrite& children) override;

  // Begin tracking the given QuerySpec.
  void SetQueryActive(const QuerySpec& query) override;

  // Stop tracking the given QuerySpec.
  void SetQueryInactive(const QuerySpec& query) override;

  // Inform the TrackedQueryManager to mark the tracked query as complete.
  void SetQueryComplete(const QuerySpec& query) override;

  // Inform the storage engine which keys should be tracked for a given Query.
  void SetTrackedQueryKeys(const QuerySpec& query,
                           const std::set<std::string>& keys) override;

  // Updates the set of keys that should be tracked for a given Query.
  void UpdateTrackedQueryKeys(const QuerySpec& query,
                              const std::set<std::string>& added,
                              const std::set<std::string>& removed) override;

  // Run a transaction. Transactions are functions that are going to change
  // values in the database, and they must do so effectively atomically. Two
  // transacations cannot be run at the same time in different threads, they
  // must be scheduled to run by Scheduler. A transaction should return true to
  // signal that it completed successfully, or false if there was an error.
  // Certain actions can only be performed from inside a transaction, and those
  // functions will assert if they are not called between calls to
  // BeingTransaction and EndTransaction.
  bool RunInTransaction(std::function<bool()> func) override;

 private:
  bool inside_transaction_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_MANAGER_H_
