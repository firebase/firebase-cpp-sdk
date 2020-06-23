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

#include "database/src/desktop/persistence/in_memory_persistence_storage_engine.h"

#include "app/src/logger.h"
#include "app/src/variant_util.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "database/src/desktop/core/compound_write.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(InMemoryPersistenceStorageEngine, Constructor) {
  SystemLogger logger;
  InMemoryPersistenceStorageEngine engine(&logger);

  // Ensure there is no crash.
  (void)engine;
}

class InMemoryPersistenceStorageEngineTest : public ::testing::Test {
 public:
  InMemoryPersistenceStorageEngineTest() : logger_(), engine_(&logger_) {}

  ~InMemoryPersistenceStorageEngineTest() override {}

 protected:
  SystemLogger logger_;
  InMemoryPersistenceStorageEngine engine_;
};

typedef InMemoryPersistenceStorageEngineTest
    InMemoryPersistenceStorageEngineDeathTest;

TEST_F(InMemoryPersistenceStorageEngineTest, LoadServerCache) {
  // This is all in-memory, so nothing to read from disk.
  EXPECT_EQ(engine_.LoadServerCache(), Variant::Null());
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, SaveUserOverwrite) {
  EXPECT_DEATH(engine_.SaveUserOverwrite(Path(), Variant::Null(), 100),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, SaveUserOverwrite) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.SaveUserOverwrite(Path(), Variant::Null(), 100);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, SaveUserMerge) {
  EXPECT_DEATH(engine_.SaveUserMerge(Path(), CompoundWrite(), 100),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, SaveUserMerge) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.SaveUserMerge(Path(), CompoundWrite(), 100);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, RemoveUserWrite) {
  EXPECT_DEATH(engine_.RemoveUserWrite(100), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, RemoveUserWrite) {
  engine_.BeginTransaction();
  engine_.RemoveUserWrite(100);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineTest, LoadUserWrites) {
  // This is all in-memory, so nothing to read from disk.
  EXPECT_TRUE(engine_.LoadUserWrites().empty());
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, RemoveAllUserWrites) {
  // Must be in a transaction.
  EXPECT_DEATH(engine_.RemoveAllUserWrites(), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, RemoveAllUserWrites) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.RemoveAllUserWrites();
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, OverwriteServerCache) {
  EXPECT_DEATH(engine_.OverwriteServerCache(Path(), Variant::Null()),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, OverwriteServerCache) {
  engine_.BeginTransaction();
  engine_.OverwriteServerCache(Path("aaa/bbb/ccc"), 100);
  engine_.OverwriteServerCache(Path("aaa/bbb/ddd"), 200);
  engine_.OverwriteServerCache(Path("zzz/yyy/xxx"), 300);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();

  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ccc")), 100);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ddd")), 200);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb")),
            Variant(std::map<Variant, Variant>{{"ccc", 100}, {"ddd", 200}}));
  // clang-format off
  EXPECT_EQ(
      engine_.ServerCache(Path()),
      Variant(std::map<Variant, Variant>{
        {"aaa", std::map<Variant, Variant>{
            {"bbb", std::map<Variant, Variant>{
                {"ccc", 100},
                {"ddd", 200},
            }}
        }},
        {"zzz", std::map<Variant, Variant>{
            {"yyy", std::map<Variant, Variant>{
                {"xxx", 300}}
            }}
        }}));
  // clang-format on
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest,
       MergeIntoServerCache_Variant) {
  EXPECT_DEATH(engine_.MergeIntoServerCache(Path(), Variant::Null()),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, MergeIntoServerCache_Variant) {
  engine_.BeginTransaction();
  engine_.OverwriteServerCache(Path("aaa/bbb/ccc"), 100);
  engine_.OverwriteServerCache(Path("aaa/bbb/ddd"), 200);
  engine_.OverwriteServerCache(Path("zzz/yyy/xxx"), 300);

  engine_.MergeIntoServerCache(
      Path("aaa/bbb"), std::map<Variant, Variant>{{"ccc", 400}, {"eee", 500}});

  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();

  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ccc")), 400);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ddd")), 200);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/eee")), 500);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb")),
            Variant(std::map<Variant, Variant>{
                {"ccc", 400}, {"ddd", 200}, {"eee", 500}}));
  // clang-format off
  EXPECT_EQ(
      engine_.ServerCache(Path()),
      Variant(std::map<Variant, Variant>{
        {"aaa", std::map<Variant, Variant>{
            {"bbb", std::map<Variant, Variant>{
                {"ccc", 400},
                {"ddd", 200},
                {"eee", 500}
            }}
        }},
        {"zzz", std::map<Variant, Variant>{
            {"yyy", std::map<Variant, Variant>{
                {"xxx", 300}}
            }}
        }}));
  // clang-format on
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest,
       MergeIntoServerCache_CompoundWrite) {
  EXPECT_DEATH(engine_.MergeIntoServerCache(Path(), CompoundWrite()),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest,
       MergeIntoServerCache_CompoundWrite) {
  engine_.BeginTransaction();
  engine_.OverwriteServerCache(Path("aaa/bbb/ccc"), 100);
  engine_.OverwriteServerCache(Path("aaa/bbb/ddd"), 200);
  engine_.OverwriteServerCache(Path("zzz/yyy/xxx"), 300);

  CompoundWrite write;
  write = write.AddWrite(Path("ccc"), 400);
  write = write.AddWrite(Path("eee"), 500);

  engine_.MergeIntoServerCache(Path("aaa/bbb"), write);

  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();

  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ccc")), 400);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/ddd")), 200);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb/eee")), 500);
  EXPECT_EQ(engine_.ServerCache(Path("aaa/bbb")),
            Variant(std::map<Variant, Variant>{
                {"ccc", 400}, {"ddd", 200}, {"eee", 500}}));
  // clang-format off
  EXPECT_EQ(
      engine_.ServerCache(Path()),
      Variant(std::map<Variant, Variant>{
        {"aaa", std::map<Variant, Variant>{
            {"bbb", std::map<Variant, Variant>{
                {"ccc", 400},
                {"ddd", 200},
                {"eee", 500}
            }}
        }},
        {"zzz", std::map<Variant, Variant>{
            {"yyy", std::map<Variant, Variant>{
                {"xxx", 300}}
            }}
        }}));
  // clang-format on
}

