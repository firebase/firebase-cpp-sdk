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

#ifndef FIREBASE_DATABASE_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_
#define FIREBASE_DATABASE_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_

#include <cstdint>
#include <map>
#include <optional>
#include <set>

#include "app/src/logger.h"
#include "app/src/path.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/cache_policy.h"
#include "database/src/desktop/core/tree.h"
#include "database/src/desktop/persistence/prune_forest.h"

namespace firebase {
namespace database {
namespace internal {

class PersistenceStorageEngine;
typedef uint64_t QueryId;

struct TrackedQuery {
  enum CompletionStatus { kIncomplete, kComplete };
  enum ActivityStatus { kInactive, kActive };

  TrackedQuery()
      : query_id(0),
        query_spec(),
        last_use(0),
        complete(false),
        active(false) {}

  TrackedQuery(QueryId _query_id, const QuerySpec& _query_spec,
               uint64_t _last_use, CompletionStatus _complete,
               ActivityStatus _active)
      : query_id(_query_id),
        query_spec(_query_spec),
        last_use(_last_use),
        complete(_complete == kComplete),
        active(_active == kActive) {}

  QueryId query_id;
  QuerySpec query_spec;
  uint64_t last_use;
  bool complete;
  bool active;
};

bool operator==(const TrackedQuery& lhs, const TrackedQuery& rhs);
bool operator!=(const TrackedQuery& lhs, const TrackedQuery& rhs);

typedef std::map<QueryParams, TrackedQuery> TrackedQueryMap;

class TrackedQueryManagerInterface {
 public:
  virtual ~TrackedQueryManagerInterface();
  virtual const TrackedQuery* FindTrackedQuery(
      const QuerySpec& query) const = 0;
  virtual void RemoveTrackedQuery(const QuerySpec& query) = 0;
  virtual void SetQueryActiveFlag(
      const QuerySpec& query, TrackedQuery::ActivityStatus activity_status) = 0;
  virtual void SetQueryCompleteIfExists(const QuerySpec& query) = 0;
  virtual void SetQueriesComplete(const Path& path) = 0;
  virtual bool IsQueryComplete(const QuerySpec& query) = 0;
  virtual PruneForest PruneOldQueries(const CachePolicy& cache_policy) = 0;
  virtual std::set<std::string> GetKnownCompleteChildren(const Path& path) = 0;
  virtual void EnsureCompleteTrackedQuery(const Path& path) = 0;
  virtual bool HasActiveDefaultQuery(const Path& path) = 0;
  virtual uint64_t CountOfPrunableQueries() = 0;
};

class TrackedQueryManager : public TrackedQueryManagerInterface {
 public:
  TrackedQueryManager(PersistenceStorageEngine* storage_engine,
                      LoggerBase* logger);
  ~TrackedQueryManager() override;

  const TrackedQuery* FindTrackedQuery(const QuerySpec& query) const override;
  void RemoveTrackedQuery(const QuerySpec& query) override;
  void SetQueryActiveFlag(
      const QuerySpec& query,
      TrackedQuery::ActivityStatus activity_status) override;
  void SetQueryCompleteIfExists(const QuerySpec& query) override;
  void SetQueriesComplete(const Path& path) override;
  bool IsQueryComplete(const QuerySpec& query) override;
  PruneForest PruneOldQueries(const CachePolicy& cache_policy) override;
  std::set<std::string> GetKnownCompleteChildren(const Path& path) override;
  void EnsureCompleteTrackedQuery(const Path& path) override;
  bool HasActiveDefaultQuery(const Path& path) override;
  uint64_t CountOfPrunableQueries() override;

 private:
  void ResetPreviouslyActiveTrackedQueries();
  bool IncludedInDefaultCompleteQuery(const Path& path);
  std::set<QueryId> FilteredQueryIdsAtPath(const Path& path);
  void CacheTrackedQuery(const TrackedQuery& query);
  void SaveTrackedQuery(const TrackedQuery& query);

  typedef bool (*TrackedQueryPredicateFn)(const TrackedQuery& tracked_query);
  std::vector<TrackedQuery> GetQueriesMatching(
      TrackedQueryPredicateFn predicate);

  PersistenceStorageEngine* storage_engine_;
  Tree<TrackedQueryMap> tracked_query_tree_;
  QueryId next_query_id_;
  LoggerBase* logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_
