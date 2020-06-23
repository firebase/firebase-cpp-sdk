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

#ifndef FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_STORAGE_ENGINE_H_
#define FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_STORAGE_ENGINE_H_

#include <vector>

#include "app/src/path.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/compound_write.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"

namespace firebase {
namespace database {
namespace internal {

class MockPersistenceStorageEngine : public PersistenceStorageEngine {
 public:
  MOCK_METHOD(void, SaveUserOverwrite,
              (const Path& path, const Variant& data, WriteId write_id),
              (override));
  MOCK_METHOD(void, SaveUserMerge,
              (const Path& path, const CompoundWrite& children,
               WriteId write_id),
              (override));
  MOCK_METHOD(void, RemoveUserWrite, (WriteId write_id), (override));
  MOCK_METHOD(std::vector<UserWriteRecord>, LoadUserWrites, (), (override));
  MOCK_METHOD(void, RemoveAllUserWrites, (), (override));
  MOCK_METHOD(Variant, ServerCache, (const Path& path), (override));
  MOCK_METHOD(void, OverwriteServerCache,
              (const Path& path, const Variant& data), (override));
  MOCK_METHOD(void, MergeIntoServerCache,
              (const Path& path, const Variant& data), (override));
  MOCK_METHOD(void, MergeIntoServerCache,
              (const Path& path, const CompoundWrite& children), (override));
  MOCK_METHOD(uint64_t, ServerCacheEstimatedSizeInBytes, (), (const, override));
  MOCK_METHOD(void, SaveTrackedQuery, (const TrackedQuery& tracked_query),
              (override));
  MOCK_METHOD(void, DeleteTrackedQuery, (QueryId tracked_query_id), (override));
  MOCK_METHOD(std::vector<TrackedQuery>, LoadTrackedQueries, (), (override));
  MOCK_METHOD(void, ResetPreviouslyActiveTrackedQueries, (uint64_t last_use),
              (override));
  MOCK_METHOD(void, SaveTrackedQueryKeys,
              (QueryId tracked_query_id, const std::set<std::string>& keys),
              (override));
  MOCK_METHOD(void, UpdateTrackedQueryKeys,
              (QueryId tracked_query_id, const std::set<std::string>& added,
               const std::set<std::string>& removed),
              (override));
  MOCK_METHOD(std::set<std::string>, LoadTrackedQueryKeys,
              (QueryId tracked_query_id), (override));
  MOCK_METHOD(std::set<std::string>, LoadTrackedQueryKeys,
              (const std::set<QueryId>& trackedQueryIds), (override));
  MOCK_METHOD(void, PruneCache,
              (const Path& root, const PruneForestRef& prune_forest),
              (override));
  MOCK_METHOD(bool, BeginTransaction, (), (override));
  MOCK_METHOD(void, EndTransaction, (), (override));
  MOCK_METHOD(void, SetTransactionSuccessful, (), (override));
};

}  // namespace internal
}  // namespace database
}  // namespace firebase

#endif  // FIREBASE_DATABASE_CLIENT_CPP_TESTS_DESKTOP_TEST_MOCK_PERSISTENCE_STORAGE_ENGINE_H_
