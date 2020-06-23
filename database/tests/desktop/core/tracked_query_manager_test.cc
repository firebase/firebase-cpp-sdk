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

#include "app/src/logger.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/persistence/persistence_storage_engine.h"
#include "database/tests/desktop/test/mock_persistence_storage_engine.h"

using testing::_;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAre;

namespace firebase {
namespace database {
namespace internal {

TEST(TrackedQuery, Equality) {
  TrackedQuery query(123, QuerySpec(Path("some/path")), 123,
                     TrackedQuery::kIncomplete, TrackedQuery::kInactive);
  TrackedQuery same(123, QuerySpec(Path("some/path")), 123,
                    TrackedQuery::kIncomplete, TrackedQuery::kInactive);
  TrackedQuery different_query_id(999, QuerySpec(Path("some/path")), 123,
                                  TrackedQuery::kIncomplete,
                                  TrackedQuery::kInactive);
  TrackedQuery different_query_spec(123, QuerySpec(Path("some/other/path")),
                                    123, TrackedQuery::kIncomplete,
                                    TrackedQuery::kInactive);
  TrackedQuery different_complete(123, QuerySpec(Path("some/path")), 123,
                                  TrackedQuery::kComplete,
                                  TrackedQuery::kInactive);
  TrackedQuery different_active(123, QuerySpec(Path("some/path")), 123,
                                TrackedQuery::kIncomplete,
                                TrackedQuery::kActive);

  // Check for equality.
  EXPECT_TRUE(query == same);
  EXPECT_FALSE(query != same);

  // Check each way it can differ.
  EXPECT_FALSE(query == different_query_id);
  EXPECT_TRUE(query != different_query_id);

  EXPECT_FALSE(query == different_query_spec);
  EXPECT_TRUE(query != different_query_spec);

  EXPECT_FALSE(query == different_complete);
  EXPECT_TRUE(query != different_complete);

  EXPECT_FALSE(query == different_active);
  EXPECT_TRUE(query != different_active);
}

TEST(TrackedQueryManager, Constructor) {
  MockPersistenceStorageEngine storage_engine;
  SystemLogger logger;

  InSequence seq;
  EXPECT_CALL(storage_engine, BeginTransaction());
  EXPECT_CALL(storage_engine, ResetPreviouslyActiveTrackedQueries(_));
  EXPECT_CALL(storage_engine, SetTransactionSuccessful());
  EXPECT_CALL(storage_engine, EndTransaction());
  EXPECT_CALL(storage_engine, LoadTrackedQueries());
  TrackedQueryManager manager(&storage_engine, &logger);
}

class TrackedQueryManagerTest : public ::testing::Test {
  void SetUp() override {
    spec_incomplete_inactive_.path = Path("test/path/incomplete_inactive");
    spec_incomplete_active_.path = Path("test/path/incomplete_active");
    spec_complete_inactive_.path = Path("test/path/complete_inactive");
    spec_complete_active_.path = Path("test/path/complete_active");

    // Populate with fake data.
    ON_CALL(storage_engine_, LoadTrackedQueries())
        .WillByDefault(Return(std::vector<TrackedQuery>{
            TrackedQuery(100, spec_incomplete_inactive_, 0,
                         TrackedQuery::kIncomplete, TrackedQuery::kInactive),
            TrackedQuery(200, spec_incomplete_active_, 0,
                         TrackedQuery::kIncomplete, TrackedQuery::kActive),
            TrackedQuery(300, spec_complete_inactive_, 0,
                         TrackedQuery::kComplete, TrackedQuery::kInactive),
            TrackedQuery(400, spec_complete_active_, 0, TrackedQuery::kComplete,
                         TrackedQuery::kActive),
        }));

    manager_ = new TrackedQueryManager(&storage_engine_, &logger_);
  }

  void TearDown() override { delete manager_; }

 protected:
  SystemLogger logger_;
  NiceMock<MockPersistenceStorageEngine> storage_engine_;
  TrackedQueryManager* manager_;

