// Copyright 2021 Google LLC. All rights reserved.
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
#include <map>
#include <vector>

#include "app_framework.h"  // NOLINT
#include "firebase/app.h"
#include "firebase/ump.h"
#include "firebase/util.h"
#include "firebase_test_framework.h"  // NOLINT

#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// includes for phone-only tests.
#include <pthread.h>
#include <semaphore.h>
#endif  // defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)

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

// Sample test device IDs to use in making the request.
// You can replace these with actual device IDs for UMP tests
// to work on hardware devices.
const std::vector<std::string> kTestDeviceIDs = {
    "2077ef9a63d2b398840261c8221a0c9b", "098fe087d987c9a878965454a65654d7"};

using app_framework::LogDebug;
using app_framework::LogInfo;
using app_framework::LogWarning;
using app_framework::ProcessEvents;

using firebase_test_framework::FirebaseTest;

using testing::AnyOf;
using testing::Contains;
using testing::ElementsAre;
using testing::Eq;
using testing::HasSubstr;
using testing::Pair;
using testing::Property;

class FirebaseUmpTest : public FirebaseTest {
 public:
  FirebaseUmpTest() : consent_info_(nullptr) {}

  // Whether to call ConsentInfo::Reset() upon initialization, which
  // resets UMP's consent state to as if the app was first installed.
  enum ResetOption { kReset, kNoReset };

  void InitializeUmp(ResetOption reset = kReset);
  void TerminateUmp(ResetOption reset = kReset);

  static void SetUpTestSuite();
  static void TearDownTestSuite();

  void SetUp() override;
  void TearDown() override;

 protected:
  static firebase::App* shared_app_;
  firebase::ump::ConsentInfo* consent_info_;
};

firebase::App* FirebaseUmpTest::shared_app_ = nullptr;

void FirebaseUmpTest::SetUpTestSuite() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(ANDROID)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(ANDROID)
}

void FirebaseUmpTest::TearDownTestSuite() {
  LogDebug("Shutdown Firebase App.");
  delete shared_app_;
  shared_app_ = nullptr;
}

void FirebaseUmpTest::InitializeUmp(ResetOption reset) {
  using firebase::ump::ConsentInfo;
  firebase::InitResult result;
  consent_info_ = ConsentInfo::GetInstance(*shared_app_, &result);

  EXPECT_NE(consent_info_, nullptr);
  EXPECT_EQ(result, firebase::kInitResultSuccess);

  if (consent_info_ != nullptr && reset == kReset) {
    consent_info_->Reset();
  }
}

void FirebaseUmpTest::TerminateUmp(ResetOption reset) {
  if (consent_info_) {
    if (reset == kReset) {
      consent_info_->Reset();
    }
    delete consent_info_;
    consent_info_ = nullptr;
  }
}

void FirebaseUmpTest::SetUp() {
  InitializeUmp();
  ASSERT_NE(consent_info_, nullptr);
}

void FirebaseUmpTest::TearDown() { TerminateUmp(); }

// Tests for User Messaging Platform
TEST_F(FirebaseUmpTest, TestUmpInitialization) {
  // Initialize handled automatically in test setup.
  EXPECT_NE(consent_info_, nullptr);
  // Terminate handled automatically in test teardown.
}

// Tests for User Messaging Platform
TEST_F(FirebaseUmpTest, TestUmpDefaultsToUnknownStatus) {
  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusUnknown);
  EXPECT_EQ(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusUnknown);
  EXPECT_EQ(consent_info_->GetPrivacyOptionsRequirementStatus(),
            firebase::ump::kPrivacyOptionsRequirementStatusUnknown);
  EXPECT_FALSE(consent_info_->CanRequestAds());
}

