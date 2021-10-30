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
#include <map>
#include <vector>

#include "app_framework.h"  // NOLINT
#include "firebase/admob.h"
#include "firebase/admob/types.h"
#include "firebase/app.h"
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

// The AdMob app IDs for the test app.
#if defined(__ANDROID__)
// If you change the AdMob app ID for your Android app, make sure to change it
// in AndroidManifest.xml as well.
const char* kAdMobAppID = "ca-app-pub-3940256099942544~3347511713";
#else
// If you change the AdMob app ID for your iOS app, make sure to change the
// value for "GADApplicationIdentifier" in your Info.plist as well.
const char* kAdMobAppID = "ca-app-pub-3940256099942544~1458002511";
#endif

// These ad units IDs have been created specifically for testing, and will
// always return test ads.
#if defined(__ANDROID__)
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/6300978111";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/1033173712";
#else
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/2934735716";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/4411468910";
#endif

// Used in a test to send an errant ad unit id.
const char* kBadAdUnit = "oops";

// Standard Banner Size.
static const int kBannerWidth = 320;
static const int kBannerHeight = 50;

enum AdCallbackEvent {
  AdCallbackEventClicked = 0,
  AdCallbackEventClosed,
  AdCallbackEventAdImpression,
  AdCallbackEventOpened,
  AdCallbackEventPaidEvent
};

// Error domains vary across phone SDKs.
#if defined(__ANDROID__)
const char* kErrorDomain = "com.google.android.gms.ads";
#else
const char* kErrorDomain = "com.google.admob";
#endif

// Sample test device IDs to use in making the request.
const std::vector<std::string> kTestDeviceIDs = {
    "2077ef9a63d2b398840261c8221a0c9b", "098fe087d987c9a878965454a65654d7"};

// Sample keywords to use in making the request.
static const std::vector<std::string> kKeywords({"AdMob", "C++", "Fun"});

// "Extra" key value pairs can be added to the request as well. Typically
// these are used when testing new features.
static const std::map<std::string, std::string> kAdMobAdapterExtras = {
    {"the_name_of_an_extra", "the_value_for_that_extra"},
    {"heres", "a second example"}};

#if defined(__ANDROID__)
static const char* kAdNetworkExtrasClassName =
    "com/google/ads/mediation/admob/AdMobAdapter";
#else
static const char* kAdNetworkExtrasClassName = "GADExtras";
#endif

// Used to detect kAdMobErrorAdNetworkClassLoadErrors when loading
// ads.
static const char* kAdNetworkExtrasInvalidClassName = "abc123321cba";

static const char* kContentUrl = "http://www.firebase.com";

using app_framework::LogDebug;
using app_framework::ProcessEvents;

using firebase_test_framework::FirebaseTest;

class FirebaseAdMobTest : public FirebaseTest {
 public:
  FirebaseAdMobTest();
  ~FirebaseAdMobTest() override;

  static void SetUpTestSuite();
  static void TearDownTestSuite();

  void SetUp() override;
  void TearDown() override;

 protected:
  firebase::admob::AdRequest GetAdRequest();

  static firebase::App* shared_app_;
};

firebase::App* FirebaseAdMobTest::shared_app_ = nullptr;

void BrieflyPauseForVisualInspection() { app_framework::ProcessEvents(300); }

void FirebaseAdMobTest::SetUpTestSuite() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(__ANDROID__)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(__ANDROID__)

  LogDebug("Initializing AdMob.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(shared_app_, nullptr,
                         [](::firebase::App* app, void* /* userdata */) {
                           LogDebug("Try to initialize AdMob");
                           return ::firebase::admob::Initialize(*app);
                         });

  WaitForCompletion(initializer.InitializeLastResult(), "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized AdMob.");
}

void FirebaseAdMobTest::TearDownTestSuite() {
  // Workaround: AdMob does some of its initialization in the main
  // thread, so if you terminate it too quickly after initialization
  // it can cause issues.  Add a small delay here in case most of the
  // tests are skipped.
  ProcessEvents(1000);
  LogDebug("Shutdown AdMob.");
  firebase::admob::Terminate();
  LogDebug("Shutdown Firebase App.");
  delete shared_app_;
  shared_app_ = nullptr;
}

FirebaseAdMobTest::FirebaseAdMobTest() {}

FirebaseAdMobTest::~FirebaseAdMobTest() {}

void FirebaseAdMobTest::SetUp() {
  FirebaseTest::SetUp();

  // This example uses ad units that are specially configured to return test ads
  // for every request. When using your own ad unit IDs, however, it's important
  // to register the device IDs associated with any devices that will be used to
  // test the app. This ensures that regardless of the ad unit ID, those
  // devices will always receive test ads in compliance with AdMob policy.
  //
  // Device IDs can be obtained by checking the logcat or the Xcode log while
  // debugging. They appear as a long string of hex characters.
  firebase::admob::RequestConfiguration request_configuration;
  request_configuration.test_device_ids = kTestDeviceIDs;
  firebase::admob::SetRequestConfiguration(request_configuration);
}

