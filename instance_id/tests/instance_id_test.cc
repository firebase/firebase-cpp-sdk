// Copyright 2017 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// WARNING: Some Code from this file is included verbatim in the C++
//          documentation. Only change existing code if it is safe to release
//          to the public. Otherwise, a tech writer may make an unrelated
//          modification, regenerate the docs, and unwittingly release an
//          unannounced modification to the public.

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#include <unistd.h>
#define __ANDROID__
#include <jni.h>

#include <functional>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif  // defined(__APPLE__)

// [START instance_id_includes]
#include "app/src/include/firebase/app.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/log.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "instance_id/src/include/firebase/instance_id.h"
// [END instance_id_includes]
#if TARGET_OS_IPHONE
#include "instance_id/src_ios/fake/FIRInstanceID.h"
#endif  // TARGET_OS_IPHONE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"

using testing::Eq;
using testing::IsNull;
using testing::MatchesRegex;
using testing::NotNull;

namespace firebase {
namespace instance_id {

class InstanceIdTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    AppOptions options;
    options.set_messaging_sender_id("123456");
#if TARGET_OS_IPHONE
    FIRInstanceIDInitialize();
#endif  // TARGET_OS_IPHONE
    reporter_.reset();
    app_ = testing::CreateApp();
  }

  void TearDown() override {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
    SetThrowExceptionMessage(nullptr);
    SetBlockingMethodCallsEnable(false);
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
    firebase::testing::cppsdk::ConfigReset();
    delete app_;
    app_ = nullptr;
    EXPECT_THAT(reporter_.getFakeReports(), Eq(reporter_.getExpectations()));
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  void AddExpectationIos(const char* fake,
                         std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  void AddExpectationAndroidIos(const char* fake,
                                std::initializer_list<std::string> args) {
    AddExpectationAndroid(fake, args);
    AddExpectationIos(fake, args);
  }

  // Wait for a future up to the specified number of milliseconds.
  template <typename T>
  static void WaitForFutureWithTimeout(
      const Future<T>& future,
      int timeout_milliseconds = kFutureTimeoutMilliseconds,
      FutureStatus expected_status = kFutureStatusComplete) {
    while (future.status() != expected_status && timeout_milliseconds-- > 0) {
      ::firebase::internal::Sleep(1);
    }
  }

  // Validate that a future completed successfully and has the specified
  // result.
  template <typename T>
  static void CheckSuccessWithValue(const Future<T>& future, const T& result) {
    WaitForFutureWithTimeout<T>(future);
    EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(future.error(), Eq(instance_id::kErrorNone));
    EXPECT_THAT(*future.result(), Eq(result));
  }

  // Validate that a future completed successfully.
  static void CheckSuccess(const Future<void>& future) {
    WaitForFutureWithTimeout<void>(future);
    EXPECT_THAT(future.status(), Eq(kFutureStatusComplete));
    EXPECT_THAT(future.error(), Eq(instance_id::kErrorNone));
  }

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  // Find the mock FirebaseInstanceId class.
  static void GetMockClass(
      const std::function<void(JNIEnv*, jclass)>& retrieved_class) {
    JNIEnv* env = firebase::testing::cppsdk::GetTestJniEnv();
    jclass clazz = env->FindClass("com/google/firebase/iid/FirebaseInstanceId");
    retrieved_class(env, clazz);
    env->DeleteLocalRef(clazz);
  }
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  // Set the exception message to throw from the next method call to the fake.
  static void SetThrowExceptionMessage(const char* message) {
    GetMockClass([&message](JNIEnv* env, jclass clazz) {
      jmethodID methodId = env->GetStaticMethodID(
          clazz, "setThrowExceptionMessage", "(Ljava/lang/String;)V");
      jobject stringobj = message ? env->NewStringUTF(message) : nullptr;
      env->CallStaticVoidMethod(clazz, methodId, stringobj);
      if (env->ExceptionCheck()) env->ExceptionClear();
      if (stringobj) env->DeleteLocalRef(stringobj);
    });
  }
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

  // Enable / disable indefinite blocking of all mock method calls.
  static bool SetBlockingMethodCallsEnable(bool enable) {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
    bool successful = false;
    GetMockClass([&enable, &successful](JNIEnv* env, jclass clazz) {
      jmethodID methodId =
          env->GetStaticMethodID(clazz, "setBlockingMethodCallsEnable", "(Z)Z");
      successful = env->CallStaticBooleanMethod(clazz, methodId, enable);
      if (env->ExceptionCheck()) env->ExceptionClear();
    });
    return successful;
#elif TARGET_OS_IPHONE
    return FIRInstanceIDSetBlockingMethodCallsEnable(enable);
#endif
    return false;
  }

  // Wait for the worker thread to start, returning true if the thread started,
  // false otherwise.
  static bool WaitForBlockedThread() {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
    bool successful = false;
    GetMockClass([&successful](JNIEnv* env, jclass clazz) {
      jmethodID methodId =
          env->GetStaticMethodID(clazz, "waitForBlockedThread", "()Z");
      successful = env->CallStaticBooleanMethod(clazz, methodId);
      if (env->ExceptionCheck()) env->ExceptionClear();
    });
    return successful;
#elif TARGET_OS_IPHONE
    return FIRInstanceIDWaitForBlockedThread();
#endif
    return false;
  }

  // Validate the specified future handle is invalid.
  template <typename T>
  static void ExpectInvalidFuture(const Future<T>& future) {
    EXPECT_THAT(future.status(), Eq(kFutureStatusInvalid));
    EXPECT_THAT(future.error_message(), IsNull());
  }

  App* app_ = nullptr;
  firebase::testing::cppsdk::Reporter reporter_;
  static const char* const kTokenEntity;
  static const char* const kTokenScope;
  static const char* const kTokenScopeAll;
  static const int kMicrosecondsPerMillisecond;
  // Default time to wait for future status changes.
  static const int kFutureTimeoutMilliseconds;
};

const char* const InstanceIdTest::kTokenEntity = "an_entity";
const char* const InstanceIdTest::kTokenScope = "a_scope";
const char* const InstanceIdTest::kTokenScopeAll = "*";
const int InstanceIdTest::kMicrosecondsPerMillisecond = 1000;
const int InstanceIdTest::kFutureTimeoutMilliseconds = 1000;

// Validate creation of an InstanceId instance.
TEST_F(InstanceIdTest, TestCreate) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_THAT(instance_id, NotNull());
  EXPECT_THAT(init_result, Eq(kInitResultSuccess));
  delete instance_id;
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
// Test creation that fails.
TEST_F(InstanceIdTest, TestCreateWithError) {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  SetThrowExceptionMessage("Failed to initialize");
#else
  FIRInstanceIDSetNextErrorCode(kFIRInstanceIDErrorCodeUnknown);
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_THAT(instance_id, IsNull());
  EXPECT_THAT(init_result, Eq(kInitResultFailedMissingDependency));
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

// Ensure the retrieving the an InstanceId from the same app returns the same
// instance.
TEST_F(InstanceIdTest, TestCreateAndGet) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  auto* instance_id1 = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_THAT(instance_id1, NotNull());
  EXPECT_THAT(init_result, Eq(kInitResultSuccess));
  auto* instance_id2 = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_THAT(instance_id2, Eq(instance_id1));
  delete instance_id1;
}

// Validate InstanceId instance is destroyed when the corresponding app is
// destroyed.
// NOTE: It's not possible to execute this test on iOS as we can only create an
// instance ID object for the default app.
#if !TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestCreateAndDestroyApp) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  const char* kAppNames[] = {"named_app1", "named_app2"};
  auto* app = testing::CreateApp(testing::MockAppOptions(), kAppNames[0]);
  auto* instance_id1 = InstanceId::GetInstanceId(app, &init_result);
  EXPECT_THAT(instance_id1, NotNull());
  EXPECT_THAT(init_result, Eq(kInitResultSuccess));

  // Temporarily disable LogAssert() which causes the application to assert.
  void* log_callback_data;
  LogCallback log_callback = LogGetCallback(&log_callback_data);
  LogSetCallback(
      [](LogLevel log_level, const char* log_message, void* callback_data) {
        if (log_level == kLogLevelAssert) {
          ASSERT_THAT(
              log_message,
              MatchesRegex(
                  "InstanceId object 0x[0-9A-Fa-f]+ should be "
                  "deleted before the App 0x[0-9A-Fa-f]+ it depends upon."));
          log_level = kLogLevelError;
        }
        reinterpret_cast<LogCallback>(callback_data)(log_level, log_message,
                                                     nullptr);
      },
      reinterpret_cast<void*>(log_callback));

  delete app;  // This should delete instance_id1's internal data, not
               // instance_id1 itself.
  EXPECT_THAT(instance_id1, NotNull());
  delete instance_id1;

  LogSetCallback(log_callback, log_callback_data);

  app = testing::CreateApp(testing::MockAppOptions(), kAppNames[1]);
  // Validate the new app instance yields a new Instance ID object.
  auto* instance_id2 = InstanceId::GetInstanceId(app, &init_result);
  EXPECT_THAT(std::string(instance_id2->app().name()),
              Eq(std::string(kAppNames[1])));
  EXPECT_THAT(init_result, Eq(kInitResultSuccess));
}
#endif  // !TARGET_OS_IPHONE

TEST_F(InstanceIdTest, TestGetCreationTime) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
#if !TARGET_OS_IPHONE
  // At the moment creation_time() is not exposed on iOS.
  AddExpectationAndroidIos("FirebaseInstanceId.getCreationTime", {});
#endif  // !TARGET_OS_IPHONE
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  EXPECT_THAT(instance_id->creation_time(), Eq(1512000287000));
#else
  EXPECT_THAT(instance_id->creation_time(), Eq(0));
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  delete instance_id;
}

