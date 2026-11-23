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

// Offline desktop multi-App lifecycle stress tests targeting sanitizer and
// concurrency regressions across App, Auth/UserSecureManager, AppCheck,
// Storage, Database, and Firestore.

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "app/src/function_registry.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/secure/user_secure_internal.h"
#include "app/src/secure/user_secure_manager.h"
#include "app_check/src/include/firebase/app_check.h"
#include "auth/src/include/firebase/auth.h"
#include "database/src/include/firebase/database.h"
#include "functions/src/include/firebase/functions.h"
#include "gtest/gtest.h"
#include "installations/src/include/firebase/installations.h"
#include "remote_config/src/include/firebase/remote_config.h"
#include "storage/src/include/firebase/storage.h"
#include "storage/src/include/firebase/storage/listener.h"

#if defined(FIREBASE_STRESS_WITH_FIRESTORE)
#include "firestore/src/include/firebase/firestore.h"
#endif

namespace firebase {
namespace {

AppOptions MakeTestAppOptions() {
  AppOptions options;
  options.set_app_id("1:1234567890:android:0123456789abcdef");
  options.set_api_key("not_a_real_api_key");
  options.set_project_id("multi-app-stress-test");
  options.set_storage_bucket("multi-app-stress-test.appspot.com");
  options.set_database_url("http://127.0.0.1:19999/?ns=multi-app-stress-test");
  return options;
}

class NoOpStorageListener : public storage::Listener {
 public:
  void OnPaused(storage::Controller*) override {}
  void OnProgress(storage::Controller*) override {}
};

class FakeAppCheckProvider : public app_check::AppCheckProvider {
 public:
  explicit FakeAppCheckProvider(std::atomic<int>* calls) : calls_(calls) {}
  ~FakeAppCheckProvider() override = default;

  void GetToken(
      std::function<void(app_check::AppCheckToken, int, const std::string&)>
          completion_callback) override {
    calls_->fetch_add(1);
    app_check::AppCheckToken token;
    token.token = "fake-app-check-token";
    token.expire_time_millis = 4102444800000LL;  // 2100-01-01
    completion_callback(token, app_check::kAppCheckErrorNone, "");
  }

 private:
  std::atomic<int>* calls_;
};

class FakeAppCheckProviderFactory : public app_check::AppCheckProviderFactory {
 public:
  ~FakeAppCheckProviderFactory() override = default;

  app_check::AppCheckProvider* CreateProvider(App* /*app*/) override {
    auto provider = std::make_unique<FakeAppCheckProvider>(&calls_);
    auto* raw = provider.get();
    providers_.push_back(std::move(provider));
    return raw;
  }

  int call_count() const { return calls_.load(); }

 private:
  std::atomic<int> calls_{0};
  std::vector<std::unique_ptr<FakeAppCheckProvider>> providers_;
};

// Slow fake secure storage that mimics macOS Keychain latency so
// UserSecureManager::~UserSecureManager() overlaps with an active scheduler
// task even on fast Linux runners (reproduces #1969).
class DelayedFakeUserSecure : public app::secure::UserSecureInternal {
 public:
  explicit DelayedFakeUserSecure(std::atomic<bool>* task_started)
      : task_started_(task_started) {}

  std::string LoadUserData(const std::string& /*app_name*/) override {
    return "";
  }