  QuerySpec spec_incomplete_inactive_;
  QuerySpec spec_incomplete_active_;
  QuerySpec spec_complete_inactive_;
  QuerySpec spec_complete_active_;
};

// We need the death tests to be separate from the regular tests, but we still
// want to set up the same data.
class TrackedQueryManagerDeathTest : public TrackedQueryManagerTest {};

TEST_F(TrackedQueryManagerTest, FindTrackedQuery_Success) {
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);
  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_FALSE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_incomplete_active_);
  EXPECT_EQ(result->query_id, 200);
  EXPECT_EQ(result->query_spec, spec_incomplete_active_);
  EXPECT_FALSE(result->complete);
  EXPECT_TRUE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_inactive_);
  EXPECT_EQ(result->query_id, 300);
  EXPECT_EQ(result->query_spec, spec_complete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_active_);
  EXPECT_EQ(result->query_id, 400);
  EXPECT_EQ(result->query_spec, spec_complete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerTest, FindTrackedQuery_Failure) {
  QuerySpec bad_spec(Path("wrong/path"));
  const TrackedQuery* result = manager_->FindTrackedQuery(bad_spec);
  EXPECT_EQ(result, nullptr);
}

TEST_F(TrackedQueryManagerTest, RemoveTrackedQuery) {
  EXPECT_NE(manager_->FindTrackedQuery(spec_incomplete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_incomplete_active_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_active_), nullptr);

  EXPECT_CALL(storage_engine_, DeleteTrackedQuery(100));
  manager_->RemoveTrackedQuery(spec_incomplete_inactive_);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_incomplete_active_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_active_), nullptr);

  EXPECT_CALL(storage_engine_, DeleteTrackedQuery(200));
  manager_->RemoveTrackedQuery(spec_incomplete_active_);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_inactive_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_active_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_active_), nullptr);

  EXPECT_CALL(storage_engine_, DeleteTrackedQuery(300));
  manager_->RemoveTrackedQuery(spec_complete_inactive_);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_inactive_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_active_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_complete_inactive_), nullptr);
  EXPECT_NE(manager_->FindTrackedQuery(spec_complete_active_), nullptr);

  EXPECT_CALL(storage_engine_, DeleteTrackedQuery(400));
  manager_->RemoveTrackedQuery(spec_complete_active_);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_inactive_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_incomplete_active_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_complete_inactive_), nullptr);
  EXPECT_EQ(manager_->FindTrackedQuery(spec_complete_active_), nullptr);
}

TEST_F(TrackedQueryManagerDeathTest, RemoveTrackedQuery_Failure) {
  QuerySpec not_tracked(Path("a/path/not/being/tracked"));
  // Can't remove a query unless you're already tracking it.
  EXPECT_DEATH(manager_->RemoveTrackedQuery(not_tracked), DEATHTEST_SIGABRT);
}

TEST_F(TrackedQueryManagerTest, SetQueryActiveFlag_NewQuery) {
  QuerySpec new_spec(Path("new/active/query"));
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->SetQueryActiveFlag(new_spec, TrackedQuery::kActive);
  const TrackedQuery* result = manager_->FindTrackedQuery(new_spec);

  // result->query_id should be one digit higher than the highest query_id
  // loaded.
  EXPECT_EQ(result->query_id, 401);
  EXPECT_EQ(result->query_spec.params, new_spec.params);
  EXPECT_EQ(result->query_spec.path, new_spec.path);
  EXPECT_FALSE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerTest, SetQueryActiveFlag_ExistingQueryAlreadyTrue) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->SetQueryActiveFlag(spec_complete_active_, TrackedQuery::kActive);
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_complete_active_);

  EXPECT_EQ(result->query_id, 400);
  EXPECT_EQ(result->query_spec, spec_complete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerTest, SetQueryActiveFlag_ExistingQueryWasFalse) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->SetQueryActiveFlag(spec_incomplete_inactive_,
                               TrackedQuery::kActive);
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);

  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_FALSE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerDeathTest, SetQueryInactive_NewQuery) {
  QuerySpec new_spec(Path("new/active/query"));
  // Can't set a query inactive unless you are already tracking it.
  EXPECT_DEATH(manager_->SetQueryActiveFlag(new_spec, TrackedQuery::kInactive),
               DEATHTEST_SIGABRT);
}

TEST_F(TrackedQueryManagerTest, SetQueryInactive_ExistingQuery) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->SetQueryActiveFlag(spec_complete_active_, TrackedQuery::kInactive);
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_complete_active_);

  EXPECT_EQ(result->query_id, 400);
  EXPECT_EQ(result->query_spec, spec_complete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);
}

TEST_F(TrackedQueryManagerTest, SetQueryCompleteIfExists_DoesExist) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->SetQueryCompleteIfExists(spec_incomplete_inactive_);
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);

  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);
}

TEST_F(TrackedQueryManagerTest, SetQueryCompleteIfExists_DoesNotExist) {
  QuerySpec new_spec(Path("new/active/query"));
  manager_->SetQueryCompleteIfExists(new_spec);
  const TrackedQuery* result = manager_->FindTrackedQuery(new_spec);

  EXPECT_EQ(result, nullptr);
}

