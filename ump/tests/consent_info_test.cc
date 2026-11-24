/*
 * Copyright 2026 Google LLC
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

#include "ump/src/include/firebase/ump/consent_info.h"

#include "app/src/include/firebase/app.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace ump {
namespace {

TEST(UmpConsentInfoTest, DefaultHandleIsInvalid) {
  ConsentInfo info;
  EXPECT_FALSE(info.is_valid());
  EXPECT_FALSE(static_cast<bool>(info));
  EXPECT_EQ(info.GetConsentStatus(), kConsentStatusUnknown);
  EXPECT_EQ(info.GetConsentFormStatus(), kConsentFormStatusUnknown);
  EXPECT_EQ(info.GetPrivacyOptionsRequirementStatus(),
            kPrivacyOptionsRequirementStatusUnknown);
  EXPECT_FALSE(info.CanRequestAds());
  EXPECT_EQ(info.RequestConsentInfoUpdateLastResult().status(),
            kFutureStatusInvalid);
}

TEST(UmpConsentInfoTest, ProcessWideSingletonEqualAcrossApps) {
  App app1 = testing::CreateApp(testing::MockAppOptions(), "ump_app_1");
  App app2 = testing::CreateApp(testing::MockAppOptions(), "ump_app_2");

  InitResult result1 = kInitResultFailedMissingDependency;
  ConsentInfo info1 = ConsentInfo::GetInstance(app1, &result1);
  ASSERT_EQ(result1, kInitResultSuccess);
  ASSERT_TRUE(info1.is_valid());

  InitResult result2 = kInitResultFailedMissingDependency;
  ConsentInfo info2 = ConsentInfo::GetInstance(app2, &result2);
  ASSERT_EQ(result2, kInitResultSuccess);
  EXPECT_EQ(info1, info2);

  ConsentInfo info3 = ConsentInfo::GetInstance();
  EXPECT_EQ(info1, info3);
}

TEST(UmpConsentInfoTest, DroppingLastHandleTearsDownAndInvalidatesFutures) {
  Future<void> future;
  {
    ConsentInfo info = ConsentInfo::GetInstance();
    ASSERT_TRUE(info.is_valid());
    ConsentRequestParameters params;
    future = info.RequestConsentInfoUpdate(params);
    EXPECT_EQ(future.status(), kFutureStatusComplete);
    EXPECT_EQ(info.GetConsentStatus(), kConsentStatusObtained);
  }
  // Last handle dropped -> ConsentInfoInternal destroyed, future invalidated.
  EXPECT_EQ(future.status(), kFutureStatusInvalid);

  // Subsequent GetInstance creates a fresh instance with reset state.
  ConsentInfo fresh = ConsentInfo::GetInstance();
  ASSERT_TRUE(fresh.is_valid());
  EXPECT_EQ(fresh.GetConsentStatus(), kConsentStatusUnknown);
}

TEST(UmpConsentInfoTest, StaticDurationConsentInfoSurvivesExit) {
  static ConsentInfo static_info;
  static_info = ConsentInfo::GetInstance();
  ASSERT_TRUE(static_info.is_valid());
}

}  // namespace
}  // namespace ump
}  // namespace firebase