  void SaveUserData(const std::string& /*app_name*/,
                    const std::string& /*user_data*/) override {
    if (task_started_) task_started_->store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  void DeleteUserData(const std::string& /*app_name*/) override {
    if (task_started_) task_started_->store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  void DeleteAllData() override {}

 private:
  std::atomic<bool>* task_started_;
};

// Reproduces #1966: Storage ListenerInternal never initializes rest_operation_.
// Scrambling heap reuse before allocation makes uninitialized reads reliably
// crash or trigger ASan/UBSan on Linux too.
TEST(MultiAppLifecycleUnitTest, StorageListenerUnattachedDestroy) {
  std::unique_ptr<App> app(App::Create(MakeTestAppOptions()));
  ASSERT_NE(app, nullptr);
  std::unique_ptr<storage::Storage> store(
      storage::Storage::GetInstance(app.get()));
  ASSERT_NE(store, nullptr);

  // Poison heap memory of matching sizes to expose uninitialized pointers on
  // Linux where fresh allocator pages are often zeroed.
  {
    std::vector<std::unique_ptr<char[]>> poison;
    for (int i = 0; i < 64; ++i) {
      auto buf = std::make_unique<char[]>(128);
      std::memset(buf.get(), 0xA5, 128);
      poison.push_back(std::move(buf));
    }
  }

  auto listener = std::make_unique<NoOpStorageListener>();
  auto future =
      store->GetReference("probe/missing")
          .PutFile("/no/such/firebase_cpp_sdk_stress_file", listener.get());
  ASSERT_TRUE(future.Wait(5000));
  EXPECT_NE(future.error(), storage::kErrorNone);

  // Destroying a Listener whose operation failed before creating RestOperation
  // dereferences uninitialized ListenerInternal::rest_operation_ (#1966).
  listener.reset();
}

// Reproduces #1970 (App Check only registers on first App) and secondary bug
// BUG-2085 (FunctionRegistry::UnregisterFunction calls erase(itr, itr)).
TEST(MultiAppLifecycleUnitTest, AppCheckSecondAppRegistersTokenFunction) {
  FakeAppCheckProviderFactory factory;
  app_check::AppCheck::SetAppCheckProviderFactory(&factory);

  std::unique_ptr<App> app_a(App::Create(MakeTestAppOptions()));
  std::unique_ptr<App> app_b(App::Create(MakeTestAppOptions(), "second"));
  ASSERT_NE(app_a, nullptr);
  ASSERT_NE(app_b, nullptr);

  std::unique_ptr<app_check::AppCheck> ac_a(
      app_check::AppCheck::GetInstance(app_a.get()));
  std::unique_ptr<app_check::AppCheck> ac_b(
      app_check::AppCheck::GetInstance(app_b.get()));
  ASSERT_NE(ac_a, nullptr);
  ASSERT_NE(ac_b, nullptr);

  EXPECT_TRUE(app_a->function_registry()->FunctionExists(
      internal::FnAppCheckGetTokenAsync));
  // Fails on main due to process-global g_app_check_registry_count (#1970).
  EXPECT_TRUE(app_b->function_registry()->FunctionExists(
      internal::FnAppCheckGetTokenAsync));

  Future<std::string> token_a;
  EXPECT_TRUE(app_a->function_registry()->CallFunction(
      internal::FnAppCheckGetTokenAsync, app_a.get(), nullptr, &token_a));
  if (token_a.status() != kFutureStatusInvalid) {
    EXPECT_TRUE(token_a.Wait(5000));
  }

  Future<std::string> token_b;
  EXPECT_TRUE(app_b->function_registry()->CallFunction(
      internal::FnAppCheckGetTokenAsync, app_b.get(), nullptr, &token_b));
  if (token_b.status() != kFutureStatusInvalid) {
    EXPECT_TRUE(token_b.Wait(5000));
  }

  ac_b.reset();
  ac_a.reset();

  // Fails on main because FunctionRegistry::UnregisterFunction calls
  // erase(itr, itr) (BUG-2085).
  EXPECT_FALSE(app_a->function_registry()->FunctionExists(
      internal::FnAppCheckGetTokenAsync));
  EXPECT_FALSE(app_b->function_registry()->FunctionExists(
      internal::FnAppCheckGetTokenAsync));

  app_check::AppCheck::SetAppCheckProviderFactory(nullptr);
}

// Reproduces #1969: UserSecureManager::~UserSecureManager() races the shared
// static s_scheduler_ destroying the callback's SafeFutureHandle when a second
// UserSecureManager keeps s_scheduler_ alive.
TEST(MultiAppLifecycleUnitTest, TwoAppUserSecureManagerConcurrentTeardown) {
  // Keep one manager alive so DestroyScheduler() does not join s_scheduler_.
  app::secure::UserSecureManager keeper(
      std::make_unique<DelayedFakeUserSecure>(nullptr));

  for (int i = 0; i < 40; ++i) {
    std::atomic<bool> started{false};
    auto transient = std::make_unique<app::secure::UserSecureManager>(
        std::make_unique<DelayedFakeUserSecure>(&started));
    transient->DeleteUserData("second_" + std::to_string(i));

    // Wait until the worker thread enters DeleteUserData() so ClearReference()
    // blocks in the destructor and exits right before WorkerThreadRoutine
    // destroys the CallbackVariadic holding UserSecureDataHandle.
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    while (!started.load() && std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    transient.reset();
  }
}

// Repeatedly creates and tears down a second named App with multiple products
// while a default App remains alive.
TEST(MultiAppLifecycleUnitTest, TwoAppMultiProductCreateDestroyLoop) {
  std::unique_ptr<App> default_app(App::Create(MakeTestAppOptions()));
  ASSERT_NE(default_app, nullptr);
  std::unique_ptr<auth::Auth> default_auth(
      auth::Auth::GetAuth(default_app.get()));
  std::unique_ptr<app_check::AppCheck> default_ac(
      app_check::AppCheck::GetInstance(default_app.get()));

  for (int i = 0; i < 10; ++i) {
    std::string name = "ephemeral_" + std::to_string(i);
    std::unique_ptr<App> app(App::Create(MakeTestAppOptions(), name.c_str()));
    ASSERT_NE(app, nullptr);

    std::unique_ptr<auth::Auth> auth(auth::Auth::GetAuth(app.get()));
    std::unique_ptr<app_check::AppCheck> ac(
        app_check::AppCheck::GetInstance(app.get()));
    std::unique_ptr<storage::Storage> store(
        storage::Storage::GetInstance(app.get()));
    std::unique_ptr<functions::Functions> fn(
        functions::Functions::GetInstance(app.get()));
    std::unique_ptr<installations::Installations> fis(
        installations::Installations::GetInstance(app.get()));
    std::unique_ptr<remote_config::RemoteConfig> rc(
        remote_config::RemoteConfig::GetInstance(app.get()));
#if defined(FIREBASE_STRESS_WITH_FIRESTORE)
    std::unique_ptr<firestore::Firestore> fs(
        firestore::Firestore::GetInstance(app.get()));
#endif
    // Destroy in documented reverse order before app.
#if defined(FIREBASE_STRESS_WITH_FIRESTORE)
    fs.reset();
#endif
    rc.reset();
    fis.reset();
    fn.reset();
    store.reset();
    ac.reset();
    auth.reset();
    app.reset();
  }
}

}  // namespace
}  // namespace firebase
