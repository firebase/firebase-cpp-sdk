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

#include "database/src/include/firebase/database/database_reference.h"

#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/src/service_interfaces.h"
#include "app/src/thread.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "database/src/common/database_reference.h"
#include "database/src/desktop/data_snapshot_desktop.h"
#include "database/src/desktop/database_desktop.h"
#include "database/src/desktop/database_reference_desktop.h"
#include "database/src/include/firebase/database.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using firebase::App;
using firebase::AppOptions;

using testing::Eq;

static const char kApiKey[] = "MyFakeApiKey";
static const char kDatabaseUrl[] = "https://abc-xyz-123.firebaseio.com";

namespace firebase {
namespace database {

class DatabaseReferenceTest : public ::testing::Test {
 public:
  void SetUp() override {
    AppOptions options = testing::MockAppOptions();
    options.set_database_url(kDatabaseUrl);
    options.set_api_key(kApiKey);
    app_ = testing::CreateApp(options);
    database_ = Database::GetInstance(app_);
  }

  void DropDatabaseAndApp() {
    database_ = Database();
    app_ = App();
  }

  void TearDown() override { DropDatabaseAndApp(); }

 protected:
  App app_;
  Database database_;
};

// Test DatabaseReference()
TEST_F(DatabaseReferenceTest, DefaultConstructor) {
  DatabaseReference ref;
  EXPECT_FALSE(ref.is_valid());
}

// Test DatabaseReference(DatabaseReferenceInternal*)
TEST_F(DatabaseReferenceTest, ConstructorWithInternalPointer) {
  // Assume Database::GetReference() would utilize DatabaseReferenceInternal*
  // created from different platform-dependent code to create
  // DatabaseReference.
  EXPECT_TRUE(database_.GetReference().is_valid());
  EXPECT_TRUE(database_.GetReference().is_root());
  EXPECT_THAT(database_.GetReference().key_string(), Eq(""));

  EXPECT_TRUE(database_.GetReference("child").is_valid());
  EXPECT_FALSE(database_.GetReference("child").is_root());
  EXPECT_THAT(database_.GetReference("child").key_string(), Eq("child"));
}

// Test DatabaseReference(const DatabaseReference&)
TEST_F(DatabaseReferenceTest, CopyConstructor) {
  DatabaseReference ref_null;
  DatabaseReference ref_copy_null(ref_null);
  EXPECT_FALSE(ref_copy_null.is_valid());

  DatabaseReference ref_copy_root(database_.GetReference());
  EXPECT_TRUE(ref_copy_root.is_valid());
  EXPECT_TRUE(ref_copy_root.is_root());
  EXPECT_THAT(ref_copy_root.key_string(), Eq(""));

  DatabaseReference ref_copy_child(database_.GetReference("child"));
  EXPECT_TRUE(ref_copy_child.is_valid());
  EXPECT_FALSE(ref_copy_child.is_root());
  EXPECT_THAT(ref_copy_child.key_string(), Eq("child"));
}

// Test DatabaseReference(DatabaseReference&&)
TEST_F(DatabaseReferenceTest, MoveConstructor) {
  DatabaseReference ref_null;
  DatabaseReference ref_move_null(std::move(ref_null));
  EXPECT_FALSE(ref_move_null.is_valid());

  DatabaseReference ref_root = database_.GetReference();
  DatabaseReference ref_move_root(std::move(ref_root));
  EXPECT_FALSE(ref_root.is_valid());  // NOLINT
  EXPECT_TRUE(ref_move_root.is_valid());
  EXPECT_TRUE(ref_move_root.is_root());
  EXPECT_THAT(ref_move_root.key_string(), Eq(""));

  DatabaseReference ref_child = database_.GetReference("child");
  DatabaseReference ref_move_child(std::move(ref_child));
  EXPECT_FALSE(ref_child.is_valid());  // NOLINT
  EXPECT_TRUE(ref_move_child.is_valid());
  EXPECT_FALSE(ref_move_child.is_root());
  EXPECT_THAT(ref_move_child.key_string(), Eq("child"));
}

// Test operator=(const DatabaseReference&)
TEST_F(DatabaseReferenceTest, CopyOperator) {
  DatabaseReference ref_copy_null;
  ref_copy_null = DatabaseReference();
  EXPECT_FALSE(ref_copy_null.is_valid());

  DatabaseReference ref_copy_root;
  ref_copy_root = database_.GetReference();
  EXPECT_TRUE(ref_copy_root.is_valid());
  EXPECT_TRUE(ref_copy_root.is_root());
  EXPECT_THAT(ref_copy_root.key_string(), Eq(""));

  DatabaseReference ref_copy_child;
  ref_copy_child = database_.GetReference("child");
  EXPECT_TRUE(ref_copy_child.is_valid());
  EXPECT_FALSE(ref_copy_child.is_root());
  EXPECT_THAT(ref_copy_child.key_string(), Eq("child"));
}

// Test operator=(DatabaseReference&&)
TEST_F(DatabaseReferenceTest, MoveOperator) {
  DatabaseReference ref_null;
  DatabaseReference ref_move_null;
  ref_move_null = std::move(ref_null);
  EXPECT_FALSE(ref_move_null.is_valid());

  DatabaseReference ref_root = database_.GetReference();
  DatabaseReference ref_move_root;
  ref_move_root = std::move(ref_root);
  EXPECT_FALSE(ref_root.is_valid());  // NOLINT
  EXPECT_TRUE(ref_move_root.is_valid());
  EXPECT_TRUE(ref_move_root.is_root());
  EXPECT_THAT(ref_move_root.key_string(), Eq(""));

  DatabaseReference ref_child = database_.GetReference("child");
  DatabaseReference ref_move_child;
  ref_move_child = std::move(ref_child);
  EXPECT_FALSE(ref_child.is_valid());  // NOLINT
  EXPECT_TRUE(ref_move_child.is_valid());
  EXPECT_FALSE(ref_move_child.is_root());
  EXPECT_THAT(ref_move_child.key_string(), Eq("child"));
}

// Live DatabaseReferences keep the underlying Database and AppState alive even
// after the caller drops its App and Database handles; AppState tears down once
// the last handle is released.
TEST_F(DatabaseReferenceTest, HandlesKeepDatabaseAndAppAlive) {
  std::weak_ptr<::firebase::internal::AppState> weak_state =
      ::firebase::internal::AppState::FromApp(app_);
  ASSERT_FALSE(weak_state.expired());

  // Reused temporary DatabaseReference to be moved to another DatabaseReference
  DatabaseReference ref_to_be_moved;

  // Null DatabaseReference created through default constructor, copy
  // constructor, copy operator, move constructor and move operator
  DatabaseReference ref_null;
  DatabaseReference ref_copy_const_null(ref_null);
  DatabaseReference ref_copy_op_null;
  ref_copy_op_null = ref_null;
  ref_to_be_moved = ref_null;
  DatabaseReference ref_move_const_null(std::move(ref_to_be_moved));
  ref_to_be_moved = ref_null;
  DatabaseReference ref_move_op_null;
  ref_move_op_null = std::move(ref_to_be_moved);

  {
    // Root DatabaseReference created through default constructor, copy
    // constructor, copy operator, move constructor and move operator
    DatabaseReference ref_root = database_.GetReference();
    DatabaseReference ref_copy_const_root(ref_root);
    DatabaseReference ref_copy_op_root;
    ref_copy_op_root = ref_root;
    ref_to_be_moved = ref_root;
    DatabaseReference ref_move_const_root(std::move(ref_to_be_moved));
    ref_to_be_moved = ref_root;
    DatabaseReference ref_move_op_root;
    ref_move_op_root = std::move(ref_to_be_moved);

    // Child DatabaseReference created through default constructor, copy
    // constructor, copy operator, move constructor and move operator
    DatabaseReference ref_child = database_.GetReference("child");
    DatabaseReference ref_copy_const_child(ref_child);
    DatabaseReference ref_copy_op_child;
    ref_copy_op_child = ref_child;
    ref_to_be_moved = ref_child;
    DatabaseReference ref_move_const_child(std::move(ref_to_be_moved));
    ref_to_be_moved = ref_child;
    DatabaseReference ref_move_op_child;
    ref_move_op_child = std::move(ref_to_be_moved);

    DropDatabaseAndApp();

    EXPECT_FALSE(weak_state.expired());

    EXPECT_FALSE(ref_null.is_valid());
    EXPECT_FALSE(ref_copy_const_null.is_valid());
    EXPECT_FALSE(ref_copy_op_null.is_valid());
    EXPECT_FALSE(ref_move_const_null.is_valid());
    EXPECT_FALSE(ref_move_op_null.is_valid());

    EXPECT_TRUE(ref_root.is_valid());
    EXPECT_TRUE(ref_copy_const_root.is_valid());
    EXPECT_TRUE(ref_copy_op_root.is_valid());
    EXPECT_TRUE(ref_move_const_root.is_valid());
    EXPECT_TRUE(ref_move_op_root.is_valid());

    EXPECT_TRUE(ref_child.is_valid());
    EXPECT_TRUE(ref_copy_const_child.is_valid());
    EXPECT_TRUE(ref_copy_op_child.is_valid());
    EXPECT_TRUE(ref_move_const_child.is_valid());
    EXPECT_TRUE(ref_move_op_child.is_valid());

    EXPECT_TRUE(ref_child.database().is_valid());
    EXPECT_TRUE(ref_child.database().app().is_valid());
    EXPECT_FALSE(ref_to_be_moved.is_valid());  // NOLINT
  }

  // Once all DatabaseReference handles leave scope, AppState is destroyed.
  EXPECT_TRUE(weak_state.expired());
}

TEST_F(DatabaseReferenceTest, InvalidAppAndMultiAppIsolation) {
  InitResult init_result = kInitResultSuccess;
  Database invalid_db = Database::GetInstance(App(), &init_result);
  EXPECT_FALSE(invalid_db.is_valid());
  EXPECT_EQ(kInitResultFailedMissingDependency, init_result);

  AppOptions options2 = testing::MockAppOptions();
  options2.set_database_url("https://other-db.firebaseio.com");
  options2.set_api_key(kApiKey);
  App app2 = testing::CreateApp(options2, "db_isolation_app2");
  Database db2 = Database::GetInstance(app2);
  ASSERT_TRUE(db2.is_valid());
  EXPECT_NE(database_, db2);
  EXPECT_EQ(database_, Database::GetInstance(app_));
  EXPECT_EQ(db2, Database::GetInstance(app2));
}

TEST_F(DatabaseReferenceTest, WeakDataSnapshotDoesNotPreventTeardown) {
  std::weak_ptr<::firebase::internal::AppState> weak_state =
      ::firebase::internal::AppState::FromApp(app_);
  auto state = weak_state.lock();
  ASSERT_TRUE(state != nullptr);

  // Create a real DatabaseInternal owned by AppState to verify that
  // DataSnapshotInternal holds a weak_ptr and does not form a cycle or prevent
  // teardown when App and Database handles drop.
  static constexpr int kTestDbTag = 0;
  auto db_internal = state->GetOrCreateService<internal::DatabaseInternal>(
      &kTestDbTag, "weak_snap_test",
      [this](::firebase::internal::AppState& app_state) {
        return std::make_shared<internal::DatabaseInternal>(app_state, app_);
      });
  ASSERT_TRUE(db_internal != nullptr);

  internal::DataSnapshotInternal snap(db_internal.get(), Variant(42),
                                      internal::QuerySpec(Path("items/a")));
  EXPECT_EQ(42, snap.GetValue().int64_value());
  {
    std::unique_ptr<internal::DatabaseReferenceInternal> snap_ref(
        snap.GetReference());
    EXPECT_TRUE(snap_ref != nullptr);
  }

  db_internal.reset();
  state.reset();
  DropDatabaseAndApp();
  EXPECT_TRUE(weak_state.expired());
  EXPECT_EQ(nullptr, snap.GetReference());
  // Value still readable from snapshot even after Database & App tore down.
  EXPECT_EQ(42, snap.GetValue().int64_value());
}

TEST_F(DatabaseReferenceTest, StaticDurationDatabaseSurvivesExit) {
  static Database static_db = []() {
    AppOptions opts = testing::MockAppOptions();
    opts.set_database_url(kDatabaseUrl);
    opts.set_api_key(kApiKey);
    App app = testing::CreateApp(opts, "db_static_exit");
    return Database::GetInstance(app);
  }();
  EXPECT_TRUE(static_db.is_valid());
}

// Ensure that creating and moving around DatabaseReferences in one thread while
// the Database handle is dropped from another thread is thread-safe.
TEST_F(DatabaseReferenceTest, RaceConditionTest) {
  struct TestUserdata {
    DatabaseReference ref_null;
    DatabaseReference ref_root;
    DatabaseReference ref_child;
  };

  const int kThreadCount = 100;
  std::vector<Thread> threads;
  threads.reserve(kThreadCount);

  for (int i = 0; i < kThreadCount; i++) {
    TestUserdata* userdata = new TestUserdata;
    userdata->ref_root = database_.GetReference();
    userdata->ref_child = database_.GetReference("child");

    threads.emplace_back(
        [](void* void_userdata) {
          TestUserdata* userdata = static_cast<TestUserdata*>(void_userdata);

          DatabaseReference ref_move_null;
          ref_move_null = std::move(userdata->ref_null);
          (void)ref_move_null;

          DatabaseReference ref_move_root;
          ref_move_root = std::move(userdata->ref_root);
          EXPECT_TRUE(ref_move_root.is_valid());

          DatabaseReference ref_move_child;
          ref_move_child = std::move(userdata->ref_child);
          EXPECT_TRUE(ref_move_child.is_valid());

          delete userdata;
        },
        userdata);
  }

  DropDatabaseAndApp();

  for (Thread& t : threads) {
    t.Join();
  }
}

TEST_F(DatabaseReferenceTest, DataSnapshotEqualityAcrossTwoDatabaseUrls) {
  auto state = ::firebase::internal::AppState::FromApp(app_);
  ASSERT_TRUE(state != nullptr);

  static constexpr int kDb1Tag = 0;
  static constexpr int kDb2Tag = 0;
  auto db1 = state->GetOrCreateService<internal::DatabaseInternal>(
      &kDb1Tag, "db_eq_1", [this](::firebase::internal::AppState& app_state) {
        return std::make_shared<internal::DatabaseInternal>(
            app_state, app_, "https://db-one.firebaseio.com");
      });
  auto db2 = state->GetOrCreateService<internal::DatabaseInternal>(
      &kDb2Tag, "db_eq_2", [this](::firebase::internal::AppState& app_state) {
        return std::make_shared<internal::DatabaseInternal>(
            app_state, app_, "https://db-two.firebaseio.com");
      });
  ASSERT_TRUE(db1 != nullptr);
  ASSERT_TRUE(db2 != nullptr);

  internal::DataSnapshotInternal snap1(db1.get(), Variant("hello"),
                                       internal::QuerySpec(Path("items/a")));
  internal::DataSnapshotInternal snap1_same(
      db1.get(), Variant("hello"), internal::QuerySpec(Path("items/a")));
  internal::DataSnapshotInternal snap2(db2.get(), Variant("hello"),
                                       internal::QuerySpec(Path("items/a")));

  EXPECT_TRUE(snap1 == snap1_same);
  EXPECT_FALSE(snap1 != snap1_same);
  EXPECT_FALSE(snap1 == snap2);
  EXPECT_TRUE(snap1 != snap2);
}

TEST_F(DatabaseReferenceTest, DatabaseCreatedBeforeAuthActivatesTokenRefresh) {
  struct FakeAuthProvider : public ::firebase::internal::AuthTokenProvider {
    std::string GetCurrentToken() override { return "tok"; }
    Future<std::string> GetTokenAsync(bool) override { return {}; }
    std::string GetCurrentUserUid() override { return "uid"; }
    void StartTokenListener() override { start_count.fetch_add(1); }
    void StopTokenListener() override { stop_count.fetch_add(1); }

    std::atomic<int> start_count{0};
    std::atomic<int> stop_count{0};
  };

  AppOptions opts = testing::MockAppOptions();
  opts.set_database_url(kDatabaseUrl);
  opts.set_api_key(kApiKey);

  std::weak_ptr<::firebase::internal::AppState> weak_state;
  std::shared_ptr<FakeAuthProvider> raw_provider;
  {
    App app = testing::CreateApp(opts, "db_before_auth_app");
    auto state = ::firebase::internal::AppState::FromApp(app);
    weak_state = state;
    // Create Database BEFORE AuthTokenProvider is attached.
    Database db = Database::GetInstance(app);
    ASSERT_TRUE(db.is_valid());

    auto provider_handle = state->GetOrCreateService<FakeAuthProvider>(
        &::firebase::internal::kAuthServiceTag, "",
        [&raw_provider](::firebase::internal::AppState&) {
          raw_provider = std::make_shared<FakeAuthProvider>();
          return raw_provider;
        });
    ASSERT_TRUE(provider_handle != nullptr);
    EXPECT_EQ(1, raw_provider->start_count.load());
    EXPECT_EQ(0, raw_provider->stop_count.load());
  }
  EXPECT_TRUE(weak_state.expired());
  ASSERT_TRUE(raw_provider != nullptr);
  EXPECT_EQ(1, raw_provider->start_count.load());
  EXPECT_EQ(1, raw_provider->stop_count.load());
}

TEST_F(DatabaseReferenceTest, MovedFromAndUniformHandleOperators) {
  Database db_copy = database_;
  EXPECT_TRUE(static_cast<bool>(db_copy));
  EXPECT_EQ(db_copy, database_);
  EXPECT_FALSE(db_copy != database_);

  Database db_moved(std::move(db_copy));
  EXPECT_FALSE(static_cast<bool>(db_copy));  // NOLINT
  EXPECT_FALSE(db_copy.is_valid());          // NOLINT
  EXPECT_NE(db_moved, db_copy);              // NOLINT

  DatabaseReference ref1 = database_.GetReference("alpha");
  DatabaseReference ref2 = database_.GetReference("alpha");
  DatabaseReference ref3 = database_.GetReference("beta");
  EXPECT_TRUE(static_cast<bool>(ref1));
  EXPECT_EQ(ref1, ref2);
  EXPECT_NE(ref1, ref3);
  DatabaseReference ref_moved(std::move(ref2));
  EXPECT_FALSE(static_cast<bool>(ref2));  // NOLINT
  EXPECT_NE(ref_moved, ref2);             // NOLINT

  Query q1 = ref1.OrderByKey();
  Query q2 = ref1.OrderByKey();
  Query q3 = ref3.OrderByKey();
  EXPECT_TRUE(static_cast<bool>(q1));
  EXPECT_EQ(q1, q2);
  EXPECT_NE(q1, q3);
  Query q_moved(std::move(q2));
  EXPECT_FALSE(static_cast<bool>(q2));  // NOLINT
  EXPECT_NE(q_moved, q2);               // NOLINT
}

}  // namespace database
}  // namespace firebase
