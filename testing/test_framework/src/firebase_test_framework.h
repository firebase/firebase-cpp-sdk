// Copyright 2019 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FIREBASE_TEST_FRAMEWORK_H_  // NOLINT
#define FIREBASE_TEST_FRAMEWORK_H_  // NOLINT

#include <functional>
#include <iostream>
#include <sstream>
#include <vector>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/platform.h"
#include "firebase/util.h"
#include "firebase/variant.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

// Include this internal header so we have access to UnitTestImpl and
// ClearTestPartResults. We are not supposed to use these, but we do anyway as a
// workaround for handling flaky test sections.
#include "gtest/../../src/gtest-internal-inl.h"

namespace firebase_test_framework {

// Use this macro to skip an entire test if it is an non UI Test and we are
// not running it in UItest mode (for example, on UI Test workflow).
#define TEST_DOES_NOT_REQUIRE_USER_INTERACTION                            \
  if (!ShouldRunNonUITests()) {                                           \
    app_framework::LogInfo(                                               \
        "Skipping %s, as it is a Non UI Test.",                           \
        ::testing::UnitTest::GetInstance()->current_test_info()->name()); \
    GTEST_SKIP();                                                         \
    return;                                                               \
  }

// Use this macro to skip an entire test if it requires interactivity and we are
// not running in interactive mode (for example, on FTL).
#define TEST_REQUIRES_USER_INTERACTION                                    \
  if (!ShouldRunUITests()) {                                              \
    app_framework::LogInfo(                                               \
        "Skipping %s, as it requires user interaction.",                  \
        ::testing::UnitTest::GetInstance()->current_test_info()->name()); \
    GTEST_SKIP();                                                         \
    return;                                                               \
  }

#if TARGET_OS_IPHONE
#define TEST_REQUIRES_USER_INTERACTION_ON_IOS TEST_REQUIRES_USER_INTERACTION
#define TEST_REQUIRES_USER_INTERACTION_ON_ANDROID ((void)0)
#elif defined(ANDROID)
#define TEST_REQUIRES_USER_INTERACTION_ON_IOS ((void)0)
#define TEST_REQUIRES_USER_INTERACTION_ON_ANDROID TEST_REQUIRES_USER_INTERACTION
#else
#define TEST_REQUIRES_USER_INTERACTION_ON_IOS ((void)0)
#define TEST_REQUIRES_USER_INTERACTION_ON_ANDROID ((void)0)
#endif  // TARGET_OS_IPHONE

// Macros for skipping tests on various platforms.
//
// Simply place the macro at the top of the test to skip that test on
// the given platform.
// For example:
// TEST_F(MyFirebaseTest, TestThatFailsOnDesktop) {
//   SKIP_TEST_ON_DESKTOP;
//   EXPECT_TRUE(do_test_things(...))
// }
//
// SKIP_TEST_ON_MOBILE
// SKIP_TEST_ON_IOS
// SKIP_TEST_ON_ANDROID
// SKIP_TEST_ON_DESKTOP
// SKIP_TEST_ON_LINUX
// SKIP_TEST_ON_WINDOWS
// SKIP_TEST_ON_MACOS
// SKIP_TEST_ON_SIMULATOR / SKIP_TEST_ON_EMULATOR (identical)
// SKIP_TEST_ON_IOS_SIMULATOR / SKIP_TEST_ON_ANDROID_EMULATOR
// SKIP_TEST_ON_ANDROID_IF_GOOGLE_PLAY_SERVICES_IS_OLDER_THAN(version_code)
//
// Also includes a special macro SKIP_TEST_IF_USING_STLPORT if compiling for
// Android STLPort, which does not fully support C++11.

#if !defined(ANDROID) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define SKIP_TEST_ON_DESKTOP                                               \
  {                                                                        \
    app_framework::LogInfo("Skipping %s on desktop.", test_info_->name()); \
    GTEST_SKIP();                                                          \
    return;                                                                \
  }
#else
#define SKIP_TEST_ON_DESKTOP ((void)0)
#endif  // !defined(ANDROID) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

#if (defined(TARGET_OS_OSX) && TARGET_OS_OSX)
#define SKIP_TEST_ON_MACOS                                               \
  {                                                                      \
    app_framework::LogInfo("Skipping %s on MacOS.", test_info_->name()); \
    GTEST_SKIP();                                                        \
    return;                                                              \
  }
#else
#define SKIP_TEST_ON_MACOS ((void)0)
#endif  // (defined(TARGET_OS_OSX) && TARGET_OS_OSX)

#if defined(_WIN32)
#define SKIP_TEST_ON_WINDOWS                                               \
  {                                                                        \
    app_framework::LogInfo("Skipping %s on Windows.", test_info_->name()); \
    GTEST_SKIP();                                                          \
    return;                                                                \
  }
#else
#define SKIP_TEST_ON_WINDOWS ((void)0)
#endif  // defined(_WIN32)

#if defined(__linux__)
#define SKIP_TEST_ON_LINUX                                               \
  {                                                                      \
    app_framework::LogInfo("Skipping %s on Linux.", test_info_->name()); \
    GTEST_SKIP();                                                        \
    return;                                                              \
  }
#else
#define SKIP_TEST_ON_LINUX ((void)0)
#endif  // defined(__linux__)

#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define SKIP_TEST_ON_MOBILE                                               \
  {                                                                       \
    app_framework::LogInfo("Skipping %s on mobile.", test_info_->name()); \
    GTEST_SKIP();                                                         \
    return;                                                               \
  }
#else
#define SKIP_TEST_ON_MOBILE ((void)0)
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#define SKIP_TEST_ON_IOS                                               \
  {                                                                    \
    app_framework::LogInfo("Skipping %s on iOS.", test_info_->name()); \
    GTEST_SKIP();                                                      \
    return;                                                            \
  }
#else
#define SKIP_TEST_ON_IOS ((void)0)
#endif  // (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

#if (defined(TARGET_OS_TV) && TARGET_OS_TV)
#define SKIP_TEST_ON_TVOS                                               \
  {                                                                     \
    app_framework::LogInfo("Skipping %s on tvOS.", test_info_->name()); \
    GTEST_SKIP();                                                       \
    return;                                                             \
  }
#else
#define SKIP_TEST_ON_TVOS ((void)0)
#endif  // (defined(TARGET_OS_TV) && TARGET_OS_TV)

#if defined(ANDROID)
#define SKIP_TEST_ON_ANDROID                                               \
  {                                                                        \
    app_framework::LogInfo("Skipping %s on Android.", test_info_->name()); \
    GTEST_SKIP();                                                          \
    return;                                                                \
  }
#else
#define SKIP_TEST_ON_ANDROID ((void)0)
#endif  // defined(ANDROID)

// Android needs to determine emulator at runtime, so we can't just use #ifdef.
#define SKIP_TEST_ON_SIMULATOR                                     \
  {                                                                \
    if (IsRunningOnEmulator()) {                                   \
      app_framework::LogInfo("Skipping %s on simulator/emulator.", \
                             test_info_->name());                  \
      GTEST_SKIP();                                                \
      return;                                                      \
    }                                                              \
  }

// Accept either name, simulator or emulator.
#define SKIP_TEST_ON_EMULATOR SKIP_TEST_ON_SIMULATOR

#if defined(ANDROID)
#define SKIP_TEST_ON_ANDROID_EMULATOR SKIP_TEST_ON_EMULATOR
#define SKIP_TEST_ON_IOS_SIMULATOR ((void)0)
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define SKIP_TEST_ON_ANDROID_EMULATOR ((void)0)
#define SKIP_TEST_ON_IOS_SIMULATOR SKIP_TEST_ON_SIMULATOR
#else
#define SKIP_TEST_ON_IOS_SIMULATOR ((void)0)
#define SKIP_TEST_ON_ANDROID_EMULATOR ((void)0)
#endif

#if defined(ANDROID)
#define SKIP_TEST_ON_ANDROID_IF_GOOGLE_PLAY_SERVICES_IS_OLDER_THAN(x)     \
  {                                                                       \
    int _required_ver_ = (x);                                             \
    /* Example: 23.1.2 has version code 230102???. */                     \
    /* Allow specifying version as 230102 or as 230102000. */             \
    if (_required_ver_ < 10000000) {                                      \
      _required_ver_ *= 1000;                                             \
    }                                                                     \
    int _actual_ver_ = GetGooglePlayServicesVersion();                    \
    if (_actual_ver_ > 0 && _actual_ver_ < _required_ver_) {              \
      app_framework::LogInfo(                                             \
          "Skipping %s, as Google Play services %d is below required %d", \
          test_info_->name(), _actual_ver_, _required_ver_);              \
      GTEST_SKIP();                                                       \
      return;                                                             \
    }                                                                     \
  }
#else
#define SKIP_TEST_ON_ANDROID_IF_GOOGLE_PLAY_SERVICES_IS_OLDER_THAN(x) ((void)0)
#endif  // defined(ANDROID)

#if defined(STLPORT)
#define SKIP_TEST_IF_USING_STLPORT                                             \
  {                                                                            \
    app_framework::LogInfo("Skipping %s due to incompatibility with STLPort.", \
                           test_info_->name());                                \
    GTEST_SKIP();                                                              \
    return;                                                                    \
  }
#else
#define SKIP_TEST_IF_USING_STLPORT ((void)0)
#endif  // defined(STLPORT)

#if defined(QUICK_CHECK)
#define SKIP_TEST_ON_QUICK_CHECK                                               \
  {                                                                            \
    app_framework::LogInfo("Skipping %s on quick check.", test_info_->name()); \
    GTEST_SKIP();                                                              \
    return;                                                                    \
  }
#else
#define SKIP_TEST_ON_QUICK_CHECK ((void)0)
#endif  // defined(QUICK_CHECK)

#define KNOWN_FAILURE(explanation) \
  { FAIL() << test_info_->name() << " has a known failure: " << explanation; }

#if FIREBASE_PLATFORM_LINUX || FIREBASE_PLATFORM_OSX
#define DEATHTEST_SIGABRT "SIGABRT"
#else
#define DEATHTEST_SIGABRT ""
#endif

// Macros to surround a flaky section of your test.
// If this section fails, it will retry several times until it succeeds.
#define FLAKY_TEST_SECTION_BEGIN() RunFlakyTestSection([&]() { (void)0
#define FLAKY_TEST_SECTION_END() \
  })

