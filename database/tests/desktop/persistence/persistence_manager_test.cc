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

#include "database/src/desktop/persistence/persistence_manager.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/tests/desktop/test/mock_cache_policy.h"
#include "database/tests/desktop/test/mock_persistence_storage_engine.h"
#include "database/tests/desktop/test/mock_tracked_query_manager.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace firebase {
namespace database {
namespace internal {
namespace {

class PersistenceManagerTest : public Test {
 public:
  void SetUp() override {
    storage_engine_ = new NiceMock<MockPersistenceStorageEngine>();
    UniquePtr<MockPersistenceStorageEngine> storage_engine_ptr(storage_engine_);

    tracked_query_manager_ = new NiceMock<MockTrackedQueryManager>();
    UniquePtr<MockTrackedQueryManager> tracked_query_manager_ptr(
        tracked_query_manager_);

    cache_policy_ = new NiceMock<MockCachePolicy>();
    UniquePtr<MockCachePolicy> cache_policy_ptr(cache_policy_);

    manager_ = new PersistenceManager(std::move(storage_engine_ptr),
                                      std::move(tracked_query_manager_ptr),
                                      std::move(cache_policy_ptr), &logger_);
  }

  void TearDown() override { delete manager_; }

 protected:
  MockPersistenceStorageEngine* storage_engine_;
  MockTrackedQueryManager* tracked_query_manager_;
  MockCachePolicy* cache_policy_;
  SystemLogger logger_;

  PersistenceManager* manager_;
};

TEST_F(PersistenceManagerTest, SaveUserOverwrite) {
  EXPECT_CALL(
      *storage_engine_,
      SaveUserOverwrite(Path("test/path"), Variant("test_variant"), 100));

  manager_->SaveUserOverwrite(Path("test/path"), Variant("test_variant"), 100);
}

TEST_F(PersistenceManagerTest, SaveUserMerge) {
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);

  EXPECT_CALL(*storage_engine_, SaveUserMerge(Path("test/path"), write, 100));

  manager_->SaveUserMerge(Path("test/path"), write, 100);
}

TEST_F(PersistenceManagerTest, RemoveUserWrite) {
  EXPECT_CALL(*storage_engine_, RemoveUserWrite(100));

  manager_->RemoveUserWrite(100);
}

TEST_F(PersistenceManagerTest, RemoveAllUserWrites) {
  EXPECT_CALL(*storage_engine_, RemoveAllUserWrites());

  manager_->RemoveAllUserWrites();
}

TEST_F(PersistenceManagerTest, ApplyUserWriteToServerCacheWithoutActiveQuery) {
  // If there is no active default query, we expect it to apply the variant to
  // the storage engine at the given path.
  EXPECT_CALL(*tracked_query_manager_, HasActiveDefaultQuery(Path("abc")))
      .WillOnce(Return(false));
  EXPECT_CALL(*storage_engine_,
              OverwriteServerCache(Path("abc"), Variant("zyx")));
  EXPECT_CALL(*tracked_query_manager_, EnsureCompleteTrackedQuery(Path("abc")));

  manager_->ApplyUserWriteToServerCache(Path("abc"), "zyx");
}

TEST_F(PersistenceManagerTest, ApplyUserWriteToServerCacheWithActiveQuery) {
  // If there is an active default query, nothing should happen.
  EXPECT_CALL(*tracked_query_manager_, HasActiveDefaultQuery(Path("abc")))
      .WillOnce(Return(true));

  manager_->ApplyUserWriteToServerCache(Path("abc"), Variant("zyx"));
}

TEST_F(PersistenceManagerTest, ApplyUserWriteToServerCacheWithCompoundWrite) {
  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);

  EXPECT_CALL(*tracked_query_manager_, HasActiveDefaultQuery(_))
      .WillRepeatedly(Return(false));

  EXPECT_CALL(*storage_engine_, OverwriteServerCache(Path("aaa"), Variant(1)));
  EXPECT_CALL(*tracked_query_manager_, EnsureCompleteTrackedQuery(Path("aaa")));

  EXPECT_CALL(*storage_engine_, OverwriteServerCache(Path("bbb"), Variant(2)));
  EXPECT_CALL(*tracked_query_manager_, EnsureCompleteTrackedQuery(Path("bbb")));

  EXPECT_CALL(*storage_engine_,
              OverwriteServerCache(Path("ccc/ddd"), Variant(3)));
  EXPECT_CALL(*tracked_query_manager_,
              EnsureCompleteTrackedQuery(Path("ccc/ddd")));

