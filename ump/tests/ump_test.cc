/*
 * Copyright 2023 Google LLC
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

#include "firebase/ump.h"

#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"

namespace firebase {
namespace ump {
namespace {

class UmpTest : public ::testing::Test {
 protected:
  void SetUp() override {
    app_ = testing::CreateApp();
    ASSERT_NE(app_, nullptr);
    InitResult init_result = kInitResultFailedMissingDependency;
    consent_info_ = ConsentInfo::GetInstance(*app_, &init_result);
    ASSERT_NE(consent_info_, nullptr);
    EXPECT_EQ(init_result, kInitResultSuccess);
    consent_info_->Reset();
  }

  void TearDown() override {
    if (consent_info_) {
      delete consent_info_;
      consent_info_ = nullptr;
    }
    if (app_) {
      delete app_;
      app_ = nullptr;
    }
  }

  App* app_ = nullptr;
  ConsentInfo* consent_info_ = nullptr;
};

TEST_F(UmpTest, GetInstanceOverloadsAndAppBinding) {
  EXPECT_EQ(consent_info_->app(), app_);
  EXPECT_EQ(ConsentInfo::GetInstance(), consent_info_);
  EXPECT_EQ(ConsentInfo::GetInstance(nullptr), consent_info_);
  EXPECT_EQ(ConsentInfo::GetInstance(app_), consent_info_);

  InitResult result = kInitResultFailedMissingDependency;
  EXPECT_EQ(ConsentInfo::GetInstance(app_, &result), consent_info_);
  EXPECT_EQ(result, kInitResultSuccess);

  InitResult null_result = kInitResultSuccess;
  App* null_app = nullptr;
  EXPECT_EQ(ConsentInfo::GetInstance(null_app, &null_result), nullptr);
  EXPECT_EQ(null_result, kInitResultFailedMissingDependency);
}

TEST_F(UmpTest, AppDestructionCleansUpConsentInfoSafely) {
  delete app_;
  app_ = nullptr;

  // After owning App is destroyed, ConsentInfo methods should be safe no-ops.
  EXPECT_EQ(consent_info_->app(), nullptr);
  EXPECT_EQ(consent_info_->GetConsentStatus(), kConsentStatusUnknown);
  EXPECT_EQ(consent_info_->GetConsentFormStatus(), kConsentFormStatusUnknown);
  EXPECT_FALSE(consent_info_->CanRequestAds());
}

TEST_F(UmpTest, ParameterStructDefaultsAndEquality) {
  ConsentRequestParameters p1;
  ConsentRequestParameters p2;
  EXPECT_FALSE(p1.tag_for_under_age_of_consent);
  EXPECT_EQ(p1.debug_settings.debug_geography, kConsentDebugGeographyDisabled);
  EXPECT_EQ(p1, p2);

  p2.debug_settings.debug_geography = kConsentDebugGeographyRegulatedUSState;
  EXPECT_NE(p1, p2);
}

TEST_F(UmpTest, EeaAndRegulatedUsStateConsentLifecycle) {
  EXPECT_EQ(consent_info_->GetConsentStatus(), kConsentStatusUnknown);
  EXPECT_EQ(consent_info_->GetConsentFormStatus(), kConsentFormStatusUnknown);
  EXPECT_EQ(consent_info_->GetPrivacyOptionsRequirementStatus(),
            kPrivacyOptionsRequirementStatusUnknown);
  EXPECT_FALSE(consent_info_->CanRequestAds());

  // ShowConsentForm before loading must fail with Unavailable.
  Future<void> premature_show = consent_info_->ShowConsentForm();
  EXPECT_EQ(premature_show.status(), kFutureStatusComplete);
  EXPECT_EQ(premature_show.error(), kConsentFormErrorUnavailable);

  ConsentRequestParameters params;
  params.debug_settings.debug_geography = kConsentDebugGeographyEEA;
  Future<void> req = consent_info_->RequestConsentInfoUpdate(params);
  EXPECT_EQ(req.status(), kFutureStatusComplete);
  EXPECT_EQ(req.error(), kConsentRequestSuccess);
  EXPECT_EQ(consent_info_->GetConsentStatus(), kConsentStatusRequired);
  EXPECT_EQ(consent_info_->GetConsentFormStatus(), kConsentFormStatusAvailable);
  EXPECT_FALSE(consent_info_->CanRequestAds());

  Future<void> load = consent_info_->LoadConsentForm();
  EXPECT_EQ(load.status(), kFutureStatusComplete);
  EXPECT_EQ(load.error(), kConsentFormSuccess);

  Future<void> show = consent_info_->ShowConsentForm();
  EXPECT_EQ(show.status(), kFutureStatusComplete);
  EXPECT_EQ(show.error(), kConsentFormSuccess);
  EXPECT_EQ(consent_info_->GetConsentStatus(), kConsentStatusObtained);
  EXPECT_TRUE(consent_info_->CanRequestAds());
  EXPECT_EQ(consent_info_->GetPrivacyOptionsRequirementStatus(),
            kPrivacyOptionsRequirementStatusRequired);

  // Showing a second time without reloading must fail with Unavailable.
  Future<void> second_show = consent_info_->ShowConsentForm();
  EXPECT_EQ(second_show.status(), kFutureStatusComplete);
  EXPECT_EQ(second_show.error(), kConsentFormErrorUnavailable);

  // Reset clears all states back to Unknown.
  consent_info_->Reset();
  EXPECT_EQ(consent_info_->GetConsentStatus(), kConsentStatusUnknown);
  EXPECT_EQ(consent_info_->GetConsentFormStatus(), kConsentFormStatusUnknown);
  EXPECT_EQ(consent_info_->GetPrivacyOptionsRequirementStatus(),
            kPrivacyOptionsRequirementStatusUnknown);
  EXPECT_FALSE(consent_info_->CanRequestAds());
}

}  // namespace
}  // namespace ump
}  // namespace firebase