void FirebaseAdMobTest::TearDown() { FirebaseTest::TearDown(); }

firebase::admob::AdRequest FirebaseAdMobTest::GetAdRequest() {
  firebase::admob::AdRequest request;

  // Additional keywords to be used in targeting.
  for (auto keyword_iter = kKeywords.begin(); keyword_iter != kKeywords.end();
       ++keyword_iter) {
    request.add_keyword((*keyword_iter).c_str());
  }

  for (auto extras_iter = kAdMobAdapterExtras.begin();
       extras_iter != kAdMobAdapterExtras.end(); ++extras_iter) {
    request.add_extra(kAdNetworkExtrasClassName, extras_iter->first.c_str(),
                      extras_iter->second.c_str());
  }

  request.set_content_url(kContentUrl);

  return request;
}

// Test cases below.
TEST_F(FirebaseAdMobTest, TestGetAdRequest) { GetAdRequest(); }

TEST_F(FirebaseAdMobTest, TestGetAdRequestValues) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdRequest request = GetAdRequest();

  // Content URL.
  EXPECT_TRUE(request.content_url() == std::string(kContentUrl));

  // Extras.
  std::map<std::string, std::map<std::string, std::string> > configured_extras =
      request.extras();

  EXPECT_EQ(configured_extras.size(), 1);
  for (auto extras_name_iter = configured_extras.begin();
       extras_name_iter != configured_extras.end(); ++extras_name_iter) {
    // Confirm class name.
    const std::string class_name = extras_name_iter->first;
    EXPECT_EQ(class_name, kAdNetworkExtrasClassName);

    // Grab the extras.
    const std::map<std::string, std::string>& configured_extras =
        extras_name_iter->second;
    EXPECT_EQ(configured_extras.size(), kAdMobAdapterExtras.size());

    // Check the extra key value pairs.
    for (auto constant_extras_iter = kAdMobAdapterExtras.begin();
         constant_extras_iter != kAdMobAdapterExtras.end();
         ++constant_extras_iter) {
      // Ensure the configured value matches the constant for the same key.
      EXPECT_EQ(configured_extras.at(constant_extras_iter->first),
                constant_extras_iter->second);
    }
  }

  const std::unordered_set<std::string> configured_keywords =
      request.keywords();
  EXPECT_EQ(configured_keywords.size(), kKeywords.size());
  for (auto keyword_iter = kKeywords.begin(); keyword_iter != kKeywords.end();
       ++keyword_iter) {
    EXPECT_TRUE(configured_keywords.find(*keyword_iter) !=
                configured_keywords.end());
  }
}

class TestBoundingBoxListener
    : public firebase::admob::AdViewBoundingBoxListener {
 public:
  void OnBoundingBoxChanged(firebase::admob::AdView* ad_view,
                            firebase::admob::BoundingBox box) override {
    LogDebug("Bounding box changed[%d]", bounding_box_changes_.size() + 1);
    bounding_box_changes_.push_back(box);
  }
  std::vector<firebase::admob::BoundingBox> bounding_box_changes_;
};

// A simple listener to help test changes an Ad.
class TestAdListener : public firebase::admob::AdListener {
 public:
  TestAdListener()
      : num_on_ad_clicked_(0),
        num_on_ad_closed_(0),
        num_on_ad_impression_(0),
        num_on_ad_opened_(0) {}

  void OnAdClicked() override { num_on_ad_clicked_++; }
  void OnAdClosed() override { num_on_ad_closed_++; }
  void OnAdImpression() override { num_on_ad_impression_++; }
  void OnAdOpened() override { num_on_ad_opened_++; }

  int num_on_ad_clicked_;
  int num_on_ad_closed_;
  int num_on_ad_impression_;
  int num_on_ad_opened_;
};

class TestFullScreenContentListener
    : public firebase::admob::FullScreenContentListener {
 public:
  TestFullScreenContentListener()
      : num_on_ad_clicked_(0),
        num_on_ad_dismissed_full_screen_content_(0),
        num_on_ad_failed_to_show_full_screen_content_(0),
        num_on_ad_impression_(0),
        num_on_ad_showed_full_screen_content_(0) {}

  void OnAdClicked() override { num_on_ad_clicked_++; }

  void OnAdDismissedFullScreenContent() override {
    num_on_ad_dismissed_full_screen_content_++;
  }

  void OnAdFailedToShowFullScreenContent(
      const firebase::admob::AdResult& ad_result) override {
    num_on_ad_failed_to_show_full_screen_content_++;
  }

  void OnAdImpression() override { num_on_ad_impression_++; }

  void OnAdShowedFullScreenContent() override {
    num_on_ad_showed_full_screen_content_++;
  }

  int num_on_ad_clicked_;
  int num_on_ad_dismissed_full_screen_content_;
  int num_on_ad_failed_to_show_full_screen_content_;
  int num_on_ad_impression_;
  int num_on_ad_showed_full_screen_content_;
};

