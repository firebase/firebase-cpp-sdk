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
#include <cassert>
#include <cstdint>
#include <ctime>
#include <map>

#include "app/src/assert.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/src/desktop/persistence/prune_forest.h"
#include "database/src/desktop/util_desktop.h"

namespace firebase {
namespace database {
namespace internal {

bool operator==(const TrackedQuery& lhs, const TrackedQuery& rhs) {
  return lhs.query_id == rhs.query_id && lhs.query_spec == rhs.query_spec &&
         lhs.last_use == rhs.last_use && lhs.complete == rhs.complete &&
         lhs.active == rhs.active;
}

bool operator!=(const TrackedQuery& lhs, const TrackedQuery& rhs) {
  return !(lhs == rhs);
}

TrackedQueryManagerInterface::~TrackedQueryManagerInterface() {}

static bool HasDefaultCompletePredicate(
    const TrackedQueryMap& tracked_queries) {
  const TrackedQuery* tq = MapGet(&tracked_queries, QueryParams());
  return tq && tq->complete;
}

static bool HasActiveDefaultPredicate(const TrackedQueryMap& tracked_queries) {
  const TrackedQuery* tq = MapGet(&tracked_queries, QueryParams());
  return tq && tq->active;
}

static bool IsQueryPrunablePredicate(const TrackedQuery& query) {
  return !query.active;
}

static bool IsQueryUnprunablePredicate(const TrackedQuery& query) {
  return query.active;
}

static QuerySpec GetNormalizedQuery(const QuerySpec& query_spec) {
  return QuerySpecLoadsAllData(query_spec) ? MakeDefaultQuerySpec(query_spec)
                                           : query_spec;
}

TrackedQueryManager::TrackedQueryManager(
    PersistenceStorageEngine* storage_engine, LoggerBase* logger)
    : storage_engine_(storage_engine), next_query_id_(0), logger_(logger) {
  ResetPreviouslyActiveTrackedQueries();
  for (const TrackedQuery& query : storage_engine_->LoadTrackedQueries()) {
    next_query_id_ = std::max(query.query_id + 1, next_query_id_);
    CacheTrackedQuery(query);
  }
}

TrackedQueryManager::~TrackedQueryManager() {}

const TrackedQuery* TrackedQueryManager::FindTrackedQuery(
    const QuerySpec& query_spec) const {
  QuerySpec normalized = GetNormalizedQuery(query_spec);
  const TrackedQueryMap* set = tracked_query_tree_.GetValueAt(normalized.path);
  return set ? MapGet(set, normalized.params) : nullptr;
}

void TrackedQueryManager::RemoveTrackedQuery(const QuerySpec& query_spec) {
  QuerySpec normalized = GetNormalizedQuery(query_spec);
  const TrackedQuery* tracked_query = FindTrackedQuery(normalized);
  FIREBASE_DEV_ASSERT_MESSAGE(tracked_query, "Query must exist to be removed.");
  storage_engine_->DeleteTrackedQuery(tracked_query->query_id);
  TrackedQueryMap* tracked_queries =
      tracked_query_tree_.GetValueAt(normalized.path);
  tracked_queries->erase(normalized.params);
  if (tracked_queries->empty()) {
    tracked_query_tree_.SetValueAt(normalized.path, std::nullopt);
  }
}

void TrackedQueryManager::SetQueryActiveFlag(
    const QuerySpec& query_spec, TrackedQuery::ActivityStatus activity_status) {
  QuerySpec normalized = GetNormalizedQuery(query_spec);
  const TrackedQuery* tracked_query = FindTrackedQuery(normalized);
  uint64_t last_use = time(nullptr) * 1000;
  if (tracked_query != nullptr) {
    TrackedQuery updated = *tracked_query;
    updated.last_use = last_use;
    updated.active = activity_status;
    SaveTrackedQuery(updated);
  } else {
    FIREBASE_DEV_ASSERT_MESSAGE(
        activity_status == TrackedQuery::kActive,
        "If we're setting the query to inactive, we should already be "
        "tracking it!");
    SaveTrackedQuery(TrackedQuery(next_query_id_++, normalized, last_use,
                                  TrackedQuery::kIncomplete, activity_status));
  }
}

void TrackedQueryManager::SetQueryCompleteIfExists(
    const QuerySpec& query_spec) {
  if (const TrackedQuery* tq =
          FindTrackedQuery(GetNormalizedQuery(query_spec))) {
    if (!tq->complete) {
      TrackedQuery updated = *tq;
      updated.complete = TrackedQuery::kComplete;
      SaveTrackedQuery(updated);
    }
  }
}

void TrackedQueryManager::SetQueriesComplete(const Path& path) {
  tracked_query_tree_.CallOnEach(
      path, [this](const Path& /*p*/, TrackedQueryMap& tracked_queries) {
        for (const auto& [params, tq] : tracked_queries) {
          if (!tq.complete) {
            TrackedQuery updated = tq;
            updated.complete = TrackedQuery::kComplete;
            this->SaveTrackedQuery(updated);
          }
        }
      });
}

bool TrackedQueryManager::IsQueryComplete(const QuerySpec& query_spec) {
  if (IncludedInDefaultCompleteQuery(query_spec.path)) return true;
  if (QuerySpecLoadsAllData(query_spec)) return false;
  TrackedQueryMap* tracked_queries =
      tracked_query_tree_.GetValueAt(query_spec.path);
  const TrackedQuery* tq =
      tracked_queries ? MapGet(tracked_queries, query_spec.params) : nullptr;
  return tq && tq->complete;
}

static uint64_t CalculateCountToPrune(const CachePolicy& cache_policy,
                                      uint64_t prunable_count) {
  double percent_to_keep =
      1.0 - cache_policy.GetPercentOfQueriesToPruneAtOnce();
  uint64_t count_to_keep = std::min(
      static_cast<uint64_t>(prunable_count * percent_to_keep),
      cache_policy.GetMaxNumberOfQueriesToKeep());
  return prunable_count - count_to_keep;
}

PruneForest TrackedQueryManager::PruneOldQueries(
    const CachePolicy& cache_policy) {
  std::vector<TrackedQuery> prunable =
      GetQueriesMatching(IsQueryPrunablePredicate);
  uint64_t count_to_prune =
      CalculateCountToPrune(cache_policy, prunable.size());
  logger_->LogDebug("Pruning old queries. Prunable: %i Count to prune: %i",
                    static_cast<int>(prunable.size()),
                    static_cast<int>(count_to_prune));

  std::sort(prunable.begin(), prunable.end(),
            [](const TrackedQuery& q1, const TrackedQuery& q2) {
              return q1.last_use < q2.last_use;
            });

  PruneForest forest;
  PruneForestRef forest_ref(&forest);
  for (uint64_t i = 0; i < count_to_prune; i++) {
    forest_ref.Prune(prunable[i].query_spec.path);
    RemoveTrackedQuery(prunable[i].query_spec);
  }
  for (uint64_t i = count_to_prune; i < prunable.size(); i++) {
    forest_ref.Keep(prunable[i].query_spec.path);
  }
  std::vector<TrackedQuery> unprunable =
      GetQueriesMatching(IsQueryUnprunablePredicate);
  logger_->LogDebug("Unprunable queries: %i",
                    static_cast<int>(unprunable.size()));
  for (const TrackedQuery& to_keep : unprunable) {
    forest_ref.Keep(to_keep.query_spec.path);
  }
  return forest;
}

std::set<std::string> TrackedQueryManager::GetKnownCompleteChildren(
    const Path& path) {
  FIREBASE_DEV_ASSERT_MESSAGE(!IsQueryComplete(QuerySpec(path)),
                              "Path is fully complete.");
  std::set<std::string> complete_children;
  std::set<QueryId> query_ids = FilteredQueryIdsAtPath(path);
  if (!query_ids.empty()) {
    std::set<std::string> loaded =
        storage_engine_->LoadTrackedQueryKeys(query_ids);
    complete_children.insert(loaded.begin(), loaded.end());
  }
  for (auto& [child_key, child_tree] :
       tracked_query_tree_.GetChild(path)->children()) {
    if (child_tree.value().has_value() &&
        HasDefaultCompletePredicate(*child_tree.value())) {
      complete_children.insert(child_key);
    }
  }
  return complete_children;
}

void TrackedQueryManager::EnsureCompleteTrackedQuery(const Path& path) {
  if (IncludedInDefaultCompleteQuery(path)) return;
  QuerySpec query_spec(path);
  const TrackedQuery* tq = FindTrackedQuery(query_spec);
  if (tq == nullptr) {
    SaveTrackedQuery(TrackedQuery(next_query_id_++, query_spec, 0,
                                  TrackedQuery::kComplete,
                                  TrackedQuery::kInactive));
  } else {
    FIREBASE_DEV_ASSERT_MESSAGE(!tq->complete,
                                "This should have been handled above!");
    TrackedQuery updated = *tq;
    updated.complete = TrackedQuery::kComplete;
    SaveTrackedQuery(updated);
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
  std::optional<TrackedQueryMap>& queries =
      tracked_query_tree_.GetChild(path)->value();
  if (queries.has_value()) {
    for (const auto& [params, query] : *queries) {
      if (!QuerySpecLoadsAllData(query.query_spec)) ids.insert(query.query_id);
    }
  }
  return ids;
}

void TrackedQueryManager::CacheTrackedQuery(const TrackedQuery& tracked_query) {
  FIREBASE_DEV_ASSERT_MESSAGE(
      !QuerySpecLoadsAllData(tracked_query.query_spec) ||
          QuerySpecIsDefault(tracked_query.query_spec),
      "Can't have tracked non-default query that loads all data");
  const Path& path = tracked_query.query_spec.path;
  TrackedQueryMap* tracked_set = tracked_query_tree_.GetValueAt(path);
  if (tracked_set == nullptr) {
    tracked_set =
        &tracked_query_tree_.SetValueAt(path, TrackedQueryMap()).value();
  }
  auto iter = tracked_set->find(tracked_query.query_spec.params);
  FIREBASE_DEV_ASSERT(iter == tracked_set->end() ||
                      iter->second.query_id == tracked_query.query_id);
  (*tracked_set)[tracked_query.query_spec.params] = tracked_query;
}

void TrackedQueryManager::SaveTrackedQuery(const TrackedQuery& tracked_query) {
  CacheTrackedQuery(tracked_query);
  storage_engine_->SaveTrackedQuery(tracked_query);
}

std::vector<TrackedQuery> TrackedQueryManager::GetQueriesMatching(
    TrackedQueryPredicateFn predicate) {
  std::vector<TrackedQuery> matching;
  tracked_query_tree_.CallOnEach(
      Path(), [&matching, predicate](Path /*p*/, TrackedQueryMap& map) {
        for (const auto& [params, tq] : map) {
          if (predicate(tq)) matching.push_back(tq);
        }
      });
  return matching;
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
