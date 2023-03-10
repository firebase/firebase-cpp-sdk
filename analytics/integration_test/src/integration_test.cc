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
#include "firebase/analytics.h"
#include "firebase/analytics/event_names.h"
#include "firebase/analytics/parameter_names.h"
#include "firebase/analytics/user_property_names.h"
#include "firebase/app.h"
#include "firebase/util.h"
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

using app_framework::LogInfo;
using app_framework::ProcessEvents;
using firebase_test_framework::FirebaseTest;

class FirebaseAnalyticsTest : public FirebaseTest {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  static firebase::App* shared_app_;
  static bool did_test_setconsent_;
};

firebase::App* FirebaseAnalyticsTest::shared_app_ = nullptr;
bool FirebaseAnalyticsTest::did_test_setconsent_ = false;

void FirebaseAnalyticsTest::SetUpTestSuite() {
#if defined(__ANDROID__)
  shared_app_ = firebase::App::Create(app_framework::GetJniEnv(),
                                      app_framework::GetActivity());
#else
  shared_app_ = firebase::App::Create();
#endif  // defined(__ANDROID__)

  firebase::analytics::Initialize(*shared_app_);
  did_test_setconsent_ = false;
}

void FirebaseAnalyticsTest::TearDownTestSuite() {
  firebase::analytics::Terminate();
  delete shared_app_;
  shared_app_ = nullptr;

  // The Analytics integration test is too fast for FTL, so pause a few seconds
  // here.
  ProcessEvents(1000);
  ProcessEvents(1000);
  ProcessEvents(1000);
  ProcessEvents(1000);
  ProcessEvents(1000);
}

TEST_F(FirebaseAnalyticsTest, TestSetCollectionEnabled) {
  // Can't confirm that these do anything but just run them all to ensure the
  // app doesn't crash.
  firebase::analytics::SetAnalyticsCollectionEnabled(true);
  firebase::analytics::SetAnalyticsCollectionEnabled(false);
  firebase::analytics::SetAnalyticsCollectionEnabled(true);
}

TEST_F(FirebaseAnalyticsTest, TestSetSessionTimeoutDuraction) {
  firebase::analytics::SetSessionTimeoutDuration(1000 * 60 * 5);
  firebase::analytics::SetSessionTimeoutDuration(1000 * 60 * 15);
  firebase::analytics::SetSessionTimeoutDuration(1000 * 60 * 30);
}

TEST_F(FirebaseAnalyticsTest, TestGetAnalyticsInstanceID) {
  firebase::Future<std::string> future =
      firebase::analytics::GetAnalyticsInstanceId();
  WaitForCompletion(future, "GetAnalyticsInstanceId");
  EXPECT_FALSE(future.result()->empty());
}

TEST_F(FirebaseAnalyticsTest, TestGetSessionID) {
  // Don't run this test if Google Play services is < 23.0.0.
  SKIP_TEST_ON_ANDROID_IF_GOOGLE_PLAY_SERVICES_IS_OLDER_THAN(230000);

  // iOS simulator tests are currently extra flaky, occasionally failing with an
  // "Analytics uninitialized" error even after multiple attempts.
  SKIP_TEST_ON_IOS_SIMULATOR;

  // On Android, if SetConsent was tested, this test will fail, since the app
  // needs to be restarted after consent is denied or it won't generate a new
  // sessionID. To not break the tests, skip this test in that case.
#if defined(__ANDROID__)
  // Log the Google Play services version for debugging in case this test fails.
  LogInfo("Google Play services version: %d", GetGooglePlayServicesVersion());

  if (did_test_setconsent_) {
    LogInfo(
        "Skipping TestGetSessionID after TestSetConsent, as GetSessionId() "
        "will fail until the app is restarted.");
    GTEST_SKIP();
    return;
  }
#endif
  // Log an event once, to ensure that there is currently an active Analytics
  // session.
  firebase::analytics::LogEvent(firebase::analytics::kEventSignUp);

  firebase::Future<int64_t> future;

  // Give Analytics a moment to initialize and create a session.
  ProcessEvents(1000);

  // It can take Analytics even more time to initialize and create a session, so
  // retry GetSessionId() if it returns an error.
  FLAKY_TEST_SECTION_BEGIN();

  future = firebase::analytics::GetSessionId();
  WaitForCompletion(future, "GetSessionId");

  FLAKY_TEST_SECTION_END();

  EXPECT_TRUE(future.result() != nullptr);
  EXPECT_NE(*future.result(), static_cast<int64_t>(0L));
  if (future.result() != nullptr) {
    LogInfo("Got session ID: %" PRId64, *future.result());
  }
}

