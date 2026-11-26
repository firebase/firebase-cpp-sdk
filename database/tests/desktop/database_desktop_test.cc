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

#include "database/src/desktop/database_desktop.h"

#include <memory>

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace database {
namespace internal {
namespace {

constexpr char kDefaultDatabaseUrl[] = "https://abc-xyz-123.firebaseio.com";

class DatabaseDesktopTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AppOptions options = firebase::testing::MockAppOptions();
    options.set_database_url(kDefaultDatabaseUrl);
    app_.reset(firebase::testing::CreateApp(options));
  }

  void TearDown() override { app_.reset(); }

  std::unique_ptr<App> app_;
};

TEST_F(DatabaseDesktopTest, PersistenceEnabledDefaultsToFalse) {
  DatabaseInternal db(app_.get());
  EXPECT_FALSE(db.persistence_enabled());
}

TEST_F(DatabaseDesktopTest, PersistenceEnabledPersistsAcrossInstances) {
  {
    DatabaseInternal db(app_.get());
    EXPECT_FALSE(db.persistence_enabled());
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }

  // A new DatabaseInternal instance for the same App and URL should retain
  // persistence_enabled_ = true.
  {
    DatabaseInternal db(app_.get());
    EXPECT_TRUE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, SetPersistenceEnabledFalseOverridesTrue) {
  {
    DatabaseInternal db(app_.get());
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get());
    EXPECT_TRUE(db.persistence_enabled());
    db.SetPersistenceEnabled(false);
    EXPECT_FALSE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get());
    EXPECT_FALSE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, PersistenceEnabledNormalizesUrls) {
  // Set with trailing slash and uppercase host.
  {
    DatabaseInternal db(app_.get(), "https://ABC-XYZ-123.firebaseio.com/");
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Default URL (from AppOptions) should match.
  {
    DatabaseInternal db(app_.get());
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Explicit URL without trailing slash should match.
  {
    DatabaseInternal db(app_.get(), "https://abc-xyz-123.firebaseio.com");
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Omitted scheme defaults to https:// in ParseUrl and should match.
  {
    DatabaseInternal db(app_.get(), "abc-xyz-123.firebaseio.com");
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Subpath on the same host should normalize to the same host URL.
  {
    DatabaseInternal db(app_.get(), "https://abc-xyz-123.firebaseio.com/child");
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Different scheme (http:// vs https://) should NOT match.
  {
    DatabaseInternal db(app_.get(), "http://abc-xyz-123.firebaseio.com");
    EXPECT_FALSE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, PersistenceEnabledInvalidUrlFallback) {
  {
    DatabaseInternal db(app_.get(), "not-a-valid-url");
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get(), "not-a-valid-url");
    EXPECT_TRUE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get(), "other-invalid-url");
    EXPECT_FALSE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, PersistenceEnabledIsolatedAcrossAppsAndUrls) {
  AppOptions options = firebase::testing::MockAppOptions();
  options.set_database_url(kDefaultDatabaseUrl);
  std::unique_ptr<App> second_app(
      firebase::testing::CreateApp(options, "second_app"));

  {
    DatabaseInternal db(app_.get(), "https://db-one.firebaseio.com");
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Same App, different URL should remain false.
  {
    DatabaseInternal db(app_.get(), "https://db-two.firebaseio.com");
    EXPECT_FALSE(db.persistence_enabled());
  }

  // Different App, same URL should remain false.
  {
    DatabaseInternal db(second_app.get(), "https://db-one.firebaseio.com");
    EXPECT_FALSE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, SetPersistenceEnabledIsNoOpAfterRepoInitialized) {
  // Use an unparseable URL so Repo::Repo returns early without spinning up a
  // network connection or writing to disk, while still initializing repo_.
  constexpr char kLocalOnlyUrl[] = "invalid-url-for-repo-init";

  // Case 1: persistence enabled -> init repo -> attempt to disable -> no-op.
  {
    DatabaseInternal db(app_.get(), kLocalOnlyUrl);
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
    db.GetReference();
    ASSERT_NE(db.repo(), nullptr);

    db.SetPersistenceEnabled(false);
    EXPECT_TRUE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get(), kLocalOnlyUrl);
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Case 2: persistence disabled -> init repo -> attempt to enable -> no-op.
  constexpr char kLocalOnlyUrl2[] = "invalid-url-for-repo-init-2";
  {
    DatabaseInternal db(app_.get(), kLocalOnlyUrl2);
    EXPECT_FALSE(db.persistence_enabled());
    db.GetReference();
    ASSERT_NE(db.repo(), nullptr);

    db.SetPersistenceEnabled(true);
    EXPECT_FALSE(db.persistence_enabled());
  }
  {
    DatabaseInternal db(app_.get(), kLocalOnlyUrl2);
    EXPECT_FALSE(db.persistence_enabled());
  }
}

TEST_F(DatabaseDesktopTest, PersistenceEnabledClearedWhenAppDestroyed) {
  AppOptions options = firebase::testing::MockAppOptions();
  options.set_database_url(kDefaultDatabaseUrl);

  {
    std::unique_ptr<App> temp_app(
        firebase::testing::CreateApp(options, "ephemeral_app"));
    DatabaseInternal db(temp_app.get());
    db.SetPersistenceEnabled(true);
    EXPECT_TRUE(db.persistence_enabled());
  }

  // Recreating an App with the same name must start with fresh persistence
  // state, matching Android and iOS App lifecycle semantics.
  {
    std::unique_ptr<App> recreated_app(
        firebase::testing::CreateApp(options, "ephemeral_app"));
    DatabaseInternal db(recreated_app.get());
    EXPECT_FALSE(db.persistence_enabled());
  }
}

}  // namespace
}  // namespace internal
}  // namespace database
}  // namespace firebase
