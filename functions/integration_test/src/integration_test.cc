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

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/app_check.h"
#include "firebase/app_check/debug_provider.h"
#include "firebase/auth.h"
#include "firebase/functions.h"
#include "firebase/util.h"
#include "firebase/variant.h"
#include "firebase_test_framework.h"  // NOLINT

#if !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
#include "functions/src/desktop/functions_desktop.h"
#endif  // !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

// The TO_STRING macro is useful for command line defined strings as the quotes
// get stripped.
#define TO_STRING_EXPAND(X) #X
#define TO_STRING(X) TO_STRING_EXPAND(X)

// Path to the Firebase config file to load.
#ifdef FIREBASE_CONFIG
#define FIREBASE_CONFIG_STRING TO_STRING(FIREBASE_CONFIG)
#else
#define FIREBASE_CONFIG_STRING ""
#endif  // FIREBASE_CONFIG

namespace firebase_testapp_automated {

using app_framework::LogDebug;

using firebase::App;
using firebase::Future;
using firebase::Variant;
using firebase::auth::Auth;
using firebase::functions::Functions;
using firebase::functions::HttpsCallableOptions;
using firebase::functions::HttpsCallableReference;
using firebase::functions::HttpsCallableResult;
using firebase_test_framework::FirebaseTest;

// Your Firebase project's Debug token goes here.
// You can get this from Firebase Console, in the App Check settings.
const char kAppCheckDebugToken[] = "REPLACE_WITH_APP_CHECK_TOKEN";

class FirebaseFunctionsTest : public FirebaseTest {
 public:
  FirebaseFunctionsTest();
  ~FirebaseFunctionsTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void InitializeFunctions(const char* region = nullptr);
  void TerminateFunctions();

  void InitializeAuth();
  void TerminateAuth();

  Functions* functions_ = nullptr;
  Auth* auth_ = nullptr;
  std::string emulator_host_;
};

FirebaseFunctionsTest::FirebaseFunctionsTest() {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

  const int argc = app_framework::GetArgc();
  const char** argv = app_framework::GetArgv();
  const char kFlagPrefix[] = "--functions_emulator_host=";
  for (int i = 1; i < argc; ++i) {
    if (argv[i] &&
        strncmp(argv[i], kFlagPrefix, sizeof(kFlagPrefix) - 1) == 0) {
      emulator_host_ = argv[i] + sizeof(kFlagPrefix) - 1;
      break;
    }
  }
  if (emulator_host_.empty()) {
    const char* env_host = std::getenv("FUNCTIONS_EMULATOR_HOST");
    if (env_host != nullptr && env_host[0] != '\0') {
      emulator_host_ = env_host;
    }
  }
#if defined(__ANDROID__)
  auto replace_host = [this](const std::string& from) {
    if (emulator_host_.compare(0, from.size(), from) == 0 &&
        (emulator_host_.size() == from.size() ||
         emulator_host_[from.size()] == ':')) {
      emulator_host_.replace(0, from.size(), "10.0.2.2");
    }
  };
  replace_host("localhost");
  replace_host("127.0.0.1");
  replace_host("http://localhost");
  replace_host("http://127.0.0.1");
#endif  // defined(__ANDROID__)
}

FirebaseFunctionsTest::~FirebaseFunctionsTest() {
  assert(functions_ == nullptr);
  assert(app_ == nullptr);
  assert(auth_ == nullptr);
}

void FirebaseFunctionsTest::SetUp() {
  FirebaseTest::SetUp();
  firebase::app_check::DebugAppCheckProviderFactory::GetInstance()
      ->SetDebugToken(kAppCheckDebugToken);
  firebase::app_check::AppCheck::SetAppCheckProviderFactory(
      firebase::app_check::DebugAppCheckProviderFactory::GetInstance());
  InitializeApp();
  if (app_) {
    firebase::app_check::AppCheck::GetInstance(app_);
  }
  InitializeFunctions();
}

void FirebaseFunctionsTest::TearDown() {
  TerminateFunctions();
  TerminateAuth();
  if (app_) {
    delete firebase::app_check::AppCheck::GetInstance(app_);
  }
  TerminateApp();
  firebase::app_check::AppCheck::SetAppCheckProviderFactory(nullptr);
  FirebaseTest::TearDown();
}

void FirebaseFunctionsTest::InitializeFunctions(const char* region) {
  LogDebug("Initializing Functions.");

  ::firebase::ModuleInitializer initializer;
  void* ptr = &functions_;
  initializer.Initialize(app_, ptr,
                         [region](::firebase::App* app, void* target) {
                           LogDebug("Try to initialize Firebase Functions");
                           Functions** functions_ptr =
                               reinterpret_cast<Functions**>(target);
                           firebase::InitResult result;
                           if (region) {
                             *functions_ptr =
                                 Functions::GetInstance(app, region, &result);
                           } else {
                             *functions_ptr =
                                 Functions::GetInstance(app, &result);
                           }
                           return result;
                         });

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Functions.");

  if (!emulator_host_.empty() && functions_ != nullptr) {
    LogDebug("Using Functions emulator at %s", emulator_host_.c_str());
    functions_->UseFunctionsEmulator(emulator_host_.c_str());
  }
}

void FirebaseFunctionsTest::TerminateFunctions() {
  if (functions_) {
    LogDebug("Shutdown the Functions library.");
    delete functions_;
    functions_ = nullptr;
  }
}

void FirebaseFunctionsTest::InitializeAuth() {
  LogDebug("Initializing Firebase Auth.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, &auth_, [](::firebase::App* app, void* target) {
    LogDebug("Attempting to initialize Firebase Auth.");
    firebase::InitResult result;
    *reinterpret_cast<Auth**>(target) = Auth::GetAuth(app, &result);
    return result;
  });

