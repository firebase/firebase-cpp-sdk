// Copyright 2023 Google LLC
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

#include "app_check/src/include/firebase/app_check.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace app_check {
namespace {

template <typename T>
bool WaitForFuture(const Future<T>& future,
                   std::chrono::milliseconds timeout =
                       std::chrono::milliseconds(5000)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (future.status() == kFutureStatusPending) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return true;
}

class FakeProvider : public AppCheckProvider {
 public:
  void GetToken(std::function<void(AppCheckToken, int, const std::string&)>
                    completion_callback) override {
    int call_num = get_token_calls.fetch_add(1) + 1;
    int err = error_code.load();
    if (err != kAppCheckErrorNone) {
      completion_callback(AppCheckToken{}, err, "simulated_provider_error");
      return;
    }
    AppCheckToken tok;
    tok.token = "custom_token_" + std::to_string(call_num);
    tok.expire_time_millis = 4102444800000LL;  // Year 2100 in ms
    completion_callback(tok, kAppCheckErrorNone, "");
  }

  void GetLimitedUseToken(
      std::function<void(AppCheckToken, int, const std::string&)>
          completion_callback) override {
    get_limited_use_token_calls.fetch_add(1);
    int err = error_code.load();
    if (err != kAppCheckErrorNone) {
      completion_callback(AppCheckToken{}, err, "simulated_provider_error");
      return;
    }
    AppCheckToken tok;
    tok.token = "limited_token_456";
    tok.expire_time_millis = 4102444800000LL;
    completion_callback(tok, kAppCheckErrorNone, "");
  }

  std::atomic<int> get_token_calls{0};
  std::atomic<int> get_limited_use_token_calls{0};
  std::atomic<int> error_code{kAppCheckErrorNone};
};

class FakeProviderFactory : public AppCheckProviderFactory {
 public:
  AppCheckProvider* CreateProvider(App* app) override {
    if (!app) return nullptr;
    return &provider_;
  }

  FakeProvider& provider() { return provider_; }

 private:
  FakeProvider provider_;
};

class ScopedTestListener : public AppCheckListener {
 public:
  explicit ScopedTestListener(AppCheck* app_check = nullptr)
      : app_check_(app_check) {
    if (app_check_ != nullptr) {
      app_check_->AddAppCheckListener(this);
    }
  }

  ~ScopedTestListener() override {
    // Unregister before `last_token_` and `mutex_` are destroyed so an
    // in-flight callback cannot race with derived member destruction.
    if (app_check_ != nullptr) {
      app_check_->RemoveAppCheckListener(this);
    }
  }

  void OnAppCheckTokenChanged(const AppCheckToken& token) override {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      last_token_ = token;
    }
    call_count.fetch_add(1);
  }

  AppCheckToken last_token() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return last_token_;
  }

  std::atomic<int> call_count{0};

 private:
  AppCheck* app_check_ = nullptr;
  mutable std::mutex mutex_;
  AppCheckToken last_token_;
};

// Stateless listener without derived members used specifically to verify that
// the base `~AppCheckListener()` destructor unregisters the listener.
class BareAutoUnregisterListener : public AppCheckListener {
 public:
  void OnAppCheckTokenChanged(const AppCheckToken& /*token*/) override {}
};

class AppCheckTest : public ::testing::Test {
 protected:
  void SetUp() override {
    AppCheck::SetAppCheckProviderFactory(&factory_);
    app_.reset(testing::CreateApp());
  }

  void TearDown() override {
    app_.reset();
    AppCheck::SetAppCheckProviderFactory(nullptr);
  }

  FakeProviderFactory factory_;
  std::unique_ptr<App> app_;
};

TEST_F(AppCheckTest, TokenDefaultInitializationAndEquality) {
  AppCheckToken t1;
  EXPECT_EQ(t1.token, "");
  EXPECT_EQ(t1.expire_time_millis, 0);
  EXPECT_FALSE(t1.is_valid());

  AppCheckToken t2;
  EXPECT_EQ(t1, t2);

  t2.token = "abc";
  t2.expire_time_millis = 1000;
  EXPECT_TRUE(t2.is_valid());
  EXPECT_NE(t1, t2);
}

TEST_F(AppCheckTest, GetInstanceNullAppReturnsNullptr) {
  InitResult init_result = kInitResultSuccess;
  EXPECT_EQ(AppCheck::GetInstance(nullptr, &init_result), nullptr);
  EXPECT_EQ(init_result, kInitResultFailedMissingDependency);
}

