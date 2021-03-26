// Copyright 2020 Google LLC
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

#include "database/src/desktop/persistence/level_db_persistence_storage_engine.h"

#include <fstream>
#include <iostream>
#include <streambuf>

#include "app/src/logger.h"
#include "app/src/variant_util.h"
#include "database/src/desktop/persistence/flatbuffer_conversions.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

using testing::Eq;
using testing::Pointwise;

namespace firebase {
namespace database {
namespace internal {
namespace {

// TODO(amablue): Consider refactoring this into a common location.
#if defined(_WIN32)
static const char kDirectorySeparator[] = "\\";
#else
static const char kDirectorySeparator[] = "/";
#endif  // defined(_WIN32)

static std::string GetTestTmpDir(const char test_namespace[]) {
#if defined(_WIN32)
  char buf[MAX_PATH + 1];
  if (GetEnvironmentVariableA("TEST_TMPDIR", buf, sizeof(buf))) {
    return std::string(buf) + kDirectorySeparator + test_namespace;
  }
#else
  // Linux and OS X should either have the TEST_TMPDIR environment variable set.
  if (const char* value = getenv("TEST_TMPDIR")) {
    return std::string(value) + kDirectorySeparator + test_namespace;
  }
#endif  // defined(_WIN32)
  // If we weren't able to get TEST_TMPDIR, just use a subdirectory.
  return test_namespace;
}

TEST(LevelDbPersistenceStorageEngine, ConstructorBasic) {
  const std::string kDatabaseFilename = GetTestTmpDir(test_info_->name());

  // Just ensure that nothing crashes.
  SystemLogger logger;
  LevelDbPersistenceStorageEngine engine(&logger);
  engine.Initialize(kDatabaseFilename);
}

class LevelDbPersistenceStorageEngineTest : public ::testing::Test {
 protected:
  void SetUp() override {
    engine_ = new LevelDbPersistenceStorageEngine(&logger_);
  }

  void TearDown() override { delete engine_; }

  // All tests should start with this. This sets the path Level DB should read
  // from and write to, and caches that path so that when we re-start Level DB
  // we have the path we used on the previous run.
  void InitializeLevelDb(const std::string& test_name) {
    database_path_ = GetTestTmpDir(test_name.c_str());
    engine_->Initialize(database_path_);
  }

  // We want to run all of our tests twice: Once immediately after the functions
  // have been called on the database, and then once again after the database
  // has been shut down and restarted.
  template <typename Func>
  void RunTwice(const Func& func) {
    func();
    TearDown();
    SetUp();
    engine_->Initialize(database_path_);
    func();
  }