class FirebaseTest : public testing::Test {
 public:
  FirebaseTest();
  ~FirebaseTest() override;

  void SetUp() override;
  void TearDown() override;

  // Check the given directory, the current directory, and the directory
  // containing the binary for google-services.json, and change to whichever
  // directory contains it.
  static void FindFirebaseConfig(const char* try_directory);

  static void SetArgs(int argc, char* argv[]) {
    argc_ = argc;
    argv_ = argv;
  }

  // Convert a Variant into a string (including all nested Variants) for
  // debugging.
  static std::string VariantToString(const firebase::Variant& variant);

  // Run an operation that returns a bool. If it fails (and the bool returns
  // false), try it again, after a short delay. Returns true once it succeeds,
  // or if it fails enough times, returns false.
  // This is designed to allow you to try a flaky set of operations multiple
  // times until it succeeds.
  //
  // Note that the callback must return a bool or a type implicitly convertable
  // to bool.
  template <class CallbackType, class ContextType>
  static bool RunFlakyBlock(CallbackType flaky_callback,
                            ContextType* context_typed, const char* name = "") {
    struct RunData {
      CallbackType callback;
      ContextType* context;
    };
    RunData run_data = {flaky_callback, context_typed};
    return RunFlakyBlockBase(
        [](void* ctx) {
          CallbackType callback = static_cast<RunData*>(ctx)->callback;
          ContextType* context = static_cast<RunData*>(ctx)->context;
          return callback(context);
        },
        static_cast<void*>(&run_data), name);
  }

