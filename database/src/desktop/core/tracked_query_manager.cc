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

#include "database/src/desktop/core/tracked_query_manager.h"

#include <algorithm>
#include <cstdint>
#include <map>

#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

TrackedQueryManagerInterface::~TrackedQueryManagerInterface() {}

// Returns true if the given TrackedQueryMap has a complete default query.
static bool HasDefaultCompletePredicate(
    const TrackedQueryMap& tracked_queries) {
  const TrackedQuery* tracked_query = MapGet(&tracked_queries, QueryParams());
  return tracked_query && tracked_query->complete;
}

// Returns true if the given TrackedQueryMap has a complete active query.
static bool HasActiveDefaultPredicate(const TrackedQueryMap& tracked_queries) {
  const TrackedQuery* tracked_query = MapGet(&tracked_queries, QueryParams());
  return tracked_query && tracked_query->active;
}

// Returns true if the given TrackedQuery is prunable. A query is considered
// prunable if it is not active.
static bool IsQueryPrunablePredicate(const TrackedQuery& query) {
  return !query.active;
}

// Returns true if the given TrackedQuery is not prunable. A query is considered
// prunable if it is not active.
static bool IsQueryUnPrunablePredicate(const TrackedQuery& query) {
  return query.active;
}

static QuerySpec GetNormalizedQuery(const QuerySpec& query_spec) {
  // If the query loads all data, we don't care about order_by.
  // So just treat it as a default query.
  return QuerySpecLoadsAllData(query_spec) ? MakeDefaultQuerySpec(query_spec)
                                           : query_spec;
}

static void AssertValidTrackedQuery(const QuerySpec& query_spec) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      !QuerySpecLoadsAllData(query_spec) || QuerySpecIsDefault(query_spec),
      "Can't have tracked non-default query that loads all data");
}

TrackedQueryManager::TrackedQueryManager(
    PersistenceStorageEngine* storage_engine)
    : storage_engine_(storage_engine),
      tracked_query_tree_(),
      next_query_id_(0) {
  ResetPreviouslyActiveTrackedQueries();

  // Populate our cache from the storage layer.
  std::vector<TrackedQuery> tracked_queries =
      storage_engine_->LoadTrackedQueries();
  for (TrackedQuery query : tracked_queries) {
    next_query_id_ = std::max(query.query_id + 1, next_query_id_);
    CacheTrackedQuery(query);
  }
}

const TrackedQuery* TrackedQueryManager::FindTrackedQuery(
    const QuerySpec& query_spec) const {
  QuerySpec normalized_spec = GetNormalizedQuery(query_spec);
  const TrackedQueryMap* set =
      tracked_query_tree_.GetValueAt(normalized_spec.path);
  return set ? MapGet(set, normalized_spec.params) : nullptr;
}

void TrackedQueryManager::RemoveTrackedQuery(const QuerySpec& query_spec) {
  QuerySpec normalized_spec = GetNormalizedQuery(query_spec);
  const TrackedQuery* tracked_query = FindTrackedQuery(normalized_spec);
  FIREBASE_DEV_ASSERT_MESSAGE(tracked_query, "Query must exist to be removed.");

  storage_engine_->DeleteTrackedQuery(tracked_query->query_id);
  TrackedQueryMap* tracked_queries =
      tracked_query_tree_.GetValueAt(normalized_spec.path);

  auto to_erase = tracked_queries->find(normalized_spec.params);
  tracked_queries->erase(to_erase);
  if (tracked_queries->empty()) {
    tracked_query_tree_.SetValueAt(normalized_spec.path,
                                   Optional<TrackedQueryMap>());
  }
}