class TestPaidEventListener : public firebase::admob::PaidEventListener {
 public:
  TestPaidEventListener() : num_on_paid_event_(0) {}
  void OnPaidEvent(const firebase::admob::AdValue& value) override {
    ++num_on_paid_event_;
    // These are the values for AdMob test ads.  If they change then we should
    // alter the test to match the new expected values.
    EXPECT_EQ(value.currency_code(), "USD");
    EXPECT_EQ(value.precision_type(),
              firebase::admob::AdValue::kdValuePrecisionUnknown);
    EXPECT_EQ(value.value_micros(), 0);
  }
  int num_on_paid_event_;
};

TEST_F(FirebaseAdMobTest, TestAdSize) {
  uint32_t kWidth = 50;
  uint32_t kHeight = 10;

  using firebase::admob::AdSize;

  const AdSize adaptive_landscape =
      AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(kWidth);
  EXPECT_EQ(adaptive_landscape.width(), kWidth);
  EXPECT_EQ(adaptive_landscape.height(), 0);
  EXPECT_EQ(adaptive_landscape.type(), AdSize::kTypeAnchoredAdaptive);
  EXPECT_EQ(adaptive_landscape.orientation(), AdSize::kOrientationLandscape);

  const AdSize adaptive_portrait =
      AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(kWidth);
  EXPECT_EQ(adaptive_portrait.width(), kWidth);
  EXPECT_EQ(adaptive_portrait.height(), 0);
  EXPECT_EQ(adaptive_portrait.type(), AdSize::kTypeAnchoredAdaptive);
  EXPECT_EQ(adaptive_portrait.orientation(), AdSize::kOrientationPortrait);

  EXPECT_FALSE(adaptive_portrait == adaptive_landscape);
  EXPECT_TRUE(adaptive_portrait != adaptive_landscape);

  const firebase::admob::AdSize adaptive_current =
      AdSize::GetCurrentOrientationAnchoredAdaptiveBannerAdSize(kWidth);
  EXPECT_EQ(adaptive_current.width(), kWidth);
  EXPECT_EQ(adaptive_current.height(), 0);
  EXPECT_EQ(adaptive_current.type(), AdSize::kTypeAnchoredAdaptive);
  EXPECT_EQ(adaptive_current.orientation(), AdSize::kOrientationCurrent);

  const firebase::admob::AdSize custom_ad_size(kWidth, kHeight);
  EXPECT_EQ(custom_ad_size.width(), kWidth);
  EXPECT_EQ(custom_ad_size.height(), kHeight);
  EXPECT_EQ(custom_ad_size.type(), AdSize::kTypeStandard);
  EXPECT_EQ(custom_ad_size.orientation(), AdSize::kOrientationCurrent);

  const AdSize custom_ad_size_2(kWidth, kHeight);
  EXPECT_TRUE(custom_ad_size == custom_ad_size_2);
  EXPECT_FALSE(custom_ad_size != custom_ad_size_2);

  const AdSize banner = AdSize::kBanner;
  EXPECT_EQ(banner.width(), 320);
  EXPECT_EQ(banner.height(), 50);
  EXPECT_EQ(banner.type(), AdSize::kTypeStandard);
  EXPECT_EQ(banner.orientation(), AdSize::kOrientationCurrent);

  const AdSize fullbanner = AdSize::kFullBanner;
  EXPECT_EQ(fullbanner.width(), 468);
  EXPECT_EQ(fullbanner.height(), 60);
  EXPECT_EQ(fullbanner.type(), AdSize::kTypeStandard);
  EXPECT_EQ(fullbanner.orientation(), AdSize::kOrientationCurrent);

  const AdSize leaderboard = AdSize::kLeaderBoard;
  EXPECT_EQ(leaderboard.width(), 728);
  EXPECT_EQ(leaderboard.height(), 90);
  EXPECT_EQ(leaderboard.type(), AdSize::kTypeStandard);
  EXPECT_EQ(leaderboard.orientation(), AdSize::kOrientationCurrent);

  const AdSize medium_rectangle = AdSize::kMediumRectangle;
  EXPECT_EQ(medium_rectangle.width(), 300);
  EXPECT_EQ(medium_rectangle.height(), 250);
  EXPECT_EQ(medium_rectangle.type(), AdSize::kTypeStandard);
  EXPECT_EQ(medium_rectangle.orientation(), AdSize::kOrientationCurrent);
}

