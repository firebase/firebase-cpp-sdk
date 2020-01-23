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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_STORAGE_ENGINE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_STORAGE_ENGINE_H_

#include <cstdint>
#include <set>

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"

namespace firebase {
namespace database {
namespace internal {

class NoopPersistenceStorageEngine : public PersistenceStorageEngine {
 public:
  ~NoopPersistenceStorageEngine() override;

  void SaveUserOverwrite(const Path& path, const Variant& data,
                         WriteId write_id) override;

  void SaveUserMerge(const Path& path, const CompoundWrite& children,
                     WriteId write_id) override;

  void RemoveUserWrite(WriteId write_id) override;

  std::vector<UserWriteRecord> LoadUserWrites() override;

  void RemoveAllUserWrites() override;

  const Variant& ServerCache(const Path& path) override;

  void OverwriteServerCache(const Path& path, const Variant& data) override;

  void MergeIntoServerCache(const Path& path, const Variant& data) override;

  void MergeIntoServerCache(const Path& path,
                            const CompoundWrite& children) override;

  uint64_t ServerCacheEstimatedSizeInBytes() const override;

  void SaveTrackedQuery(const TrackedQuery& tracked_query) override;

  void DeleteTrackedQuery(QueryId query_id) override;

  std::vector<TrackedQuery> LoadTrackedQueries() override;

  void ResetPreviouslyActiveTrackedQueries(uint64_t last_use) override;

  void SaveTrackedQueryKeys(QueryId query_id,
                            const std::set<std::string>& keys) override;

  void UpdateTrackedQueryKeys(QueryId query_id,
                              const std::set<std::string>& added,
                              const std::set<std::string>& removed) override;

  std::set<std::string> LoadTrackedQueryKeys(QueryId query_id) override;

  std::set<std::string> LoadTrackedQueryKeys(
      const std::set<QueryId>& query_ids) override;

  void PruneCache(const Path& root,
                  const PruneForestRef& prune_forest) override;

  bool BeginTransaction() override;

  void EndTransaction() override;

  void SetTransactionSuccessful() override;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_PERSISTENCE_NOOP_PERSISTENCE_STORAGE_ENGINE_H_
