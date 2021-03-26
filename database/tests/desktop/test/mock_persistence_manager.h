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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_MANAGER_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_MANAGER_H_

#include "app/src/include/firebase/variant.h"
#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/cache_policy.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/persistence/persistence_manager.h"
#include "database/src/desktop/view/view_cache.h"

namespace firebase {
namespace database {
namespace internal {

class MockPersistenceManager : public PersistenceManager {
 public:
  MockPersistenceManager(
      UniquePtr<PersistenceStorageEngine> storage_engine,
      UniquePtr<TrackedQueryManagerInterface> tracked_query_manager,
      UniquePtr<CachePolicy> cache_policy, LoggerBase* logger)
      : PersistenceManager(std::move(storage_engine),
                           std::move(tracked_query_manager),
                           std::move(cache_policy), logger) {}
  ~MockPersistenceManager() override {}

  MOCK_METHOD(void, SaveUserOverwrite,
              (const Path& path, const Variant& variant, WriteId write_id),
              (override));
  MOCK_METHOD(void, SaveUserMerge,
              (const Path& path, const CompoundWrite& children,
               WriteId write_id),
              (override));
  MOCK_METHOD(void, RemoveUserWrite, (WriteId write_id), (override));
  MOCK_METHOD(void, RemoveAllUserWrites, (), (override));
  MOCK_METHOD(void, ApplyUserWriteToServerCache,
              (const Path& path, const Variant& variant), (override));
  MOCK_METHOD(void, ApplyUserWriteToServerCache,
              (const Path& path, const CompoundWrite& merge), (override));
  MOCK_METHOD(std::vector<UserWriteRecord>, LoadUserWrites, (), (override));
  MOCK_METHOD(CacheNode, ServerCache, (const QuerySpec& query), (override));
  MOCK_METHOD(void, UpdateServerCache,
              (const QuerySpec& query, const Variant& variant), (override));
  MOCK_METHOD(void, UpdateServerCache,
              (const Path& path, const CompoundWrite& children), (override));
  MOCK_METHOD(void, SetQueryActive, (const QuerySpec& query), (override));
  MOCK_METHOD(void, SetQueryInactive, (const QuerySpec& query), (override));
  MOCK_METHOD(void, SetQueryComplete, (const QuerySpec& query), (override));
  MOCK_METHOD(void, SetTrackedQueryKeys,
              (const QuerySpec& query, const std::set<std::string>& keys),
              (override));
  MOCK_METHOD(void, UpdateTrackedQueryKeys,
              (const QuerySpec& query, const std::set<std::string>& added,
               const std::set<std::string>& removed),
              (override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_MANAGER_H_