TEST_F(AppCheckTest, GetAppCheckTokenCachingForceRefreshAndLimitedUseToken) {
  InitResult init_result = kInitResultFailedMissingDependency;
  AppCheck* ac = AppCheck::GetInstance(app_.get(), &init_result);
  ASSERT_NE(ac, nullptr);
  EXPECT_EQ(init_result, kInitResultSuccess);
  EXPECT_EQ(ac->app(), app_.get());

  ac->SetTokenAutoRefreshEnabled(false);
  ac->SetTokenAutoRefreshEnabled(true);

  ScopedTestListener listener(ac);

  Future<AppCheckToken> f = ac->GetAppCheckToken(false);
  ASSERT_TRUE(WaitForFuture(f));
  EXPECT_EQ(f.error(), kAppCheckErrorNone);
  ASSERT_NE(f.result(), nullptr);
  EXPECT_EQ(f.result()->token, "custom_token_1");
  EXPECT_EQ(factory_.provider().get_token_calls.load(), 1);
  EXPECT_GE(listener.call_count.load(), 1);
  EXPECT_EQ(listener.last_token().token, "custom_token_1");

  // Cached fetch (`force_refresh = false`) returns the cached token without
  // invoking the provider again.
  Future<AppCheckToken> cached = ac->GetAppCheckToken(false);
  ASSERT_TRUE(WaitForFuture(cached));
  EXPECT_EQ(cached.error(), kAppCheckErrorNone);
  ASSERT_NE(cached.result(), nullptr);
  EXPECT_EQ(cached.result()->token, "custom_token_1");
  EXPECT_EQ(factory_.provider().get_token_calls.load(), 1);

  // Explicit `RemoveAppCheckListener` stops further notifications to `listener`.
  ac->RemoveAppCheckListener(&listener);
  int count_before_refresh = listener.call_count.load();

  // Forced refresh (`force_refresh = true`) invokes the provider again.
  Future<AppCheckToken> refreshed = ac->GetAppCheckToken(true);
  ASSERT_TRUE(WaitForFuture(refreshed));
  EXPECT_EQ(refreshed.error(), kAppCheckErrorNone);
  ASSERT_NE(refreshed.result(), nullptr);
  EXPECT_EQ(refreshed.result()->token, "custom_token_2");
  EXPECT_EQ(factory_.provider().get_token_calls.load(), 2);
  EXPECT_EQ(listener.call_count.load(), count_before_refresh);

  Future<AppCheckToken> lim = ac->GetLimitedUseAppCheckToken();
  ASSERT_TRUE(WaitForFuture(lim));
  EXPECT_EQ(lim.error(), kAppCheckErrorNone);
  ASSERT_NE(lim.result(), nullptr);
  EXPECT_EQ(lim.result()->token, "limited_token_456");
  EXPECT_EQ(factory_.provider().get_limited_use_token_calls.load(), 1);
}

TEST_F(AppCheckTest, ProviderErrorPropagatesToFuture) {
  AppCheck* ac = AppCheck::GetInstance(app_.get());
  ASSERT_NE(ac, nullptr);

  factory_.provider().error_code.store(kAppCheckErrorServerUnreachable);

  Future<AppCheckToken> f = ac->GetAppCheckToken(true);
  ASSERT_TRUE(WaitForFuture(f));
  EXPECT_EQ(f.error(), kAppCheckErrorServerUnreachable);

  Future<AppCheckToken> lim = ac->GetLimitedUseAppCheckToken();
  ASSERT_TRUE(WaitForFuture(lim));
  EXPECT_EQ(lim.error(), kAppCheckErrorServerUnreachable);
}

TEST_F(AppCheckTest, ListenerDestructorAutomaticallyUnregisters) {
  AppCheck* ac = AppCheck::GetInstance(app_.get());
  ASSERT_NE(ac, nullptr);
  {
    BareAutoUnregisterListener short_lived_listener;
    ac->AddAppCheckListener(&short_lived_listener);
  }
  // Triggering a force refresh after listener destruction must not UAF.
  Future<AppCheckToken> f = ac->GetAppCheckToken(true);
  ASSERT_TRUE(WaitForFuture(f));
  EXPECT_EQ(f.error(), kAppCheckErrorNone);
}

TEST_F(AppCheckTest, UnsupportedProviderStubsAndDebugFactoryAreSafe) {
#if !FIREBASE_PLATFORM_ANDROID
  EXPECT_EQ(PlayIntegrityProviderFactory::GetInstance(), nullptr);
#endif

#if !(FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS)
  EXPECT_EQ(DeviceCheckProviderFactory::GetInstance(), nullptr);
  EXPECT_EQ(AppAttestProviderFactory::GetInstance(), nullptr);
#endif

  DebugAppCheckProviderFactory* debug_factory =
      DebugAppCheckProviderFactory::GetInstance();
  ASSERT_NE(debug_factory, nullptr);
  EXPECT_EQ(debug_factory->CreateProvider(nullptr), nullptr);
  EXPECT_NE(debug_factory->CreateProvider(app_.get()), nullptr);
}

}  // namespace
}  // namespace app_check
}  // namespace firebase
