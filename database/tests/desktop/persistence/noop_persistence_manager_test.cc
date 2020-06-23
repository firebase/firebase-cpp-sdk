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

#include "database/src/desktop/persistence/noop_persistence_manager.h"

#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

TEST(NoopPersistenceManager, Constructor) {
  // Ensure there is no crash.
  NoopPersistenceManager manager;
  (void)manager;
}

TEST(NoopPersistenceManager, LoadUserWrites) {
  NoopPersistenceManager manager;
  EXPECT_TRUE(manager.LoadUserWrites().empty());
}

TEST(NoopPersistenceManager, ServerCache) {
  NoopPersistenceManager manager;
  EXPECT_EQ(manager.ServerCache(QuerySpec()), CacheNode());
}

TEST(NoopPersistenceManager, InsideTransaction) {
  // Make sure none of these functions result in a crash. There is no state we
  // can query or other side effects that we can test.
  NoopPersistenceManager manager;
  EXPECT_TRUE(manager.RunInTransaction([&manager]() {
    manager.SaveUserMerge(Path(), CompoundWrite(), 100);
    manager.RemoveUserWrite(100);
    manager.RemoveAllUserWrites();
    manager.ApplyUserWriteToServerCache(Path("a/b/c"), Variant::FromInt64(123));
    manager.ApplyUserWriteToServerCache(Path("a/b/c"), CompoundWrite());
    manager.UpdateServerCache(QuerySpec(), Variant::FromInt64(123));
    manager.UpdateServerCache(Path("a/b/c"), CompoundWrite());
    manager.SetQueryActive(QuerySpec());
    manager.SetQueryInactive(QuerySpec());
    manager.SetQueryComplete(QuerySpec());
    manager.SetTrackedQueryKeys(QuerySpec(),
                                std::set<std::string>{"aaa", "bbb"});
    manager.UpdateTrackedQueryKeys(QuerySpec(),
                                   std::set<std::string>{"aaa", "bbb"},
                                   std::set<std::string>{"ccc", "ddd"});
    return true;
  }));
}

TEST(NoopPersistenceManagerDeathTest, NestedTransaction) {
  // Make sure none of these functions result in a crash. There is no state we
  // can query or other side effects that we can test.
  NoopPersistenceManager manager;
  EXPECT_DEATH(manager.RunInTransaction([&manager]() {
    // This transaction should run.
    manager.RunInTransaction([]() {
      // This transaction should not run, since the nested call to
      // RunInTransaction should assert.
      return true;
    });
    return true;
  }),
               DEATHTEST_SIGABRT);
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