  SystemLogger logger_;
  LevelDbPersistenceStorageEngine* engine_;
  std::string database_path_;
};

TEST_F(LevelDbPersistenceStorageEngineTest, SaveUserOverwrite) {
  InitializeLevelDb(test_info_->name());

  Path path_a("aaa/bbb");
  Variant data_a("variant_data");
  WriteId write_id_a = 100;

  Path path_b("ccc/ddd");
  Variant data_b("variant_data_two");
  WriteId write_id_b = 101;

  // Compare to ensure the written value is the expected value.
  engine_->BeginTransaction();
  engine_->SaveUserOverwrite(path_a, data_a, write_id_a);
  engine_->SaveUserOverwrite(path_b, data_b, write_id_b);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<UserWriteRecord> result = engine_->LoadUserWrites();
    std::vector<UserWriteRecord> expected{
        UserWriteRecord(100, Path("aaa/bbb"), "variant_data", true),
        UserWriteRecord(101, Path("ccc/ddd"), "variant_data_two", true)};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, SaveUserMerge) {
  InitializeLevelDb(test_info_->name());

  Path path("this/is/a/test/path");
  CompoundWrite children = CompoundWrite::FromPathMerge(std::map<Path, Variant>{
      std::make_pair(Path("larry"), 999),
      std::make_pair(Path("curly"), 888),
      std::make_pair(Path("moe"), 777),
  });
  WriteId write_id = 100;

  // Compare to ensure the written value is the expected value.
  engine_->BeginTransaction();
  engine_->SaveUserMerge(path, children, write_id);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<UserWriteRecord> result = engine_->LoadUserWrites();
    std::vector<UserWriteRecord> expected{
        UserWriteRecord(100, Path("this/is/a/test/path"),
                        CompoundWrite::FromPathMerge(std::map<Path, Variant>{
                            std::make_pair(Path("larry"), 999),
                            std::make_pair(Path("curly"), 888),
                            std::make_pair(Path("moe"), 777),
                        }))};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, RemoveUserWrite) {
  InitializeLevelDb(test_info_->name());

  Path path_a("this/is/a/test/path");
  Variant data_a("variant_data");
  WriteId write_id_a = 100;

  Path path_b("this/is/another/test/path");
  Variant data_b("variant_data_two");
  WriteId write_id_b = 101;

  // Compare to ensure the written value is the expected value.
  engine_->BeginTransaction();
  engine_->SaveUserOverwrite(path_a, data_a, write_id_a);
  engine_->SaveUserOverwrite(path_b, data_b, write_id_b);
  engine_->RemoveUserWrite(100);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<UserWriteRecord> result = engine_->LoadUserWrites();
    std::vector<UserWriteRecord> expected{
        UserWriteRecord(101, Path("this/is/another/test/path"),
                        Variant("variant_data_two"), true)};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, RemoveAllUserWrites) {
  InitializeLevelDb(test_info_->name());

  Path path_a("this/is/a/test/path");
  Variant data_a("variant_data");
  WriteId write_id_a = 100;

  Path path_b("this/is/another/test/path");
  Variant data_b("variant_data_two");
  WriteId write_id_b = 101;

  // Compare to ensure the written value is the expected value.
  engine_->BeginTransaction();
  engine_->SaveUserOverwrite(path_a, data_a, write_id_a);
  engine_->SaveUserOverwrite(path_b, data_b, write_id_b);
  engine_->RemoveAllUserWrites();
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<UserWriteRecord> result = engine_->LoadUserWrites();
    std::vector<UserWriteRecord> expected{};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, OverwriteServerCache) {
  InitializeLevelDb(test_info_->name());

  engine_->BeginTransaction();
  engine_->OverwriteServerCache(Path("aaa/bbb"), Variant("some value"));
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb"));
      Variant expected("some value");
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa"));
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("bbb", "some value"),
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path());
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("aaa", std::map<Variant, Variant>{
              std::make_pair("bbb", "some value"),
          })
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, OverwriteServerCache_Overwrite) {
  InitializeLevelDb(test_info_->name());

  engine_->BeginTransaction();
  engine_->OverwriteServerCache(Path("aaa/bbb"), Variant("some value"));
  engine_->OverwriteServerCache(Path("aaa"), Variant("Overwrite!"));
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb"));
      Variant expected = Variant::Null();
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa"));
      Variant expected("Overwrite!");
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path());
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("aaa", "Overwrite!"),
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, MergeIntoServerCacheWithVariant) {
  InitializeLevelDb(test_info_->name());

  Variant merge = std::map<Variant, Variant>{
      std::make_pair("ccc", std::map<Variant, Variant>{std::make_pair(
                                "ddd", "some value")}),
      std::make_pair("eee", "adjacent value"),
  };

  engine_->BeginTransaction();
  engine_->MergeIntoServerCache(Path("aaa/bbb"), merge);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb/ccc/ddd"));
      Variant expected = "some value";
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb/eee"));
      Variant expected("adjacent value");
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb"));
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("ccc", std::map<Variant, Variant>{
              std::make_pair("ddd", "some value"),
          }),
          std::make_pair("eee", "adjacent value"),
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest,
       MergeIntoServerCacheWithCompoundWrite) {
  InitializeLevelDb(test_info_->name());

  CompoundWrite merge = CompoundWrite::FromPathMerge(std::map<Path, Variant>{
      std::make_pair(Path("ccc/ddd"), "some value"),
      std::make_pair(Path("eee"), "adjacent value"),
  });

  engine_->BeginTransaction();
  engine_->MergeIntoServerCache(Path("aaa/bbb"), merge);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb/ccc/ddd"));
      Variant expected = "some value";
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb/eee"));
      Variant expected("adjacent value");
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path("aaa/bbb"));
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("ccc", std::map<Variant, Variant>{
              std::make_pair("ddd", "some value"),
          }),
          std::make_pair("eee", "adjacent value"),
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
    {
      Variant result = engine_->ServerCache(Path());
      // clang-format off
      Variant expected = std::map<Variant, Variant>{
          std::make_pair("aaa", std::map<Variant, Variant>{
              std::make_pair("bbb", std::map<Variant, Variant>{
                  std::make_pair("ccc", std::map<Variant, Variant>{
                      std::make_pair("ddd", "some value"),
                  }),
                  std::make_pair("eee", "adjacent value"),
              }),
          }),
      };
      // clang-format on
      EXPECT_EQ(result, expected);
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, ServerCacheEstimatedSizeInBytes) {
  InitializeLevelDb(test_info_->name());

  std::string long_string(1024, 'x');

  engine_->BeginTransaction();
  engine_->OverwriteServerCache(Path("aaa"), long_string);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    uint64 result = engine_->ServerCacheEstimatedSizeInBytes();
    uint64 expected = 1024 + strlen("aaa");

    // This is only an estimate, so as long as we're within a few bytes it's
    // okay.
    EXPECT_NEAR(result, expected, 16);
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, SaveTrackedQuery) {
  InitializeLevelDb(test_info_->name());

  TrackedQuery tracked_query_a(100, QuerySpec(Path("aaa/bbb/ccc")), 1234,
                               TrackedQuery::kComplete, TrackedQuery::kActive);
  TrackedQuery tracked_query_b(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                               TrackedQuery::kIncomplete,
                               TrackedQuery::kInactive);

  engine_->BeginTransaction();
  engine_->SaveTrackedQuery(tracked_query_a);
  engine_->SaveTrackedQuery(tracked_query_b);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<TrackedQuery> result = engine_->LoadTrackedQueries();
    std::vector<TrackedQuery> expected{
        TrackedQuery(100, QuerySpec(Path("aaa/bbb/ccc")), 1234,
                     TrackedQuery::kComplete, TrackedQuery::kActive),
        TrackedQuery(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                     TrackedQuery::kIncomplete, TrackedQuery::kInactive)};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, DeleteTrackedQuery) {
  InitializeLevelDb(test_info_->name());

  TrackedQuery tracked_query_a(100, QuerySpec(Path("aaa/bbb/ccc")), 1234,
                               TrackedQuery::kComplete, TrackedQuery::kActive);
  TrackedQuery tracked_query_b(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                               TrackedQuery::kIncomplete,
                               TrackedQuery::kInactive);

  engine_->BeginTransaction();
  engine_->SaveTrackedQuery(tracked_query_a);
  engine_->SaveTrackedQuery(tracked_query_b);
  engine_->DeleteTrackedQuery(100);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<TrackedQuery> result = engine_->LoadTrackedQueries();
    std::vector<TrackedQuery> expected{
        TrackedQuery(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                     TrackedQuery::kIncomplete, TrackedQuery::kInactive)};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest,
       ResetPreviouslyActiveTrackedQueries) {
  InitializeLevelDb(test_info_->name());

  TrackedQuery tracked_query_a(100, QuerySpec(Path("aaa/bbb/ccc")), 1234,
                               TrackedQuery::kComplete, TrackedQuery::kActive);
  TrackedQuery tracked_query_b(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                               TrackedQuery::kIncomplete,
                               TrackedQuery::kInactive);

  engine_->BeginTransaction();
  engine_->SaveTrackedQuery(tracked_query_a);
  engine_->SaveTrackedQuery(tracked_query_b);
  engine_->ResetPreviouslyActiveTrackedQueries(9999);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    std::vector<TrackedQuery> result = engine_->LoadTrackedQueries();
    std::vector<TrackedQuery> expected{
        TrackedQuery(100, QuerySpec(Path("aaa/bbb/ccc")), 9999,
                     TrackedQuery::kComplete, TrackedQuery::kInactive),
        TrackedQuery(101, QuerySpec(Path("aaa/bbb/ddd")), 5678,
                     TrackedQuery::kIncomplete, TrackedQuery::kInactive)};

    EXPECT_THAT(result, Pointwise(Eq(), expected));
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, SaveTrackedQueryKeys) {
  InitializeLevelDb(test_info_->name());

  engine_->BeginTransaction();
  engine_->SaveTrackedQueryKeys(100,
                                std::set<std::string>{"key1", "key2", "key3"});
  engine_->SaveTrackedQueryKeys(101,
                                std::set<std::string>{"key4", "key5", "key6"});
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      std::set<std::string> result = engine_->LoadTrackedQueryKeys(100);
      std::set<std::string> expected{"key1", "key2", "key3"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
    {
      std::set<std::string> result = engine_->LoadTrackedQueryKeys(101);
      std::set<std::string> expected{"key4", "key5", "key6"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
    {
      std::set<std::string> result =
          engine_->LoadTrackedQueryKeys(std::set<QueryId>{100, 101});
      std::set<std::string> expected{"key1", "key2", "key3",
                                     "key4", "key5", "key6"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, UpdateTrackedQueryKeys) {
  InitializeLevelDb(test_info_->name());

  engine_->BeginTransaction();
  engine_->SaveTrackedQueryKeys(100,
                                std::set<std::string>{"key1", "key2", "key3"});
  engine_->SaveTrackedQueryKeys(101,
                                std::set<std::string>{"key4", "key5", "key6"});
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    {
      std::set<std::string> result = engine_->LoadTrackedQueryKeys(100);
      std::set<std::string> expected{"key1", "key2", "key3"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
    {
      std::set<std::string> result = engine_->LoadTrackedQueryKeys(101);
      std::set<std::string> expected{"key4", "key5", "key6"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
    {
      std::set<std::string> result =
          engine_->LoadTrackedQueryKeys(std::set<QueryId>{100, 101});
      std::set<std::string> expected{"key1", "key2", "key3",
                                     "key4", "key5", "key6"};
      EXPECT_THAT(result, Pointwise(Eq(), expected));
    }
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, PruneCache) {
  InitializeLevelDb(test_info_->name());

  // clang-format off
  Variant initial_data = std::map<Variant, Variant>{
          std::make_pair("the_root", std::map<Variant, Variant>{
              std::make_pair("delete_me", std::map<Variant, Variant>{
                  std::make_pair("but_keep_me", 111),
                  std::make_pair("ill_be_gone", 222),
              }),
              std::make_pair("keep_me", std::map<Variant, Variant>{
                  std::make_pair("but_delete_me", 333),
                  std::make_pair("ill_be_here", 444),
              }),
          }),
      };
  // clang-format on

  PruneForest prune_forest;
  PruneForestRef prune_forest_ref(&prune_forest);
  prune_forest_ref.Prune(Path("delete_me"));
  prune_forest_ref.Keep(Path("delete_me/but_keep_me"));
  prune_forest_ref.Prune(Path("keep_me/but_delete_me"));

  engine_->BeginTransaction();
  engine_->OverwriteServerCache(Path(), initial_data);
  engine_->PruneCache(Path("the_root"), prune_forest_ref);
  engine_->SetTransactionSuccessful();
  engine_->EndTransaction();

  RunTwice([this]() {
    Variant result = engine_->ServerCache(Path());
    // clang-format off
    Variant expected = std::map<Variant, Variant>{
            std::make_pair("the_root", std::map<Variant, Variant>{
                std::make_pair("delete_me", std::map<Variant, Variant>{
                    std::make_pair("but_keep_me", 111),
                }),
                std::make_pair("keep_me", std::map<Variant, Variant>{
                    std::make_pair("ill_be_here", 444),
                }),
            }),
        };
    // clang-format on
    EXPECT_EQ(result, expected);
  });
}

TEST_F(LevelDbPersistenceStorageEngineTest, BeginTransaction) {
  // BeginTransaction should return true, indicating success.
  EXPECT_TRUE(engine_->BeginTransaction());
}

TEST_F(LevelDbPersistenceStorageEngineTest, EndTransaction) {
  EXPECT_TRUE(engine_->BeginTransaction());
  engine_->EndTransaction();
}

// Many functions are designed to assert if called outside a transaction. Ensure
// they crash as expected.
using LevelDbPersistenceStorageEngineDeathTest =
    LevelDbPersistenceStorageEngineTest;

TEST_F(LevelDbPersistenceStorageEngineDeathTest, SaveUserOverwrite) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->SaveUserOverwrite(Path(), Variant(), 0),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, SaveUserMerge) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->SaveUserMerge(Path(), CompoundWrite(), 0),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, RemoveUserWrite) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->RemoveUserWrite(0), DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, RemoveAllUserWrites) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->RemoveAllUserWrites(), DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, OverwriteServerCache) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->OverwriteServerCache(Path(), Variant()),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, MergeIntoServerCacheVariant) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->MergeIntoServerCache(Path(), Variant()),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest,
       MergeIntoServerCacheCompoundWrite) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->MergeIntoServerCache(Path(), CompoundWrite()),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, SaveTrackedQuery) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->SaveTrackedQuery(TrackedQuery()), DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, DeleteTrackedQuery) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->DeleteTrackedQuery(0), DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest,
       ResetPreviouslyActiveTrackedQueries) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->ResetPreviouslyActiveTrackedQueries(0),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, SaveTrackedQueryKeys) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->SaveTrackedQueryKeys(0, std::set<std::string>()),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, UpdateTrackedQueryKeys) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->UpdateTrackedQueryKeys(0, std::set<std::string>(),
                                               std::set<std::string>()),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, PruneCache) {
  InitializeLevelDb(test_info_->name());
  EXPECT_DEATH(engine_->PruneCache(Path(), PruneForestRef(nullptr)),
               DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, BeginTransaction) {
  EXPECT_TRUE(engine_->BeginTransaction());
  // Cannot begin a transaction while in a transaction.
  EXPECT_DEATH(engine_->BeginTransaction(), DEATHTEST_SIGABRT);
}

TEST_F(LevelDbPersistenceStorageEngineDeathTest, EndTransaction) {
  // Cannot end a transaction unless in a transaction.
  EXPECT_DEATH(engine_->EndTransaction(), DEATHTEST_SIGABRT);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