TEST_F(FirebaseAdMobTest, TestRequestConfigurationSetGetEmptyConfig) {
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::RequestConfiguration set_configuration;
  firebase::admob::SetRequestConfiguration(set_configuration);
  firebase::admob::RequestConfiguration retrieved_configuration =
      firebase::admob::GetRequestConfiguration();

  EXPECT_EQ(
      retrieved_configuration.max_ad_content_rating,
      firebase::admob::RequestConfiguration::kMaxAdContentRatingUnspecified);
  EXPECT_EQ(retrieved_configuration.tag_for_child_directed_treatment,
            firebase::admob::RequestConfiguration::
                kChildDirectedTreatmentUnspecified);
  EXPECT_EQ(
      retrieved_configuration.tag_for_under_age_of_consent,
      firebase::admob::RequestConfiguration::kUnderAgeOfConsentUnspecified);
  EXPECT_EQ(retrieved_configuration.test_device_ids.size(), 0);
}

TEST_F(FirebaseAdMobTest, TestRequestConfigurationSetGet) {
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::RequestConfiguration set_configuration;
  set_configuration.max_ad_content_rating =
      firebase::admob::RequestConfiguration::kMaxAdContentRatingPG;
  set_configuration.tag_for_child_directed_treatment =
      firebase::admob::RequestConfiguration::kChildDirectedTreatmentTrue;
  set_configuration.tag_for_under_age_of_consent =
      firebase::admob::RequestConfiguration::kUnderAgeOfConsentFalse;
  set_configuration.test_device_ids.push_back("1");
  set_configuration.test_device_ids.push_back("2");
  set_configuration.test_device_ids.push_back("3");
  firebase::admob::SetRequestConfiguration(set_configuration);

  firebase::admob::RequestConfiguration retrieved_configuration =
      firebase::admob::GetRequestConfiguration();

  EXPECT_EQ(retrieved_configuration.max_ad_content_rating,
            firebase::admob::RequestConfiguration::kMaxAdContentRatingPG);

#if defined(__ANDROID__)
  EXPECT_EQ(retrieved_configuration.tag_for_child_directed_treatment,
            firebase::admob::RequestConfiguration::kChildDirectedTreatmentTrue);
  EXPECT_EQ(retrieved_configuration.tag_for_under_age_of_consent,
            firebase::admob::RequestConfiguration::kUnderAgeOfConsentFalse);
#else  // iOS
  // iOS doesn't allow for the querying of these values.
  EXPECT_EQ(retrieved_configuration.tag_for_child_directed_treatment,
            firebase::admob::RequestConfiguration::
                kChildDirectedTreatmentUnspecified);
  EXPECT_EQ(
      retrieved_configuration.tag_for_under_age_of_consent,
      firebase::admob::RequestConfiguration::kUnderAgeOfConsentUnspecified);
#endif

  EXPECT_EQ(retrieved_configuration.test_device_ids.size(), 3);
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "1"));
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "2"));
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "3"));
}

