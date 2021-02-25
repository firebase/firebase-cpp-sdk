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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_TRACKED_QUERY_MANAGER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_TRACKED_QUERY_MANAGER_H_

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/common/query_spec.h"
#include "database/src/desktop/core/tracked_query_manager.h"

namespace firebase {
namespace database {
namespace internal {

class MockTrackedQueryManager : public TrackedQueryManagerInterface {
 public:
  MOCK_METHOD(const TrackedQuery*, FindTrackedQuery, (const QuerySpec& query),
              (const, override));
  MOCK_METHOD(void, RemoveTrackedQuery, (const QuerySpec& query), (override));
  MOCK_METHOD(void, SetQueryActiveFlag,
              (const QuerySpec& query,
               TrackedQuery::ActivityStatus activity_status),
              (override));
  MOCK_METHOD(void, SetQueryCompleteIfExists, (const QuerySpec& query),
              (override));
  MOCK_METHOD(void, SetQueriesComplete, (const Path& path), (override));
  MOCK_METHOD(bool, IsQueryComplete, (const QuerySpec& query), (override));
  MOCK_METHOD(PruneForest, PruneOldQueries, (const CachePolicy& cache_policy),
              (override));
  MOCK_METHOD(std::set<std::string>, GetKnownCompleteChildren,
              (const Path& path), (override));
  MOCK_METHOD(void, EnsureCompleteTrackedQuery, (const Path& path), (override));
  MOCK_METHOD(bool, HasActiveDefaultQuery, (const Path& path), (override));
  MOCK_METHOD(uint64_t, CountOfPrunableQueries, (), (override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase
#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_TRACKED_QUERY_MANAGER_H_
