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

#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "app/src/app_state.h"
#include "app/src/include/firebase/app.h"
#include "app/src/service_interfaces.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "firebase/app_check.h"
#include "gtest/gtest.h"

namespace firebase {
namespace app_check {
namespace {

class FakeAppCheckProvider : public AppCheckProvider {
 public:
  explicit FakeAppCheckProvider(std::string token_prefix)
      : token_prefix_(std::move(token_prefix)) {}

  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override {
    int n = ++calls_;
    if (defer_completion_) {
      std::lock_guard<std::mutex> lock(mu_);
      pending_callbacks_.push_back(std::move(completion_callback));
      return;
    }
    AppCheckToken tok;
    tok.token = token_prefix_ + "-" + std::to_string(n);
    tok.expire_time_millis =
        (static_cast<int64_t>(std::time(nullptr)) + 3600) * 1000;
    completion_callback(tok, kAppCheckErrorNone, "");
  }

  void CompleteAllPending(const std::string& token_value) {
    std::vector<std::function<void(AppCheckToken, int, const std::string&)>>
        callbacks;
    {
      std::lock_guard<std::mutex> lock(mu_);
      callbacks.swap(pending_callbacks_);
    }
    AppCheckToken tok;
    tok.token = token_value;
    tok.expire_time_millis =
        (static_cast<int64_t>(std::time(nullptr)) + 3600) * 1000;
    for (auto& cb : callbacks) {
      cb(tok, kAppCheckErrorNone, "");
    }
  }

  void set_defer_completion(bool defer) { defer_completion_ = defer; }
  int calls() const { return calls_.load(); }

 private:
  std::string token_prefix_;
  std::atomic<int> calls_{0};
  bool defer_completion_ = false;
  std::mutex mu_;
  std::vector<std::function<void(AppCheckToken, int, const std::string&)>>
      pending_callbacks_;
};

class FakeAppCheckProviderFactory : public AppCheckProviderFactory {
 public:
  AppCheckProvider* CreateProvider(const App& app) override {
    std::lock_guard<std::mutex> lock(mu_);
    auto& provider = providers_[app.name()];
    if (!provider) {
      provider = std::make_unique<FakeAppCheckProvider>(std::string("tok-") +
                                                        app.name());
      provider->set_defer_completion(defer_completion_);
    }
    return provider.get();
  }

  FakeAppCheckProvider* GetProvider(const std::string& app_name) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = providers_.find(app_name);
    return it == providers_.end() ? nullptr : it->second.get();
  }

  void set_defer_completion(bool defer) { defer_completion_ = defer; }

 private:
  std::mutex mu_;
  bool defer_completion_ = false;
  std::map<std::string, std::unique_ptr<FakeAppCheckProvider>> providers_;
};

class RecordingListener : public AppCheckListener {
 public:
  void OnAppCheckTokenChanged(const AppCheckToken& token) override {
    std::lock_guard<std::mutex> lock(mu_);
    tokens_.push_back(token.token);
  }

  std::vector<std::string> tokens() const {
    std::lock_guard<std::mutex> lock(mu_);
    return tokens_;
  }