TEST_F(FirebaseAdMobTest, TestBannerView) {
  // AdMob cannot be tested on Firebase Test Lab, so disable tests on FTL.
  TEST_REQUIRES_USER_INTERACTION;
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Set the listener.
  TestBoundingBoxListener bounding_box_listener;
  banner->SetBoundingBoxListener(&bounding_box_listener);
  int expected_num_bounding_box_changes = 0;
  EXPECT_EQ(expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Load the banner ad.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> load_ad_future =
      banner->LoadAd(request);
  WaitForCompletion(load_ad_future, "LoadAd");
  EXPECT_EQ(expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());
  const firebase::admob::LoadAdResult* result_ptr = load_ad_future.result();
  EXPECT_TRUE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->code(), firebase::admob::kAdMobErrorNone);
  EXPECT_TRUE(result_ptr->message().empty());
  EXPECT_TRUE(result_ptr->domain().empty());
  EXPECT_TRUE(result_ptr->ToString().empty());
  const firebase::admob::ResponseInfo response_info =
      result_ptr->response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  load_ad_future.Release();

  // Make the BannerView visible.
  WaitForCompletion(banner->Show(), "Show 0");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Move to each of the six pre-defined positions.
  WaitForCompletion(banner->SetPosition(firebase::admob::AdView::kPositionTop),
                    "SetPosition(Top)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionTop);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(
      banner->SetPosition(firebase::admob::AdView::kPositionTopLeft),
      "SetPosition(TopLeft)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionTopLeft);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(
      banner->SetPosition(firebase::admob::AdView::kPositionTopRight),
      "SetPosition(TopRight)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionTopRight);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(
      banner->SetPosition(firebase::admob::AdView::kPositionBottom),
      "SetPosition(Bottom)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionBottom);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(
      banner->SetPosition(firebase::admob::AdView::kPositionBottomLeft),
      "SetPosition(BottomLeft)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionBottomLeft);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(
      banner->SetPosition(firebase::admob::AdView::kPositionBottomRight),
      "SetPosition(BottomRight)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionBottomRight);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Move to some coordinates.
  WaitForCompletion(banner->SetPosition(100, 300), "SetPosition(x0, y0)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionUndefined);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(banner->SetPosition(100, 400), "SetPosition(x1, y1)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionUndefined);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Try hiding and showing the BannerView.
  WaitForCompletion(banner->Hide(), "Hide 1");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(banner->Show(), "Show 1");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Move again after hiding/showing.
  WaitForCompletion(banner->SetPosition(100, 300), "SetPosition(x2, y2)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionUndefined);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(banner->SetPosition(100, 400), "SetPosition(x3, y3)");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(banner->bounding_box().position,
            firebase::admob::AdView::kPositionUndefined);
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  WaitForCompletion(banner->Hide(), "Hide 2");
  BrieflyPauseForVisualInspection();
  EXPECT_EQ(expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
  app_framework::ProcessEvents(2000);

  WaitForCompletion(banner->Destroy(), "Destroy BannerView");
  banner->SetBoundingBoxListener(nullptr);
  delete banner;

  BrieflyPauseForVisualInspection();
  EXPECT_EQ(++expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

#if defined(__ANDROID__) || TARGET_OS_IPHONE
  // As an extra check, all bounding boxes except the last should have the same
  // size aspect ratio that we requested. For example if you requested a 320x50
  // banner, you can get one with the size 960x150. Use EXPECT_NEAR because the
  // calculation can have a small bit of error.
  double kAspectRatioAllowedError = 0.02;  // Allow about 2% of error.
  double expected_aspect_ratio =
      static_cast<double>(kBannerWidth) / static_cast<double>(kBannerHeight);
  for (int i = 0; i < bounding_box_listener.bounding_box_changes_.size() - 1;
       ++i) {
    double actual_aspect_ratio =
        static_cast<double>(
            bounding_box_listener.bounding_box_changes_[i].width) /
        static_cast<double>(
            bounding_box_listener.bounding_box_changes_[i].height);
    EXPECT_NEAR(actual_aspect_ratio, expected_aspect_ratio,
                kAspectRatioAllowedError)
        << "Banner size "
        << bounding_box_listener.bounding_box_changes_[i].width << "x"
        << bounding_box_listener.bounding_box_changes_[i].height
        << " does not have the same aspect ratio as requested size "
        << kBannerWidth << "x" << kBannerHeight << ".";
  }

  // And finally, the last bounding box change, when the banner is deleted,
  // should have invalid values (-1,-1, -1, -1).
  EXPECT_TRUE(bounding_box_listener.bounding_box_changes_.back().x == -1 &&
              bounding_box_listener.bounding_box_changes_.back().y == -1 &&
              bounding_box_listener.bounding_box_changes_.back().width == -1 &&
              bounding_box_listener.bounding_box_changes_.back().height == -1);
#endif
}

TEST_F(FirebaseAdMobTest, TestBannerViewStress) {
  // AdMob cannot be tested on Firebase Test Lab, so disable tests on FTL.
  TEST_REQUIRES_USER_INTERACTION;
  SKIP_TEST_ON_DESKTOP;

  for (int i = 0; i < 10; ++i) {
    const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
    firebase::admob::BannerView* banner = new firebase::admob::BannerView();
    WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                         kBannerAdUnit, banner_ad_size),
                      "TestBannerViewStress Initialize");

    // Load the banner ad.
    firebase::admob::AdRequest request = GetAdRequest();
    WaitForCompletion(banner->LoadAd(request), "TestBannerViewStress LoadAd");
    WaitForCompletion(banner->Destroy(), "Destroy BannerView");
    delete banner;
  }
}

TEST_F(FirebaseAdMobTest, TestBannerViewAdOpenedAdClosed) {
  SKIP_TEST_ON_DESKTOP;
  TEST_REQUIRES_USER_INTERACTION;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Set the listener.
  TestAdListener ad_listener;
  banner->SetAdListener(&ad_listener);

  TestPaidEventListener paid_event_listener;
  banner->SetPaidEventListener(&paid_event_listener);

  // Load the banner ad.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> load_ad_future =
      banner->LoadAd(request);
  WaitForCompletion(load_ad_future, "LoadAd");
  WaitForCompletion(banner->Show(), "Show 0");

  // Ad Events differ per platform. See the following for more info:
  // https://www.googblogs.com/google-mobile-ads-sdk-a-note-on-ad-click-events/
  // and https://groups.google.com/g/google-admob-ads-sdk/c/lzdt5szxSVU
#if defined(ANDROID)
  LogDebug("Click the Ad, and then close the ad to continue");

  while (ad_listener.num_on_ad_opened_ == 0) {
    app_framework::ProcessEvents(1000);
  }

  while (ad_listener.num_on_ad_closed_ == 0) {
    app_framework::ProcessEvents(1000);
  }

  // Ensure all of the expected events were triggered on Android.
  EXPECT_EQ(ad_listener.num_on_ad_clicked_, 1);
  EXPECT_EQ(ad_listener.num_on_ad_impression_, 1);
  EXPECT_EQ(ad_listener.num_on_ad_opened_, 1);
  EXPECT_EQ(ad_listener.num_on_ad_closed_, 1);
  EXPECT_EQ(paid_event_listener.num_on_paid_event_, 1);
#else
  LogDebug("Click the Ad to continue");

  while (ad_listener.num_on_ad_clicked_ == 0) {
    app_framework::ProcessEvents(1000);
  }

  LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
  app_framework::ProcessEvents(3000);

  // Ensure all of the expected events were triggered on iOS.
  EXPECT_EQ(ad_listener.num_on_ad_clicked_, 1);
  EXPECT_EQ(ad_listener.num_on_ad_impression_, 1);
  EXPECT_EQ(paid_event_listener.num_on_paid_event_, 1);
  EXPECT_EQ(ad_listener.num_on_ad_opened_, 0);
  EXPECT_EQ(ad_listener.num_on_ad_closed_, 0);
#endif

  load_ad_future.Release();
  banner->SetAdListener(nullptr);
  banner->SetPaidEventListener(nullptr);
  WaitForCompletion(banner->Destroy(), "Destroy BannerView");
  delete banner;
}

TEST_F(FirebaseAdMobTest, TestBannerViewErrorAlreadyInitialized) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);

  for (int i = 0; i < 5; ++i) {
    firebase::admob::BannerView* banner = new firebase::admob::BannerView();
    firebase::Future<void> first_initialize = banner->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);
    firebase::Future<void> second_initialize = banner->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);

    WaitForCompletionAnyResult(second_initialize, "Second Initialize");
    WaitForCompletionAnyResult(first_initialize, "First Initialize");

    LogDebug("first_initialize.error(): %d", first_initialize.error());
    LogDebug("second_initialize.error(): %d", second_initialize.error());

    if (first_initialize.error() == firebase::admob::kAdMobErrorNone) {
      EXPECT_EQ(second_initialize.error(),
                firebase::admob::kAdMobErrorAlreadyInitialized);
    } else {
      EXPECT_EQ(first_initialize.error(),
                firebase::admob::kAdMobErrorAlreadyInitialized);
      EXPECT_EQ(second_initialize.error(), firebase::admob::kAdMobErrorNone);
    }
    first_initialize.Release();
    second_initialize.Release();
    delete banner;
  }
}

