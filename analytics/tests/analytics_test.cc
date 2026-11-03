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

#include <iterator>

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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testing/config.h"
#include "testing/reporter.h"
#include "testing/ticker.h"

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
  LogEvent("my_event", parameters, std::size(parameters));
}

TEST_F(AnalyticsTest,
       TestInitiateOnDeviceConversionMeasurementWithEmailAddress) {
  // InitiateOnDeviceConversionMeasurementWithEmailAddress is no-op on Android
  AddExpectationApple(
      "+[FIRAnalytics initiateOnDeviceConversionMeasurementWithEmailAddress:]",
      {"my_email"});

  InitiateOnDeviceConversionMeasurementWithEmailAddress("my_email");
}

TEST_F(AnalyticsTest,
       TestInitiateOnDeviceConversionMeasurementWithPhoneNumber) {
  // InitiateOnDeviceConversionMeasurementWithPhoneNumber is no-op on Android
  AddExpectationApple(
      "+[FIRAnalytics initiateOnDeviceConversionMeasurementWithPhoneNumber:]",
      {"+15551234567"});

  InitiateOnDeviceConversionMeasurementWithPhoneNumber("+15551234567");
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
  ASSERT_NE(nullptr, result.result());
  EXPECT_EQ(std::string("FakeAnalyticsInstanceId0"), *result.result());
}

TEST_F(AnalyticsTest, TestParameterConstructorsAndEquality) {
  Parameter p_cstr("key", "value");
  {
    // Scoped std::string lifetime safety: Parameter copies string value into a
    // mutable string Variant so dangling c_str() is avoided after scope exit.
    std::string scoped_value("temp_value");
    p_cstr = Parameter("key", scoped_value.c_str());
  }
  EXPECT_STREQ("key", p_cstr.name);
  EXPECT_TRUE(p_cstr.value.is_string());
  EXPECT_TRUE(p_cstr.value.is_mutable_string());
  EXPECT_STREQ("temp_value", p_cstr.value.string_value());

  Parameter p_std_str("std_key", std::string("std_value"));
  EXPECT_STREQ("std_value", p_std_str.value.string_value());

  Parameter p_sv("sv_key", std::string_view("sv_value_extra", 8));
  EXPECT_STREQ("sv_value", p_sv.value.string_value());

  Parameter p_null_str("null_key", static_cast<const char*>(nullptr));
  EXPECT_TRUE(p_null_str.value.is_null());

  Parameter p_int("int_key", 42);
  Parameter p_int64("int_key", int64_t(42));
  Parameter p_size_t("size_key", static_cast<size_t>(42));
  Parameter p_uint64("uint64_key", static_cast<uint64_t>(42));
  Parameter p_double("double_key", 3.14);
  Parameter p_bool("bool_key", true);

  EXPECT_EQ(p_int, p_int64);
  EXPECT_NE(p_int, p_double);
  EXPECT_EQ(42, p_size_t.value.int64_value());
  EXPECT_EQ(42, p_uint64.value.int64_value());
  EXPECT_TRUE(p_bool.value.is_int64());
  EXPECT_EQ(1, p_bool.value.int64_value());
}

TEST_F(AnalyticsTest, TestDestroyNonDefaultApp) {
  Terminate();
  App* custom_app =
      testing::CreateApp(testing::MockAppOptions(), "custom_analytics_app");
  ASSERT_NE(nullptr, custom_app);
  AddExpectationAndroid("FirebaseAnalytics.getInstance", {});
  Initialize(*custom_app);
  EXPECT_TRUE(internal::IsInitialized());

  delete custom_app;
  EXPECT_FALSE(internal::IsInitialized());
}

TEST_F(AnalyticsTest, TestReinitializeCycles) {
  for (int i = 0; i < 3; ++i) {
    Terminate();
    EXPECT_FALSE(internal::IsInitialized());
    AddExpectationAndroid("FirebaseAnalytics.getInstance", {});
    Initialize(*firebase_app_);
    EXPECT_TRUE(internal::IsInitialized());
  }
}

TEST_F(AnalyticsTest, TestNullAndInvalidInputs) {
  // Null or empty event names should be safely rejected without calling native
  // methods.
  LogEvent(nullptr);
  LogEvent("");
  LogEvent(nullptr, "param", "value");
  LogEvent("my_event", nullptr, "value");
  LogEvent("my_event", nullptr, 1.0);
  LogEvent("my_event", nullptr, int64_t(1));
  LogEvent("my_event", nullptr, 1);

  // Null parameters pointer with non-zero count must return early safely.
  LogEvent("my_event", static_cast<const Parameter*>(nullptr), 3);
  SetDefaultEventParameters(static_cast<const Parameter*>(nullptr), 2);

  // Null user property name must return early safely.
  SetUserProperty(nullptr, "value");
  SetUserProperty("", "value");
}