#if FIREBASE_PLATFORM_MOBILE
TEST_F(InstanceIdTest, TestGetId) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.getId", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  const std::string expected_value("FakeId");
  CheckSuccessWithValue(instance_id->GetId(), expected_value);
  CheckSuccessWithValue(instance_id->GetIdLastResult(), expected_value);
  delete instance_id;
}
#endif  // FIREBASE_PLATFORM_MOBILE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestGetIdTeardown) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.getId", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_TRUE(SetBlockingMethodCallsEnable(true));
  auto future = instance_id->GetId();
  EXPECT_TRUE(WaitForBlockedThread());
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  delete instance_id;
  EXPECT_TRUE(SetBlockingMethodCallsEnable(false));
  ExpectInvalidFuture(future);
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

TEST_F(InstanceIdTest, TestDeleteId) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.deleteId", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  CheckSuccess(instance_id->DeleteId());
  CheckSuccess(instance_id->DeleteIdLastResult());
  delete instance_id;
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestDeleteIdFailed) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  Error expected_error = kErrorUnknown;
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  SetThrowExceptionMessage("Error while reading ID");
#else
  FIRInstanceIDSetNextErrorCode(kFIRInstanceIDErrorCodeNoAccess);
  expected_error = kErrorNoAccess;
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  auto future = instance_id->DeleteId();
  WaitForFutureWithTimeout(future);
  EXPECT_THAT(future.error(), Eq(expected_error));
  EXPECT_THAT(future.error_message(), NotNull());
  delete instance_id;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestDeleteIdTeardown) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.deleteId", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_TRUE(SetBlockingMethodCallsEnable(true));
  auto future = instance_id->DeleteId();
  EXPECT_TRUE(WaitForBlockedThread());
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  delete instance_id;
  EXPECT_TRUE(SetBlockingMethodCallsEnable(false));
  ExpectInvalidFuture(future);
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