TEST_F(InMemoryPersistenceStorageEngineTest, ServerCacheEstimatedSizeInBytes) {
  engine_.BeginTransaction();
  engine_.OverwriteServerCache(Path("aaaa/bbbb"),
                               Variant::FromMutableString("abcdefghijklm"));
  engine_.OverwriteServerCache(Path("aaaa/cccc"),
                               Variant::FromMutableString("nopqrstuvwxyz"));
  engine_.OverwriteServerCache(Path("aaaa/dddd"), Variant::FromInt64(12345));
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();

  const int kKeyLengths = 4;     // The keys used above are 4 characters;
  const int kValueLengths = 13;  // The values used above are 13 characters;
  EXPECT_EQ(engine_.ServerCacheEstimatedSizeInBytes(),
            9 * sizeof(Variant) + 4 * kKeyLengths + 2 * kValueLengths);
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, SaveTrackedQuery) {
  // Must be in a transaction.
  EXPECT_DEATH(engine_.SaveTrackedQuery(TrackedQuery()), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, SaveTrackedQuery) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.SaveTrackedQuery(TrackedQuery());
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, DeleteTrackedQuery) {
  // Must be in a transaction.
  EXPECT_DEATH(engine_.DeleteTrackedQuery(100), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, DeleteTrackedQuery) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.DeleteTrackedQuery(100);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineTest, LoadTrackedQueries) {
  // This is all in-memory, so nothing to read from disk.
  EXPECT_TRUE(engine_.LoadTrackedQueries().empty());
}

