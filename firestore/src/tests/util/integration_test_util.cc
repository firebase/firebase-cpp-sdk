#include <chrono>  // NOLINT(build/c++11)
#include <thread>  // NOLINT(build/c++11)

#include "devtools/build/runtime/get_runfiles_dir.h"
#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore.h"
#include "firestore/src/ios/firestore_ios.h"
#include "firestore/src/ios/hard_assert_ios.h"
#include "absl/memory/memory.h"
#include "Firestore/core/src/auth/empty_credentials_provider.h"

namespace firebase {
namespace firestore {

using auth::EmptyCredentialsProvider;

struct TestFriend {
  static FirestoreInternal* CreateTestFirestoreInternal(App* app) {
    return new FirestoreInternal(app,
                                 absl::make_unique<EmptyCredentialsProvider>());
  }
};

App* GetApp(const char* name) {
  // TODO(varconst): try to avoid using a real project ID when possible. iOS
  // unit tests achieve this by using fake options:
  // https://github.com/firebase/firebase-ios-sdk/blob/9a5afbffc17bb63b7bb7f51b9ea9a6a9e1c88a94/Firestore/core/test/firebase/firestore/testutil/app_testing.mm#L29

  // Note: setting the default config path doesn't affect anything on iOS.
  // This is done unconditionally to simplify the logic.
  std::string google_json_dir = devtools_build::testonly::GetTestSrcdir() +
                                "/google3/firebase/firestore/client/cpp/";
  App::SetDefaultConfigPath(google_json_dir.c_str());

  if (name == nullptr || std::string{name} == kDefaultAppName) {
    return App::Create();
  } else {
    App* default_app = App::GetInstance();
    HARD_ASSERT_IOS(default_app,
                    "Cannot create a named app before the default app");
    return App::Create(default_app->options(), name);
  }
}

App* GetApp() { return GetApp(nullptr); }

// TODO(varconst): it's brittle and potentially flaky, look into using some
// notification mechanism instead.
bool ProcessEvents(int millis) {
  std::this_thread::sleep_for(std::chrono::milliseconds(millis));
  // `false` means "don't shut down the application".
  return false;
}

FirestoreInternal* CreateTestFirestoreInternal(App* app) {
  return TestFriend::CreateTestFirestoreInternal(app);
}

#ifndef __APPLE__
void InitializeFirestore(Firestore* instance) {
  Firestore::set_log_level(LogLevel::kLogLevelDebug);
}
#endif  // #ifndef __APPLE__

}  // namespace firestore
}  // namespace firebase