void TrackedQueryManager::SetQueryActiveFlag(
    const QuerySpec& query_spec, TrackedQuery::ActivityStatus activity_status) {
  QuerySpec normalized_spec = GetNormalizedQuery(query_spec);
  const TrackedQuery* tracked_query = FindTrackedQuery(normalized_spec);

  uint64_t last_use = 0;
  if (tracked_query != nullptr) {
    TrackedQuery updated_tracked_query = *tracked_query;
    updated_tracked_query.last_use = last_use;
    updated_tracked_query.active = activity_status;
    SaveTrackedQuery(updated_tracked_query);
  } else {
    FIREBASE_DEV_ASSERT_MESSAGE(
        activity_status == TrackedQuery::kActive,
        "If we're setting the query to inactive, we should already be "
        "tracking it!");
    SaveTrackedQuery(TrackedQuery(next_query_id_++, normalized_spec, last_use,
                                  TrackedQuery::kIncomplete, activity_status));
  }
}

void TrackedQueryManager::SetQueryCompleteIfExists(
    const QuerySpec& query_spec) {
  QuerySpec normalized_spec = GetNormalizedQuery(query_spec);
  const TrackedQuery* tracked_query = FindTrackedQuery(normalized_spec);
  if (tracked_query != nullptr && !tracked_query->complete) {
    TrackedQuery updated_tracked_query = *tracked_query;
    updated_tracked_query.complete = TrackedQuery::kComplete;
    SaveTrackedQuery(updated_tracked_query);
  }
}

void TrackedQueryManager::SetQueriesComplete(const Path& path) {
  tracked_query_tree_.CallOnEach(
      path, [this](const Path& path, TrackedQueryMap& tracked_queries) {
        for (auto key_value : tracked_queries) {
          TrackedQuery& tracked_query = key_value.second;
          if (!tracked_query.complete) {
            TrackedQuery updated_tracked_query = tracked_query;
            updated_tracked_query.complete = TrackedQuery::kComplete;
            this->SaveTrackedQuery(updated_tracked_query);
          }
        }
      });
}

bool TrackedQueryManager::IsQueryComplete(const QuerySpec& query_spec) {
  if (IncludedInDefaultCompleteQuery(query_spec.path)) {
    return true;
  } else if (QuerySpecLoadsAllData(query_spec)) {
    // We didn't find a default complete query, so must not be complete.
    return false;
  } else {
    TrackedQueryMap* tracked_queries =
        tracked_query_tree_.GetValueAt(query_spec.path);
    if (!tracked_queries) {
      return false;
    }
    const TrackedQuery* tracked_query =
        MapGet(tracked_queries, query_spec.params);
    return tracked_query && tracked_query->complete;
  }
}

std::set<std::string> TrackedQueryManager::GetKnownCompleteChildren(
    const Path& path) {
  QuerySpec default_at_path(path);
  FIREBASE_DEV_ASSERT_MESSAGE(!IsQueryComplete(default_at_path),
                              "Path is fully complete.");

  std::set<std::string> complete_children;
  // First, get complete children from any queries at this location.
  std::set<QueryId> query_ids = FilteredQueryIdsAtPath(path);
  if (!query_ids.empty()) {
    std::set<std::string> loaded_queries =
        storage_engine_->LoadTrackedQueryKeys(query_ids);
    complete_children.insert(loaded_queries.begin(), loaded_queries.end());
  }

  // Second, get any complete default queries immediately below us.
  for (auto& child_entry : tracked_query_tree_.GetChild(path)->children()) {
    std::string child_key = child_entry.first;
    Tree<TrackedQueryMap> child_tree = child_entry.second;
    if (child_tree.value().has_value() &&
        HasDefaultCompletePredicate(child_tree.value().value())) {
      complete_children.insert(child_key);
    }
  }

  return complete_children;
}

