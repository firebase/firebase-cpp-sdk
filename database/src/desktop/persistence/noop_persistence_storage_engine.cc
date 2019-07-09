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

#include "database/src/desktop/persistence/noop_persistence_storage_engine.h"

namespace firebase {
namespace database {
namespace internal {

NoopPersistenceStorageEngine::~NoopPersistenceStorageEngine() {}

void NoopPersistenceStorageEngine::SaveUserOverwrite(const Path& path,
                                                     const Variant& data,
                                                     WriteId write_id) {}

void NoopPersistenceStorageEngine::SaveUserMerge(const Path& path,
                                                 const CompoundWrite& children,
                                                 WriteId write_id) {}

void NoopPersistenceStorageEngine::RemoveUserWrite(WriteId write_id) {}

std::vector<UserWriteRecord> NoopPersistenceStorageEngine::LoadUserWrites() {
  return std::vector<UserWriteRecord>();
}

void NoopPersistenceStorageEngine::RemoveAllUserWrites() {}

const Variant& NoopPersistenceStorageEngine::ServerCache(const Path& path) {
  return kNullVariant;
}

void NoopPersistenceStorageEngine::OverwriteServerCache(const Path& path,
                                                        const Variant& data) {}

void NoopPersistenceStorageEngine::MergeIntoServerCache(const Path& path,
                                                        const Variant& data) {}

void NoopPersistenceStorageEngine::MergeIntoServerCache(
    const Path& path, const CompoundWrite& children) {}

void NoopPersistenceStorageEngine::SaveTrackedQuery(
    const TrackedQuery& tracked_query) {}

void NoopPersistenceStorageEngine::DeleteTrackedQuery(QueryId query_id) {}

std::vector<TrackedQuery> NoopPersistenceStorageEngine::LoadTrackedQueries() {
  return std::vector<TrackedQuery>();
}

void NoopPersistenceStorageEngine::ResetPreviouslyActiveTrackedQueries(
    uint64_t last_use) {}

void NoopPersistenceStorageEngine::SaveTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& keys) {}

void NoopPersistenceStorageEngine::UpdateTrackedQueryKeys(
    QueryId query_id, const std::set<std::string>& added,
    const std::set<std::string>& removed) {}

std::set<std::string> NoopPersistenceStorageEngine::LoadTrackedQueryKeys(
    QueryId query_id) {
  return std::set<std::string>();
}

std::set<std::string> NoopPersistenceStorageEngine::LoadTrackedQueryKeys(
    const std::set<QueryId>& query_ids) {
  return std::set<std::string>();
}

bool NoopPersistenceStorageEngine::BeginTransaction() { return true; }

void NoopPersistenceStorageEngine::EndTransaction() {}

void NoopPersistenceStorageEngine::SetTransactionSuccessful() {}

}  // namespace internal
}  // namespace database
}  // namespace firebase