// Tests for User Messaging Platform
TEST_F(FirebaseUmpTest, TestUmpGetInstanceIsAlwaysEqual) {
  using firebase::ump::ConsentInfo;

  EXPECT_NE(consent_info_, nullptr);

  // Ensure that GetInstance() with any options is always equal.
  EXPECT_EQ(consent_info_, ConsentInfo::GetInstance());
  EXPECT_EQ(consent_info_, ConsentInfo::GetInstance(*shared_app_));

#if defined(ANDROID)
  EXPECT_EQ(consent_info_,
            ConsentInfo::GetInstance(app_framework::GetJniEnv(),
                                     app_framework::GetActivity()));

  firebase::App* second_app = firebase::App::Create(
      firebase::AppOptions(), "2ndApp", app_framework::GetJniEnv(),
      app_framework::GetActivity());
#else
  firebase::App* second_app =
      firebase::App::Create(firebase::AppOptions(), "2ndApp");
#endif  // defined(ANDROID)

  EXPECT_EQ(consent_info_, ConsentInfo::GetInstance(*second_app));

  delete second_app;
}

TEST_F(FirebaseUmpTest, TestUmpRequestConsentInfoUpdate) {
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  EXPECT_TRUE(future == consent_info_->RequestConsentInfoUpdateLastResult());

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  EXPECT_NE(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusUnknown);
  EXPECT_NE(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusUnknown);
  EXPECT_NE(consent_info_->GetPrivacyOptionsRequirementStatus(),
            firebase::ump::kPrivacyOptionsRequirementStatusUnknown);
}

TEST_F(FirebaseUmpTest, TestUmpRequestConsentInfoUpdateDebugEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);
}

TEST_F(FirebaseUmpTest, TestUmpRequestConsentInfoUpdateDebugNonEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  EXPECT_THAT(consent_info_->GetConsentStatus(),
              AnyOf(Eq(firebase::ump::kConsentStatusNotRequired),
                    Eq(firebase::ump::kConsentStatusRequired)));
}

TEST_F(FirebaseUmpTest, TestUmpLoadForm) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);

  EXPECT_EQ(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusAvailable);

  // Load the form. Run this step with retry in case of network timeout.
  WaitForCompletion(
      RunWithRetry([&]() { return consent_info_->LoadConsentForm(); }),
      "LoadConsentForm",
      {firebase::ump::kConsentFormSuccess,
       firebase::ump::kConsentFormErrorTimeout});

  firebase::Future<void> future = consent_info_->LoadConsentFormLastResult();

  EXPECT_EQ(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusAvailable);

  if (future.error() == firebase::ump::kConsentFormErrorTimeout) {
    LogWarning("Timed out after multiple tries, but passing anyway.");
  }
}

TEST_F(FirebaseUmpTest, TestUmpShowForm) {
  TEST_REQUIRES_USER_INTERACTION;

  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);

  EXPECT_EQ(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusAvailable);

  WaitForCompletion(consent_info_->LoadConsentForm(), "LoadConsentForm");

  EXPECT_EQ(consent_info_->GetConsentFormStatus(),
            firebase::ump::kConsentFormStatusAvailable);

  firebase::Future<void> future =
      consent_info_->ShowConsentForm(app_framework::GetWindowController());

  EXPECT_TRUE(future == consent_info_->ShowConsentFormLastResult());

  WaitForCompletion(future, "ShowConsentForm");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusObtained);
}

TEST_F(FirebaseUmpTest, TestUmpLoadFormUnderAgeOfConsent) {
  SKIP_TEST_ON_IOS_SIMULATOR;

  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = true;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  firebase::Future<void> load_future = consent_info_->LoadConsentForm();
  WaitForCompletion(load_future, "LoadConsentForm",
                    {firebase::ump::kConsentFormErrorUnavailable,
                     firebase::ump::kConsentFormErrorTimeout,
                     firebase::ump::kConsentFormSuccess});
}

TEST_F(FirebaseUmpTest, TestUmpLoadFormUnavailableDebugNonEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  if (consent_info_->GetConsentStatus() !=
      firebase::ump::kConsentStatusRequired) {
    WaitForCompletion(consent_info_->LoadConsentForm(), "LoadConsentForm",
                      firebase::ump::kConsentFormErrorUnavailable);
  }
}