  EXPECT_CALL(*storage_engine_,
              OverwriteServerCache(Path("ccc/eee"), Variant(4)));
  EXPECT_CALL(*tracked_query_manager_,
              EnsureCompleteTrackedQuery(Path("ccc/eee")));

  manager_->ApplyUserWriteToServerCache(Path(), write);
}

TEST_F(PersistenceManagerTest, LoadUserWrites) {
  EXPECT_CALL(*storage_engine_, LoadUserWrites());
  manager_->LoadUserWrites();
}

TEST_F(PersistenceManagerTest, ServerCache_QueryComplete) {
  QuerySpec query_spec;
  query_spec.params.start_at_value = "zzz";
  query_spec.path = Path("abc");

  TrackedQuery tracked_query;
  tracked_query.query_id = 1234;
  tracked_query.active = true;
  tracked_query.complete = true;

  std::set<std::string> tracked_keys{"aaa", "ccc"};

  Variant server_cache(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  });

  EXPECT_CALL(*tracked_query_manager_, IsQueryComplete(query_spec))
      .WillOnce(Return(true));
  EXPECT_CALL(*tracked_query_manager_, FindTrackedQuery(query_spec))
      .WillOnce(Return(&tracked_query));
  EXPECT_CALL(*storage_engine_, LoadTrackedQueryKeys(1234))
      .WillOnce(Return(tracked_keys));
  EXPECT_CALL(*storage_engine_, ServerCache(Path("abc")))
      .WillOnce(Return(server_cache));

  CacheNode result = manager_->ServerCache(query_spec);
  CacheNode expected_result(
      IndexedVariant(Variant(std::map<Variant, Variant>{
                         std::make_pair("aaa", 1),
                         std::make_pair("ccc",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ddd", 3),
                                            std::make_pair("eee", 4),
                                        }),
                     }),
                     query_spec.params),
      true, true);

  EXPECT_EQ(result, expected_result);
}

TEST_F(PersistenceManagerTest, ServerCache_QueryIncomplete) {
  QuerySpec query_spec;
  query_spec.params.start_at_value = "zzz";
  query_spec.path = Path("abc");

  TrackedQuery tracked_query;
  tracked_query.query_id = 1234;
  tracked_query.active = true;
  tracked_query.complete = false;

  std::set<std::string> tracked_keys{"aaa", "ccc"};

  Variant server_cache(std::map<Variant, Variant>{
      std::make_pair("aaa", 1),
      std::make_pair("bbb", 2),
      std::make_pair("ccc",
                     std::map<Variant, Variant>{
                         std::make_pair("ddd", 3),
                         std::make_pair("eee", 4),
                     }),
  });

  EXPECT_CALL(*tracked_query_manager_, IsQueryComplete(query_spec))
      .WillOnce(Return(false));
  EXPECT_CALL(*tracked_query_manager_, GetKnownCompleteChildren(Path("abc")))
      .WillOnce(Return(tracked_keys));
  EXPECT_CALL(*storage_engine_, ServerCache(Path("abc")))
      .WillOnce(Return(server_cache));

  CacheNode result = manager_->ServerCache(query_spec);
  CacheNode expected_result(
      IndexedVariant(Variant(std::map<Variant, Variant>{
                         std::make_pair("aaa", 1),
                         std::make_pair("ccc",
                                        std::map<Variant, Variant>{
                                            std::make_pair("ddd", 3),
                                            std::make_pair("eee", 4),
                                        }),
                     }),
                     query_spec.params),
      false, true);

  EXPECT_EQ(result, expected_result);
}

TEST_F(PersistenceManagerTest, UpdateServerCache_LoadsAllData) {
  Path path;
  Variant variant;
  QuerySpec query_spec;
  query_spec.path = path;

  EXPECT_CALL(*storage_engine_, OverwriteServerCache(path, variant));

  manager_->UpdateServerCache(query_spec, variant);
}

TEST_F(PersistenceManagerTest, UpdateServerCache_DoesntLoadAllData) {
  Path path;
  Variant variant;
  QuerySpec query_spec;
  query_spec.params.start_at_value = "bbb";
  query_spec.path = path;

  EXPECT_CALL(*storage_engine_, MergeIntoServerCache(path, variant));

  manager_->UpdateServerCache(query_spec, variant);
}

