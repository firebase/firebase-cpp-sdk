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

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "app/src/callback.h"
#include "app/src/cleanup_notifier.h"
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/future.h"
#include "app/src/log.h"
#include "app/src/reference_counted_future_impl.h"
#include "app/src/util.h"
#include "gtest/gtest.h"

namespace firebase {
namespace {

// GoogleTest has already computed its exit status by the time static
// destructors run after main() returns, so post-main checks must abort directly
// on failure to fail CTest reliably.
void AbortIfFalse(bool condition, const char* description) {
  if (!condition) {
    std::fprintf(stderr, "StaticDestructionTest post-main check failed: %s\n",
                 description);
    std::fflush(stderr);
    std::abort();
  }
}

AppOptions MakeTestAppOptions() {
  AppOptions options;
  options.set_app_id("1:1234567890:android:abcdef123456");
  options.set_api_key("not_a_real_api_key");
  options.set_project_id("static-destruction-test-project");
  options.set_database_url(
      "https://static-destruction-test-project.firebaseio.com");
  options.set_storage_bucket("static-destruction-test-project.appspot.com");
  return options;
}

// Simulates a Firebase product instance registered with an App's
// CleanupNotifier so both normal teardown (subscriber before App) and
// out-of-order teardown (App before subscriber) can be exercised.
class ScopedCleanupSubscriber {
 public:
  explicit ScopedCleanupSubscriber(App* app) : app_(app) {
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
    AbortIfFalse(notifier != nullptr,
                 "CleanupNotifier::FindByOwner(app) returned nullptr");
    notifier->RegisterObject(this, &ScopedCleanupSubscriber::OnOwnerCleanedUp);
  }

  ~ScopedCleanupSubscriber() { Cleanup(); }

  bool cleaned_up() const { return cleaned_up_; }

 private:
  static void OnOwnerCleanedUp(void* obj) {
    static_cast<ScopedCleanupSubscriber*>(obj)->Cleanup();
  }

  void Cleanup() {
    if (cleaned_up_) return;
    cleaned_up_ = true;
    CleanupNotifier* notifier = CleanupNotifier::FindByOwner(app_);
    if (notifier != nullptr) {
      notifier->UnregisterObject(this);
    }
    app_ = nullptr;
  }

  App* app_;
  bool cleaned_up_ = false;
};

int g_post_main_callback_runs = 0;
bool g_exit_time_holder_destroyed = false;

void RecordPostMainCallback() { ++g_post_main_callback_runs; }

// Defined BEFORE g_exit_time_sdk_holder in this translation unit so C++
// constructs it first and destroys it AFTER ~ExitTimeSdkHolder() runs during
// process exit ([basic.start.term]). Verifies that all NoDestructor-backed SDK
// registries, mutexes, callbacks, futures, and loggers remain usable even for
// the final static destructor in the binary.
struct FinalPostMainVerifier {
  bool armed = false;

  ~FinalPostMainVerifier() {
    if (!armed) return;

    AbortIfFalse(g_exit_time_holder_destroyed,
                 "ExitTimeSdkHolder must be destroyed before "
                 "FinalPostMainVerifier");
    AbortIfFalse(App::GetApps().empty(),
                 "All Apps should be unregistered after ExitTimeSdkHolder "
                 "teardown");
    AbortIfFalse(App::GetInstance() == nullptr,
                 "Default App should be nullptr after ExitTimeSdkHolder "
                 "teardown");
    AbortIfFalse(
        CleanupNotifier::FindByOwner(reinterpret_cast<void*>(0x1)) == nullptr,
        "CleanupNotifier registry lookup failed during late exit");
    AbortIfFalse(ReferenceCountedFutureImpl::InvalidHandle().id() ==
                     kInvalidFutureHandle,
                 "ReferenceCountedFutureImpl::InvalidHandle() corrupted");
    AbortIfFalse(SafeFutureHandle<int>::InvalidHandle().get().id() ==
                     ReferenceCountedFutureImpl::InvalidHandle().id(),
                 "SafeFutureHandle<T>::InvalidHandle() mismatch");

    const int before = g_post_main_callback_runs;
    callback::AddCallback(new callback::CallbackVoid(&RecordPostMainCallback));
    callback::PollCallbacks();
    AbortIfFalse(g_post_main_callback_runs == before + 1,
                 "callback::PollCallbacks() did not run queued callback during "
                 "process exit");

    LogInfo("StaticDestructionTest: post-main static teardown verified.");
  }
};

// Defined AFTER g_final_post_main_verifier so its destructor runs during real
// __cxa_atexit / atexit processing after main() returns, tearing down live App
// instances, CleanupNotifier subscribers (in both orders), and Future backing
// state.
struct ExitTimeSdkHolder {
  bool armed = false;
  std::unique_ptr<App> default_app;
  std::unique_ptr<ScopedCleanupSubscriber> subscriber_deleted_before_app;
  std::unique_ptr<App> secondary_app;
  std::unique_ptr<ScopedCleanupSubscriber> subscriber_outliving_app;
  std::unique_ptr<ReferenceCountedFutureImpl> future_impl;
  Future<int> exit_future;
  SafeFutureHandle<int> exit_future_handle;

