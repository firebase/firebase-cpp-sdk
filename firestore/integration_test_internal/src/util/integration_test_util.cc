// Copyright 2021 Google LLC

#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "Firestore/core/src/auth/empty_credentials_provider.h"
#include "absl/memory/memory.h"
#include "app_framework.h"
#include "firebase/app.h"
#include "firebase/firestore.h"
#include "firestore/src/common/hard_assert_common.h"
#include "firestore/src/main/firestore_main.h"

namespace firebase {
namespace firestore {

using auth::EmptyCredentialsProvider;

struct TestFriend {
  static FirestoreInternal* CreateTestFirestoreInternal(App* app) {
#if !defined(__ANDROID__)
    return new FirestoreInternal(app,
                                 absl::make_unique<EmptyCredentialsProvider>());
#else
    return new FirestoreInternal(app);
#endif  // !defined(__ANDROID__)
  }
};

App* GetApp(const char* name) {
  // TODO(varconst): try to avoid using a real project ID when possible. iOS
  // unit tests achieve this by using fake options:
  // https://github.com/firebase/firebase-ios-sdk/blob/9a5afbffc17bb63b7bb7f51b9ea9a6a9e1c88a94/Firestore/core/test/firebase/firestore/testutil/app_testing.mm#L29

  if (name == nullptr || std::string{name} == kDefaultAppName) {
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
#if defined(__ANDROID__)
    return App::Create(default_app->options(), name, app_framework::GetJniEnv(),
                       app_framework::GetActivity());
#else
    return App::Create(default_app->options(), name);
#endif  // defined(__ANDROID__)
  }
}

App* GetApp() { return GetApp(nullptr); }

FirestoreInternal* CreateTestFirestoreInternal(App* app) {
  return TestFriend::CreateTestFirestoreInternal(app);
}

void InitializeFirestore(Firestore* instance) {
  Firestore::set_log_level(LogLevel::kLogLevelDebug);
}

}  // namespace firestore
}  // namespace firebase