  WaitForCompletion(initializer.InitializeLastResult(), "InitializeAuth");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Starting Auth.");
}

void FirebaseFunctionsTest::TerminateAuth() {
  if (auth_) {
    LogDebug("Signing out.");
    auth_->SignOut();
    LogDebug("Shutdown the Auth library.");
    delete auth_;
    auth_ = nullptr;
  }
}

// Test cases below.

TEST_F(FirebaseFunctionsTest, TestInitializeAndTerminate) {
  // Tested via SetUp() and TearDown().
}

#if !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
TEST_F(FirebaseFunctionsTest, TestGetUrl) {
  firebase::AppOptions options;
  options.set_project_id("my-project-id");
  options.set_app_id("1:1234567890:android:abcdef");
  options.set_api_key("fake-api-key");
  App* custom_app = App::Create(options, "url_test_app");
  ASSERT_NE(custom_app, nullptr);

  firebase::functions::internal::FunctionsInternal internal_default(
      custom_app, "us-central1");
  EXPECT_EQ(internal_default.GetUrl("myendpoint"),
            "https://us-central1-my-project-id.cloudfunctions.net/myendpoint");

  firebase::functions::internal::FunctionsInternal internal_region(
      custom_app, "us-west1");
  EXPECT_EQ(internal_region.GetUrl("myendpoint"),
            "https://us-west1-my-project-id.cloudfunctions.net/myendpoint");

  internal_region.UseFunctionsEmulator("http://localhost:5005");
  EXPECT_EQ(internal_region.GetUrl("myendpoint"),
            "http://localhost:5005/my-project-id/us-west1/myendpoint");

  delete custom_app;
}
#endif  // !defined(__ANDROID__) && !(defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

TEST_F(FirebaseFunctionsTest, TestData) {
  Variant input = Variant::EmptyMap();
  input.map()["bool"] = true;
  input.map()["int"] = 2;
  input.map()["long"] = 3LL;
  input.map()["string"] = "four";
  input.map()["array"] = std::vector<Variant>{5, 6};
  input.map()["null"] = Variant::Null();

  Variant expected = input;
  expected.map()["message"] = "stub response";
  expected.map()["code"] = 42;
  expected.map()["long"] = 420LL;

  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("dataTest");
  Future<HttpsCallableResult> future = ref.Call(input);
  WaitForCompletion(future, "Call dataTest");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), expected);
}

