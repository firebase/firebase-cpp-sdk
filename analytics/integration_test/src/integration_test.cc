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

using firebase_test_framework::FirebaseTest;

class FirebaseAnalyticsTest : public FirebaseTest {
 public:
  static void SetUpTestSuite();
  static void TearDownTestSuite();

  static firebase::App* shared_app_;
};

firebase::App* FirebaseAnalyticsTest::shared_app_;

void FirebaseAnalyticsTest::SetUpTestSuite() {
#if defined(__ANDROID__)
  shared_app_ = firebase::App::Create(app_framework::GetJniEnv(),
                                      app_framework::GetActivity());
#else
  shared_app_ = firebase::App::Create();
#endif  // defined(__ANDROID__)

  firebase::analytics::Initialize(*shared_app_);
}

void FirebaseAnalyticsTest::TearDownTestSuite() {
  firebase::analytics::Terminate();
  delete shared_app_;
  shared_app_ = nullptr;
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

TEST_F(FirebaseAnalyticsTest, TestSetProperties) {
  // Set the user's sign up method.
  firebase::analytics::SetUserProperty(
      firebase::analytics::kUserPropertySignUpMethod, "Google");
  // Set the user ID.
  firebase::analytics::SetUserId("my_integration_test_user");
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