TEST_F(FirebaseAnalyticsTest, TestSetConsent) {
  // Can't confirm that these do anything but just run them all to ensure the
  // app doesn't crash.
  std::map<firebase::analytics::ConsentType, firebase::analytics::ConsentStatus>
      consent_settings_allow = {
          {firebase::analytics::kConsentTypeAnalyticsStorage,
           firebase::analytics::kConsentStatusGranted},
          {firebase::analytics::kConsentTypeAdStorage,
           firebase::analytics::kConsentStatusGranted}};
  std::map<firebase::analytics::ConsentType, firebase::analytics::ConsentStatus>
      consent_settings_deny = {
          {firebase::analytics::kConsentTypeAnalyticsStorage,
           firebase::analytics::kConsentStatusDenied},
          {firebase::analytics::kConsentTypeAdStorage,
           firebase::analytics::kConsentStatusDenied}};
  std::map<firebase::analytics::ConsentType, firebase::analytics::ConsentStatus>
      consent_settings_empty;
  firebase::analytics::SetConsent(consent_settings_empty);
  ProcessEvents(1000);
  firebase::analytics::SetConsent(consent_settings_deny);
  ProcessEvents(1000);
  firebase::analytics::SetConsent(consent_settings_allow);
  ProcessEvents(1000);

  did_test_setconsent_ = true;
}

TEST_F(FirebaseAnalyticsTest, TestSetProperties) {
  // Set the user's sign up method.
  firebase::analytics::SetUserProperty(
      firebase::analytics::kUserPropertySignUpMethod, "Google");
  // Set the user ID.
  firebase::analytics::SetUserId("my_integration_test_user");
  // Initiate on-device conversion measurement.
  firebase::analytics::InitiateOnDeviceConversionMeasurementWithEmailAddress(
      "my_email@site.com");
}

TEST_F(FirebaseAnalyticsTest, TestLogEvents) {
  // Log an event with no parameters.
  firebase::analytics::LogEvent(firebase::analytics::kEventLogin);

  // Log an event with a floating point parameter.
  firebase::analytics::LogEvent("progress", "percent", 0.4f);

  // Log an event with an integer parameter.
  firebase::analytics::LogEvent(firebase::analytics::kEventPostScore,
                                firebase::analytics::kParameterScore, 42);

  // Log an event with a string parameter.
  firebase::analytics::LogEvent(firebase::analytics::kEventJoinGroup,
                                firebase::analytics::kParameterGroupID,
                                "spoon_welders");
}

TEST_F(FirebaseAnalyticsTest, TestLogEventWithMultipleParameters) {
  const firebase::analytics::Parameter kLevelUpParameters[] = {
      firebase::analytics::Parameter(firebase::analytics::kParameterLevel, 5),
      firebase::analytics::Parameter(firebase::analytics::kParameterCharacter,
                                     "mrspoon"),
      firebase::analytics::Parameter("hit_accuracy", 3.14f),
  };
  firebase::analytics::LogEvent(
      firebase::analytics::kEventLevelUp, kLevelUpParameters,
      sizeof(kLevelUpParameters) / sizeof(kLevelUpParameters[0]));
}

TEST_F(FirebaseAnalyticsTest, TestResettingGivesNewInstanceId) {
  // Test is flaky on iPhone due to a known issue in iOS.  See b/143656277.
#if TARGET_OS_IPHONE
  FLAKY_TEST_SECTION_BEGIN();
#endif  // TARGET_OS_IPHONE

  firebase::Future<std::string> future =
      firebase::analytics::GetAnalyticsInstanceId();
  WaitForCompletion(future, "GetAnalyticsInstanceId");
  EXPECT_FALSE(future.result()->empty());
  std::string instance_id = *future.result();

  firebase::analytics::ResetAnalyticsData();

  future = firebase::analytics::GetAnalyticsInstanceId();
  WaitForCompletion(future, "GetAnalyticsInstanceId after ResetAnalyticsData");
  std::string new_instance_id = *future.result();
  EXPECT_FALSE(future.result()->empty());
  EXPECT_NE(instance_id, new_instance_id);

#if TARGET_OS_IPHONE
  FLAKY_TEST_SECTION_END();
#endif  // TARGET_OS_IPHONE
}

}  // namespace firebase_testapp_automated