TEST_F(FirebaseUmpTest, TestUmpLoadAndShowIfRequiredDebugNonEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future =
      consent_info_->RequestConsentInfoUpdate(params);

  WaitForCompletion(future, "RequestConsentInfoUpdate",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});
  // Retry only network errors.
  EXPECT_NE(future.error(), firebase::ump::kConsentRequestErrorNetwork);

  FLAKY_TEST_SECTION_END();

  EXPECT_THAT(consent_info_->GetConsentStatus(),
              AnyOf(Eq(firebase::ump::kConsentStatusNotRequired),
                    Eq(firebase::ump::kConsentStatusRequired)));

  if (consent_info_->GetConsentStatus() ==
          firebase::ump::kConsentStatusNotRequired ||
      ShouldRunUITests()) {
    // If ConsentStatus is Required, we only want to do this next part if UI
    // interaction is allowed, as it will show a consent form which won't work
    // in automated testing.
    firebase::Future<void> future =
        consent_info_->LoadAndShowConsentFormIfRequired(
            app_framework::GetWindowController());

    EXPECT_TRUE(future ==
                consent_info_->LoadAndShowConsentFormIfRequiredLastResult());

    WaitForCompletion(future, "LoadAndShowConsentFormIfRequired");
  }
}

TEST_F(FirebaseUmpTest, TestUmpLoadAndShowIfRequiredDebugEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  TEST_REQUIRES_USER_INTERACTION;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);

  firebase::Future<void> future =
      consent_info_->LoadAndShowConsentFormIfRequired(
          app_framework::GetWindowController());

  EXPECT_TRUE(future ==
              consent_info_->LoadAndShowConsentFormIfRequiredLastResult());

  WaitForCompletion(future, "LoadAndShowConsentFormIfRequired");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusObtained);
}

TEST_F(FirebaseUmpTest, TestUmpPrivacyOptions) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;
  using firebase::ump::PrivacyOptionsRequirementStatus;

  TEST_REQUIRES_USER_INTERACTION;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);

  EXPECT_FALSE(consent_info_->CanRequestAds());

  WaitForCompletion(consent_info_->LoadAndShowConsentFormIfRequired(
                        app_framework::GetWindowController()),
                    "LoadAndShowConsentFormIfRequired");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusObtained);

  EXPECT_TRUE(consent_info_->CanRequestAds()) << "After consent obtained";

  LogInfo(
      "******** On the Privacy Options screen that is about to appear, please "
      "select DO NOT CONSENT.");

  ProcessEvents(5000);

  EXPECT_EQ(consent_info_->GetPrivacyOptionsRequirementStatus(),
            firebase::ump::kPrivacyOptionsRequirementStatusRequired);

  firebase::Future<void> future = consent_info_->ShowPrivacyOptionsForm(
      app_framework::GetWindowController());

  EXPECT_TRUE(future == consent_info_->ShowPrivacyOptionsFormLastResult());

  WaitForCompletion(future, "ShowPrivacyOptionsForm");
}

TEST_F(FirebaseUmpTest, TestCanRequestAdsNonEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_THAT(consent_info_->GetConsentStatus(),
              AnyOf(Eq(firebase::ump::kConsentStatusNotRequired),
                    Eq(firebase::ump::kConsentStatusRequired)));

  if (consent_info_->GetConsentStatus() ==
      firebase::ump::kConsentStatusNotRequired) {
    EXPECT_TRUE(consent_info_->CanRequestAds());
  }
}

TEST_F(FirebaseUmpTest, TestCanRequestAdsEEA) {
  using firebase::ump::ConsentDebugSettings;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  EXPECT_EQ(consent_info_->GetConsentStatus(),
            firebase::ump::kConsentStatusRequired);

  EXPECT_FALSE(consent_info_->CanRequestAds());
}