  // Same as RunFlakyBlock above, but use std::function to allow captures.
  static bool RunFlakyBlock(std::function<bool()> flaky_callback,
                            const char* name = "") {
    struct RunData {
      std::function<bool()>* callback;
    };
    RunData run_data = {&flaky_callback};
    return RunFlakyBlockBase(
        [](void* ctx) {
          auto& callback = *static_cast<RunData*>(ctx)->callback;
          return callback();
        },
        static_cast<void*>(&run_data), name);
  }

 protected:
  // Set up firebase::App with default settings.
  void InitializeApp();
  // Shut down firebase::App.
  void TerminateApp();

  // Returns true if interactive tests are allowed, false if only
  // fully-automated tests should be run.
  bool AreInteractiveTestsAllowed();

  // Give the static helper methods "public" visibility so that they can be used
  // by helper functions defined outside of subclasses of `FirebaseTest`, such
  // as functions defined in anonymous namespaces.
 public:
  // Get a persistent string value that was previously set via
  // SetPersistentString. Returns true if the value was set, false if not or if
  // something went wrong.
  static bool GetPersistentString(const char* key, std::string* value_out);
  // Set a persistent string value that can be accessed the next time the test
  // loads. Specify nullptr for value to delete the key. Returns true if
  // successful, false if something went wrong.
  static bool SetPersistentString(const char* key, const char* value);

