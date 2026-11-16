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

// Regression test for https://github.com/firebase/firebase-cpp-sdk/issues/110:
// an Auth owned by a static object is destroyed during exit(), after main()
// has returned, and must still be able to remove itself from the SDK's
// App-to-Auth registry.
//
// The checks happen after GoogleTest has reported its result, so a regression
// shows up as the test binary crashing or aborting on exit rather than as a
// failed expectation.

#include <memory>

#include "app/src/include/firebase/app.h"
#include "auth/src/include/firebase/auth.h"
#include "gtest/gtest.h"

namespace firebase {
namespace auth {
namespace {

// Owns Apps and Auths the way an application-level singleton would. Members are
// destroyed in reverse order of declaration.
struct ExitTimeOwner {
  // Destroyed after `app_destroyed_first`, so by then it has already been
  // cleaned up by its App.
  std::unique_ptr<Auth> auth_outliving_app;
  std::unique_ptr<App> app_destroyed_first;

  std::unique_ptr<App> app;
  // Destroyed before `app`, which is the documented order.
  std::unique_ptr<Auth> auth;
};

// Constructed during static initialization of this translation unit, which the
// linker places ahead of the SDK's libraries, so it is destroyed after any
// static the SDK itself registers for destruction at exit.
ExitTimeOwner g_exit_time_owner;

AppOptions TestAppOptions() {
  AppOptions options;
  options.set_app_id("1:1234567890:android:0123456789abcdef");
  options.set_api_key("not_a_real_api_key");
  options.set_project_id("static-destruction-test");
  return options;
}

TEST(AuthStaticDestructionTest, AuthOwnedByStaticIsDestroyedAfterMain) {
  g_exit_time_owner.app.reset(App::Create(TestAppOptions()));
  ASSERT_NE(g_exit_time_owner.app, nullptr);
  InitResult init_result = kInitResultFailedMissingDependency;
  g_exit_time_owner.auth.reset(
      Auth::GetAuth(g_exit_time_owner.app.get(), &init_result));
  ASSERT_EQ(init_result, kInitResultSuccess);
  ASSERT_NE(g_exit_time_owner.auth, nullptr);

  g_exit_time_owner.app_destroyed_first.reset(
      App::Create(TestAppOptions(), "app_destroyed_first"));
  ASSERT_NE(g_exit_time_owner.app_destroyed_first, nullptr);
  init_result = kInitResultFailedMissingDependency;
  g_exit_time_owner.auth_outliving_app.reset(
      Auth::GetAuth(g_exit_time_owner.app_destroyed_first.get(), &init_result));
  ASSERT_EQ(init_result, kInitResultSuccess);
  ASSERT_NE(g_exit_time_owner.auth_outliving_app, nullptr);
}

}  // namespace
}  // namespace auth
}  // namespace firebase
