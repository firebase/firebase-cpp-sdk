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

#include "app/src/app_state.h"

#include <memory>
#include <string>
#include <vector>

#include "app/src/app_common.h"
#include "app/src/include/firebase/app.h"
#include "app/src/service_interfaces.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase {
namespace internal {
namespace {

using ::testing::ElementsAre;

AppOptions MakeTestOptions(const char* app_id = "1:123:web:abc") {
  AppOptions options;
  options.set_app_id(app_id);
  options.set_api_key("test_api_key");
  options.set_project_id("test_project_id");
  return options;
}

class RecordingService : public Service {
 public:
  RecordingService(std::string id, std::vector<std::string>* log,
                   int priority = kDefaultServiceTeardownPriority)
      : id_(std::move(id)), log_(log), priority_(priority) {}

  ~RecordingService() override {
    if (log_) log_->push_back(id_);
  }

  int teardown_priority() const override { return priority_; }
  const std::string& id() const { return id_; }

 private:
  std::string id_;
  std::vector<std::string>* log_;
  int priority_;
};

class ConsumerCheckingProviderOnDestroy : public Service {
 public:
  ConsumerCheckingProviderOnDestroy(AppState& state, const void* provider_tag,
                                    bool* saw_provider_alive)
      : state_(state),
        provider_tag_(provider_tag),
        saw_provider_alive_(saw_provider_alive) {}

  ~ConsumerCheckingProviderOnDestroy() override {
    *saw_provider_alive_ =
        state_.FindService<RecordingService>(provider_tag_) != nullptr;
  }