TEST_F(FirebaseUmpTest, TestUmpCleanupWithDelay) {
  // Ensure that if ConsentInfo is deleted after a delay, Futures are
  // properly invalidated.
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future_request =
      consent_info_->RequestConsentInfoUpdate(params);
  firebase::Future<void> future_load = consent_info_->LoadConsentForm();
  firebase::Future<void> future_show =
      consent_info_->ShowConsentForm(app_framework::GetWindowController());
  firebase::Future<void> future_load_and_show =
      consent_info_->LoadAndShowConsentFormIfRequired(
          app_framework::GetWindowController());
  firebase::Future<void> future_privacy = consent_info_->ShowPrivacyOptionsForm(
      app_framework::GetWindowController());

  ProcessEvents(5000);

  TerminateUmp(kNoReset);

  EXPECT_EQ(future_request.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_load.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_show.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_load_and_show.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_privacy.status(), firebase::kFutureStatusInvalid);
}

TEST_F(FirebaseUmpTest, TestUmpCleanupRaceCondition) {
  // Ensure that if ConsentInfo is deleted immediately, operations
  // (and their Futures) are properly invalidated.

  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future_request =
      consent_info_->RequestConsentInfoUpdate(params);
  firebase::Future<void> future_load = consent_info_->LoadConsentForm();
  firebase::Future<void> future_show =
      consent_info_->ShowConsentForm(app_framework::GetWindowController());
  firebase::Future<void> future_load_and_show =
      consent_info_->LoadAndShowConsentFormIfRequired(
          app_framework::GetWindowController());
  firebase::Future<void> future_privacy = consent_info_->ShowPrivacyOptionsForm(
      app_framework::GetWindowController());

  TerminateUmp(kNoReset);

  EXPECT_EQ(future_request.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_load.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_show.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_load_and_show.status(), firebase::kFutureStatusInvalid);
  EXPECT_EQ(future_privacy.status(), firebase::kFutureStatusInvalid);

  ProcessEvents(5000);
}

TEST_F(FirebaseUmpTest, TestUmpCallbacksOnWrongInstance) {
  // Ensure that if ConsentInfo is deleted and then recreated, stale
  // callbacks don't call into the new instance and cause crashes.
  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  LogDebug("RequestConsentInfoUpdate");
  consent_info_->RequestConsentInfoUpdate(params).OnCompletion(
      [](const firebase::Future<void>&) {
        LogDebug("RequestConsentInfoUpdate done");
      });
  LogDebug("LoadConsentForm");
  consent_info_->LoadConsentForm().OnCompletion(
      [](const firebase::Future<void>&) { LogDebug("LoadConsentForm done"); });
  // In automated tests, only check RequestConsentInfoUpdate and LoadConsentForm
  // as the rest may show UI.
  if (ShouldRunUITests()) {
    LogDebug("ShowConsentForm");
    consent_info_->ShowConsentForm(app_framework::GetWindowController())
        .OnCompletion([](const firebase::Future<void>&) {
          LogDebug("ShowConsentForm done");
        });
    LogDebug("LoadAndShowConsentFormIfRequired");
    consent_info_
        ->LoadAndShowConsentFormIfRequired(app_framework::GetWindowController())
        .OnCompletion([](const firebase::Future<void>&) {
          LogDebug("LoadAndShowConsentFormIfRequired done");
        });
    LogDebug("ShowPrivacyOptionsForm");
    consent_info_->ShowPrivacyOptionsForm(app_framework::GetWindowController())
        .OnCompletion([](const firebase::Future<void>&) {
          LogDebug("ShowPrivacyOptionsForm done");
        });
  }

  LogDebug("Terminate");
  TerminateUmp(kNoReset);

  LogDebug("Initialize");
  InitializeUmp(kNoReset);

  // Give the operations time to complete.
  LogDebug("Wait");
  ProcessEvents(5000);

  LogDebug("Done");
}