TEST_F(AnalyticsTest, TestLogEventItemsVectorOfMaps) {
  const char* kExpectedParams =
      "currency=USD,items=[{item_id=sku_123, item_name=Sword, price=9.99, "
      "quantity=2}, {item_id=sku_456, item_name=Shield, price=14.5, "
      "quantity=1}],value=34.48";
  AddExpectationAndroid("FirebaseAnalytics.logEvent",
                        {"purchase", kExpectedParams});
  AddExpectationApple("+[FIRAnalytics logEventWithName:parameters:]",
                      {"purchase", kExpectedParams});

  Variant item1 = Variant::EmptyMap();
  item1.map()[Variant("item_id")] = Variant("sku_123");
  item1.map()[Variant("item_name")] = Variant("Sword");
  item1.map()[Variant("quantity")] = Variant(int64_t(2));
  item1.map()[Variant("price")] = Variant(9.99);

  Variant item2 = Variant::EmptyMap();
  item2.map()[Variant("item_id")] = Variant("sku_456");
  item2.map()[Variant("item_name")] = Variant("Shield");
  item2.map()[Variant("quantity")] = Variant(int64_t(1));
  item2.map()[Variant("price")] = Variant(14.50);

  Variant items_vec = Variant::EmptyVector();
  items_vec.vector().push_back(item1);
  items_vec.vector().push_back(item2);

  Parameter params[] = {
      Parameter("currency", "USD"),
      Parameter("value", 34.48),
      Parameter("items", items_vec),
  };
  LogEvent("purchase", params, std::size(params));
}

TEST_F(AnalyticsTest, TestSetDefaultEventParametersAndClear) {
  AddExpectationAndroid(
      "FirebaseAnalytics.setDefaultEventParameters",
      {"difficulty=3,level_name=dungeon_1,obsolete_param=null"});
  AddExpectationApple(
      "+[FIRAnalytics setDefaultEventParameters:]",
      {"difficulty=3,level_name=dungeon_1,obsolete_param=<null>"});
  AddExpectationAndroid("FirebaseAnalytics.setDefaultEventParameters",
                        {"null"});
  AddExpectationApple("+[FIRAnalytics setDefaultEventParameters:]", {"nil"});

  Parameter params[] = {
      Parameter("level_name", "dungeon_1"),
      Parameter("difficulty", 3),
      Parameter("obsolete_param", Variant::Null()),
  };
  SetDefaultEventParameters(params, std::size(params));
  SetDefaultEventParameters(nullptr, 0);
}

TEST_F(AnalyticsTest, TestSetConsent) {
  AddExpectationAndroid(
      "FirebaseAnalytics.setConsent",
      {"AD_PERSONALIZATION=DENIED,AD_STORAGE=GRANTED,"
       "AD_USER_DATA=GRANTED,ANALYTICS_STORAGE=DENIED"});
  AddExpectationApple(
      "+[FIRAnalytics setConsent:]",
      {"AD_PERSONALIZATION=DENIED,AD_STORAGE=GRANTED,"
       "AD_USER_DATA=GRANTED,ANALYTICS_STORAGE=DENIED"});

  std::map<ConsentType, ConsentStatus> consent_settings;
  consent_settings[kConsentTypeAdStorage] = kConsentStatusGranted;
  consent_settings[kConsentTypeAnalyticsStorage] = kConsentStatusDenied;
  consent_settings[kConsentTypeAdUserData] = kConsentStatusGranted;
  consent_settings[kConsentTypeAdPersonalization] = kConsentStatusDenied;
  SetConsent(consent_settings);
}

TEST_F(AnalyticsTest, TestGetSessionIdAndAppleTransaction) {
  AddExpectationAndroid("FirebaseAnalytics.getSessionId", {});
  AddExpectationApple("+[FIRAnalytics sessionIDWithCompletion:]", {});

  Future<int64_t> session_future = GetSessionId();
  WaitForFutureWithTimeout(session_future, 1000,
                           firebase::kFutureStatusComplete);
  EXPECT_EQ(firebase::kFutureStatusComplete, session_future.status());
  EXPECT_EQ(kErrorNone, session_future.error());
  ASSERT_NE(nullptr, session_future.result());
  EXPECT_NE(0, *session_future.result());

  Future<int64_t> session_last = GetSessionIdLastResult();
  EXPECT_EQ(firebase::kFutureStatusComplete, session_last.status());

  Future<void> tx_future = LogAppleTransaction("1000000123456789");
  WaitForFutureWithTimeout(tx_future, 1000, firebase::kFutureStatusComplete);
  EXPECT_EQ(firebase::kFutureStatusComplete, tx_future.status());
  EXPECT_EQ(kErrorNone, tx_future.error());

  Future<void> tx_last = LogAppleTransactionLastResult();
  EXPECT_EQ(firebase::kFutureStatusComplete, tx_last.status());
}

}  // namespace analytics
}  // namespace firebase