TEST_F(InMemoryPersistenceStorageEngineTest, PruneCache) {
  engine_.BeginTransaction();
  // clang-format off
  engine_.OverwriteServerCache(
      Path(),
      std::map<Variant, Variant>{
        {"aaa", std::map<Variant, Variant>{
            {"bbb", std::map<Variant, Variant>{
                {"ccc", 100},
                {"ddd", 200}
            }}
        }},
        {"zzz", std::map<Variant, Variant>{
            {"yyy", std::map<Variant, Variant>{
                {"xxx", 300}}
            }}
        }});
  // clang-format on
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();

  PruneForest forest;
  PruneForestRef ref(&forest);

  ref.Prune(Path("aaa/bbb"));
  ref.Keep(Path("aaa/bbb/ccc"));
  ref.Prune(Path("zzz"));

  engine_.PruneCache(Path(), ref);

  // clang-format off
  EXPECT_EQ(
      engine_.ServerCache(Path()),
      Variant(std::map<Variant, Variant>{
        {"aaa", std::map<Variant, Variant>{
            {"bbb", std::map<Variant, Variant>{
                {"ccc", 100}
            }}
        }}
      })) << util::VariantToJson(engine_.ServerCache(Path()));
  // clang-format on
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest,
       ResetPreviouslyActiveTrackedQueries) {
  // Must be in a transaction.
  EXPECT_DEATH(engine_.ResetPreviouslyActiveTrackedQueries(100),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest,
       ResetPreviouslyActiveTrackedQueries) {
  // This is all in-memory, so nothing to save to disk.
  // There is nothing to check except that it doesn't crash.
  engine_.BeginTransaction();
  engine_.ResetPreviouslyActiveTrackedQueries(100);
  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, SaveTrackedQueryKeys) {
  // Must be in a transaction.
  EXPECT_DEATH(engine_.SaveTrackedQueryKeys(100, std::set<std::string>()),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, UpdateTrackedQueryKeys) {
  EXPECT_DEATH(engine_.UpdateTrackedQueryKeys(100, std::set<std::string>(),
                                              std::set<std::string>()),
               DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, TrackedQueryKeys) {
  engine_.BeginTransaction();

  EXPECT_TRUE(engine_.LoadTrackedQueryKeys(100).empty());

  engine_.SaveTrackedQueryKeys(100, {"aaa", "bbb", "ccc"});
  engine_.SaveTrackedQueryKeys(200, {"zzz", "yyy", "xxx"});

  EXPECT_EQ(engine_.LoadTrackedQueryKeys(100),
            std::set<std::string>({"aaa", "bbb", "ccc"}));
  EXPECT_EQ(engine_.LoadTrackedQueryKeys(200),
            std::set<std::string>({"zzz", "yyy", "xxx"}));
  EXPECT_TRUE(engine_.LoadTrackedQueryKeys(300).empty());

  engine_.UpdateTrackedQueryKeys(100, std::set<std::string>({"ddd", "eee"}),
                                 std::set<std::string>({"aaa", "bbb"}));

  EXPECT_EQ(engine_.LoadTrackedQueryKeys(100),
            std::set<std::string>({"ccc", "ddd", "eee"}));
  EXPECT_EQ(engine_.LoadTrackedQueryKeys(200),
            std::set<std::string>({"zzz", "yyy", "xxx"}));
  EXPECT_TRUE(engine_.LoadTrackedQueryKeys(300).empty());

  EXPECT_EQ(engine_.LoadTrackedQueryKeys(std::set<QueryId>({100})),
            std::set<std::string>({"ccc", "ddd", "eee"}));
  EXPECT_EQ(engine_.LoadTrackedQueryKeys(std::set<QueryId>({100, 200})),
            std::set<std::string>({"ccc", "ddd", "eee", "zzz", "yyy", "xxx"}));

  engine_.SetTransactionSuccessful();
  engine_.EndTransaction();
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, BeginTransaction) {
  EXPECT_TRUE(engine_.BeginTransaction());
  // Cannot begin a transaction while in a transaction.
  EXPECT_DEATH(engine_.BeginTransaction(), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, BeginTransaction) {
  // BeginTransaction should return true, indicating success.
  EXPECT_TRUE(engine_.BeginTransaction());
}

TEST_F(InMemoryPersistenceStorageEngineDeathTest, EndTransaction) {
  // Cannot end a transaction unless in a transaction.
  EXPECT_DEATH(engine_.EndTransaction(), DEATHTEST_SIGABRT);
}

TEST_F(InMemoryPersistenceStorageEngineTest, EndTransaction) {
  EXPECT_TRUE(engine_.BeginTransaction());
  engine_.EndTransaction();
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