#if FIREBASE_PLATFORM_MOBILE
TEST_F(InstanceIdTest, TestGetTokenEntityScope) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.getToken",
                           {kTokenEntity, kTokenScope});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  const std::string expected_value("FakeToken");
  CheckSuccessWithValue(instance_id->GetToken(kTokenEntity, kTokenScope),
                        expected_value);
  CheckSuccessWithValue(instance_id->GetTokenLastResult(), expected_value);
  delete instance_id;
}

TEST_F(InstanceIdTest, TestGetToken) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos(
      "FirebaseInstanceId.getToken",
      {app_->options().messaging_sender_id(), kTokenScopeAll});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  const std::string expected_value("FakeToken");
  CheckSuccessWithValue(instance_id->GetToken(), expected_value);
  CheckSuccessWithValue(instance_id->GetTokenLastResult(), expected_value);
  delete instance_id;
}

// Sample code that creates an InstanceId for the default app and gets a token.
TEST_F(InstanceIdTest, TestGetTokenSample) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos(
      "FirebaseInstanceId.getToken",
      {app_->options().messaging_sender_id(), kTokenScopeAll});
  // [START instance_id_get_token]
  firebase::InitResult init_result;
  auto* instance_id_object = firebase::instance_id::InstanceId::GetInstanceId(
      firebase::App::GetInstance(), &init_result);
  instance_id_object->GetToken().OnCompletion(
      [](const firebase::Future<std::string>& future) {
        if (future.status() == kFutureStatusComplete &&
            future.error() == firebase::instance_id::kErrorNone) {
          printf("Instance ID Token %s\n", future.result()->c_str());
        }
      });
  // [END instance_id_get_token]
  // WaitForFutureWithTimeout(instance_id_object->GetTokenLastResult());
  CheckSuccessWithValue(instance_id_object->GetTokenLastResult(),
                        std::string("FakeToken"));
  delete instance_id_object;
}
#endif  // FIREBASE_PLATFORM_MOBILE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestGetTokenFailed) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  Error expected_error = kErrorUnknown;
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  SetThrowExceptionMessage("INSTANCE_ID_RESET");
  expected_error = kErrorIdInvalid;