TEST_F(PersistenceManagerTest, UpdateServerCache_WithCompoundWrite) {
  Path path;

  const std::map<Path, Variant>& merge{
      std::make_pair(Path("aaa"), 1),
      std::make_pair(Path("bbb"), 2),
      std::make_pair(Path("ccc/ddd"), 3),
      std::make_pair(Path("ccc/eee"), 4),
  };
  CompoundWrite write = CompoundWrite::FromPathMerge(merge);

  EXPECT_CALL(*storage_engine_, MergeIntoServerCache(path, write));

  manager_->UpdateServerCache(path, write);
}

TEST_F(PersistenceManagerTest, SetQueryActive) {
  EXPECT_CALL(*tracked_query_manager_,
              SetQueryActiveFlag(QuerySpec(), TrackedQuery::kActive));

  manager_->SetQueryActive(QuerySpec());
}

TEST_F(PersistenceManagerTest, SetQueryInactive) {
  EXPECT_CALL(*tracked_query_manager_,
              SetQueryActiveFlag(QuerySpec(), TrackedQuery::kInactive));

  manager_->SetQueryInactive(QuerySpec());
}

TEST_F(PersistenceManagerTest, SetQueryComplete) {
  QuerySpec loads_all_data;
  loads_all_data.path = Path("aaa");
  QuerySpec does_not_load_all_data;
  does_not_load_all_data.path = Path("bbb");
  does_not_load_all_data.params.start_at_value = "abc";

  EXPECT_CALL(*tracked_query_manager_, SetQueriesComplete(Path("aaa")));
  manager_->SetQueryComplete(loads_all_data);

  EXPECT_CALL(*tracked_query_manager_,
              SetQueryCompleteIfExists(does_not_load_all_data));
  manager_->SetQueryComplete(does_not_load_all_data);
}

TEST_F(PersistenceManagerTest, SetTrackedQueryKeys) {
  QuerySpec query_spec;
  query_spec.params.start_at_value = "baa";
  std::set<std::string> keys{"foo", "bar", "baz"};

  TrackedQuery tracked_query;
  tracked_query.query_id = 1234;
  tracked_query.active = true;
  EXPECT_CALL(*tracked_query_manager_, FindTrackedQuery(query_spec))
      .WillOnce(Return(&tracked_query));
  EXPECT_CALL(*storage_engine_, SaveTrackedQueryKeys(1234, keys));

  manager_->SetTrackedQueryKeys(query_spec, keys);
}

TEST_F(PersistenceManagerTest, UpdateTrackedQueryKeys) {
  QuerySpec query_spec;
  query_spec.params.start_at_value = "baa";
  std::set<std::string> added{"foo", "bar", "baz"};
  std::set<std::string> removed{"qux", "quux", "quuz"};

  TrackedQuery tracked_query;
  tracked_query.query_id = 9876;
  tracked_query.active = true;
  EXPECT_CALL(*tracked_query_manager_, FindTrackedQuery(query_spec))
      .WillOnce(Return(&tracked_query));
  EXPECT_CALL(*storage_engine_, UpdateTrackedQueryKeys(9876, added, removed));

  manager_->UpdateTrackedQueryKeys(query_spec, added, removed);
}

TEST(PersistenceManager, DoPruneCheckAfterServerUpdate_DoNotCheckCacheSize) {
  MockPersistenceStorageEngine* storage_engine =
      new NiceMock<MockPersistenceStorageEngine>();
  UniquePtr<MockPersistenceStorageEngine> storage_engine_ptr(storage_engine);

  MockTrackedQueryManager* tracked_query_manager =
      new NiceMock<MockTrackedQueryManager>();
  UniquePtr<MockTrackedQueryManager> tracked_query_manager_ptr(
      tracked_query_manager);

  MockCachePolicy* cache_policy = new StrictMock<MockCachePolicy>();
  UniquePtr<MockCachePolicy> cache_policy_ptr(cache_policy);

  SystemLogger logger;
  PersistenceManager manager(std::move(storage_engine_ptr),
                             std::move(tracked_query_manager_ptr),
                             std::move(cache_policy_ptr), &logger);

  // After the server cache is updated, DoPruneCheckAfterServerUpdate will be
  // called. It should call CachePolicy::ShouldCheckCacheSize once, and if it
  // returns false, it should not do anything else.
  EXPECT_CALL(*cache_policy, ShouldCheckCacheSize(_)).WillOnce(Return(false));

  manager.UpdateServerCache(QuerySpec(), Variant());
}