  // Return true if the app is running on simulator/emulator, false if
  // on a real device (or on desktop).
  static bool IsRunningOnEmulator();

  // If on Android and Google Play services is available, returns the
  // Google Play services version. Otherwise, returns 0.
  static int GetGooglePlayServicesVersion();

  // Returns true if the future completed as expected, fails the test and
  // returns false otherwise.
  static bool WaitForCompletion(const firebase::FutureBase& future,
                                const char* name, int expected_error = 0);

  // Just wait for completion, not caring what the result is (as long as
  // it's not Invalid). Returns true, unless Invalid.
  static bool WaitForCompletionAnyResult(const firebase::FutureBase& future,
                                         const char* name);

  // Run a flaky section of a test. If any expectations fail, it will clear
  // those failures and retry the section.
  //
  // Typically, you wouldn't use this method directly. Instead, you should use
  // it via the FLAKY_TEST_SECTION_BEGIN() and FLAKY_TEST_SECTION_END() macros
  // defined above.
  //
  // For example:
  // TEST_F(MyTestClass, MyTestCase) {
  //   /* do some non-flaky stuff here */
  //   FLAKY_TEST_SECTION_BEGIN();
  //   /* do some stuff that might need to be retried here */
  //   FLAKY_TEST_SECTION_END();
  //   /* do some more non-flaky stuff here */
  // }
  void RunFlakyTestSection(std::function<void()> flaky_test_section) {
    // Save the current state of test results.
    auto saved_test_results = SaveTestPartResults();
    RunFlakyBlock([&]() {
      RestoreTestPartResults(saved_test_results);

      flaky_test_section();

      return !HasFailure();
    });
  }

  // Run an operation that returns a Future (via a callback), retrying with
  // exponential backoff if the operation fails.
  //
  // Blocks until the operation succeeds (the Future completes, with error
  // matching expected_error) or if the final attempt is started (in which case
  // the Future returned may still be in progress). You should use
  // WaitForCompletion to await the results of this function in any case.
  //
  // For example, to add retry, you would change:
  //
  // bool success = WaitForCompletion(
  //                      auth_->DeleteUser(auth->current_user()),
  //                      "DeleteUser");
  // To this:
  //
  // bool success = WaitForCompletion(RunWithRetry(
  //                        [](Auth* auth) {
  //                            return auth->DeleteUser(auth->current_user());
  //                        }, auth_), "DeleteUser"));
  template <class CallbackType, class ContextType>
  static firebase::FutureBase RunWithRetry(CallbackType run_future_typed,
                                           ContextType* context_typed,
                                           const char* name = "",
                                           int expected_error = 0) {
    struct RunData {
      CallbackType callback;
      ContextType* context;
    };
    RunData run_data = {run_future_typed, context_typed};
    return RunWithRetryBase(
        [](void* ctx) {
          CallbackType callback = static_cast<RunData*>(ctx)->callback;
          ContextType* context = static_cast<RunData*>(ctx)->context;
          return static_cast<firebase::FutureBase>(callback(context));
        },
        static_cast<void*>(&run_data), name, expected_error);
  }

  // Same as RunWithRetry, but templated to return a Future<ResultType>
  // rather than a FutureBase, in case you want to use the result data
  // of the Future. You need to explicitly provide the template parameter,
  // e.g. RunWithRetry<int>(..) to return a Future<int>.
  template <class ResultType, class CallbackType, class ContextType>
  static firebase::Future<ResultType> RunWithRetry(
      CallbackType run_future_typed, ContextType* context_typed,
      const char* name = "", int expected_error = 0) {
    struct RunData {
      CallbackType callback;
      ContextType* context;
    };
    RunData run_data = {run_future_typed, context_typed};
    firebase::FutureBase result_base = RunWithRetryBase(
        [](void* ctx) {
          CallbackType callback = static_cast<RunData*>(ctx)->callback;
          ContextType* context = static_cast<RunData*>(ctx)->context;
          // The following line checks that CallbackType actually returns a
          // Future<ResultType>. If it returns any other type, the compiler will
          // complain about implicit conversion to Future<ResultType> here.
          firebase::Future<ResultType> future_result = callback(context);
          return static_cast<firebase::FutureBase>(future_result);
        },
        static_cast<void*>(&run_data), name, expected_error);
    // Future<T> and FutureBase are reinterpret_cast-compatible, by design.
    return *reinterpret_cast<firebase::Future<ResultType>*>(&result_base);
  }

