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

TEST(FirestoreLifecycleTest, InvalidAppAndNullDatabaseThrow) {
  App invalid_app;
  EXPECT_THROW(Firestore::GetInstance(invalid_app), std::invalid_argument);
  EXPECT_THROW(Firestore::GetInstance(), std::invalid_argument);

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

  Firestore db = Firestore::GetInstance();
  ASSERT_TRUE(db.is_valid());
  EXPECT_EQ(db.app(), app);
  EXPECT_EQ(db, Firestore::GetInstance(app));

  Firestore custom_db = Firestore::GetInstance("custom-reg-db");
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
