/*
 * Copyright 2019 Google LLC
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

#ifndef FIREBASE_APP_CLIENT_CPP_TESTS_INCLUDE_FIREBASE_APP_FOR_TESTING_H_
#define FIREBASE_APP_CLIENT_CPP_TESTS_INCLUDE_FIREBASE_APP_FOR_TESTING_H_

#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "testing/run_all_tests.h"

namespace firebase {
namespace testing {

// Populate AppOptions with mock required values for testing.
static AppOptions MockAppOptions() {
  AppOptions options;
  options.set_app_id("com.google.firebase.testing");
  options.set_api_key("not_a_real_api_key");
  options.set_project_id("not_a_real_project_id");
  return options;
}

// Create a named firebase::App with the specified options.
static App* CreateApp(const AppOptions& options, const char* name) {
  return App::Create(options, name
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
                     ,
                     // Additional parameters are required for Android.
                     firebase::testing::cppsdk::GetTestJniEnv(),
                     firebase::testing::cppsdk::GetTestActivity()
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  );
}

// Create a default firebase::App with the specified options.
static App* CreateApp(const AppOptions& options) {
  return CreateApp(options, firebase::kDefaultAppName);
}

// Create a firebase::App with mock options.
static App* CreateApp() { return CreateApp(MockAppOptions()); }

}  // namespace testing
}  // namespace firebase

#endif  // FIREBASE_APP_CLIENT_CPP_TESTS_INCLUDE_FIREBASE_APP_FOR_TESTING_H_
