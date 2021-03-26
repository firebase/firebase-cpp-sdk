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

#include "database/src/desktop/persistence/persistence_manager.h"

#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/log.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

PersistenceManager::PersistenceManager(
    UniquePtr<PersistenceStorageEngine> storage_engine,
    UniquePtr<TrackedQueryManagerInterface> tracked_query_manager,
    UniquePtr<CachePolicy> cache_policy, LoggerBase* logger)
    : storage_engine_(std::move(storage_engine)),
      tracked_query_manager_(std::move(tracked_query_manager)),
      cache_policy_(std::move(cache_policy)),
      server_cache_updates_since_last_prune_check_(0),
      logger_(logger) {}

void PersistenceManager::SaveUserOverwrite(const Path& path,
                                           const Variant& variant,
                                           WriteId write_id) {
  storage_engine_->SaveUserOverwrite(path, variant, write_id);
}

void PersistenceManager::SaveUserMerge(const Path& path,
                                       const CompoundWrite& children,
                                       WriteId write_id) {
  storage_engine_->SaveUserMerge(path, children, write_id);
}

void PersistenceManager::RemoveUserWrite(WriteId write_id) {
  storage_engine_->RemoveUserWrite(write_id);
}

void PersistenceManager::RemoveAllUserWrites() {
  storage_engine_->RemoveAllUserWrites();
}

void PersistenceManager::ApplyUserWriteToServerCache(const Path& path,
                                                     const Variant& variant) {
  if (!tracked_query_manager_->HasActiveDefaultQuery(path)) {
    storage_engine_->OverwriteServerCache(path, variant);
    tracked_query_manager_->EnsureCompleteTrackedQuery(path);
  }
}

void PersistenceManager::ApplyUserWriteToServerCache(
    const Path& path, const CompoundWrite& merge) {
  merge.write_tree().CallOnEach(
      Path(), [&](const Path child_path, const Variant& variant) {
        Path write_path = path.GetChild(child_path);
        ApplyUserWriteToServerCache(write_path, variant);
      });
}

std::vector<UserWriteRecord> PersistenceManager::LoadUserWrites() {
  return storage_engine_->LoadUserWrites();
}

CacheNode PersistenceManager::ServerCache(const QuerySpec& query_spec) {
  std::set<std::string> tracked_keys;
  bool found_tracked_keys = false;
  bool complete;
  if (tracked_query_manager_->IsQueryComplete(query_spec)) {
    complete = true;
    const TrackedQuery* tracked_query =
        tracked_query_manager_->FindTrackedQuery(query_spec);
    if (!QuerySpecLoadsAllData(query_spec) && tracked_query != nullptr &&
        tracked_query->complete) {
      found_tracked_keys = true;
      tracked_keys =
          storage_engine_->LoadTrackedQueryKeys(tracked_query->query_id);
    } else {
      found_tracked_keys = false;
    }
  } else {
    found_tracked_keys = true;
    complete = false;
    tracked_keys =
        tracked_query_manager_->GetKnownCompleteChildren(query_spec.path);
  }

  const Variant& server_cache_node =
      storage_engine_->ServerCache(query_spec.path);
  if (found_tracked_keys) {
    Variant filtered_node = Variant::EmptyMap();
    for (const std::string& key : tracked_keys) {
      VariantUpdateChild(&filtered_node, key,
                         VariantGetChild(&server_cache_node, key));
    }
    return CacheNode(IndexedVariant(filtered_node, query_spec.params), complete,
                     true);
  } else {
    return CacheNode(IndexedVariant(server_cache_node, query_spec.params),
                     complete, false);
  }
}

void PersistenceManager::UpdateServerCache(const QuerySpec& query_spec,
                                           const Variant& variant) {
  if (QuerySpecLoadsAllData(query_spec)) {
    storage_engine_->OverwriteServerCache(query_spec.path, variant);
  } else {
    storage_engine_->MergeIntoServerCache(query_spec.path, variant);
  }
  SetQueryComplete(query_spec);
  DoPruneCheckAfterServerUpdate();
}