TEST_F(FirebaseAdMobTest, TestBannerViewErrorLoadInProgress) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Load the banner ad.
  // Note potential flake: this test assumes the attempt to load an ad
  // won't resolve immediately.  If it does then the result may be two
  // successful ad loads instead of the expected
  // kAdMobErrorLoadInProgress error.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> first_load_ad =
      banner->LoadAd(request);
  firebase::Future<firebase::admob::LoadAdResult> second_load_ad =
      banner->LoadAd(request);

  WaitForCompletion(second_load_ad, "Second LoadAd",
                    firebase::admob::kAdMobErrorLoadInProgress);
  WaitForCompletion(first_load_ad, "First LoadAd");

  const firebase::admob::LoadAdResult* result_ptr = second_load_ad.result();
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->code(), firebase::admob::kAdMobErrorLoadInProgress);
  EXPECT_EQ(result_ptr->message(), "Ad is currently loading.");
  EXPECT_EQ(result_ptr->domain(), "SDK");
  const firebase::admob::ResponseInfo response_info =
      result_ptr->response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());

  first_load_ad.Release();
  second_load_ad.Release();

  WaitForCompletion(banner->Destroy(), "Destroy BannerView");
  delete banner;
}

TEST_F(FirebaseAdMobTest, TestBannerViewErrorBadAdUnitId) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBadAdUnit, banner_ad_size),
                    "Initialize");

  // Load the banner ad.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> load_ad =
      banner->LoadAd(request);
  WaitForCompletion(load_ad, "LoadAd",
                    firebase::admob::kAdMobErrorInvalidRequest);

  const firebase::admob::LoadAdResult* result_ptr = load_ad.result();
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->code(), firebase::admob::kAdMobErrorInvalidRequest);

  EXPECT_FALSE(result_ptr->message().empty());
  EXPECT_EQ(result_ptr->domain(), kErrorDomain);

  const firebase::admob::ResponseInfo response_info =
      result_ptr->response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  load_ad.Release();

  WaitForCompletion(banner->Destroy(), "Destroy BannerView");
  delete banner;
}

