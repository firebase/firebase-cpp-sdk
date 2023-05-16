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

#include <inttypes.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/auth.h"
#include "firebase/functions.h"
#include "firebase/util.h"
#include "firebase/variant.h"
#include "firebase_test_framework.h"  // NOLINT

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

using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

const char kIntegrationTestRootPath[] = "integration_test_data";

class FirebaseFunctionsTest : public FirebaseTest {
 public:
  FirebaseFunctionsTest();
  ~FirebaseFunctionsTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // Initialize Firebase App, Firebase Auth, and Firebase Functions.
  void Initialize();
  // Shut down Firebase Functions, Firebase Auth, and Firebase App.
  void Terminate();
  // Sign in an anonymous user.
  void SignIn();

  firebase::Future<firebase::functions::HttpsCallableResult> TestFunctionHelper(
      const char* function_name,
      firebase::functions::HttpsCallableReference& ref,
      const firebase::Variant* const function_data,
      const firebase::Variant& expected_result,
      firebase::functions::Error expected_error =
          firebase::functions::kErrorNone);

  firebase::Future<firebase::functions::HttpsCallableResult> TestFunction(
      const char* function_name, const firebase::Variant* const function_data,
      const firebase::Variant& expected_result,
      firebase::functions::Error expected_error =
          firebase::functions::kErrorNone);

  firebase::Future<firebase::functions::HttpsCallableResult>
  TestFunctionFromURL(const char* function_url,
                      const firebase::Variant* const function_data,
                      const firebase::Variant& expected_result,
                      firebase::functions::Error expected_error =
                          firebase::functions::kErrorNone);

  bool initialized_;
  firebase::auth::Auth* auth_;
  firebase::functions::Functions* functions_;
};

FirebaseFunctionsTest::FirebaseFunctionsTest()
    : initialized_(false), auth_(nullptr), functions_(nullptr) {
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
}

FirebaseFunctionsTest::~FirebaseFunctionsTest() {
  // Must be cleaned up on exit.
  assert(app_ == nullptr);
  assert(auth_ == nullptr);
  assert(functions_ == nullptr);
}

void FirebaseFunctionsTest::SetUp() {
  FirebaseTest::SetUp();
  Initialize();
}

void FirebaseFunctionsTest::TearDown() {
  // Delete the shared path, if there is one.
  if (initialized_) {
    Terminate();
  }
  FirebaseTest::TearDown();
}

void FirebaseFunctionsTest::Initialize() {
  if (initialized_) return;

  InitializeApp();

  LogDebug("Initializing Firebase Auth and Firebase Functions.");

  // 0th element has a reference to this object, the rest have the initializer
  // targets.
  void* initialize_targets[] = {&auth_, &functions_};

  const firebase::ModuleInitializer::InitializerFn initializers[] = {
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Firebase Auth.");
        ::firebase::InitResult result;
        *reinterpret_cast<::firebase::auth::Auth**>(targets[0]) =
            ::firebase::auth::Auth::GetAuth(app, &result);
        return result;
      },
      [](::firebase::App* app, void* data) {
        void** targets = reinterpret_cast<void**>(data);
        LogDebug("Attempting to initialize Firebase Functions.");
        ::firebase::InitResult result;
        firebase::functions::Functions* functions =
            firebase::functions::Functions::GetInstance(app, &result);
        *reinterpret_cast<::firebase::functions::Functions**>(targets[1]) =
            functions;
        return result;
      }};

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(app_, initialize_targets, initializers,
                         sizeof(initializers) / sizeof(initializers[0]));

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized Firebase Auth and Firebase Functions.");

  initialized_ = true;
}

void FirebaseFunctionsTest::Terminate() {
  if (!initialized_) return;

  if (functions_) {
    LogDebug("Shutdown the Functions library.");
    delete functions_;
    functions_ = nullptr;
  }
  if (auth_) {
    LogDebug("Shutdown the Auth library.");
    delete auth_;
    auth_ = nullptr;
  }

  TerminateApp();

  initialized_ = false;

  ProcessEvents(100);
}

