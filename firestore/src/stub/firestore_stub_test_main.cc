#include "devtools/build/runtime/get_runfiles_dir.h"
#include "app/src/include/firebase/app.h"
#include "firestore/src/include/firebase/firestore.h"
#include "testing/base/public/gmock.h"
#include "gtest/gtest.h"

#ifdef _WIN32
#include <windows.h>
#else  // _WIN32
#include <unistd.h>
#endif  // _WIN32

// The logic of GetApp() and ProcessEvents() is from
// firebase/firestore/client/cpp/testapp/src/desktop/desktop_main.cc.
namespace firebase {
namespace firestore {
App* GetApp(const char* name) {
  const std::string google_json_dir =
      devtools_build::testonly::GetTestSrcdir() +
      "/google3/firebase/firestore/client/cpp/";
  App::SetDefaultConfigPath(google_json_dir.c_str());
  if (name == nullptr) {
    return App::Create();
  } else {
    return App::Create(AppOptions{}, name);
  }
}

App* GetApp() { return GetApp(nullptr); }

// For desktop stub, we just let it sleep for the specified time and return
// false, which means the app does not receive an event requesting exit.
bool ProcessEvents(int msec) {
#ifdef _WIN32
  ::Sleep(msec);
#else
  usleep(msec * 1000);
#endif  // _WIN32
  // Simply return false. Reviewers prefer not registering signal and handling
  // exit request for simplicity. Presumbly, the default signal processing
  // already deals with the exit correctly and thus we do not need to deal with
  // it.
  return false;
}

FirestoreInternal* CreateTestFirestoreInternal(App* app) {
  return new FirestoreInternal(app);
}

void InitializeFirestore(Firestore*) {
  // No extra initialization necessary
}

}  // namespace firestore
}  // namespace firebase

// MS C++ compiler/linker has a bug on Windows (not on Windows CE), which
// causes a link error when _tmain is defined in a static library and UNICODE
// is enabled. For this reason instead of _tmain, main function is used on
// Windows. See the following link to track the current status of this bug:
// http://connect.microsoft.com/VisualStudio/feedback/ViewFeedback.aspx?FeedbackID=394464  // NOLINT
#if GTEST_OS_WINDOWS_MOBILE
# include <tchar.h>  // NOLINT

GTEST_API_ int _tmain(int argc, TCHAR** argv) {
#else
GTEST_API_ int main(int argc, char** argv) {
#endif  // GTEST_OS_WINDOWS_MOBILE
  std::cout << "Running main() from gmock_main.cc\n";
  // Since Google Mock depends on Google Test, InitGoogleMock() is
  // also responsible for initializing Google Test.  Therefore there's
  // no need for calling testing::InitGoogleTest() separately.
  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
