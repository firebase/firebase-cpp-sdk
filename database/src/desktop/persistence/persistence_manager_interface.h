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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_MANAGER_INTERFACE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_MANAGER_INTERFACE_H_

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class PersistenceManagerInterface {
 public:
  virtual ~PersistenceManagerInterface() {}

  // Persist a user write to the storage engine.
  //
  // @param path The path for this write.
  // @param variant The variant for this write.
  // @param write_id The write id that was used for this write.
  virtual void SaveUserOverwrite(const Path& path, const Variant& variant,
                                 WriteId write_id) = 0;

  // Persist a user merge to the storage engine.
  //
  // @param path The path for this merge.
  // @param children The children for this merge.
  // @param write_id The write id that was used for this merge.
  virtual void SaveUserMerge(const Path& path, const CompoundWrite& children,
                             WriteId write_id) = 0;

  // Remove the user write with the given write id.
  //
  // @param write_id The write id to remove.
  virtual void RemoveUserWrite(WriteId write_id) = 0;

  // Remove all user writes.
  virtual void RemoveAllUserWrites() = 0;

  // Applies the write to the storage engine so that it can be persisted.
  virtual void ApplyUserWriteToServerCache(const Path& path,
                                           const Variant& variant) = 0;

  // Applies the merge to the storage engine so that it can be persisted.
  virtual void ApplyUserWriteToServerCache(const Path& path,
                                           const CompoundWrite& merge) = 0;

  // Return a list of all writes that were persisted
  //
  // @return The list of writes
  virtual std::vector<UserWriteRecord> LoadUserWrites() = 0;

  // Returns any cached variant or children as a CacheNode. The query is *not*
  // used to filter the variant but rather to determine if it can be considered
  // complete.
  //
  // @param query The query at the path
  // @return The cached variant or an empty CacheNode if no cache is available
  virtual CacheNode ServerCache(const QuerySpec& query) = 0;

  // Overwrite the server cache at the location given by the given QuerySpec.
  virtual void UpdateServerCache(const QuerySpec& query,
                                 const Variant& variant) = 0;

  // Merge the given write into theserver cache at the location given by the
  // given QuerySpec.
  virtual void UpdateServerCache(const Path& path,
                                 const CompoundWrite& children) = 0;

  // Begin tracking the given QuerySpec.
  virtual void SetQueryActive(const QuerySpec& query) = 0;

  // Stop tracking the given QuerySpec.
  virtual void SetQueryInactive(const QuerySpec& query) = 0;

  // Inform the TrackedQueryManager to mark the tracked query as complete.
  virtual void SetQueryComplete(const QuerySpec& query) = 0;

  // Inform the storage engine which keys should be tracked for a given Query.
  virtual void SetTrackedQueryKeys(const QuerySpec& query,
                                   const std::set<std::string>& keys) = 0;

  // Updates the set of keys that should be tracked for a given Query.
  virtual void UpdateTrackedQueryKeys(const QuerySpec& query,
                                      const std::set<std::string>& added,
                                      const std::set<std::string>& removed) = 0;

  virtual bool RunInTransaction(std::function<bool()> transaction_func) = 0;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_PERSISTENCE_MANAGER_INTERFACE_H_