TEST_F(FirebaseFunctionsTest, TestDataWithOptions) {
  Variant input = Variant::EmptyMap();
  input.map()["bool"] = true;
  input.map()["int"] = 2;
  input.map()["long"] = 3LL;
  input.map()["string"] = "four";
  input.map()["array"] = std::vector<Variant>{5, 6};
  input.map()["null"] = Variant::Null();

  Variant expected = input;
  expected.map()["message"] = "stub response";
  expected.map()["code"] = 42;
  expected.map()["long"] = 420LL;

  HttpsCallableReference ref;
  HttpsCallableOptions options(true);
  ref = functions_->GetHttpsCallableWithOptions("dataTest", options);
  Future<HttpsCallableResult> future = ref.Call(input);
  WaitForCompletion(future, "Call dataTest");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), expected);
}

TEST_F(FirebaseFunctionsTest, TestScalar) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("scalarTest");
  Future<HttpsCallableResult> future = ref.Call(17);
  WaitForCompletion(future, "Call scalarTest");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), Variant(76));
}

TEST_F(FirebaseFunctionsTest, TestScalarWithOptions) {
  HttpsCallableReference ref;
  HttpsCallableOptions options(false);
  ref = functions_->GetHttpsCallableWithOptions("scalarTest", options);
  Future<HttpsCallableResult> future = ref.Call(17);
  WaitForCompletion(future, "Call scalarTest");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), Variant(76));
}

TEST_F(FirebaseFunctionsTest, TestNull) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("nullTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call nullTest (no args)");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), Variant::Null());

  // Test the version that takes a null argument explicitly.
  future = ref.Call(Variant::Null());
  WaitForCompletion(future, "Call nullTest (with null)");
  EXPECT_EQ(future.error(), firebase::functions::kErrorNone);
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), Variant::Null());
}

TEST_F(FirebaseFunctionsTest, TestMissingResult) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("missingResultTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call missingResultTest",
                    firebase::functions::kErrorInternal);
  EXPECT_STREQ(future.error_message(), "Response is missing data field.");
}

TEST_F(FirebaseFunctionsTest, TestUnhandledError) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("unhandledErrorTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call unhandledErrorTest",
                    firebase::functions::kErrorInternal);
  EXPECT_STREQ(future.error_message(), "INTERNAL");
}

TEST_F(FirebaseFunctionsTest, TestUnknownError) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("unknownErrorTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call unknownErrorTest",
                    firebase::functions::kErrorInternal);
  EXPECT_STREQ(future.error_message(), "INTERNAL");
}

TEST_F(FirebaseFunctionsTest, TestExplicitError) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("explicitErrorTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call explicitErrorTest",
                    firebase::functions::kErrorOutOfRange);
  EXPECT_STREQ(future.error_message(), "explicit nope");
}

TEST_F(FirebaseFunctionsTest, TestHttpError) {
  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("httpErrorTest");
  Future<HttpsCallableResult> future = ref.Call();
  WaitForCompletion(future, "Call httpErrorTest",
                    firebase::functions::kErrorInvalidArgument);
}

TEST_F(FirebaseFunctionsTest, TestTimeout) {
  HttpsCallableOptions options(/*limited_use_app_check_tokens=*/false,
                               /*timeout_ms=*/50);
  HttpsCallableReference ref =
      functions_->GetHttpsCallableWithOptions("timeoutTest", options);
  Future<HttpsCallableResult> future = ref.Call();
  while (future.status() == firebase::kFutureStatusPending) {
    app_framework::ProcessEvents(10);
  }
  EXPECT_EQ(future.status(), firebase::kFutureStatusComplete);
  EXPECT_TRUE(future.error() == firebase::functions::kErrorDeadlineExceeded ||
              future.error() == firebase::functions::kErrorUnavailable);
}

// Disabling TestToken as the integration test backend is no longer valid, needs
// to be migrated to an emulator test.
TEST_F(FirebaseFunctionsTest, DISABLED_TestToken) {
  InitializeAuth();
  WaitForCompletion(auth_->SignInAnonymously(), "SignInAnonymously");

  HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable("tokenTest");
  Future<HttpsCallableResult> future = ref.Call(Variant::EmptyMap());
  WaitForCompletion(future, "Call tokenTest");
  ASSERT_NE(future.result(), nullptr);
  EXPECT_EQ(future.result()->data(), Variant::EmptyMap());

  TerminateAuth();
}

}  // namespace firebase_testapp_automated