void TrackedQueryManager::EnsureCompleteTrackedQuery(const Path& path) {
  if (!IncludedInDefaultCompleteQuery(path)) {
    QuerySpec query_spec = QuerySpec(path);
    const TrackedQuery* tracked_query = FindTrackedQuery(query_spec);
    if (tracked_query == nullptr) {
      SaveTrackedQuery(TrackedQuery(next_query_id_++, query_spec, 0,
                                    TrackedQuery::kComplete,
                                    TrackedQuery::kInactive));
    } else {
      FIREBASE_DEV_ASSERT_MESSAGE(!tracked_query->complete,
                                  "This should have been handled above!");
      TrackedQuery updated_tracked_query = *tracked_query;
      updated_tracked_query.complete = TrackedQuery::kComplete;
      SaveTrackedQuery(updated_tracked_query);
    }
  }
}

bool TrackedQueryManager::HasActiveDefaultQuery(const Path& path) {
  return tracked_query_tree_
      .FindRootMostMatchingPath(path, HasActiveDefaultPredicate)
      .has_value();
}

uint64_t TrackedQueryManager::CountOfPrunableQueries() {
  return GetQueriesMatching(IsQueryPrunablePredicate).size();
}

void TrackedQueryManager::ResetPreviouslyActiveTrackedQueries() {
  storage_engine_->BeginTransaction();
  storage_engine_->ResetPreviouslyActiveTrackedQueries(0);
  storage_engine_->SetTransactionSuccessful();
  storage_engine_->EndTransaction();
}

bool TrackedQueryManager::IncludedInDefaultCompleteQuery(const Path& path) {
  return tracked_query_tree_
      .FindRootMostMatchingPath(path, HasDefaultCompletePredicate)
      .has_value();
}

std::set<QueryId> TrackedQueryManager::FilteredQueryIdsAtPath(
    const Path& path) {
  std::set<QueryId> ids;
  auto* subtree = tracked_query_tree_.GetChild(path);
  Optional<TrackedQueryMap>& queries = subtree->value();
  if (queries.has_value()) {
    for (const auto& query_spec_tracked_query_pair : *queries) {
      const TrackedQuery& query = query_spec_tracked_query_pair.second;
      if (!QuerySpecLoadsAllData(query.query_spec)) {
        ids.insert(query.query_id);
      }
    }
  }
  return ids;
}

void TrackedQueryManager::CacheTrackedQuery(const TrackedQuery& tracked_query) {
  AssertValidTrackedQuery(tracked_query.query_spec);

  const Path& path = tracked_query.query_spec.path;
  TrackedQueryMap* tracked_set = tracked_query_tree_.GetValueAt(path);

  // Try to find an existing tracked query map.
  if (tracked_set == nullptr) {
    tracked_set =
        &tracked_query_tree_.SetValueAt(path, TrackedQueryMap()).value();
  }

  // The map should either not contain the tracked query, or already contain it
  // with the proper query_id.
  auto iter = tracked_set->find(tracked_query.query_spec.params);
  FIREBASE_DEV_ASSERT(iter == tracked_set->end() ||
                      iter->second.query_id == tracked_query.query_id);

  // Insert the tracked query.
  auto result = tracked_set->insert(
      std::make_pair(tracked_query.query_spec.params, tracked_query));
  // If the insert was unsuccessful, copy over the existing value.
  bool success = result.second;
  if (!success) {
    auto iter = result.first;
    iter->second = tracked_query;
  }
}

void TrackedQueryManager::SaveTrackedQuery(const TrackedQuery& tracked_query) {
  CacheTrackedQuery(tracked_query);
  storage_engine_->SaveTrackedQuery(tracked_query);
}

std::vector<TrackedQuery> TrackedQueryManager::GetQueriesMatching(
    TrackedQueryPredicateFn predicate) {
  std::vector<TrackedQuery> matching;

  tracked_query_tree_.CallOnEach(
      Path(),
      [&matching, predicate](Path path, TrackedQueryMap& tracked_query_map) {
        for (const auto& query_spec_tracked_query_pair : tracked_query_map) {
          if (predicate(query_spec_tracked_query_pair.second)) {
            matching.push_back(query_spec_tracked_query_pair.second);
          }
        }
      });
  return matching;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
