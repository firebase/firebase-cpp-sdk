// Copyright 2026 Google LLC
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

#include "database/src/desktop/core/repo.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>

#include "app/src/callback.h"
#include "app/src/include/firebase/app.h"
#include "app/src/semaphore.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/include/firebase/database.h"
#include "gtest/gtest.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

constexpr char kEmulatorDatabaseUrl[] =
    "http://127.0.0.1:9000/?ns=test-project";

void WaitForRepoScheduler() {
  auto done = std::make_shared<Semaphore>(0);
  Repo::scheduler().Schedule(callback::NewCallback(
      [](std::shared_ptr<Semaphore> sem) { sem->Post(); }, done));
  done->Wait();
}

#if defined(__linux__)
// Temporarily points XDG_DATA_HOME at a regular file so AppDataDir() cannot
// create subdirectories under it.
class ScopedUncreatableXdgDataHome {
 public:
  ScopedUncreatableXdgDataHome() {
    const char* existing = std::getenv("XDG_DATA_HOME");
    if (existing != nullptr) {
      had_original_ = true;
      original_ = existing;
    }
    blocker_file_ =
        ::testing::TempDir() + "/firebase_rtdb_repo_test_xdg_blocker";
    std::ofstream out(blocker_file_);
    out << "not a directory";
    out.close();
    setenv("XDG_DATA_HOME", blocker_file_.c_str(), /*overwrite=*/1);
  }

  ~ScopedUncreatableXdgDataHome() {
    if (had_original_) {
      setenv("XDG_DATA_HOME", original_.c_str(), /*overwrite=*/1);
    } else {
      unsetenv("XDG_DATA_HOME");
    }
    std::remove(blocker_file_.c_str());
  }

 private:
  bool had_original_ = false;
  std::string original_;
  std::string blocker_file_;
};
#endif  // defined(__linux__)

// Regression test for https://github.com/firebase/firebase-cpp-sdk/issues/1981.
// With persistence disabled, failing to place an on-disk cache directory (for
// example on Windows against an emulator URL with ':' and '?', or on Linux
// with an unwritable XDG_DATA_HOME) must not skip SyncTree construction or
// crash on the first write.
TEST(RepoDesktopTest, SetValueWithPersistenceOffAndUncreatableCacheDir) {
#if defined(__linux__)
  ScopedUncreatableXdgDataHome uncreatable_xdg;
#endif
  AppOptions options = testing::MockAppOptions();
  options.set_database_url(kEmulatorDatabaseUrl);
  std::unique_ptr<App> app(testing::CreateApp(options));
  ASSERT_NE(app, nullptr);

  std::unique_ptr<Database> db(Database::GetInstance(app.get()));
  ASSERT_NE(db, nullptr);
  db->set_persistence_enabled(false);
  db->GoOffline();

  DatabaseReference ref = db->GetReference("probe/x");
  ASSERT_TRUE(ref.is_valid());
  Future<void> write_future = ref.SetValue(1);
  EXPECT_NE(write_future.status(), kFutureStatusInvalid);

  WaitForRepoScheduler();
}

// Even when persistence is enabled, an unavailable persistence directory (such
// as missing package_name or uncreatable AppDataDir) must fall back to a no-op
// persistence manager rather than leaving server_sync_tree_ null.
TEST(RepoDesktopTest, SetValueWithPersistenceOnFallsBackWhenCacheUnavailable) {
#if defined(__linux__)
  ScopedUncreatableXdgDataHome uncreatable_xdg;
#endif
  AppOptions options = testing::MockAppOptions();
  options.set_package_name("");
  options.set_database_url(kEmulatorDatabaseUrl);
  std::unique_ptr<App> app(testing::CreateApp(options));
  ASSERT_NE(app, nullptr);

  std::unique_ptr<Database> db(Database::GetInstance(app.get()));
  ASSERT_NE(db, nullptr);
  db->set_persistence_enabled(true);
  db->GoOffline();

  DatabaseReference ref = db->GetReference("probe/y");
  ASSERT_TRUE(ref.is_valid());
  Future<void> write_future = ref.SetValue(2);
  EXPECT_NE(write_future.status(), kFutureStatusInvalid);

  WaitForRepoScheduler();
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