TEST_F(FirebaseAdMobTest, TestBannerViewErrorBadExtrasClassName) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Load the banner ad.
  firebase::admob::AdRequest request = GetAdRequest();
  request.add_extra(kAdNetworkExtrasInvalidClassName, "shouldnot", "work");
  WaitForCompletion(banner->LoadAd(request), "LoadAd",
                    firebase::admob::kAdMobErrorAdNetworkClassLoadError);
  WaitForCompletion(banner->Destroy(), "Destroy BannerView");
  delete banner;
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdLoad) {
  SKIP_TEST_ON_DESKTOP;

  // Note: while showing an ad requires user interaction (below),
  // we test that we can simply load an ad first.

  firebase::admob::InterstitialAd* interstitial =
      new firebase::admob::InterstitialAd();

  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // When the InterstitialAd is initialized, load an ad.
  firebase::admob::AdRequest request = GetAdRequest();
  WaitForCompletion(interstitial->LoadAd(kInterstitialAdUnit, request),
                    "LoadAd");

  delete interstitial;
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdLoadAndShow) {
  TEST_REQUIRES_USER_INTERACTION;
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::InterstitialAd* interstitial =
      new firebase::admob::InterstitialAd();

  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  TestFullScreenContentListener full_screen_content_listener;
  interstitial->SetFullScreenContentListener(&full_screen_content_listener);

  TestPaidEventListener paid_event_listener;
  interstitial->SetPaidEventListener(&paid_event_listener);

  // When the InterstitialAd is initialized, load an ad.
  firebase::admob::AdRequest request = GetAdRequest();
  WaitForCompletion(interstitial->LoadAd(kInterstitialAdUnit, request),
                    "LoadAd");

  WaitForCompletion(interstitial->Show(), "Show");

  LogDebug("Click the Ad, and then return to the app to continue");

  while (
      full_screen_content_listener.num_on_ad_dismissed_full_screen_content_ ==
      0) {
    app_framework::ProcessEvents(1000);
  }

  LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
  app_framework::ProcessEvents(3000);

  EXPECT_EQ(full_screen_content_listener.num_on_ad_clicked_, 1);
  EXPECT_EQ(full_screen_content_listener.num_on_ad_showed_full_screen_content_,
            1);
  EXPECT_EQ(full_screen_content_listener.num_on_ad_impression_, 1);
  EXPECT_EQ(paid_event_listener.num_on_paid_event_, 1);
  EXPECT_EQ(
      full_screen_content_listener.num_on_ad_dismissed_full_screen_content_, 1);

  interstitial->SetFullScreenContentListener(nullptr);
  interstitial->SetPaidEventListener(nullptr);

  delete interstitial;
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdStress) {
  TEST_REQUIRES_USER_INTERACTION;
  SKIP_TEST_ON_DESKTOP;

  for (int i = 0; i < 10; ++i) {
    firebase::admob::InterstitialAd* interstitial =
        new firebase::admob::InterstitialAd();

    WaitForCompletion(
        interstitial->Initialize(app_framework::GetWindowContext()),
        "TestInterstitialAdStress Initialize");

    // When the InterstitialAd is initialized, load an ad.
    firebase::admob::AdRequest request = GetAdRequest();
    WaitForCompletion(interstitial->LoadAd(kInterstitialAdUnit, request),
                      "TestInterstitialAdStress LoadAd");
    delete interstitial;
  }
}

