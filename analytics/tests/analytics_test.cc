/*
 * Copyright 2017 Google LLC
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

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#define __ANDROID__
#include <jni.h>

#include "testing/run_all_tests.h"
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "analytics/src/analytics_common.h"
#include "analytics/src/include/firebase/analytics.h"
#include "app/src/include/firebase/app.h"
#include "app/src/time.h"
#include "app/tests/include/firebase/app_for_testing.h"

#ifdef __ANDROID__
#include "app/src/semaphore.h"
#include "app/src/util_android.h"
#endif  // __ANDROID__

#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
#undef __ANDROID__
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)

#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

namespace firebase {
namespace analytics {

class AnalyticsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    firebase::testing::cppsdk::TickerReset();
    firebase::testing::cppsdk::ConfigSet("{}");
    reporter_.reset();

    firebase_app_ = testing::CreateApp();
    AddExpectationAndroid("FirebaseAnalytics.getInstance", {});
    analytics::Initialize(*firebase_app_);
  }

  void TearDown() override {
    firebase::testing::cppsdk::ConfigReset();
    Terminate();
    delete firebase_app_;
    firebase_app_ = nullptr;
    EXPECT_THAT(reporter_.getFakeReports(),
                ::testing::Eq(reporter_.getExpectations()));
  }

  void AddExpectationAndroid(const char* fake,
                             std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kAndroid,
                             args);
  }

  void AddExpectationApple(const char* fake,
                           std::initializer_list<std::string> args) {
    reporter_.addExpectation(fake, "", firebase::testing::cppsdk::kIos, args);
  }

  // Wait for a task executing on the main thread.
  void WaitForMainThreadTask() {
#if defined(FIREBASE_ANDROID_FOR_DESKTOP)
    Semaphore main_thread_signal(0);
    util::RunOnMainThread(
        firebase_app_->GetJNIEnv(), firebase_app_->activity(),
        [](void* data) { reinterpret_cast<Semaphore*>(data)->Post(); },
        &main_thread_signal);
    main_thread_signal.Wait();
#endif  // defined(FIREBASE_ANDROID_FOR_DESKTOP)
  }

  // Wait for a future up to the specified number of milliseconds.
  template <typename T>
  static void WaitForFutureWithTimeout(const Future<T>& future,
                                       int timeout_milliseconds,
                                       FutureStatus expected_status) {
    while (future.status() != expected_status && timeout_milliseconds-- > 0) {
      ::firebase::internal::Sleep(1);
    }
  }

  App* firebase_app_ = nullptr;

  firebase::testing::cppsdk::Reporter reporter_;
};

TEST_F(AnalyticsTest, TestDestroyDefaultApp) {
  EXPECT_TRUE(internal::IsInitialized());
  delete firebase_app_;
  firebase_app_ = nullptr;
  EXPECT_FALSE(internal::IsInitialized());
}

TEST_F(AnalyticsTest, TestSetAnalyticsCollectionEnabled) {
  AddExpectationAndroid("FirebaseAnalytics.setAnalyticsCollectionEnabled",
                        {"true"});
  AddExpectationApple("+[FIRAnalytics setAnalyticsCollectionEnabled:]",
                      {"YES"});
  SetAnalyticsCollectionEnabled(true);
}

TEST_F(AnalyticsTest, TestSetAnalyticsCollectionDisabled) {
  AddExpectationAndroid("FirebaseAnalytics.setAnalyticsCollectionEnabled",
                        {"false"});
  AddExpectationApple("+[FIRAnalytics setAnalyticsCollectionEnabled:]", {"NO"});
  SetAnalyticsCollectionEnabled(false);
}

TEST_F(AnalyticsTest, TestLogEventString) {
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"my_event", "my_param=my_value"});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", "my_param=my_value"});

  LogEvent("my_event", "my_param", "my_value");
}

TEST_F(AnalyticsTest, TestLogEventDouble) {
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"my_event", "my_param=1.01"});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", "my_param=1.01"});
  LogEvent("my_event", "my_param", 1.01);
}

TEST_F(AnalyticsTest, TestLogEventInt64) {
  int64_t value = 101;
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"my_event", "my_param=101"});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", "my_param=101"});

  LogEvent("my_event", "my_param", value);
}

TEST_F(AnalyticsTest, TestLogEventInt) {
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"my_event", "my_param=101"});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", "my_param=101"});

  LogEvent("my_event", "my_param", 101);
}

TEST_F(AnalyticsTest, TestLogEvent) {
  AddExpectationAndroid("FirebaseAnalytics.logEvent", {"my_event", ""});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", ""});

  LogEvent("my_event");
}

TEST_F(AnalyticsTest, TestLogEvent40CharName) {
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"0123456789012345678901234567890123456789", ""});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"0123456789012345678901234567890123456789", ""});

  LogEvent("0123456789012345678901234567890123456789");
}

TEST_F(AnalyticsTest, TestLogEventString40CharName) {
  AddExpectationAndroid(
      "FirebaseAnalytics.logEvent",
      {"my_event", "0123456789012345678901234567890123456789=my_value"});
  AddExpectationApple(
      "+[FIRAnalytics logEventWithName:parameters:]",
      {"my_event", "0123456789012345678901234567890123456789=my_value"});
  LogEvent("my_event", "0123456789012345678901234567890123456789", "my_value");
}

TEST_F(AnalyticsTest, TestLogEventString100CharValue) {
  const std::string long_string =
      "0123456789012345678901234567890123456789"
      "012345678901234567890123456789012345678901234567890123456789";
  const std::string result = "my_event=" + long_string;
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"my_event", result.c_str()});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event", result.c_str()});
  LogEvent("my_event", "my_event", long_string.c_str());
}

TEST_F(AnalyticsTest, TestLogEventParameters) {
  // Params are sorted alphabetically by mock.
  AddExpectationAndroid(
      "FirebaseAnalytics.logEvent",
      {"my_event",
       "my_param_bool=1,my_param_double=1.01,my_param_int=101,"
       "my_param_string=my_value"});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"my_event",
                       "my_param_bool=1,my_param_double=1.01,my_param_int=101,"
                       "my_param_string=my_value"});

  Parameter parameters[] = {
      Parameter("my_param_string", "my_value"),
      Parameter("my_param_double", 1.01),
      Parameter("my_param_int", 101),
      Parameter("my_param_bool", true),
  };
  LogEvent("my_event", parameters, sizeof(parameters) / sizeof(parameters[0]));
}

TEST_F(AnalyticsTest, TestSetUserProperty) {
  AddExpectationAndroid("FirebaseAnalytics.setUserProperty",
                        {"my_property", "my_value"});
  AddExpectationApple("+[FIRAnalytics setUserPropertyString:forName:]",
                      {"my_property", "my_value"});

  SetUserProperty("my_property", "my_value");
}

TEST_F(AnalyticsTest, TestSetUserPropertyNull) {
  AddExpectationAndroid("FirebaseAnalytics.setUserProperty",
                        {"my_property", "null"});
  AddExpectationApple("+[FIRAnalytics setUserPropertyString:forName:]",
                      {"my_property", "nil"});
  SetUserProperty("my_property", nullptr);
}

TEST_F(AnalyticsTest, TestSetUserId) {
  AddExpectationAndroid("FirebaseAnalytics.setUserId", {"my_user_id"});
  AddExpectationApple("+[FIRAnalytics setUserID:]", {"my_user_id"});
  SetUserId("my_user_id");
}

TEST_F(AnalyticsTest, TestSetUserIdNull) {
  AddExpectationAndroid("FirebaseAnalytics.setUserId", {"null"});
  AddExpectationApple("+[FIRAnalytics setUserID:]", {"nil"});
  SetUserId(nullptr);
}

TEST_F(AnalyticsTest, TestSetSessionTimeoutDuration) {
  AddExpectationAndroid("FirebaseAnalytics.setSessionTimeoutDuration",
                        {"1000"});
  AddExpectationApple("+[FIRAnalytics setSessionTimeoutInterval:]", {"1.000"});

  SetSessionTimeoutDuration(1000);
}

TEST_F(AnalyticsTest, TestSetCurrentScreen) {
  AddExpectationAndroid("FirebaseAnalytics.setCurrentScreen",
                        {"android.app.Activity", "my_screen", "my_class"});
  AddExpectationApple("+[FIRAnalytics setScreenName:screenClass:]",
                      {"my_screen", "my_class"});

  SetCurrentScreen("my_screen", "my_class");
  WaitForMainThreadTask();
}

TEST_F(AnalyticsTest, TestSetCurrentScreenNullScreen) {
  AddExpectationAndroid("FirebaseAnalytics.setCurrentScreen",
                        {"android.app.Activity", "null", "my_class"});
  AddExpectationApple("+[FIRAnalytics setScreenName:screenClass:]",
                      {"nil", "my_class"});

  SetCurrentScreen(nullptr, "my_class");
  WaitForMainThreadTask();
}

TEST_F(AnalyticsTest, TestSetCurrentScreenNullClass) {
  AddExpectationAndroid("FirebaseAnalytics.setCurrentScreen",
                        {"android.app.Activity", "my_screen", "null"});
  AddExpectationApple("+[FIRAnalytics setScreenName:screenClass:]",
                      {"my_screen", "nil"});

  SetCurrentScreen("my_screen", nullptr);
  WaitForMainThreadTask();
}

TEST_F(AnalyticsTest, TestResetAnalyticsData) {
  AddExpectationAndroid("FirebaseAnalytics.resetAnalyticsData", {});
  AddExpectationApple("+[FIRAnalytics resetAnalyticsData]", {});
  AddExpectationApple("+[FIRAnalytics appInstanceID]", {});
  ResetAnalyticsData();
}

TEST_F(AnalyticsTest, TestGetAnalyticsInstanceId) {
  AddExpectationAndroid("FirebaseAnalytics.getAppInstanceId", {});
  AddExpectationApple("+[FIRAnalytics appInstanceID]", {});
  auto result = GetAnalyticsInstanceId();
  // Wait for up to a second to fetch the ID.
  WaitForFutureWithTimeout(result, 1000, firebase::kFutureStatusComplete);
  EXPECT_EQ(firebase::kFutureStatusComplete, result.status());
  EXPECT_EQ(std::string("FakeAnalyticsInstanceId0"), *result.result());
}

}  // namespace analytics
}  // namespace firebase