TEST_F(TrackedQueryManagerTest, SetQueriesComplete_CorrectPath) {
  // Only two of our four TrackedQueries will need to be updated, and thus saved
  // in the database.
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_)).Times(2);
  manager_->SetQueriesComplete(Path("test/path"));

  // All Tracked Queries should be complete.
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);
  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_incomplete_active_);
  EXPECT_EQ(result->query_id, 200);
  EXPECT_EQ(result->query_spec, spec_incomplete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_TRUE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_inactive_);
  EXPECT_EQ(result->query_id, 300);
  EXPECT_EQ(result->query_spec, spec_complete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_active_);
  EXPECT_EQ(result->query_id, 400);
  EXPECT_EQ(result->query_spec, spec_complete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerTest, SetQueriesComplete_IncorrectPath) {
  manager_->SetQueriesComplete(Path("wrong/test/path"));

  // All Tracked Queries should be unchanged.
  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);
  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_FALSE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_incomplete_active_);
  EXPECT_EQ(result->query_id, 200);
  EXPECT_EQ(result->query_spec, spec_incomplete_active_);
  EXPECT_FALSE(result->complete);
  EXPECT_TRUE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_inactive_);
  EXPECT_EQ(result->query_id, 300);
  EXPECT_EQ(result->query_spec, spec_complete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);

  result = manager_->FindTrackedQuery(spec_complete_active_);
  EXPECT_EQ(result->query_id, 400);
  EXPECT_EQ(result->query_spec, spec_complete_active_);
  EXPECT_TRUE(result->complete);
  EXPECT_TRUE(result->active);
}

TEST_F(TrackedQueryManagerTest, IsQueryComplete) {
  EXPECT_FALSE(manager_->IsQueryComplete(spec_incomplete_inactive_));
  EXPECT_FALSE(manager_->IsQueryComplete(spec_incomplete_active_));
  EXPECT_TRUE(manager_->IsQueryComplete(spec_complete_inactive_));
  EXPECT_TRUE(manager_->IsQueryComplete(spec_complete_active_));

  EXPECT_FALSE(manager_->IsQueryComplete(QuerySpec(Path("nonexistant"))));
}

TEST_F(TrackedQueryManagerTest, GetKnownCompleteChildren) {
  EXPECT_THAT(manager_->GetKnownCompleteChildren(Path("test/path")),
              UnorderedElementsAre("complete_inactive", "complete_active"));
}

TEST_F(TrackedQueryManagerTest,
       EnsureCompleteTrackedQuery_ExistingUncompletedQuery) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  manager_->EnsureCompleteTrackedQuery(Path("test/path/incomplete_inactive"));

  const TrackedQuery* result =
      manager_->FindTrackedQuery(spec_incomplete_inactive_);
  EXPECT_EQ(result->query_id, 100);
  EXPECT_EQ(result->query_spec, spec_incomplete_inactive_);
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);
}

TEST_F(TrackedQueryManagerTest, EnsureCompleteTrackedQuery_NewPath) {
  EXPECT_CALL(storage_engine_, SaveTrackedQuery(_));
  Path new_path("new/path");
  manager_->EnsureCompleteTrackedQuery(new_path);

  const TrackedQuery* result = manager_->FindTrackedQuery(QuerySpec(new_path));
  EXPECT_EQ(result->query_id, 401);
  EXPECT_EQ(result->query_spec, QuerySpec(new_path));
  EXPECT_TRUE(result->complete);
  EXPECT_FALSE(result->active);
}

TEST_F(TrackedQueryManagerTest, HasActiveDefaultQuery) {
  EXPECT_FALSE(
      manager_->HasActiveDefaultQuery(Path("test/path/incomplete_inactive")));
  EXPECT_TRUE(
      manager_->HasActiveDefaultQuery(Path("test/path/incomplete_active")));
  EXPECT_FALSE(
      manager_->HasActiveDefaultQuery(Path("test/path/complete_inactive")));
  EXPECT_TRUE(
      manager_->HasActiveDefaultQuery(Path("test/path/complete_active")));

  EXPECT_FALSE(manager_->IsQueryComplete(QuerySpec(Path("nonexistant"))));
}

TEST_F(TrackedQueryManagerTest, CountOfPrunableQueries) {
  EXPECT_EQ(manager_->CountOfPrunableQueries(), 2);
}

}  // namespace internal
}  // namespace database
}  // namespace firebase
