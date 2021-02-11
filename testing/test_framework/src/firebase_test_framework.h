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

#include <iostream>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/future.h"
#include "firebase/internal/platform.h"
#include "firebase/util.h"
#include "firebase/variant.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace firebase_test_framework {

// Use this macro to skip an entire test if it requires interactivity and we are
// not running in interactive mode (for example, on FTL).
#define TEST_REQUIRES_USER_INTERACTION                                      \
  if (!IsUserInteractionAllowed()) {                                        \
    app_framework::LogInfo("Skipping %s, as it requires user interaction.", \
                           test_info_->name());                             \
    GTEST_SKIP();                                                           \
    return;                                                                 \
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

#define KNOWN_FAILURE(explanation) \
  { FAIL() << test_info_->name() << " has a known failure: " << explanation; }

#if FIREBASE_PLATFORM_LINUX || FIREBASE_PLATFORM_OSX
#define DEATHTEST_SIGABRT "SIGABRT"
#else
#define DEATHTEST_SIGABRT ""
#endif

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

 protected:
  // Set up firebase::App with default settings.
  void InitializeApp();
  // Shut down firebase::App.
  void TerminateApp();

  // Returns true if interactive tests are allowed, false if only
  // fully-automated tests should be run.
  bool AreInteractiveTestsAllowed();

  // Get a persistent string value that was previously set via
  // SetPersistentString. Returns true if the value was set, false if not or if
  // something went wrong.
  static bool GetPersistentString(const char* key, std::string* value_out);
  // Set a persistent string value that can be accessed the next time the test
  // loads. Specify nullptr for value to delete the key. Returns true if
  // successful, false if something went wrong.
  static bool SetPersistentString(const char* key, const char* value);

  // Returns true if the future completed as expected, fails the test and
  // returns false otherwise.
  static bool WaitForCompletion(const firebase::FutureBase& future,
                                const char* name, int expected_error = 0);

  // Run the future (via a callback), retrying with exponential backoff if it
  // fails.
  static firebase::FutureBase RunWithRetry(
      firebase::FutureBase (*run_future)(void* context),
      void* context,
      const char* name,
      int expected_error = 0);

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

  // Returns true if we can run tests that require interaction, false if not.
  static bool IsUserInteractionAllowed();

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
};

}  // namespace firebase_test_framework

namespace firebase {
// Define an operator<< for Variant so that googletest can output its values
// nicely.
std::ostream& operator<<(std::ostream& os, const Variant& v);
}  // namespace firebase

extern "C" int common_main(int argc, char* argv[]);

#endif  // FIREBASE_TEST_FRAMEWORK_H_  // NOLINT