TEST_F(FirebaseUmpTest, TestUmpMethodsReturnOperationInProgress) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_IOS_SIMULATOR;  // LoadAndShowConsentFormIfRequired
                               // is too quick on simulator.

  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  // Check that all of the UMP operations properly return an OperationInProgress
  // error if called more than once at the same time.

  // This depends on timing, so it's inherently flaky.
  FLAKY_TEST_SECTION_BEGIN();

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyNonEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future_request_1 =
      consent_info_->RequestConsentInfoUpdate(params);
  firebase::Future<void> future_request_2 =
      consent_info_->RequestConsentInfoUpdate(params);
  WaitForCompletion(future_request_2, "RequestConsentInfoUpdate second",
                    firebase::ump::kConsentRequestErrorOperationInProgress);
  WaitForCompletion(future_request_1, "RequestConsentInfoUpdate first",
                    {firebase::ump::kConsentRequestSuccess,
                     firebase::ump::kConsentRequestErrorNetwork});

  consent_info_->Reset();

  FLAKY_TEST_SECTION_END();
}

TEST_F(FirebaseUmpTest, TestUmpMethodsReturnOperationInProgressWithUI) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  using firebase::ump::ConsentFormStatus;
  using firebase::ump::ConsentRequestParameters;
  using firebase::ump::ConsentStatus;

  // Check that all of the UMP operations properly return an OperationInProgress
  // error if called more than once at the same time. This test include methods
  // with UI interaction.

  ConsentRequestParameters params;
  params.tag_for_under_age_of_consent = false;
  params.debug_settings.debug_geography =
      firebase::ump::kConsentDebugGeographyEEA;
  params.debug_settings.debug_device_ids = kTestDeviceIDs;
  params.debug_settings.debug_device_ids.push_back(GetDebugDeviceId());

  firebase::Future<void> future_request_1 =
      consent_info_->RequestConsentInfoUpdate(params);
  firebase::Future<void> future_request_2 =
      consent_info_->RequestConsentInfoUpdate(params);
  WaitForCompletion(future_request_2, "RequestConsentInfoUpdate second",
                    firebase::ump::kConsentRequestErrorOperationInProgress);
  WaitForCompletion(future_request_1, "RequestConsentInfoUpdate first");

  firebase::Future<void> future_load_1 = consent_info_->LoadConsentForm();
  firebase::Future<void> future_load_2 = consent_info_->LoadConsentForm();
  WaitForCompletion(future_load_2, "LoadConsentForm second",
                    firebase::ump::kConsentFormErrorOperationInProgress);
  WaitForCompletion(future_load_1, "LoadConsentForm first");

  firebase::Future<void> future_show_1 =
      consent_info_->ShowConsentForm(app_framework::GetWindowController());
  firebase::Future<void> future_show_2 =
      consent_info_->ShowConsentForm(app_framework::GetWindowController());
  WaitForCompletion(future_show_2, "ShowConsentForm second",
                    firebase::ump::kConsentFormErrorOperationInProgress);
  WaitForCompletion(future_show_1, "ShowConsentForm first");

  firebase::Future<void> future_privacy_1 =
      consent_info_->ShowPrivacyOptionsForm(
          app_framework::GetWindowController());
  firebase::Future<void> future_privacy_2 =
      consent_info_->ShowPrivacyOptionsForm(
          app_framework::GetWindowController());
  WaitForCompletion(future_privacy_2, "ShowPrivacyOptionsForm second",
                    firebase::ump::kConsentFormErrorOperationInProgress);
  WaitForCompletion(future_privacy_1, "ShowPrivacyOptionsForm first");

  consent_info_->Reset();
  // Request again so we can test LoadAndShowConsentFormIfRequired.
  WaitForCompletion(consent_info_->RequestConsentInfoUpdate(params),
                    "RequestConsentInfoUpdate");

  firebase::Future<void> future_load_and_show_1 =
      consent_info_->LoadAndShowConsentFormIfRequired(
          app_framework::GetWindowController());
  firebase::Future<void> future_load_and_show_2 =
      consent_info_->LoadAndShowConsentFormIfRequired(
          app_framework::GetWindowController());
  WaitForCompletion(future_load_and_show_2,
                    "LoadAndShowConsentFormIfRequired second",
                    firebase::ump::kConsentFormErrorOperationInProgress);
  WaitForCompletion(future_load_and_show_1,
                    "LoadAndShowConsentFormIfRequired first");
}

}  // namespace firebase_testapp_automated