 private:
  mutable std::mutex mu_;
  std::vector<std::string> tokens_;
};

class AppCheckDesktopTest : public ::testing::Test {
 protected:
  void TearDown() override { AppCheck::SetAppCheckProviderFactory(nullptr); }
};

TEST_F(AppCheckDesktopTest, InvalidAppReturnsInvalidHandle) {
  App empty_app;
  AppCheck ac = AppCheck::GetInstance(empty_app);
  EXPECT_FALSE(ac.is_valid());
  EXPECT_FALSE(static_cast<bool>(ac));
  EXPECT_FALSE(ac.app().is_valid());
}

TEST_F(AppCheckDesktopTest, AutoAttachedAndIsolatedAcrossMultipleApps) {
  FakeAppCheckProviderFactory factory;
  AppCheck::SetAppCheckProviderFactory(&factory);

  App app1 = testing::CreateApp(testing::MockAppOptions(), "app_one");
  App app2 = testing::CreateApp(testing::MockAppOptions(), "app_two");

  // AppCheck auto-attaches on App::Create(), so AppState already exposes the
  // AppCheckTokenProvider without the caller calling AppCheck::GetInstance().
  auto* provider1 =
      ::firebase::internal::AppState::FromApp(app1)->app_check_token_provider();
  auto* provider2 =
      ::firebase::internal::AppState::FromApp(app2)->app_check_token_provider();
  ASSERT_NE(provider1, nullptr);
  ASSERT_NE(provider2, nullptr);
  EXPECT_NE(provider1, provider2);

  // Destroy app1 while app2 stays alive: app2's provider must keep working!
  app1 = App();

  Future<std::string> f2 = provider2->GetTokenAsync();
  ASSERT_EQ(f2.status(), kFutureStatusComplete);
  EXPECT_EQ(f2.error(), kAppCheckErrorNone);
  ASSERT_NE(f2.result(), nullptr);
  EXPECT_EQ(*f2.result(), "tok-app_two-1");
}

TEST_F(AppCheckDesktopTest, AppCheckHandleKeepsAppStateAlive) {
  FakeAppCheckProviderFactory factory;
  AppCheck::SetAppCheckProviderFactory(&factory);

  AppCheck ac;
  {
    App app = testing::CreateApp(testing::MockAppOptions(), "keepalive_app");
    ac = AppCheck::GetInstance(app);
    ASSERT_TRUE(ac.is_valid());
    EXPECT_EQ(ac.app(), app);
  }
  // App local handle is gone, but AppCheck keeps AppState alive.
  EXPECT_TRUE(ac.is_valid());
  EXPECT_TRUE(ac.app().is_valid());
  EXPECT_STREQ(ac.app().name(), "keepalive_app");

  RecordingListener listener;
  ac.AddAppCheckListener(&listener);
  Future<AppCheckToken> f = ac.GetAppCheckToken(false);
  ASSERT_EQ(f.status(), kFutureStatusComplete);
  EXPECT_EQ(f.error(), kAppCheckErrorNone);
  ASSERT_NE(f.result(), nullptr);
  EXPECT_EQ(f.result()->token, "tok-keepalive_app-1");

  auto seen = listener.tokens();
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(seen[0], "tok-keepalive_app-1");
  ac.RemoveAppCheckListener(&listener);
}

TEST_F(AppCheckDesktopTest, SafeWhenAppDestroyedDuringInFlightTokenFetch) {
  FakeAppCheckProviderFactory factory;
  factory.set_defer_completion(true);
  AppCheck::SetAppCheckProviderFactory(&factory);

  {
    App app = testing::CreateApp(testing::MockAppOptions(), "inflight_app");
    AppCheck ac = AppCheck::GetInstance(app);
    Future<AppCheckToken> f = ac.GetAppCheckToken(true);
    EXPECT_EQ(f.status(), kFutureStatusPending);
    // Drop both App and AppCheck while the provider callback is still pending.
  }

  // Completing the deferred callback after AppState destruction must be a
  // harmless no-op (no UAF).
  FakeAppCheckProvider* provider = factory.GetProvider("inflight_app");
  ASSERT_NE(provider, nullptr);
  provider->CompleteAllPending("late-token");
}

// Verifies that an AppCheck handle with static storage duration can be
// destroyed cleanly after main() exits without touching destroyed globals.
TEST_F(AppCheckDesktopTest, StaticStorageDurationSurvivesProcessExit) {
  static AppCheck s_static_app_check;
  App app = testing::CreateApp(testing::MockAppOptions(), "static_app_check");
  s_static_app_check = AppCheck::GetInstance(app);
  EXPECT_TRUE(s_static_app_check.is_valid());
}

TEST_F(AppCheckDesktopTest,
       ConcurrentRemoveAppCheckListenerBlocksAndAllowsSameThreadReentry) {
  FakeAppCheckProviderFactory factory;
  factory.set_defer_completion(true);
  AppCheck::SetAppCheckProviderFactory(&factory);

  App app = testing::CreateApp(testing::MockAppOptions(), "ac_listener_drain");
  AppCheck ac = AppCheck::GetInstance(app);
  ASSERT_TRUE(ac.is_valid());

  std::promise<void> entered_promise;
  std::future<void> entered_future = entered_promise.get_future();
  std::promise<void> release_promise;
  std::shared_future<void> release_future =
      release_promise.get_future().share();
  std::atomic<bool> callback_exited{false};

  struct BlockingListener : public AppCheckListener {
    std::promise<void>* entered;
    std::shared_future<void> release;
    std::atomic<bool>* exited;
    int magic = 0x55aa55aa;

    void OnAppCheckTokenChanged(const AppCheckToken& token) override {
      EXPECT_EQ(token.token, "tok-barrier");
      entered->set_value();
      release.wait();
      EXPECT_EQ(magic, 0x55aa55aa);
      exited->store(true, std::memory_order_release);
    }
  };

  auto listener = std::make_unique<BlockingListener>();
  listener->entered = &entered_promise;
  listener->release = release_future;
  listener->exited = &callback_exited;
  ac.AddAppCheckListener(listener.get());

  Future<AppCheckToken> token_future = ac.GetAppCheckToken(true);
  FakeAppCheckProvider* provider = factory.GetProvider("ac_listener_drain");
  ASSERT_NE(provider, nullptr);

  std::thread notifier(
      [provider]() { provider->CompleteAllPending("tok-barrier"); });

  ASSERT_EQ(entered_future.wait_for(std::chrono::seconds(10)),
            std::future_status::ready);

  std::atomic<bool> remove_finished{false};
  std::thread remover([&]() {
    ac.RemoveAppCheckListener(listener.get());
    listener.reset();
    remove_finished.store(true, std::memory_order_release);
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  EXPECT_FALSE(remove_finished.load(std::memory_order_acquire));

  release_promise.set_value();
  remover.join();
  notifier.join();
  EXPECT_TRUE(callback_exited.load(std::memory_order_acquire));
  EXPECT_TRUE(remove_finished.load(std::memory_order_acquire));
  EXPECT_EQ(token_future.status(), kFutureStatusComplete);

  // Part 2: Same-thread re-entrant RemoveAppCheckListener inside immediate
  // cached-token notification must not deadlock.
  struct SelfRemovingListener : public AppCheckListener {
    AppCheck* ac_ptr;
    int count = 0;
    void OnAppCheckTokenChanged(const AppCheckToken& /*token*/) override {
      ++count;
      ac_ptr->RemoveAppCheckListener(this);
    }
  };
  SelfRemovingListener self_remover;
  self_remover.ac_ptr = &ac;
  ac.AddAppCheckListener(&self_remover);
  EXPECT_EQ(self_remover.count, 1);
}

}  // namespace
}  // namespace app_check
}  // namespace firebase
