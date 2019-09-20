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
    Logger* logger)
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

const Variant& InMemoryPersistenceStorageEngine::ServerCache(const Path& path) {
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
  if (!target->is_map()) *target = Variant::EmptyMap();
  PatchVariant(data, target);
  // Clean up in case anything was removed.
  PruneNulls(target);
}

void InMemoryPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const CompoundWrite& children) {
  // TODO(amablue)
  VerifyInTransaction();
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
  tracked_keys.insert(removed.begin(), removed.end());
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

bool InMemoryPersistenceStorageEngine::BeginTransaction() {
  FIREBASE_DEV_ASSERT_MESSAGE(!inside_transaction_,
                              "runInTransaction called when an existing "
                              "transaction is already in progress.");
  logger_->LogDebug("Starting transaction.");
  inside_transaction_ = true;
  return true;
}

void InMemoryPersistenceStorageEngine::EndTransaction() {
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