TEST(PersistenceManager, DoPruneCheckAfterServerUpdate_DoCheckCacheSize) {
  MockPersistenceStorageEngine* storage_engine =
      new NiceMock<MockPersistenceStorageEngine>();
  UniquePtr<MockPersistenceStorageEngine> storage_engine_ptr(storage_engine);

  MockTrackedQueryManager* tracked_query_manager =
      new NiceMock<MockTrackedQueryManager>();
  UniquePtr<MockTrackedQueryManager> tracked_query_manager_ptr(
      tracked_query_manager);

  MockCachePolicy* cache_policy = new StrictMock<MockCachePolicy>();
  UniquePtr<MockCachePolicy> cache_policy_ptr(cache_policy);

  SystemLogger logger;
  PersistenceManager manager(std::move(storage_engine_ptr),
                             std::move(tracked_query_manager_ptr),
                             std::move(cache_policy_ptr), &logger);

  // After the server cache is updated, DoPruneCheckAfterServerUpdate will be
  // called. It should call CachePolicy::ShouldCheckCacheSize once, and if it
  // returns true, it will then check if it should prune anything. If
  // CachePolicy::ShouldPrune returns false, nothing else will happen.
  EXPECT_CALL(*cache_policy, ShouldCheckCacheSize(_)).WillOnce(Return(true));
  EXPECT_CALL(*cache_policy, ShouldPrune(_, _)).WillOnce(Return(false));

  manager.UpdateServerCache(QuerySpec(), Variant());
}

TEST(PersistenceManager, DoPruneCheckAfterServerUpdate_PruneStuff) {
  MockPersistenceStorageEngine* storage_engine =
      new NiceMock<MockPersistenceStorageEngine>();
  UniquePtr<MockPersistenceStorageEngine> storage_engine_ptr(storage_engine);

  MockTrackedQueryManager* tracked_query_manager =
      new NiceMock<MockTrackedQueryManager>();
  UniquePtr<MockTrackedQueryManager> tracked_query_manager_ptr(
      tracked_query_manager);

  MockCachePolicy* cache_policy = new StrictMock<MockCachePolicy>();
  UniquePtr<MockCachePolicy> cache_policy_ptr(cache_policy);

  SystemLogger logger;
  PersistenceManager manager(std::move(storage_engine_ptr),
                             std::move(tracked_query_manager_ptr),
                             std::move(cache_policy_ptr), &logger);

  // After the server cache is updated, DoPruneCheckAfterServerUpdate will be
  // called. It should call CachePolicy::ShouldCheckCacheSize once, and if it
  // returns true, it will then check if it should prune anything. If
  // CachePolicy::ShouldPrune returns true, it will pass the prune tree to
  // StorageEngine::PruneCache.
  EXPECT_CALL(*cache_policy, ShouldCheckCacheSize(_)).WillOnce(Return(true));
  EXPECT_CALL(*cache_policy, ShouldPrune(_, _))
      .WillOnce(Return(true))
      .WillOnce(Return(false));
  PruneForest prune_forest;
  prune_forest.set_value(true);
  EXPECT_CALL(*tracked_query_manager, PruneOldQueries(_))
      .WillOnce(Return(prune_forest));
  EXPECT_CALL(*storage_engine,
              PruneCache(Path(), PruneForestRef(&prune_forest)));

  manager.UpdateServerCache(QuerySpec(), Variant());
}

TEST_F(PersistenceManagerTest, RunInTransaction_StdFunctionSuccess) {
  EXPECT_CALL(*storage_engine_, BeginTransaction());
  EXPECT_CALL(*storage_engine_, SetTransactionSuccessful());
  EXPECT_CALL(*storage_engine_, EndTransaction());
  bool function_called = false;
  EXPECT_TRUE(manager_->RunInTransaction([&]() {
    function_called = true;
    return true;
  }));
  EXPECT_TRUE(function_called);
}

TEST_F(PersistenceManagerTest, RunInTransaction_StdFunctionFailure) {
  EXPECT_CALL(*storage_engine_, BeginTransaction());
  EXPECT_CALL(*storage_engine_, EndTransaction());
  bool function_called = false;
  EXPECT_FALSE(manager_->RunInTransaction([&]() {
    function_called = true;
    return false;
  }));
  EXPECT_TRUE(function_called);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
