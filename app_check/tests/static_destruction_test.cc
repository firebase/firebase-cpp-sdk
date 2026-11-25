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

// A AppCheck owned by a static object is destroyed during exit(), after main()
// has returned, and must still be able to remove itself from the SDK's
// registry of AppCheck instances. See
// https://github.com/firebase/firebase-cpp-sdk/issues/110 for the Auth case.
//
// The checks happen after GoogleTest has reported its result, so a regression
// shows up as the test binary crashing or aborting on exit rather than as a
// failed expectation.

#include <memory>

#include "app/src/include/firebase/app.h"
#include "app_check/src/include/firebase/app_check.h"
#include "gtest/gtest.h"

namespace firebase {
namespace app_check {
namespace {

// Owns an App and its AppCheck the way an application-level singleton would.
// Members are destroyed in reverse order of declaration.
//
// The opposite order is not covered: an App deletes its AppCheck when it is
// destroyed, so an AppCheck must never be deleted after its App.
struct ExitTimeOwner {
  std::unique_ptr<App> app;
  // Destroyed before `app`.
  std::unique_ptr<AppCheck> app_check;
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

TEST(AppCheckStaticDestructionTest, AppCheckOwnedByStaticIsDestroyedAfterMain) {
  g_exit_time_owner.app.reset(App::Create(TestAppOptions()));
  ASSERT_NE(g_exit_time_owner.app, nullptr);
  g_exit_time_owner.app_check.reset(
      AppCheck::GetInstance(g_exit_time_owner.app.get()));
  ASSERT_NE(g_exit_time_owner.app_check, nullptr);
}

}  // namespace
}  // namespace app_check
}  // namespace firebase