#else
  FIRInstanceIDSetNextErrorCode(kFIRInstanceIDErrorCodeAuthentication);
  expected_error = kErrorAuthentication;
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  auto future = instance_id->GetToken();
  WaitForFutureWithTimeout(future);
  EXPECT_THAT(future.error(), Eq(expected_error));
  EXPECT_THAT(future.error_message(), NotNull());
  delete instance_id;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestGetTokenTeardown) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos(
      "FirebaseInstanceId.getToken",
      {app_->options().messaging_sender_id(), kTokenScopeAll});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_TRUE(SetBlockingMethodCallsEnable(true));
  auto future = instance_id->GetToken();
  EXPECT_TRUE(WaitForBlockedThread());
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  delete instance_id;
  EXPECT_TRUE(SetBlockingMethodCallsEnable(false));
  ExpectInvalidFuture(future);
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

TEST_F(InstanceIdTest, TestDeleteTokenEntityScope) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos("FirebaseInstanceId.deleteToken",
                           {kTokenEntity, kTokenScope});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  CheckSuccess(instance_id->DeleteToken(kTokenEntity, kTokenScope));
  CheckSuccess(instance_id->DeleteTokenLastResult());
  delete instance_id;
}

TEST_F(InstanceIdTest, TestDeleteToken) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos(
      "FirebaseInstanceId.deleteToken",
      {app_->options().messaging_sender_id(), kTokenScopeAll});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  CheckSuccess(instance_id->DeleteToken());
  CheckSuccess(instance_id->DeleteTokenLastResult());
  delete instance_id;
}

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestDeleteTokenFailed) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
  SetThrowExceptionMessage("SERVICE_NOT_AVAILABLE");
#else
  FIRInstanceIDSetNextErrorCode(kFIRInstanceIDErrorCodeNoAccess);
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  auto future = instance_id->DeleteToken();
  WaitForFutureWithTimeout(future);
  EXPECT_THAT(future.error(), Eq(kErrorNoAccess));
  EXPECT_THAT(future.error_message(), NotNull());
  delete instance_id;
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

#if defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE
TEST_F(InstanceIdTest, TestDeleteTokenTeardown) {
  AddExpectationAndroidIos("FirebaseInstanceId.construct", {});
  AddExpectationAndroidIos(
      "FirebaseInstanceId.deleteToken",
      {app_->options().messaging_sender_id(), kTokenScopeAll});
  InitResult init_result;
  auto* instance_id = InstanceId::GetInstanceId(app_, &init_result);
  EXPECT_TRUE(SetBlockingMethodCallsEnable(true));
  auto future = instance_id->DeleteToken();
  EXPECT_TRUE(WaitForBlockedThread());
  EXPECT_THAT(future.status(), Eq(kFutureStatusPending));
  delete instance_id;
  EXPECT_TRUE(SetBlockingMethodCallsEnable(false));
  ExpectInvalidFuture(future);
}
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP) || TARGET_OS_IPHONE

}  // namespace instance_id
}  // namespace firebase
