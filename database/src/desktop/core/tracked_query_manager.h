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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_

#include <cstdint>
#include <map>
#include <set>

#include "app/src/logger.h"
#include "app/src/optional.h"
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

  // The globally unique identifier for this tracked query.
  QueryId query_id;
  // The QuerySpec representing this query. That is, the location that is being
  // watched, and the set of filters that are applied to it.
  QuerySpec query_spec;
  // The last time this query was updated.
  uint64_t last_use;
  // This query is considered complete if it is not being filtered.
  bool complete;
  // Whether this query is actively being watched. If not, it may be pruned.
  bool active;
};

bool operator==(const TrackedQuery& lhs, const TrackedQuery& rhs);
bool operator!=(const TrackedQuery& lhs, const TrackedQuery& rhs);

typedef std::map<QueryParams, TrackedQuery> TrackedQueryMap;

class TrackedQueryManagerInterface {
 public:
  virtual ~TrackedQueryManagerInterface();

  // Find and return the TrackedQuery associated with the given QuerySpec, or
  // nullptr if there is no associated TrackedQuery.
  virtual const TrackedQuery* FindTrackedQuery(
      const QuerySpec& query) const = 0;

  // Remove the TrackedQuery that is associated with the given QuerySpec. The
  // QuerySpec must have an associated TrackedQuery.
  virtual void RemoveTrackedQuery(const QuerySpec& query) = 0;

  // Set or clear the active flag on the TrackedQuery associated with the given
  // QuerySpec. If setting the query active, the TrackedQuery will be created if
  // it doesn't already exist. The query must already exist to set it inactive.
  virtual void SetQueryActiveFlag(
      const QuerySpec& query, TrackedQuery::ActivityStatus activity_status) = 0;

  // Set the TrackedQuery associated with the given QuerySpec to complete if it
  // exists.
  virtual void SetQueryCompleteIfExists(const QuerySpec& query) = 0;

  // Set the TrackedQueries at and below the given path to complete.
  virtual void SetQueriesComplete(const Path& path) = 0;

  // Check if the TrackedQuery associated with the given QuerySpec is
  // complete.
  virtual bool IsQueryComplete(const QuerySpec& query) = 0;

  // Remove queries that no longer need to be cached based on the given cache
  // policy.
  virtual PruneForest PruneOldQueries(const CachePolicy& cache_policy) = 0;

  // Return the keys of the completed TrackedQueries at the given location.
  virtual std::set<std::string> GetKnownCompleteChildren(const Path& path) = 0;

  // Set the TrackedQuery associated with the given QuerySpec to complete and
  // create it if it doesn't exist.
  virtual void EnsureCompleteTrackedQuery(const Path& path) = 0;

  // Returns true if there is an active QuerySpec at the given path.
  virtual bool HasActiveDefaultQuery(const Path& path) = 0;

  // Returns the number of TrackedQueries that can be pruned (i.e. are
  // inactive).
  virtual uint64_t CountOfPrunableQueries() = 0;
};

class TrackedQueryManager : public TrackedQueryManagerInterface {
 public:
  TrackedQueryManager(PersistenceStorageEngine* storage_engine,
                      LoggerBase* logger);

  ~TrackedQueryManager() override;

  // Find and return the TrackedQuery associated with the given QuerySpec, or
  // nullptr if there is no associated TrackedQuery.
  const TrackedQuery* FindTrackedQuery(const QuerySpec& query) const override;

  // Remove the TrackedQuery that is associated with the given QuerySpec. The
  // QuerySpec must have an associated TrackedQuery.
  void RemoveTrackedQuery(const QuerySpec& query) override;

  // Set or clear the active flag on the TrackedQuery associated with the given
  // QuerySpec. If setting the query active, the TrackedQuery will be created if
  // it doesn't already exist. The query must already exist to set it inactive.
  void SetQueryActiveFlag(
      const QuerySpec& query,
      TrackedQuery::ActivityStatus activity_status) override;

  // Set the TrackedQuery associated with the given QuerySpec to complete if it
  // exists.
  void SetQueryCompleteIfExists(const QuerySpec& query) override;

  // Set the TrackedQueries at and below the given path to complete.
  void SetQueriesComplete(const Path& path) override;

  // Check if the TrackedQuery associated with the given QuerySpec is
  // complete.
  bool IsQueryComplete(const QuerySpec& query) override;

  // Remove queries that no longer need to be cached based on the given cache
  // policy.
  PruneForest PruneOldQueries(const CachePolicy& cache_policy) override;

  // Return the keys of the completed TrackedQueries at the given location.
  std::set<std::string> GetKnownCompleteChildren(const Path& path) override;

  // Set the TrackedQuery associated with the given QuerySpec to complete and
  // create it if it doesn't exist.
  void EnsureCompleteTrackedQuery(const Path& path) override;

  // Returns true if there is an active QuerySpec at the given path.
  bool HasActiveDefaultQuery(const Path& path) override;

  // Returns the number of TrackedQueries that can be pruned (i.e. are
  // inactive).
  uint64_t CountOfPrunableQueries() override;

 private:
  // Resets the timestamp on active tracked queries.
  void ResetPreviouslyActiveTrackedQueries();

  // Returns true if the given path has a complete query.
  bool IncludedInDefaultCompleteQuery(const Path& path);

  // Return the set of QueryIds at the given path.
  std::set<QueryId> FilteredQueryIdsAtPath(const Path& path);

  // Add a tracked query to the cache, overwriting the existing value if
  // necessary.
  void CacheTrackedQuery(const TrackedQuery& query);

  // Persist a tracked query to storage, caching it in the process.
  void SaveTrackedQuery(const TrackedQuery& query);

  // The function signature for functions that are going to be used to check the
  // list of queries at a point in the database.
  typedef bool (*TrackedQueryPredicateFn)(const TrackedQuery& tracked_query);

  // Return the list of TrackedQueries that match the given predicate.
  std::vector<TrackedQuery> GetQueriesMatching(
      TrackedQueryPredicateFn predicate);

  // DB, where we permanently store tracked queries.
  PersistenceStorageEngine* storage_engine_;

  // In-memory cache of tracked queries.  Should always be in-sync with the DB.
  Tree<TrackedQueryMap> tracked_query_tree_;

  // ID we'll assign to the next tracked query.
  QueryId next_query_id_;

  LoggerBase* logger_;
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_SRC_DESKTOP_CORE_TRACKED_QUERY_MANAGER_H_