  // Same as RunWithRetry above, but use std::function to allow captures.
  static firebase::FutureBase RunWithRetry(
      std::function<firebase::FutureBase()> run_future, const char* name = "",
      int expected_error = 0) {
    struct RunData {
      std::function<firebase::FutureBase()>* callback;
    };
    RunData run_data = {&run_future};
    return RunWithRetryBase(
        [](void* ctx) {
          auto& callback = *static_cast<RunData*>(ctx)->callback;
          return static_cast<firebase::FutureBase>(callback());
        },
        static_cast<void*>(&run_data), name, expected_error);
  }
  // Same as RunWithRetry<type>, but use std::function to allow captures.
  template <class ResultType>
  static firebase::Future<ResultType> RunWithRetry(
      std::function<firebase::Future<ResultType>()> run_future,
      const char* name = "", int expected_error = 0) {
    struct RunData {
      std::function<firebase::Future<ResultType>()>* callback;
    };
    RunData run_data = {&run_future};
    firebase::FutureBase result_base = RunWithRetryBase(
        [](void* ctx) {
          auto& callback = *static_cast<RunData*>(ctx)->callback;
          // The following line checks that CallbackType actually returns a
          // Future<ResultType>. If it returns any other type, the compiler will
          // complain about implicit conversion to Future<ResultType> here.
          firebase::Future<ResultType> future_result = callback();
          return static_cast<firebase::FutureBase>(future_result);
        },
        static_cast<void*>(&run_data), name, expected_error);
    // Future<T> and FutureBase are reinterpret_cast-compatible, by design.
    return *reinterpret_cast<firebase::Future<ResultType>*>(&result_base);
  }

  // Blocking HTTP request helper function, for testing only.
  static bool SendHttpGetRequest(
      const char* url, const std::map<std::string, std::string>& headers,
      int* response_code, std::string* response);

  // Blocking HTTP request helper function, for testing only.
  static bool SendHttpPostRequest(
      const char* url, const std::map<std::string, std::string>& headers,
      const std::string& post_body, int* response_code, std::string* response);

  // Open a URL in a browser window, for testing only.
  static bool OpenUrlInBrowser(const char* url);

  // Returns true if we run tests that require interaction, false if not.
  static bool ShouldRunUITests();

  // Returns true if we run tests that do not require interaction, false if
  // not.
  static bool ShouldRunNonUITests();

  // Encode a binary string to base64. Returns true if the encoding succeeded,
  // false if it failed.
  static bool Base64Encode(const std::string& input, std::string* output);
  // Decode a base64 string to binary. Returns true if the decoding succeeded,
  // false if it failed.
  static bool Base64Decode(const std::string& input, std::string* output);

  firebase::App* app_;
  static int argc_;
  static char** argv_;
  static bool found_config_;

 private:
  // Untyped version of RunWithRetry, with implementation.
  // This is kept private because the templated version should be used instead,
  // for type safety.
  static firebase::FutureBase RunWithRetryBase(
      firebase::FutureBase (*run_future)(void* context), void* context,
      const char* name, int expected_error);
  // Untyped version of RunFlakyBlock, with implementation.
  // This is kept private because the templated version should be used instead,
  // for type safety.
  static bool RunFlakyBlockBase(bool (*flaky_block)(void* context),
                                void* context, const char* name = "");

  std::vector<::testing::TestPartResult> SaveTestPartResults() {
    return ::testing::internal::TestResultAccessor::test_part_results(
        *::testing::internal::GetUnitTestImpl()->current_test_result());
  }

  void RestoreTestPartResults(
      const std::vector<::testing::TestPartResult>& test_part_results) {
    const std::vector<testing::TestPartResult>& existing_results =
        ::testing::internal::TestResultAccessor::test_part_results(
            *::testing::internal::GetUnitTestImpl()->current_test_result());
    // Overwrite the existing test_part_results with the previously-saved
    // version.
    const_cast<std::vector<testing::TestPartResult>&>(existing_results)
        .assign(test_part_results.begin(), test_part_results.end());
  }
};

}  // namespace firebase_test_framework

namespace firebase {
// Define an operator<< for Variant so that googletest can output its values
// nicely.
std::ostream& operator<<(std::ostream& os, const Variant& v);
}  // namespace firebase

extern "C" int common_main(int argc, char* argv[]);

#endif  // FIREBASE_TEST_FRAMEWORK_H_  // NOLINT
