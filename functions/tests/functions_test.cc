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

#include "functions/src/include/firebase/functions.h"

#include <memory>
#include <utility>

#include "app/src/app_state.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace functions {
namespace {

TEST(FunctionsLifecycleTest, InvalidAppReturnsInvalidFunctions) {
  InitResult init_result = kInitResultSuccess;
  Functions invalid = Functions::GetInstance(App(), &init_result);
  EXPECT_FALSE(invalid.is_valid());
  EXPECT_EQ(kInitResultFailedMissingDependency, init_result);
  EXPECT_FALSE(invalid.app().is_valid());
  EXPECT_FALSE(invalid.GetHttpsCallable("fn").is_valid());
}

TEST(FunctionsLifecycleTest, PerRegionAndPerAppInstances) {
  App app1 = testing::CreateApp(testing::MockAppOptions(), "fn_app_1");
  App app2 = testing::CreateApp(testing::MockAppOptions(), "fn_app_2");

  Functions f1_default = Functions::GetInstance(app1);
  Functions f1_default_explicit = Functions::GetInstance(app1, "us-central1");
  Functions f1_eu = Functions::GetInstance(app1, "europe-west1");
  Functions f2_default = Functions::GetInstance(app2);

  ASSERT_TRUE(f1_default.is_valid());
  EXPECT_EQ(f1_default, f1_default_explicit);
  EXPECT_NE(f1_default, f1_eu);
  EXPECT_NE(f1_default, f2_default);
  EXPECT_EQ(f1_default.app(), app1);
  EXPECT_EQ(f2_default.app(), app2);
}

TEST(FunctionsLifecycleTest, CallableReferenceKeepsAppStateAlive) {
  HttpsCallableReference callable;
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  {
    App app = testing::CreateApp(testing::MockAppOptions(), "fn_keepalive");
    weak_state = ::firebase::internal::AppState::FromApp(app);
    Functions f = Functions::GetInstance(app);
    ASSERT_TRUE(f.is_valid());
    callable = f.GetHttpsCallable("echo");
    ASSERT_TRUE(callable.is_valid());
  }
  EXPECT_FALSE(weak_state.expired());
  EXPECT_TRUE(callable.is_valid());
  Functions f_back = callable.functions();
  EXPECT_TRUE(f_back.is_valid());
  EXPECT_STREQ("fn_keepalive", f_back.app().name());

  f_back = Functions();
  callable = HttpsCallableReference();
  EXPECT_TRUE(weak_state.expired());
}

TEST(FunctionsLifecycleTest, StaticDurationFunctionsSurvivesExit) {
  static Functions static_fn = []() {
    App app = testing::CreateApp(testing::MockAppOptions(), "fn_static_exit");
    return Functions::GetInstance(app);
  }();
  EXPECT_TRUE(static_fn.is_valid());
}

TEST(FunctionsLifecycleTest, UniformHandleOperatorsAndMovedFromSafety) {
  App app = testing::CreateApp(testing::MockAppOptions(), "fn_ops_app");
  Functions f1 = Functions::GetInstance(app);
  Functions f2 = f1;
  EXPECT_TRUE(static_cast<bool>(f1));
  EXPECT_EQ(f1, f2);
  EXPECT_FALSE(f1 != f2);

  Functions f_moved(std::move(f2));
  EXPECT_FALSE(static_cast<bool>(f2));  // NOLINT
  EXPECT_FALSE(f2.is_valid());          // NOLINT
  EXPECT_NE(f_moved, f2);               // NOLINT

  HttpsCallableReference c1 = f1.GetHttpsCallable("echo");
  HttpsCallableReference c2 = f1.GetHttpsCallable("echo");
  HttpsCallableReference c3 = f1.GetHttpsCallable("other");
  EXPECT_TRUE(static_cast<bool>(c1));
  EXPECT_EQ(c1, c2);
  EXPECT_NE(c1, c3);

  HttpsCallableReference c_moved(std::move(c2));
  EXPECT_FALSE(static_cast<bool>(c2));  // NOLINT
  EXPECT_FALSE(c2.is_valid());          // NOLINT
  EXPECT_NE(c_moved, c2);               // NOLINT
}

TEST(FunctionsLifecycleTest, TemporaryCallableAndConcurrentCallsTeardownSafe) {
  std::weak_ptr<::firebase::internal::AppState> weak_state;
  Future<HttpsCallableResult> temp_future;
  Future<HttpsCallableResult> concurrent_future_1;
  Future<HttpsCallableResult> concurrent_future_2;
  {
    App app =
        testing::CreateApp(testing::MockAppOptions(), "fn_concurrent_teardown");
    weak_state = ::firebase::internal::AppState::FromApp(app);
    Functions f = Functions::GetInstance(app);
    ASSERT_TRUE(f.is_valid());
    f.UseFunctionsEmulator("http://127.0.0.1:1");

    // 1) Launch a call on a temporary HttpsCallableReference that is destroyed
    // immediately after Call() returns.
    temp_future = f.GetHttpsCallable("tempFn").Call(Variant("payload"));
    EXPECT_NE(kFutureStatusInvalid, temp_future.status());

    // 2) Launch two concurrent calls on the same HttpsCallableReference, then
    // immediately drop all App/Functions/Callable handles to verify safe
    // cancellation and drain in ~FunctionsInternal().
    HttpsCallableReference shared_ref = f.GetHttpsCallable("sharedFn");
    concurrent_future_1 = shared_ref.Call(Variant(1));
    concurrent_future_2 = shared_ref.Call(Variant(2));
    EXPECT_NE(kFutureStatusInvalid, concurrent_future_1.status());
    EXPECT_NE(kFutureStatusInvalid, concurrent_future_2.status());
  }

  EXPECT_TRUE(weak_state.expired());
  temp_future.Wait(FutureBase::kWaitTimeoutInfinite);
  concurrent_future_1.Wait(FutureBase::kWaitTimeoutInfinite);
  concurrent_future_2.Wait(FutureBase::kWaitTimeoutInfinite);
  EXPECT_NE(kFutureStatusPending, temp_future.status());
  EXPECT_NE(kFutureStatusPending, concurrent_future_1.status());
  EXPECT_NE(kFutureStatusPending, concurrent_future_2.status());
}

}  // namespace
}  // namespace functions
}  // namespace firebase