void PersistenceManager::UpdateServerCache(const Path& path,
                                           const CompoundWrite& children) {
  storage_engine_->MergeIntoServerCache(path, children);
  DoPruneCheckAfterServerUpdate();
}

void PersistenceManager::SetQueryActive(const QuerySpec& query_spec) {
  tracked_query_manager_->SetQueryActiveFlag(query_spec, TrackedQuery::kActive);
}

void PersistenceManager::SetQueryInactive(const QuerySpec& query_spec) {
  tracked_query_manager_->SetQueryActiveFlag(query_spec,
                                             TrackedQuery::kInactive);
}

void PersistenceManager::SetQueryComplete(const QuerySpec& query_spec) {
  if (QuerySpecLoadsAllData(query_spec)) {
    tracked_query_manager_->SetQueriesComplete(query_spec.path);
  } else {
    tracked_query_manager_->SetQueryCompleteIfExists(query_spec);
  }
}

void PersistenceManager::SetTrackedQueryKeys(
    const QuerySpec& query_spec, const std::set<std::string>& keys) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      !QuerySpecLoadsAllData(query_spec),
      "We should only track keys for filtered queries.");

  const TrackedQuery* tracked_query =
      tracked_query_manager_->FindTrackedQuery(query_spec);
  FIREBASE_DEV_ASSERT_MESSAGE(
      tracked_query != nullptr && tracked_query->active,
      "We only expect tracked keys for currently-active queries.");

  storage_engine_->SaveTrackedQueryKeys(tracked_query->query_id, keys);
}

void PersistenceManager::UpdateTrackedQueryKeys(
    const QuerySpec& query_spec, const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      !QuerySpecLoadsAllData(query_spec),
      "We should only track keys for filtered queries.");
  const TrackedQuery* tracked_query =
      tracked_query_manager_->FindTrackedQuery(query_spec);
  FIREBASE_DEV_ASSERT_MESSAGE(
      tracked_query != nullptr && tracked_query->active,
      "We only expect tracked keys for currently-active queries.");

  storage_engine_->UpdateTrackedQueryKeys(tracked_query->query_id, added,
                                          removed);
}

void PersistenceManager::DoPruneCheckAfterServerUpdate() {
  server_cache_updates_since_last_prune_check_++;
  if (cache_policy_->ShouldCheckCacheSize(
          server_cache_updates_since_last_prune_check_)) {
    logger_->LogDebug("Reached prune check threshold.");

    server_cache_updates_since_last_prune_check_ = 0;
    bool can_prune = true;
    uint64_t cache_size = storage_engine_->ServerCacheEstimatedSizeInBytes();
    logger_->LogDebug("Cache size: %i", static_cast<int>(cache_size));

    while (can_prune &&
           cache_policy_->ShouldPrune(
               cache_size, tracked_query_manager_->CountOfPrunableQueries())) {
      PruneForest prune_forest =
          tracked_query_manager_->PruneOldQueries(*cache_policy_);
      PruneForestRef prune_forest_ref(&prune_forest);
      if (prune_forest_ref.PrunesAnything()) {
        std::cout << __LINE__ << std::endl;
        storage_engine_->PruneCache(Path(), prune_forest_ref);
      } else {
        std::cout << __LINE__ << std::endl;
        can_prune = false;
      }
      cache_size = storage_engine_->ServerCacheEstimatedSizeInBytes();
      logger_->LogDebug("Cache size after prune: %i",
                        static_cast<int>(cache_size));
    }
  }
}

bool PersistenceManager::RunInTransaction(
    std::function<bool()> transaction_func) {
  storage_engine_->BeginTransaction();
  bool success = transaction_func();
  if (success) {
    storage_engine_->SetTransactionSuccessful();
  }
  storage_engine_->EndTransaction();
  return success;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