  ~ExitTimeSdkHolder() {
    if (!armed) return;

    // 1. Complete and release a live Future during static destruction.
    AbortIfFalse(future_impl != nullptr, "future_impl must be initialized");
    future_impl->CompleteWithResult(exit_future_handle, 0, "", 77);
    AbortIfFalse(exit_future.status() == kFutureStatusComplete,
                 "exit_future did not complete during static destruction");
    AbortIfFalse(exit_future.result() != nullptr && *exit_future.result() == 77,
                 "exit_future result mismatch during static destruction");
    exit_future.Release();
    future_impl.reset();

    // 2. Normal destruction order during exit(): subscriber before its App.
    subscriber_deleted_before_app.reset();
    default_app.reset();

    // 3. Reverse destruction order during exit(): App destroyed while its
    // dependent subscriber still exists (the exact pattern triggered when a
    // user's static App is destroyed before a static product wrapper).
    secondary_app.reset();
    AbortIfFalse(subscriber_outliving_app != nullptr &&
                     subscriber_outliving_app->cleaned_up(),
                 "CleanupNotifier failed to clean up subscriber when App was "
                 "destroyed during process exit");
    subscriber_outliving_app.reset();

    g_exit_time_holder_destroyed = true;
  }
};

FinalPostMainVerifier g_final_post_main_verifier;
ExitTimeSdkHolder g_exit_time_sdk_holder;

int g_in_test_callback_runs = 0;
void RecordInTestCallback() { ++g_in_test_callback_runs; }

TEST(StaticDestructionTest, OutOfOrderCleanupAndRepeatedAppReinitialization) {
  const AppOptions options = MakeTestAppOptions();

  // Exercise repeated create -> destroy -> recreate cycles in a single process
  // (for example, plugin/domain reloads without restarting the host process).
  for (int cycle = 0; cycle < 3; ++cycle) {
    std::unique_ptr<App> default_app(App::Create(options));
    ASSERT_NE(default_app, nullptr);
    EXPECT_EQ(App::GetInstance(), default_app.get());

    const std::string secondary_name =
        "cycle_secondary_" + std::to_string(cycle);
    std::unique_ptr<App> secondary_app(
        App::Create(options, secondary_name.c_str()));
    ASSERT_NE(secondary_app, nullptr);
    EXPECT_EQ(App::GetInstance(secondary_name.c_str()), secondary_app.get());
    EXPECT_EQ(App::GetApps().size(), 2u);

    auto subscriber_first =
        std::make_unique<ScopedCleanupSubscriber>(default_app.get());
    auto subscriber_late =
        std::make_unique<ScopedCleanupSubscriber>(secondary_app.get());

    const int callbacks_before = g_in_test_callback_runs;
    callback::AddCallback(new callback::CallbackVoid(&RecordInTestCallback));
    callback::PollCallbacks();
    EXPECT_EQ(g_in_test_callback_runs, callbacks_before + 1);

    // Normal order: subscriber destroyed before App.
    subscriber_first.reset();
    default_app.reset();
    EXPECT_EQ(App::GetInstance(), nullptr);

    // Out-of-order: App destroyed before subscriber.
    void* destroyed_app_address = secondary_app.get();
    secondary_app.reset();
    EXPECT_TRUE(subscriber_late->cleaned_up());
    EXPECT_EQ(CleanupNotifier::FindByOwner(destroyed_app_address), nullptr);
    subscriber_late.reset();

    EXPECT_TRUE(App::GetApps().empty());
  }
}

TEST(StaticDestructionTest, SurvivesPostMainStaticDestruction) {
  const AppOptions options = MakeTestAppOptions();

  g_exit_time_sdk_holder.default_app.reset(App::Create(options));
  ASSERT_NE(g_exit_time_sdk_holder.default_app, nullptr);
  g_exit_time_sdk_holder.subscriber_deleted_before_app =
      std::make_unique<ScopedCleanupSubscriber>(
          g_exit_time_sdk_holder.default_app.get());

  g_exit_time_sdk_holder.secondary_app.reset(
      App::Create(options, "post_main_secondary_app"));
  ASSERT_NE(g_exit_time_sdk_holder.secondary_app, nullptr);
  g_exit_time_sdk_holder.subscriber_outliving_app =
      std::make_unique<ScopedCleanupSubscriber>(
          g_exit_time_sdk_holder.secondary_app.get());

  g_exit_time_sdk_holder.future_impl =
      std::make_unique<ReferenceCountedFutureImpl>(1);
  g_exit_time_sdk_holder.exit_future_handle =
      g_exit_time_sdk_holder.future_impl->SafeAlloc<int>(0);
  g_exit_time_sdk_holder.exit_future =
      Future<int>(g_exit_time_sdk_holder.future_impl.get(),
                  g_exit_time_sdk_holder.exit_future_handle.get());
  EXPECT_EQ(g_exit_time_sdk_holder.exit_future.status(), kFutureStatusPending);

  g_exit_time_sdk_holder.armed = true;
  g_final_post_main_verifier.armed = true;
}

}  // namespace
}  // namespace firebase
