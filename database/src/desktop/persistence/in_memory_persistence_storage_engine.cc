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

#include "database/src/desktop/persistence/in_memory_persistence_storage_engine.h"

#include <functional>
#include <set>
#include <string>
#include <vector>

#include "app/src/assert.h"
#include "app/src/include/firebase/variant.h"
#include "app/src/log.h"
#include "app/src/logger.h"
#include "app/src/path.h"
#include "app/src/variant_util.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/core/tracked_query_manager.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/util_desktop.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

InMemoryPersistenceStorageEngine::InMemoryPersistenceStorageEngine(
    LoggerBase* logger)
    : server_cache_(), inside_transaction_(false), logger_(logger) {}

InMemoryPersistenceStorageEngine::~InMemoryPersistenceStorageEngine() {}

Variant InMemoryPersistenceStorageEngine::LoadServerCache() {
  // No persistence, so nothing to save.
  return Variant();
}

void InMemoryPersistenceStorageEngine::SaveUserOverwrite(const Path& path,
                                                         const Variant& data,
                                                         WriteId write_id) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

void InMemoryPersistenceStorageEngine::SaveUserMerge(
    const Path& path, const CompoundWrite& children, WriteId write_id) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

void InMemoryPersistenceStorageEngine::RemoveUserWrite(WriteId write_id) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

std::vector<UserWriteRecord>
InMemoryPersistenceStorageEngine::LoadUserWrites() {
  // No persistence, so nothing to save.
  return std::vector<UserWriteRecord>();
}

void InMemoryPersistenceStorageEngine::RemoveAllUserWrites() {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

Variant InMemoryPersistenceStorageEngine::ServerCache(const Path& path) {
  return VariantGetChild(&server_cache_, path);
}

void InMemoryPersistenceStorageEngine::OverwriteServerCache(
    const Path& path, const Variant& data) {
  VerifyInTransaction();
  Variant pruned_data = data;
  PruneNulls(&pruned_data);
  VariantUpdateChild(&server_cache_, path, data);
}

void InMemoryPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const Variant& data) {
  VerifyInTransaction();
  Variant* target = MakeVariantAtPath(&server_cache_, path);
  if (data.is_map()) {
    if (!target->is_map()) *target = Variant::EmptyMap();
    PatchVariant(data, target);
  } else {
    *target = data;
  }
  // Clean up in case anything was removed.
  PruneNulls(target);
}

void InMemoryPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const CompoundWrite& children) {
  VerifyInTransaction();
  children.write_tree().CallOnEach(
      Path(), [this, &path](const Path& child_path, const Variant& value) {
        this->MergeIntoServerCache(path.GetChild(child_path), value);
      });
}

static uint64_t EstimateVariantMemoryUsage(const Variant& variant) {
  switch (variant.type()) {
    case Variant::kTypeNull:
    case Variant::kTypeInt64:
    case Variant::kTypeDouble:
    case Variant::kTypeBool: {
      return sizeof(Variant);
    }
    case Variant::kTypeStaticString: {
      return sizeof(Variant) + strlen(variant.string_value());
    }
    case Variant::kTypeMutableString: {
      return sizeof(Variant) + variant.mutable_string().size();
    }
    case Variant::kTypeVector: {
      uint64_t sum_total = 0;
      for (const auto& item : variant.vector()) {
        sum_total += EstimateVariantMemoryUsage(item);
      }
      return sizeof(Variant) + sum_total;
    }
    case Variant::kTypeMap: {
      uint64_t sum_total = 0;
      for (const auto& key_value_pair : variant.map()) {
        sum_total += EstimateVariantMemoryUsage(key_value_pair.first);
        sum_total += EstimateVariantMemoryUsage(key_value_pair.second);
      }
      return sizeof(Variant) + sum_total;
    }
    case Variant::kTypeStaticBlob:
    case Variant::kTypeMutableBlob: {
      return sizeof(Variant) + variant.blob_size();
    }
    default: {
      FIREBASE_DEV_ASSERT_MESSAGE(false, "Unhandled variant type.");
      return 0;
    }
  }
}

uint64_t InMemoryPersistenceStorageEngine::ServerCacheEstimatedSizeInBytes()
    const {
  return EstimateVariantMemoryUsage(server_cache_);
}

void InMemoryPersistenceStorageEngine::SaveTrackedQuery(
    const TrackedQuery& tracked_query) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

void InMemoryPersistenceStorageEngine::DeleteTrackedQuery(QueryId query_id) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

std::vector<TrackedQuery>
InMemoryPersistenceStorageEngine::LoadTrackedQueries() {
  // No persistence, so nothing to load.
  return std::vector<TrackedQuery>();
}

void InMemoryPersistenceStorageEngine::ResetPreviouslyActiveTrackedQueries(
    uint64_t last_use) {
  // No persistence, so nothing to save.
  VerifyInTransaction();
}

void InMemoryPersistenceStorageEngine::SaveTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& keys) {
  tracked_query_keys_[query_id] = keys;
  VerifyInTransaction();
}

void InMemoryPersistenceStorageEngine::UpdateTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& added,
    const std::set<std::string>& removed) {
  VerifyInTransaction();

  std::set<std::string>& tracked_keys = tracked_query_keys_[query_id];
  tracked_keys.insert(added.begin(), added.end());
  for (const std::string& to_remove : removed) {
    tracked_keys.erase(to_remove);
  }
}

std::set<std::string> InMemoryPersistenceStorageEngine::LoadTrackedQueryKeys(
    QueryId query_id) {
  return tracked_query_keys_[query_id];
}

std::set<std::string> InMemoryPersistenceStorageEngine::LoadTrackedQueryKeys(
    const std::set<QueryId>& query_ids) {
  std::set<std::string> result;
  for (QueryId query_id : query_ids) {
    const std::set<std::string>& tracked_keys = tracked_query_keys_[query_id];
    result.insert(tracked_keys.begin(), tracked_keys.end());
  }
  return result;
}

void PruneVariant(const Path& root, const PruneForestRef prune_forest,
                  Variant* variant) {
  Variant result = prune_forest.FoldKeptNodes(
      Variant::Null(),
      [&root, &variant](const Path& relative_path, Variant accum) {
        Variant child = VariantGetChild(variant, root.GetChild(relative_path));
        VariantUpdateChild(&accum, relative_path, child);
        return accum;
      });
  VariantUpdateChild(variant, root, result);
}

void InMemoryPersistenceStorageEngine::PruneCache(
    const Path& root, const PruneForestRef& prune_forest) {
  PruneVariant(root, prune_forest, &server_cache_);
}

bool InMemoryPersistenceStorageEngine::BeginTransaction() {
  FIREBASE_DEV_ASSERT_MESSAGE(!inside_transaction_,
                              "RunInTransaction called when an existing "
                              "transaction is already in progress.");
  logger_->LogDebug("Starting transaction.");
  inside_transaction_ = true;
  return true;
}

void InMemoryPersistenceStorageEngine::EndTransaction() {
  FIREBASE_DEV_ASSERT_MESSAGE(
      inside_transaction_, "EndTransaction called when not in a transaction");
  inside_transaction_ = false;
  logger_->LogDebug("Transaction completed.");
}

void InMemoryPersistenceStorageEngine::SetTransactionSuccessful() {}

void InMemoryPersistenceStorageEngine::VerifyInTransaction() {
  FIREBASE_DEV_ASSERT_MESSAGE(
      inside_transaction_, "Transaction expected to already be in progress.");
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