 private:
  AppState& state_;
  const void* provider_tag_;
  bool* saw_provider_alive_;
};

// Static App handle destroyed during process exit after main() returns.
// Verifies ~AppState() touches zero global registries during static teardown.
struct StaticExitAppHolder {
  App app;
  App registered_app;
};

StaticExitAppHolder& StaticHolder() {
  static StaticExitAppHolder holder;
  return holder;
}

class AppStateTest : public ::testing::Test {
 protected:
  void TearDown() override { app_common::ClearRegisteredAppsForTesting(); }
};

TEST_F(AppStateTest, UnregisteredByDefaultAllowsIndependentAppsWithSameName) {
  App app1 = App::Create(MakeTestOptions("app_1"));
  App app2 = App::Create(MakeTestOptions("app_2"));

  ASSERT_TRUE(app1.is_valid());
  ASSERT_TRUE(app2.is_valid());
  EXPECT_NE(app1, app2);
  EXPECT_STREQ(app1.name(), kDefaultAppName);
  EXPECT_STREQ(app2.name(), kDefaultAppName);
  EXPECT_STREQ(app1.options().app_id(), "app_1");
  EXPECT_STREQ(app2.options().app_id(), "app_2");

  // Neither app is visible in the global registry unless opted in.
  EXPECT_FALSE(App::GetInstance().has_value());
  EXPECT_TRUE(App::GetApps().empty());
}

TEST_F(AppStateTest, CopyAndMoveShareUnderlyingState) {
  App app1 = App::Create(MakeTestOptions(), "shared");
  App app2 = app1;
  EXPECT_EQ(app1, app2);

  App app3 = std::move(app1);
  EXPECT_FALSE(app1.is_valid());
  EXPECT_TRUE(app3.is_valid());
  EXPECT_EQ(app2, app3);
}

TEST_F(AppStateTest, OptInRegisterGetInstanceAndLazyExpiration) {
  {
    App app = App::Create(MakeTestOptions(), "ephemeral");
    ASSERT_TRUE(app.is_valid());
    EXPECT_FALSE(App::GetInstance("ephemeral").has_value());

    EXPECT_TRUE(app.Register());
    // Idempotent for the same App.
    EXPECT_TRUE(app.Register());

    auto looked_up = App::GetInstance("ephemeral");
    ASSERT_TRUE(looked_up.has_value());
    EXPECT_EQ(app, *looked_up);

    // Duplicate registration of a different live App with the same name fails.
    App duplicate = App::Create(MakeTestOptions("other"), "ephemeral");
    EXPECT_FALSE(duplicate.Register());
    EXPECT_EQ(*App::GetInstance("ephemeral"), app);
  }

  // Registry held only a weak_ptr; once all handles dropped, lookup returns
  // std::nullopt and lazily prunes the entry.
  EXPECT_FALSE(App::GetInstance("ephemeral").has_value());
  EXPECT_TRUE(App::GetApps().empty());

  // A new App with the same name can now register cleanly.
  App replacement = App::CreateRegistered(MakeTestOptions("v2"), "ephemeral");
  ASSERT_TRUE(replacement.is_valid());
  auto found = App::GetInstance("ephemeral");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(*found, replacement);
  EXPECT_STREQ(found->options().app_id(), "v2");

  replacement.Unregister();
  EXPECT_FALSE(App::GetInstance("ephemeral").has_value());
}

TEST_F(AppStateTest, GetAppsPlacesDefaultAppLast) {
  App def = App::CreateRegistered(MakeTestOptions("def"));
  App alpha = App::CreateRegistered(MakeTestOptions("a"), "alpha");
  App beta = App::CreateRegistered(MakeTestOptions("b"), "beta");

  auto apps = App::GetApps();
  ASSERT_EQ(apps.size(), 3u);
  EXPECT_EQ(apps[0], alpha);
  EXPECT_EQ(apps[1], beta);
  EXPECT_EQ(apps[2], def);
}

TEST_F(AppStateTest, AliasingServiceHandleKeepsAppStateAliveAfterAppDropped) {
  static const char kServiceTag = 0;
  std::vector<std::string> destroyed;
  std::shared_ptr<RecordingService> service_handle;

  {
    App app = App::Create(MakeTestOptions(), "keepalive");
    auto state = AppState::FromApp(app);
    service_handle = state->GetOrCreateService<RecordingService>(
        &kServiceTag, "", [&](AppState&) {
          return std::make_shared<RecordingService>("svc", &destroyed);
        });
    ASSERT_NE(service_handle, nullptr);

    // Calling GetOrCreateService again returns the same instance.
    auto same = state->GetOrCreateService<RecordingService>(
        &kServiceTag, "", [&](AppState&) {
          return std::make_shared<RecordingService>("duplicate", &destroyed);
        });
    EXPECT_EQ(same.get(), service_handle.get());
  }

  // App handle was dropped, but service_handle keeps AppState alive.
  EXPECT_TRUE(destroyed.empty());
  EXPECT_EQ(service_handle->id(), "svc");

  // Dropping the last service handle destroys AppState and the service.
  service_handle.reset();
  EXPECT_THAT(destroyed, ElementsAre("svc"));
}

TEST_F(AppStateTest, ServiceTeardownRespectsPriorityAndReverseCreationOrder) {
  static const char kTagA = 0;
  static const char kTagB = 0;
  static const char kTagProvider = 0;
  static const char kTagConsumer = 0;

  std::vector<std::string> destroyed;
  bool consumer_saw_provider_alive = false;

  {
    App app = App::Create(MakeTestOptions(), "ordering");
    auto state = AppState::FromApp(app);

    // Create consumer BEFORE provider to verify teardown_priority overrides
    // creation order and keeps providers alive while consumers tear down.
    state->GetOrCreateService<RecordingService>(
        &kTagA, "", [&](AppState&) {
          return std::make_shared<RecordingService>(
              "consumer_first", &destroyed, kDefaultServiceTeardownPriority);
        });
    state->GetOrCreateService<ConsumerCheckingProviderOnDestroy>(
        &kTagConsumer, "", [&](AppState& s) {
          return std::make_shared<ConsumerCheckingProviderOnDestroy>(
              s, &kTagProvider, &consumer_saw_provider_alive);
        });
    state->GetOrCreateService<RecordingService>(
        &kTagProvider, "", [&](AppState&) {
          return std::make_shared<RecordingService>(
              "provider", &destroyed, kProviderServiceTeardownPriority);
        });
    state->GetOrCreateService<RecordingService>(
        &kTagB, "", [&](AppState&) {
          return std::make_shared<RecordingService>(
              "consumer_second", &destroyed, kDefaultServiceTeardownPriority);
        });
  }

  EXPECT_TRUE(consumer_saw_provider_alive);
  // Consumers (priority 0) in reverse order, then provider (priority -10).
  EXPECT_THAT(destroyed,
              ElementsAre("consumer_second", "consumer_first", "provider"));
}

TEST_F(AppStateTest, DetachableServiceAllowsReplacementWhileOldHandleLives) {
  static const char kDetachTag = 0;
  std::vector<std::string> destroyed;

  App app = App::Create(MakeTestOptions(), "detach");
  auto state = AppState::FromApp(app);

  auto first = state->GetOrCreateDetachableService<RecordingService>(
      &kDetachTag, "(default)", [&](AppState&) {
        return std::make_shared<RecordingService>("first", &destroyed);
      });
  ASSERT_NE(first.second, nullptr);

  // Detach from AppState cache (simulating Firestore::Terminate()).
  EXPECT_TRUE(state->DetachService(&kDetachTag, "(default)", first.second.get()));
  // Old handle keeps the detached service alive!
  EXPECT_TRUE(destroyed.empty());

  // Creating a new instance under the same key succeeds and yields a fresh one.
  auto second = state->GetOrCreateDetachableService<RecordingService>(
      &kDetachTag, "(default)", [&](AppState&) {
        return std::make_shared<RecordingService>("second", &destroyed);
      });
  EXPECT_NE(first.second.get(), second.second.get());

  // Dropping the old handle frees it immediately while App is still alive.
  first.second.reset();
  first.first.reset();
  EXPECT_THAT(destroyed, ElementsAre("first"));

  // Dropping remaining handles destroys the second instance cleanly.
  second.second.reset();
  second.first.reset();
  state.reset();
  app = App();
  EXPECT_THAT(destroyed, ElementsAre("first", "second"));
}

TEST_F(AppStateTest, StaticDurationAppsSurviveProcessExit) {
  auto& holder = StaticHolder();
  holder.app = App::Create(MakeTestOptions("static_unreg"), "static_unreg");
  holder.registered_app =
      App::CreateRegistered(MakeTestOptions("static_reg"), "static_reg");
  ASSERT_TRUE(holder.app.is_valid());
  ASSERT_TRUE(holder.registered_app.is_valid());
  // Intentionally left alive until after main() returns to verify clean static
  // destruction under ASan/UBSan/TSan and standard builds.
}

}  // namespace
}  // namespace internal
}  // namespace firebase