TEST_F(FirebaseAdMobTest, TesInterstitialAdErrorAlreadyInitialized) {
  SKIP_TEST_ON_DESKTOP;

  for (int i = 0; i < 5; ++i) {
    firebase::admob::InterstitialAd* interstitial_ad =
        new firebase::admob::InterstitialAd();
    firebase::Future<void> first_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());
    firebase::Future<void> second_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());

    WaitForCompletionAnyResult(second_initialize, "Second Initialize");
    WaitForCompletionAnyResult(first_initialize, "First Initialize");

    LogDebug("first_initialize.error(): %d", first_initialize.error());
    LogDebug("second_initialize.error(): %d", second_initialize.error());

    if (first_initialize.error() == firebase::admob::kAdMobErrorNone) {
      EXPECT_EQ(second_initialize.error(),
                firebase::admob::kAdMobErrorAlreadyInitialized);
    } else {
      EXPECT_EQ(first_initialize.error(),
                firebase::admob::kAdMobErrorAlreadyInitialized);
      EXPECT_EQ(second_initialize.error(), firebase::admob::kAdMobErrorNone);
    }
    first_initialize.Release();
    second_initialize.Release();
    delete interstitial_ad;
  }
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdErrorLoadInProgress) {
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::InterstitialAd* interstitial_ad =
      new firebase::admob::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  // Note potential flake: this test assumes the attempt to load an ad
  // won't resolve immediately.  If it does then the result may be two
  // successful ad loads instead of the expected
  // kAdMobErrorLoadInProgress error.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> first_load_ad =
      interstitial_ad->LoadAd(kInterstitialAdUnit, request);
  firebase::Future<firebase::admob::LoadAdResult> second_load_ad =
      interstitial_ad->LoadAd(kInterstitialAdUnit, request);

  WaitForCompletion(second_load_ad, "Second LoadAd",
                    firebase::admob::kAdMobErrorLoadInProgress);
  WaitForCompletion(first_load_ad, "First LoadAd");

  const firebase::admob::LoadAdResult* result_ptr = second_load_ad.result();
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->code(), firebase::admob::kAdMobErrorLoadInProgress);
  EXPECT_EQ(result_ptr->message(), "Ad is currently loading.");
  EXPECT_EQ(result_ptr->domain(), "SDK");
  const firebase::admob::ResponseInfo response_info =
      result_ptr->response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete interstitial_ad;
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdErrorBadAdUnitId) {
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::InterstitialAd* interstitial_ad =
      new firebase::admob::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  firebase::admob::AdRequest request = GetAdRequest();
  firebase::Future<firebase::admob::LoadAdResult> load_ad =
      interstitial_ad->LoadAd(kBadAdUnit, request);
  WaitForCompletion(load_ad, "LoadAd",
                    firebase::admob::kAdMobErrorInvalidRequest);

  const firebase::admob::LoadAdResult* result_ptr = load_ad.result();
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->code(), firebase::admob::kAdMobErrorInvalidRequest);
  EXPECT_FALSE(result_ptr->message().empty());
  EXPECT_EQ(result_ptr->domain(), kErrorDomain);
  const firebase::admob::ResponseInfo response_info =
      result_ptr->response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete interstitial_ad;
}

TEST_F(FirebaseAdMobTest, TestInterstitialAdErrorBadExtrasClassName) {
  SKIP_TEST_ON_DESKTOP;

  firebase::admob::InterstitialAd* interstitial_ad =
      new firebase::admob::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  firebase::admob::AdRequest request = GetAdRequest();
  request.add_extra(kAdNetworkExtrasInvalidClassName, "shouldnot", "work");
  WaitForCompletion(interstitial_ad->LoadAd(kInterstitialAdUnit, request),
                    "LoadAd",
                    firebase::admob::kAdMobErrorAdNetworkClassLoadError);
  delete interstitial_ad;
}

#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// Test runs & compiles for phones only.

struct ThreadArgs {
  firebase::admob::BannerView* banner;
  sem_t* semaphore;
};

static void* DeleteBannerViewOnSignal(void* args) {
  ThreadArgs* thread_args = static_cast<ThreadArgs*>(args);
  sem_wait(thread_args->semaphore);
  delete thread_args->banner;
  return nullptr;
}

TEST_F(FirebaseAdMobTest, TestBannerViewMultithreadDeletion) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_MOBILE;  // TODO(b/172832275): This test is temporarily
                        // disabled on all platforms due to flakiness
                        // on Android. Once it's fixed, this test should
                        // be re-enabled on mobile.

  const firebase::admob::AdSize banner_ad_size(kBannerWidth, kBannerHeight);

  for (int i = 0; i < 5; ++i) {
    firebase::admob::BannerView* banner = new firebase::admob::BannerView();
    WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                         kBannerAdUnit, banner_ad_size),
                      "Initialize");
    sem_t semaphore;
    sem_init(&semaphore, 0, 1);

    ThreadArgs args = {banner, &semaphore};

    pthread_t t1;
    int err = pthread_create(&t1, nullptr, &DeleteBannerViewOnSignal, &args);
    EXPECT_EQ(err, 0);

    banner->Destroy();
    sem_post(&semaphore);

    // Blocks until DeleteBannerViewOnSignal function is done.
    void* result = nullptr;
    err = pthread_join(t1, &result);

    EXPECT_EQ(err, 0);
    EXPECT_EQ(result, nullptr);

    sem_destroy(&semaphore);
  }
}
#endif  // #if defined(ANDROID) || (defined(TARGET_OS_IPHONE) &&
        // TARGET_OS_IPHONE)

}  // namespace firebase_testapp_automated