void FirebaseFunctionsTest::SignIn() {
  LogDebug("Signing in.");
  firebase::Future<firebase::auth::AuthResult> sign_in_future =
      auth_->SignInAnonymously();
  WaitForCompletion(sign_in_future, "SignInAnonymously");
  if (sign_in_future.error() != 0) {
    FAIL() << "Ensure your application has the Anonymous sign-in provider "
              "enabled in Firebase Console.";
  }
  ProcessEvents(100);
}

// A helper function for calling a Firebase Function and waiting on the result.
firebase::Future<firebase::functions::HttpsCallableResult>
FirebaseFunctionsTest::TestFunctionHelper(
    const char* function_name, firebase::functions::HttpsCallableReference& ref,
    const firebase::Variant* const function_data,
    const firebase::Variant& expected_result,
    firebase::functions::Error expected_error) {
  firebase::Future<firebase::functions::HttpsCallableResult> future;
  if (function_data == nullptr) {
    future = ref.Call();
  } else {
    future = ref.Call(*function_data);
  }
  WaitForCompletion(future,
                    (std::string("CallFunction ") + function_name).c_str(),
                    expected_error);
  if (!expected_result.is_null()) {
    EXPECT_EQ(VariantToString(expected_result),
              VariantToString(future.result()->data()))
        << "Unexpected result from calling " << function_name;
  }
  return future;
}

firebase::Future<firebase::functions::HttpsCallableResult>
FirebaseFunctionsTest::TestFunction(
    const char* function_name, const firebase::Variant* const function_data,
    const firebase::Variant& expected_result,
    firebase::functions::Error expected_error) {
  // Create a callable that we can run our test with.
  LogDebug("Calling %s", function_name);
  firebase::functions::HttpsCallableReference ref;
  ref = functions_->GetHttpsCallable(function_name);

  return TestFunctionHelper(function_name, ref, function_data, expected_result,
                            expected_error);
}

firebase::Future<firebase::functions::HttpsCallableResult>
FirebaseFunctionsTest::TestFunctionFromURL(
    const char* function_url, const firebase::Variant* const function_data,
    const firebase::Variant& expected_result,
    firebase::functions::Error expected_error) {
  // Create a callable that we can run our test with.
  LogDebug("Calling by URL %s", function_url);
  firebase::functions::HttpsCallableReference ref;
  ref = functions_->GetHttpsCallableFromURL(function_url);

  return TestFunctionHelper(function_url, ref, function_data, expected_result,
                            expected_error);
}

TEST_F(FirebaseFunctionsTest, TestInitializeAndTerminate) {
  // Already tested via SetUp() and TearDown().
}

TEST_F(FirebaseFunctionsTest, TestSignIn) {
  SignIn();
  EXPECT_TRUE(auth_->current_user().is_valid());
}

TEST_F(FirebaseFunctionsTest, TestFunction) {
  SignIn();

  // addNumbers(5, 7) = 12
  firebase::Variant data(firebase::Variant::EmptyMap());
  data.map()["firstNumber"] = 5;
  data.map()["secondNumber"] = 7;
  firebase::Variant result =
      TestFunction("addNumbers", &data, firebase::Variant::Null())
          .result()
          ->data();
  EXPECT_TRUE(result.is_map());
  EXPECT_EQ(result.map()["operationResult"], 12);
}

