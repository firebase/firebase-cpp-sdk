/*
 * Copyright 2026 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "firestore/src/include/firebase/firestore.h"

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "app_check/src/include/firebase/app_check.h"
#include "auth/src/include/firebase/auth.h"
#include "gtest/gtest.h"

namespace firebase {
namespace firestore {
namespace {

template <typename T>
void WaitForFuture(const Future<T>& future) {
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (future.status() == kFutureStatusPending &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  EXPECT_EQ(future.status(), kFutureStatusComplete);
}

App CreateTestApp(const char* name, const char* project_id) {
  AppOptions options = testing::MockAppOptions();
  options.set_project_id(project_id);
  return testing::CreateApp(options, name);
}

TEST(FirestoreLifecycleTest, DefaultHandleIsInvalid) {
  Firestore db;
  EXPECT_FALSE(db.is_valid());
  EXPECT_FALSE(static_cast<bool>(db));
  EXPECT_FALSE(db.app().is_valid());
  EXPECT_FALSE(db.Document("cities/SF").is_valid());
  EXPECT_FALSE(db.Collection("cities").is_valid());
  EXPECT_FALSE(db.batch().is_valid());
}

TEST(FirestoreLifecycleTest, InvalidAppReturnsInvalidAndNullDatabaseThrows) {
  App invalid_app;
  InitResult init_result = kInitResultSuccess;
  Firestore invalid_db = Firestore::GetInstance(invalid_app, &init_result);
  EXPECT_FALSE(invalid_db.is_valid());
  EXPECT_FALSE(static_cast<bool>(invalid_db));
  EXPECT_EQ(init_result, kInitResultFailedMissingDependency);

  init_result = kInitResultSuccess;
  Firestore invalid_named =
      Firestore::GetInstance(invalid_app, "secondary", &init_result);
  EXPECT_FALSE(invalid_named.is_valid());
  EXPECT_EQ(init_result, kInitResultFailedMissingDependency);

  App app = CreateTestApp("fs_throw_app", "proj-throw");
  EXPECT_THROW(Firestore::GetInstance(app, static_cast<const char*>(nullptr)),
               std::invalid_argument);
}

TEST(FirestoreLifecycleTest, HandleKeepsAppStateAliveAndCleansUpAcyclically) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  DocumentReference doc;
  CollectionReference col;
  WriteBatch batch;

  {
    Firestore db;
    {
      App app = CreateTestApp("fs_keepalive_app", "proj-keepalive");
      weak_state = ::firebase::internal::AppState::FromApp(app);

      InitResult init_result = kInitResultFailedMissingDependency;
      db = Firestore::GetInstance(app, &init_result);
      ASSERT_EQ(init_result, kInitResultSuccess);
      ASSERT_TRUE(db.is_valid());
      EXPECT_EQ(db.app(), app);

      Settings settings = db.settings();
      settings.set_persistence_enabled(false);
      db.set_settings(settings);

      Firestore db2 = Firestore::GetInstance(app);
      EXPECT_EQ(db, db2);
    }

    // App handle dropped, but Firestore keeps AppState alive.
    EXPECT_FALSE(weak_state.expired());
    EXPECT_TRUE(db.app().is_valid());
    EXPECT_STREQ(db.app().name(), "fs_keepalive_app");

    col = db.Collection("cities");
    doc = db.Document("cities/SF");
    batch = db.batch();
    EXPECT_TRUE(col.is_valid());
    EXPECT_TRUE(doc.is_valid());
    EXPECT_TRUE(batch.is_valid());
    EXPECT_EQ(col.firestore(), db);
    EXPECT_EQ(doc.firestore(), db);
    EXPECT_EQ(doc.path(), "cities/SF");
  }

  // Last Firestore handle dropped: AppState tears down cleanly with zero
  // cycles, and surviving sub-handles are safely invalidated.
  EXPECT_TRUE(weak_state.expired());
  EXPECT_FALSE(col.is_valid());
  EXPECT_FALSE(doc.is_valid());
  EXPECT_FALSE(batch.is_valid());
  EXPECT_FALSE(col.firestore().is_valid());
  EXPECT_FALSE(doc.firestore().is_valid());
}

TEST(FirestoreLifecycleTest, MultiAppAndMultiDatabaseIsolation) {
  App app1 = CreateTestApp("fs_multi_1", "proj-multi-1");
  App app2 = CreateTestApp("fs_multi_2", "proj-multi-2");

  Firestore db1_default = Firestore::GetInstance(app1);
  Firestore db1_custom = Firestore::GetInstance(app1, "secondary");
  Firestore db2_default = Firestore::GetInstance(app2);

  ASSERT_TRUE(db1_default.is_valid());
  ASSERT_TRUE(db1_custom.is_valid());
  ASSERT_TRUE(db2_default.is_valid());

  EXPECT_NE(db1_default, db1_custom);
  EXPECT_NE(db1_default, db2_default);
  EXPECT_EQ(db1_default, Firestore::GetInstance(app1));
  EXPECT_EQ(db1_custom, Firestore::GetInstance(app1, "secondary"));
  EXPECT_EQ(db1_default.app(), app1);
  EXPECT_EQ(db1_custom.app(), app1);
  EXPECT_EQ(db2_default.app(), app2);
}

TEST(FirestoreLifecycleTest, OptInRegisteredDefaultAppLookup) {
  AppOptions options = testing::MockAppOptions();
  options.set_project_id("proj-registered");
  App app = App::CreateRegistered(options);
  ASSERT_TRUE(app.is_valid());

  auto found = App::GetInstance();
  ASSERT_TRUE(found.has_value());
  Firestore db = Firestore::GetInstance(*found);
  ASSERT_TRUE(db.is_valid());
  EXPECT_EQ(db.app(), app);
  EXPECT_EQ(db, Firestore::GetInstance(app));

  Firestore custom_db = Firestore::GetInstance(*found, "custom-reg-db");
  ASSERT_TRUE(custom_db.is_valid());
  EXPECT_NE(db, custom_db);
  EXPECT_EQ(custom_db, Firestore::GetInstance(app, "custom-reg-db"));

  app.Unregister();
}

TEST(FirestoreLifecycleTest, TerminateDetachesAndAllowsClearAndRestart) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  {
    App app = CreateTestApp("fs_term_app", "proj-term");
    weak_state = ::firebase::internal::AppState::FromApp(app);

    Firestore db1 = Firestore::GetInstance(app);
    ASSERT_TRUE(db1.is_valid());
    Settings settings = db1.settings();
    settings.set_persistence_enabled(false);
    db1.set_settings(settings);

    Future<void> term_future = db1.Terminate();
    WaitForFuture(term_future);
    EXPECT_EQ(term_future.error(), kErrorOk);

    // Terminated handle remains alive so ClearPersistence() works.
    Future<void> clear_future = db1.ClearPersistence();
    WaitForFuture(clear_future);
    EXPECT_EQ(clear_future.error(), kErrorOk);

    // GetInstance() now creates a fresh Firestore instance for the same App.
    Firestore db2 = Firestore::GetInstance(app);
    ASSERT_TRUE(db2.is_valid());
    EXPECT_NE(db1, db2);
    Settings settings2 = db2.settings();
    settings2.set_persistence_enabled(false);
    db2.set_settings(settings2);

    // Dropping the terminated handle does not affect db2 or AppState.
    db1 = Firestore();
    EXPECT_FALSE(weak_state.expired());
    EXPECT_EQ(db2.app(), app);
  }
  EXPECT_TRUE(weak_state.expired());
}

TEST(FirestoreLifecycleTest, CrossProductAuthAndAppCheckTeardownOrder) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  {
    Firestore db;
    {
      App app = CreateTestApp("fs_cross_app", "proj-cross");
      weak_state = ::firebase::internal::AppState::FromApp(app);

      auth::Auth auth = auth::Auth::GetAuth(app);
      app_check::AppCheck app_check = app_check::AppCheck::GetInstance(app);
      db = Firestore::GetInstance(app);
      ASSERT_TRUE(auth.is_valid());
      ASSERT_TRUE(app_check.is_valid());
      ASSERT_TRUE(db.is_valid());

      Settings settings = db.settings();
      settings.set_persistence_enabled(false);
      db.set_settings(settings);

      // Trigger listener registration via EnableNetwork/DisableNetwork.
      WaitForFuture(db.DisableNetwork());
    }
    // App, Auth, and AppCheck handles all dropped; Firestore still holds
    // AppState (and Auth/AppCheck providers inside AppState) alive.
    EXPECT_FALSE(weak_state.expired());
  }
  // Firestore drops -> AppState destroys Firestore (priority 0) before Auth and
  // AppCheck (priority -10), with zero UAF or leaks.
  EXPECT_TRUE(weak_state.expired());
}

TEST(FirestoreLifecycleTest, CopyAndMoveAssignOverwritingLastAppStateOwner) {
  std::weak_ptr<::firebase::internal::AppState> weak_state1;
  std::weak_ptr<::firebase::internal::AppState> weak_state2;
  {
    Firestore db1;
    Firestore db2;
    {
      App app1 = CreateTestApp("fs_assign_1", "proj-assign-1");
      App app2 = CreateTestApp("fs_assign_2", "proj-assign-2");
      weak_state1 = ::firebase::internal::AppState::FromApp(app1);
      weak_state2 = ::firebase::internal::AppState::FromApp(app2);
      db1 = Firestore::GetInstance(app1);
      db2 = Firestore::GetInstance(app2);
      Settings s1 = db1.settings();
      s1.set_persistence_enabled(false);
      db1.set_settings(s1);
      Settings s2 = db2.settings();
      s2.set_persistence_enabled(false);
      db2.set_settings(s2);
    }
    // db1 is the sole remaining owner of app1's AppState. Copy-assigning db2
    // over db1 must destroy app1 cleanly without UAF on app_state_ ordering.
    db1 = db2;
    EXPECT_TRUE(weak_state1.expired());
    EXPECT_FALSE(weak_state2.expired());
    EXPECT_EQ(db1, db2);

    // Now test move-assignment overwriting the sole owner of a fresh AppState.
    std::weak_ptr<::firebase::internal::AppState> weak_state3;
    Firestore db3;
    {
      App app3 = CreateTestApp("fs_assign_3", "proj-assign-3");
      weak_state3 = ::firebase::internal::AppState::FromApp(app3);
      db3 = Firestore::GetInstance(app3);
      Settings s3 = db3.settings();
      s3.set_persistence_enabled(false);
      db3.set_settings(s3);
    }
    db3 = std::move(db1);
    EXPECT_TRUE(weak_state3.expired());
    EXPECT_FALSE(db1.is_valid());  // NOLINT
    EXPECT_EQ(db3, db2);
  }
  EXPECT_TRUE(weak_state2.expired());
}

TEST(FirestoreLifecycleTest,
     ReverseOrderFirestoreCreatedBeforeAuthAndAppCheck) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  {
    Firestore db;
    {
      App app = CreateTestApp("fs_reverse_app", "proj-reverse");
      weak_state = ::firebase::internal::AppState::FromApp(app);

      // Create Firestore BEFORE Auth and AppCheck to exercise teardown_priority
      // AND reactive AppState token subscriptions.
      db = Firestore::GetInstance(app);
      ASSERT_TRUE(db.is_valid());
      Settings settings = db.settings();
      settings.set_persistence_enabled(false);
      db.set_settings(settings);
      WaitForFuture(db.DisableNetwork());

      auth::Auth auth = auth::Auth::GetAuth(app);
      app_check::AppCheck app_check = app_check::AppCheck::GetInstance(app);
      ASSERT_TRUE(auth.is_valid());
      ASSERT_TRUE(app_check.is_valid());
    }
    EXPECT_FALSE(weak_state.expired());
  }
  // Background Auth persistent-cache load may momentarily hold a transient
  // Auth handle during NotifyAllListeners on its scheduler thread; wait
  // briefly for that callback to finish and release AppState.
  for (int i = 0; i < 500 && !weak_state.expired(); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  EXPECT_TRUE(weak_state.expired());
}

TEST(FirestoreLifecycleTest, TerminateOnTemporaryHandleResolvesFuture) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  {
    App app = CreateTestApp("fs_temp_term_app", "proj-temp-term");
    weak_state = ::firebase::internal::AppState::FromApp(app);
    {
      Firestore db = Firestore::GetInstance(app);
      Settings settings = db.settings();
      settings.set_persistence_enabled(false);
      db.set_settings(settings);
    }
    // Temporary Firestore handle is destroyed immediately after Terminate()
    // returns, yet the returned Future<void> must still resolve cleanly.
    Future<void> term_future = Firestore::GetInstance(app).Terminate();
    WaitForFuture(term_future);
    EXPECT_EQ(term_future.error(), kErrorOk);
  }
  EXPECT_TRUE(weak_state.expired());
}

TEST(FirestoreLifecycleTest, MovedFromHandleAndChildBoolOperators) {
  App app = CreateTestApp("fs_moved_app", "proj-moved");
  Firestore db = Firestore::GetInstance(app);
  ASSERT_TRUE(static_cast<bool>(db));
  Settings settings = db.settings();
  settings.set_persistence_enabled(false);
  db.set_settings(settings);

  DocumentReference doc = db.Document("cities/SF");
  CollectionReference col = db.Collection("cities");
  EXPECT_TRUE(static_cast<bool>(doc));
  EXPECT_TRUE(static_cast<bool>(col));

  Firestore moved = std::move(db);
  EXPECT_FALSE(db.is_valid());  // NOLINT
  EXPECT_FALSE(static_cast<bool>(db));
  EXPECT_FALSE(db.app().is_valid());
  EXPECT_FALSE(db.Document("cities/LA").is_valid());
  EXPECT_FALSE(db.Collection("cities").is_valid());
  EXPECT_FALSE(db.batch().is_valid());
  Future<void> moved_terminate = db.Terminate();
  EXPECT_EQ(moved_terminate.status(), kFutureStatusComplete);
  EXPECT_EQ(moved_terminate.error(), Error::kErrorFailedPrecondition);
  Future<void> moved_clear = db.ClearPersistence();
  EXPECT_EQ(moved_clear.status(), kFutureStatusComplete);
  EXPECT_EQ(moved_clear.error(), Error::kErrorFailedPrecondition);
  EXPECT_TRUE(moved.is_valid());
}

TEST(FirestoreLifecycleTest, StaticDurationFirestoreSurvivesExit) {
  static Firestore static_db;
  App app = CreateTestApp("fs_static_exit_app", "proj-static-exit");
  static_db = Firestore::GetInstance(app);
  ASSERT_TRUE(static_db.is_valid());
  Settings settings = static_db.settings();
  settings.set_persistence_enabled(false);
  static_db.set_settings(settings);
}

}  // namespace
}  // namespace firestore
}  // namespace firebase
