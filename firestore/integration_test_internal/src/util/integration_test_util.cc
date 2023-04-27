/*
 * Copyright 2021 Google LLC
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

#include <chrono>  // NOLINT(build/c++11)
#include <memory>
#include <thread>  // NOLINT(build/c++11)

#include "app_framework.h"
#include "firebase/app.h"
#include "firebase/firestore.h"
#include "firestore/src/common/hard_assert_common.h"

#if !defined(__ANDROID__)
#include "Firestore/core/src/credentials/empty_credentials_provider.h"
#include "firestore/src/main/firestore_main.h"
#else
#include "firestore/src/android/firestore_android.h"
#endif  // !defined(__ANDROID__)

namespace firebase {
namespace firestore {

struct TestFriend {
  static FirestoreInternal* CreateTestFirestoreInternal(App* app) {
#if !defined(__ANDROID__)
    return new FirestoreInternal(
        app, std::make_unique<credentials::EmptyAuthCredentialsProvider>(),
        std::make_unique<credentials::EmptyAppCheckCredentialsProvider>());
#else
    return new FirestoreInternal(app);
#endif  // !defined(__ANDROID__)
  }
};

App* GetApp(const char* name, const std::string& override_project_id) {
  // TODO(varconst): try to avoid using a real project ID when possible. iOS
  // unit tests achieve this by using fake options:
  // https://github.com/firebase/firebase-ios-sdk/blob/9a5afbffc17bb63b7bb7f51b9ea9a6a9e1c88a94/Firestore/core/test/firebase/firestore/testutil/app_testing.mm#L29

  if (name == nullptr || std::string(name) == kDefaultAppName) {
#if defined(__ANDROID__)
    return App::Create(app_framework::GetJniEnv(),
                       app_framework::GetActivity());
#else
    return App::Create();
#endif  // defined(__ANDROID__)
  } else {
    App* default_app = App::GetInstance();
    SIMPLE_HARD_ASSERT(default_app,
                       "Cannot create a named app before the default app");

    AppOptions options = default_app->options();
    if (!override_project_id.empty()) {
      options.set_project_id(override_project_id.c_str());
    }

#if defined(__ANDROID__)
    return App::Create(options, name, app_framework::GetJniEnv(),
                       app_framework::GetActivity());
#else
    return App::Create(options, name);
#endif  // defined(__ANDROID__)
  }
}

App* GetApp() { return GetApp(/*name=*/nullptr, /*project_id=*/""); }

FirestoreInternal* CreateTestFirestoreInternal(App* app) {
  return TestFriend::CreateTestFirestoreInternal(app);
}

}  // namespace firestore
}  // namespace firebase