TEST_F(FirebaseFunctionsTest, TestFunctionWithData) {
  SignIn();

  std::map<std::string, firebase::Variant> data_map;
  data_map["bool"] = firebase::Variant::True();
  data_map["int"] = firebase::Variant(2);
  data_map["long"] = firebase::Variant(static_cast<int64_t>(3));
  // TODO: Add this back when we add Date support.
  // 1998-09-04T22:14:15.926Z
  // data["date"] = firebase::functions::EncodeDate(904947255926);
  data_map["string"] = firebase::Variant("four");
  std::vector<int> array;
  array.push_back(5);
  array.push_back(6);
  data_map["array"] = firebase::Variant(array);
  data_map["null"] = firebase::Variant::Null();

  std::map<std::string, firebase::Variant> expected;
  expected["message"] = firebase::Variant("stub response");
  expected["code"] = firebase::Variant(42);
  expected["long"] = firebase::Variant(420);
  std::vector<int> expected_array;
  expected_array.push_back(1);
  expected_array.push_back(2);
  expected_array.push_back(3);
  expected["array"] = firebase::Variant(expected_array);
  // TODO: Add this back when we add Date support.
  // 2017-09-04T22:14:15.000
  // expected["date"] =
  // firebase::Variant(static_cast<int64_t>(1504563255000));
  firebase::Variant data(data_map);
  TestFunction("dataTest", &data, firebase::Variant(expected));
}

TEST_F(FirebaseFunctionsTest, TestFunctionWithScalar) {
  SignIn();

  // Passing in and returning a scalar value instead of an object.
  firebase::Variant data(17);
  TestFunction("scalarTest", &data, firebase::Variant(76));
}

TEST_F(FirebaseFunctionsTest, TestFunctionWithAuthToken) {
  SignIn();

  // With an auth token.
  firebase::Variant data(firebase::Variant::EmptyMap());
  TestFunction("tokenTest", &data, firebase::Variant::EmptyMap());
}

// Disabling test temporarily. (b/143598197)
TEST_F(FirebaseFunctionsTest, DISABLED_TestFunctionWithInstanceId) {
#if defined(__ANDROID__) || TARGET_OS_IPHONE
  SignIn();

  // With an instance ID.
  firebase::Variant data(firebase::Variant::EmptyMap());
  TestFunction("instanceIdTest", &data, firebase::Variant::EmptyMap());
#endif  // defined(__ANDROID__) || TARGET_OS_IPHONE
}

TEST_F(FirebaseFunctionsTest, TestFunctionWithNull) {
  SignIn();

  // With an explicit null.
  firebase::Variant data(firebase::Variant::Null());
  TestFunction("nullTest", &data, firebase::Variant::Null());

  // With a void call.
  TestFunction("nullTest", nullptr, data);
}

TEST_F(FirebaseFunctionsTest, TestErrorHandling) {
  SignIn();

  // With the data/result field missing in the response.
  TestFunction("missingResultTest", nullptr, firebase::Variant::Null(),
               firebase::functions::kErrorInternal);

  // With a response that is not valid JSON.
  TestFunction("unhandledErrorTest", nullptr, firebase::Variant::Null(),
               firebase::functions::kErrorInternal);

  // With an invalid error code.
  TestFunction("unknownErrorTest", nullptr, firebase::Variant::Null(),
               firebase::functions::kErrorInternal);

  // With an explicit error code and message.
  TestFunction("explicitErrorTest", nullptr, firebase::Variant::Null(),
               firebase::functions::kErrorOutOfRange);
}

TEST_F(FirebaseFunctionsTest, TestFunctionFromURL) {
  SignIn();

  // addNumbers(4, 2) = 6
  firebase::Variant data(firebase::Variant::EmptyMap());
  data.map()["firstNumber"] = 4;
  data.map()["secondNumber"] = 2;
  std::string proj = app_->options().project_id();
  std::string url =
      "https://us-central1-" + proj + ".cloudfunctions.net/addNumbers";
  firebase::Variant result =
      TestFunctionFromURL(url.c_str(), &data, firebase::Variant::Null())
          .result()
          ->data();
  EXPECT_TRUE(result.is_map());
  EXPECT_EQ(result.map()["operationResult"], 6);
}

}  // namespace firebase_testapp_automated
