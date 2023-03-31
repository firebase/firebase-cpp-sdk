// Copyright 2021 Google Inc. All rights reserved.
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
#include "firebase/gma.h"
#include "firebase/gma/types.h"
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

// The GMA app IDs for the test app.
#if defined(ANDROID)
// If you change the GMA app ID for your Android app, make sure to change it
// in AndroidManifest.xml as well.
const char* kGmaAppID = "ca-app-pub-3940256099942544~3347511713";
#else
// If you change the GMA app ID for your iOS app, make sure to change the
// value for "GADApplicationIdentifier" in your Info.plist as well.
const char* kGmaAppID = "ca-app-pub-3940256099942544~1458002511";
#endif

// These ad units IDs have been created specifically for testing, and will
// always return test ads.
#if defined(ANDROID)
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/6300978111";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/1033173712";
const char* kRewardedAdUnit = "ca-app-pub-3940256099942544/5224354917";
#else
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/2934735716";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/4411468910";
const char* kRewardedAdUnit = "ca-app-pub-3940256099942544/1712485313";
#endif

// Used in a test to send an errant ad unit id.
const char* kBadAdUnit = "oops";

// Standard Banner Ad Size.
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
#if defined(ANDROID)
const char* kErrorDomain = "com.google.android.gms.ads";
#else
const char* kErrorDomain = "com.google.admob";
#endif

// Sample test device IDs to use in making the request.
const std::vector<std::string> kTestDeviceIDs = {
    "2077ef9a63d2b398840261c8221a0c9b", "098fe087d987c9a878965454a65654d7"};

// Sample keywords to use in making the request.
static const std::vector<std::string> kKeywords({"GMA", "C++", "Fun"});

// "Extra" key value pairs can be added to the request as well. Typically
// these are used when testing new features.
static const std::map<std::string, std::string> kGmaAdapterExtras = {
    {"the_name_of_an_extra", "the_value_for_that_extra"},
    {"heres", "a second example"}};

#if defined(ANDROID)
static const char* kAdNetworkExtrasClassName =
    "com/google/ads/mediation/admob/AdMobAdapter";
#else
static const char* kAdNetworkExtrasClassName = "GADExtras";
#endif

// Class nname of the GMA SDK returned in initialization results.
#if defined(ANDROID)
const char kGmaClassName[] = "com.google.android.gms.ads.MobileAds";
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
const char kGmaClassName[] = "GADMobileAds";
#else  // desktop
const char kGmaClassName[] = "stub";
#endif

// Used to detect kAdErrorCodeAdNetworkClassLoadErrors when loading
// ads.
static const char* kAdNetworkExtrasInvalidClassName = "abc123321cba";

static const char* kContentUrl = "http://www.firebase.com";

static const std::vector<std::string> kNeighboringContentURLs = {
    "test_url1", "test_url2", "test_url3"};

using app_framework::LogDebug;
using app_framework::ProcessEvents;

using firebase_test_framework::FirebaseTest;

using testing::AnyOf;
using testing::Contains;
using testing::ElementsAre;
using testing::HasSubstr;
using testing::Pair;
using testing::Property;

class FirebaseGmaTest : public FirebaseTest {
 public:
  FirebaseGmaTest();
  ~FirebaseGmaTest() override;

  static void SetUpTestSuite();
  static void TearDownTestSuite();

  void SetUp() override;
  void TearDown() override;

 protected:
  firebase::gma::AdRequest GetAdRequest();

  static firebase::App* shared_app_;
};

class FirebaseGmaUITest : public FirebaseGmaTest {
 public:
  FirebaseGmaUITest();
  ~FirebaseGmaUITest() override;

  void SetUp() override;
};

// Runs GMA Tests on methods and functions that should be run
// before GMA initializes.
class FirebaseGmaPreInitializationTests : public FirebaseGmaTest {
 public:
  FirebaseGmaPreInitializationTests();
  ~FirebaseGmaPreInitializationTests() override;

  static void SetUpTestSuite();

  void SetUp() override;
};

firebase::App* FirebaseGmaTest::shared_app_ = nullptr;

void PauseForVisualInspectionAndCallbacks() {
  app_framework::ProcessEvents(300);
}

void InitializeGma(firebase::App* shared_app) {
  LogDebug("Initializing GMA.");

  ::firebase::ModuleInitializer initializer;
  initializer.Initialize(shared_app, nullptr,
                         [](::firebase::App* app, void* /* userdata */) {
                           LogDebug("Try to initialize GMA");
                           firebase::InitResult result;
                           ::firebase::gma::Initialize(*app, &result);
                           return result;
                         });

  FirebaseGmaTest::WaitForCompletion(initializer.InitializeLastResult(),
                                     "Initialize");

  ASSERT_EQ(initializer.InitializeLastResult().error(), 0)
      << initializer.InitializeLastResult().error_message();

  LogDebug("Successfully initialized GMA.");
}

void FirebaseGmaTest::SetUpTestSuite() {
  LogDebug("Initialize Firebase App.");

  FindFirebaseConfig(FIREBASE_CONFIG_STRING);

#if defined(ANDROID)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(ANDROID)

  InitializeGma(shared_app_);
}

void FirebaseGmaTest::TearDownTestSuite() {
  // Workaround: GMA does some of its initialization in the main
  // thread, so if you terminate it too quickly after initialization
  // it can cause issues.  Add a small delay here in case most of the
  // tests are skipped.
  ProcessEvents(1000);
  LogDebug("Shutdown GMA.");
  firebase::gma::Terminate();
  LogDebug("Shutdown Firebase App.");
  delete shared_app_;
  shared_app_ = nullptr;
}

FirebaseGmaTest::FirebaseGmaTest() {}

FirebaseGmaTest::~FirebaseGmaTest() {}

void FirebaseGmaTest::SetUp() {
  TEST_DOES_NOT_REQUIRE_USER_INTERACTION;
  FirebaseTest::SetUp();

  // This example uses ad units that are specially configured to return test ads
  // for every request. When using your own ad unit IDs, however, it's important
  // to register the device IDs associated with any devices that will be used to
  // test the app. This ensures that regardless of the ad unit ID, those
  // devices will always receive test ads in compliance with GMA policy.
  //
  // Device IDs can be obtained by checking the logcat or the Xcode log while
  // debugging. They appear as a long string of hex characters.
  firebase::gma::RequestConfiguration request_configuration;
  request_configuration.test_device_ids = kTestDeviceIDs;
  firebase::gma::SetRequestConfiguration(request_configuration);
}

void FirebaseGmaTest::TearDown() { FirebaseTest::TearDown(); }

firebase::gma::AdRequest FirebaseGmaTest::GetAdRequest() {
  firebase::gma::AdRequest request;

  // Additional keywords to be used in targeting.
  for (auto keyword_iter = kKeywords.begin(); keyword_iter != kKeywords.end();
       ++keyword_iter) {
    request.add_keyword((*keyword_iter).c_str());
  }

  for (auto extras_iter = kGmaAdapterExtras.begin();
       extras_iter != kGmaAdapterExtras.end(); ++extras_iter) {
    request.add_extra(kAdNetworkExtrasClassName, extras_iter->first.c_str(),
                      extras_iter->second.c_str());
  }

  // Content URL
  request.set_content_url(kContentUrl);

  // Neighboring Content URLs
  request.add_neighboring_content_urls(kNeighboringContentURLs);

  return request;
}

FirebaseGmaUITest::FirebaseGmaUITest() {}

FirebaseGmaUITest::~FirebaseGmaUITest() {}

void FirebaseGmaUITest::SetUp() {
  TEST_REQUIRES_USER_INTERACTION;
  FirebaseTest::SetUp();
  // This example uses ad units that are specially configured to return test ads
  // for every request. When using your own ad unit IDs, however, it's important
  // to register the device IDs associated with any devices that will be used to
  // test the app. This ensures that regardless of the ad unit ID, those
  // devices will always receive test ads in compliance with GMA policy.
  //
  // Device IDs can be obtained by checking the logcat or the Xcode log while
  // debugging. They appear as a long string of hex characters.
  firebase::gma::RequestConfiguration request_configuration;
  request_configuration.test_device_ids = kTestDeviceIDs;
  firebase::gma::SetRequestConfiguration(request_configuration);
}

FirebaseGmaPreInitializationTests::FirebaseGmaPreInitializationTests() {}

FirebaseGmaPreInitializationTests::~FirebaseGmaPreInitializationTests() {}

void FirebaseGmaPreInitializationTests::SetUp() { FirebaseTest::SetUp(); }

void FirebaseGmaPreInitializationTests::SetUpTestSuite() {
  LogDebug("Initialize Firebase App.");
  FindFirebaseConfig(FIREBASE_CONFIG_STRING);
#if defined(ANDROID)
  shared_app_ = ::firebase::App::Create(app_framework::GetJniEnv(),
                                        app_framework::GetActivity());
#else
  shared_app_ = ::firebase::App::Create();
#endif  // defined(ANDROID)
}

// Test cases below.

TEST_F(FirebaseGmaPreInitializationTests, TestDisableMediationInitialization) {
  // Note: This test should be disabled or put in an entirely different test
  // binrary if we ever wish to test mediation in this application.
  firebase::gma::DisableMediationInitialization();

  // Ensure that GMA can initialize.
  InitializeGma(shared_app_);
  auto initialize_future = firebase::gma::InitializeLastResult();
  WaitForCompletion(initialize_future, "gma::Initialize");
  ASSERT_NE(initialize_future.result(), nullptr);
  EXPECT_EQ(*initialize_future.result(),
            firebase::gma::GetInitializationStatus());

#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
  // Check to see that only one Adapter was initialized, the base GMA adapter.
  // Note: DisableMediationInitialization is only implemented on iOS.
  std::map<std::string, firebase::gma::AdapterStatus> adapter_status =
      firebase::gma::GetInitializationStatus().GetAdapterStatusMap();
  EXPECT_EQ(adapter_status.size(), 1);
  EXPECT_THAT(
      adapter_status,
      Contains(
          Pair(kGmaClassName,
               Property(&firebase::gma::AdapterStatus::is_initialized, true))))
      << "Expected adapter class '" << kGmaClassName << "' is not loaded.";
#endif
}

TEST_F(FirebaseGmaTest, TestInitializationStatus) {
  // Ensure Initialize()'s result matches GetInitializationStatus().
  auto initialize_future = firebase::gma::InitializeLastResult();
  WaitForCompletion(initialize_future, "gma::Initialize");
  ASSERT_NE(initialize_future.result(), nullptr);
  EXPECT_EQ(*initialize_future.result(),
            firebase::gma::GetInitializationStatus());

  for (auto adapter_status :
       firebase::gma::GetInitializationStatus().GetAdapterStatusMap()) {
    LogDebug("GMA Mediation Adapter '%s' %s (latency %d ms): %s",
             adapter_status.first.c_str(),
             (adapter_status.second.is_initialized() ? "loaded" : "NOT loaded"),
             adapter_status.second.latency(),
             adapter_status.second.description().c_str());
  }

  // Confirm that the default Google Mobile Ads SDK class name shows up in the
  // list. It should either be is_initialized = true, or description should say
  // "Timeout" (this is a special case we are using to deflake this test on
  // Android emulator).
  EXPECT_THAT(
      initialize_future.result()->GetAdapterStatusMap(),
      Contains(Pair(
          kGmaClassName,
          AnyOf(Property(&firebase::gma::AdapterStatus::is_initialized, true),
                Property(&firebase::gma::AdapterStatus::description,
                         HasSubstr("Timeout"))))))
      << "Expected adapter class '" << kGmaClassName << "' is not loaded.";
}

TEST_F(FirebaseGmaPreInitializationTests, TestDisableSDKCrashReporting) {
  // We can't test to see if this method successfully reconfigures crash
  // reporting, but we're still calling it as a sanity check and to ensure
  // the symbol exists in the library.
  firebase::gma::DisableSDKCrashReporting();
}

TEST_F(FirebaseGmaTest, TestSetAppKeyEnabled) {
  // We can't test to see if this method successfully enables/disables
  // the app key, but we're still calling it as a sanity check and to
  // ensure the symbol exists in the library.
  firebase::gma::SetIsSameAppKeyEnabled(true);
}

TEST_F(FirebaseGmaTest, TestGetAdRequest) { GetAdRequest(); }

TEST_F(FirebaseGmaTest, TestGetAdRequestValues) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::AdRequest request = GetAdRequest();

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
    EXPECT_EQ(configured_extras.size(), kGmaAdapterExtras.size());

    // Check the extra key value pairs.
    for (auto constant_extras_iter = kGmaAdapterExtras.begin();
         constant_extras_iter != kGmaAdapterExtras.end();
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

  const std::unordered_set<std::string> configured_neighboring_content_urls =
      request.neighboring_content_urls();
  EXPECT_EQ(configured_neighboring_content_urls.size(),
            kNeighboringContentURLs.size());
  for (auto url_iter = kNeighboringContentURLs.begin();
       url_iter != kNeighboringContentURLs.end(); ++url_iter) {
    EXPECT_TRUE(configured_neighboring_content_urls.find(*url_iter) !=
                configured_neighboring_content_urls.end());
  }
}

// A listener to detect when the AdInspector has been closed. Additionally,
// checks for errors when opening the AdInspector while it's already open.
class TestAdInspectorClosedListener
    : public firebase::gma::AdInspectorClosedListener {
 public:
  TestAdInspectorClosedListener()
      : num_closed_events_(0), num_successful_results_(0) {}

  // Called when the user clicked the ad.
  void OnAdInspectorClosed(const firebase::gma::AdResult& ad_result) override {
    ++num_closed_events_;
    if (ad_result.is_successful()) {
      ++num_successful_results_;
    } else {
#if defined(ANDROID)
      EXPECT_EQ(ad_result.ad_error().code(),
                firebase::gma::kAdErrorCodeInsepctorAlreadyOpen);
      EXPECT_STREQ(ad_result.ad_error().message().c_str(),
                   "Ad inspector cannot be opened because it is already open.");
#else
      // The iOS GMA SDK returns internal errors for all AdInspector failures.
      EXPECT_EQ(ad_result.ad_error().code(),
                firebase::gma::kAdErrorCodeInternalError);
      EXPECT_STREQ(ad_result.ad_error().message().c_str(),
                   "Ad Inspector cannot be opened because it is already open.");
#endif
    }
  }

  uint8_t num_closed_events() const { return num_closed_events_; }
  uint8_t num_successful_results() const { return num_successful_results_; }

 private:
  uint8_t num_closed_events_;
  uint8_t num_successful_results_;
};

// This is for manual test only
// Ensure we can open the AdInspector and listen to its events.
TEST_F(FirebaseGmaTest, TestAdInspector) {
  TEST_REQUIRES_USER_INTERACTION;
  TestAdInspectorClosedListener listener;

  firebase::gma::OpenAdInspector(app_framework::GetWindowController(),
                                 &listener);

  // Call OpenAdInspector, even on Desktop (above), to ensure the stub linked
  // correctly. However, the rest of the testing is mobile-only beahvior.
  SKIP_TEST_ON_DESKTOP;

  // Open the inspector a second time to generate a
  // kAdErrorCodeInsepctorAlreadyOpen result.
  app_framework::ProcessEvents(2000);

  firebase::gma::OpenAdInspector(app_framework::GetWindowController(),
                                 &listener);

  while (listener.num_closed_events() < 2) {
    app_framework::ProcessEvents(2000);
  }

  EXPECT_EQ(listener.num_successful_results(), 1);
}

// A simple listener to help test changes to a AdViews.
class TestBoundingBoxListener
    : public firebase::gma::AdViewBoundingBoxListener {
 public:
  void OnBoundingBoxChanged(firebase::gma::AdView* ad_view,
                            firebase::gma::BoundingBox box) override {
    bounding_box_changes_.push_back(box);
  }
  std::vector<firebase::gma::BoundingBox> bounding_box_changes_;
};

// A simple listener to help test changes an Ad.
class TestAdListener : public firebase::gma::AdListener {
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

// A simple listener track FullScreen presentation changes.
class TestFullScreenContentListener
    : public firebase::gma::FullScreenContentListener {
 public:
  TestFullScreenContentListener()
      : num_on_ad_clicked_(0),
        num_on_ad_dismissed_full_screen_content_(0),
        num_on_ad_failed_to_show_full_screen_content_(0),
        num_on_ad_impression_(0),
        num_on_ad_showed_full_screen_content_(0) {}

  int num_ad_clicked() const { return num_on_ad_clicked_; }
  int num_ad_dismissed() const {
    return num_on_ad_dismissed_full_screen_content_;
  }
  int num_ad_failed_to_show_content() const {
    return num_on_ad_failed_to_show_full_screen_content_;
  }
  int num_ad_impressions() const { return num_on_ad_impression_; }
  int num_ad_showed_content() const {
    return num_on_ad_showed_full_screen_content_;
  }

  void OnAdClicked() override { num_on_ad_clicked_++; }

  void OnAdDismissedFullScreenContent() override {
    num_on_ad_dismissed_full_screen_content_++;
  }

  void OnAdFailedToShowFullScreenContent(
      const firebase::gma::AdError& ad_error) override {
    num_on_ad_failed_to_show_full_screen_content_++;
    failure_codes_.push_back(ad_error.code());
  }

  void OnAdImpression() override { num_on_ad_impression_++; }

  void OnAdShowedFullScreenContent() override {
    num_on_ad_showed_full_screen_content_++;
  }

  const std::vector<firebase::gma::AdErrorCode> failure_codes() const {
    return failure_codes_;
  }

 private:
  int num_on_ad_clicked_;
  int num_on_ad_dismissed_full_screen_content_;
  int num_on_ad_failed_to_show_full_screen_content_;
  int num_on_ad_impression_;
  int num_on_ad_showed_full_screen_content_;

  std::vector<firebase::gma::AdErrorCode> failure_codes_;
};

// A simple listener track UserEarnedReward events.
class TestUserEarnedRewardListener
    : public firebase::gma::UserEarnedRewardListener {
 public:
  TestUserEarnedRewardListener() : num_on_user_earned_reward_(0) {}

  int num_earned_rewards() const { return num_on_user_earned_reward_; }

  void OnUserEarnedReward(const firebase::gma::AdReward& reward) override {
    ++num_on_user_earned_reward_;
    EXPECT_EQ(reward.type(), "coins");
    EXPECT_EQ(reward.amount(), 10);
  }

 private:
  int num_on_user_earned_reward_;
};

// A simple listener track ad pay events.
class TestPaidEventListener : public firebase::gma::PaidEventListener {
 public:
  TestPaidEventListener() : num_on_paid_event_(0) {}

  int num_paid_events() const { return num_on_paid_event_; }

  void OnPaidEvent(const firebase::gma::AdValue& value) override {
    ++num_on_paid_event_;
    // These are the values for GMA test ads.  If they change then we should
    // alter the test to match the new expected values.
    EXPECT_EQ(value.currency_code(), "USD");
    EXPECT_EQ(value.value_micros(), 0);
  }
  int num_on_paid_event_;
};

TEST_F(FirebaseGmaTest, TestAdSize) {
  uint32_t kWidth = 50;
  uint32_t kHeight = 10;

  using firebase::gma::AdSize;

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

  const firebase::gma::AdSize adaptive_current =
      AdSize::GetCurrentOrientationAnchoredAdaptiveBannerAdSize(kWidth);
  EXPECT_EQ(adaptive_current.width(), kWidth);
  EXPECT_EQ(adaptive_current.height(), 0);
  EXPECT_EQ(adaptive_current.type(), AdSize::kTypeAnchoredAdaptive);
  EXPECT_EQ(adaptive_current.orientation(), AdSize::kOrientationCurrent);

  const firebase::gma::AdSize custom_ad_size(kWidth, kHeight);
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

  const AdSize leaderboard = AdSize::kLeaderboard;
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

TEST_F(FirebaseGmaTest, TestRequestConfigurationSetGetEmptyConfig) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::RequestConfiguration set_configuration;
  firebase::gma::SetRequestConfiguration(set_configuration);
  firebase::gma::RequestConfiguration retrieved_configuration =
      firebase::gma::GetRequestConfiguration();

  EXPECT_EQ(
      retrieved_configuration.max_ad_content_rating,
      firebase::gma::RequestConfiguration::kMaxAdContentRatingUnspecified);
  EXPECT_EQ(
      retrieved_configuration.tag_for_child_directed_treatment,
      firebase::gma::RequestConfiguration::kChildDirectedTreatmentUnspecified);
  EXPECT_EQ(retrieved_configuration.tag_for_under_age_of_consent,
            firebase::gma::RequestConfiguration::kUnderAgeOfConsentUnspecified);
  EXPECT_EQ(retrieved_configuration.test_device_ids.size(), 0);
}

TEST_F(FirebaseGmaTest, TestRequestConfigurationSetGet) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::RequestConfiguration set_configuration;
  set_configuration.max_ad_content_rating =
      firebase::gma::RequestConfiguration::kMaxAdContentRatingPG;
  set_configuration.tag_for_child_directed_treatment =
      firebase::gma::RequestConfiguration::kChildDirectedTreatmentTrue;
  set_configuration.tag_for_under_age_of_consent =
      firebase::gma::RequestConfiguration::kUnderAgeOfConsentFalse;
  set_configuration.test_device_ids.push_back("1");
  set_configuration.test_device_ids.push_back("2");
  set_configuration.test_device_ids.push_back("3");
  firebase::gma::SetRequestConfiguration(set_configuration);

  firebase::gma::RequestConfiguration retrieved_configuration =
      firebase::gma::GetRequestConfiguration();

  EXPECT_EQ(retrieved_configuration.max_ad_content_rating,
            firebase::gma::RequestConfiguration::kMaxAdContentRatingPG);

#if defined(ANDROID)
  EXPECT_EQ(retrieved_configuration.tag_for_child_directed_treatment,
            firebase::gma::RequestConfiguration::kChildDirectedTreatmentTrue);
  EXPECT_EQ(retrieved_configuration.tag_for_under_age_of_consent,
            firebase::gma::RequestConfiguration::kUnderAgeOfConsentFalse);
#else  // iOS
  // iOS doesn't allow for the querying of these values.
  EXPECT_EQ(
      retrieved_configuration.tag_for_child_directed_treatment,
      firebase::gma::RequestConfiguration::kChildDirectedTreatmentUnspecified);
  EXPECT_EQ(retrieved_configuration.tag_for_under_age_of_consent,
            firebase::gma::RequestConfiguration::kUnderAgeOfConsentUnspecified);
#endif

  EXPECT_EQ(retrieved_configuration.test_device_ids.size(), 3);
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "1"));
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "2"));
  EXPECT_TRUE(std::count(retrieved_configuration.test_device_ids.begin(),
                         retrieved_configuration.test_device_ids.end(), "3"));
}

// Simple Load Tests as a sanity check. These don't show the ad, just
// ensure that we can load them before diving into the interactive tests.
TEST_F(FirebaseGmaTest, TestAdViewLoadAd) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");
  firebase::Future<firebase::gma::AdResult> load_ad_future;
  const firebase::gma::AdResult* result_ptr = nullptr;

  load_ad_future = ad_view->LoadAd(GetAdRequest());
  WaitForCompletion(load_ad_future, "LoadAd");

  result_ptr = load_ad_future.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_TRUE(result_ptr->is_successful());

  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
  EXPECT_FALSE(
      result_ptr->response_info().mediation_adapter_class_name().empty());
  EXPECT_FALSE(result_ptr->response_info().response_id().empty());
  EXPECT_FALSE(result_ptr->response_info().ToString().empty());

  EXPECT_EQ(ad_view->ad_size().width(), kBannerWidth);
  EXPECT_EQ(ad_view->ad_size().height(), kBannerHeight);
  EXPECT_EQ(ad_view->ad_size().type(), firebase::gma::AdSize::kTypeStandard);

  load_ad_future.Release();
  WaitForCompletion(ad_view->Destroy(), "Destroy");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestInterstitialAdLoad) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  firebase::gma::InterstitialAd* interstitial =
      new firebase::gma::InterstitialAd();

  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // When the InterstitialAd is initialized, load an ad.
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      interstitial->LoadAd(kInterstitialAdUnit, GetAdRequest());

  WaitForCompletion(load_ad_future, "LoadAd");
  const firebase::gma::AdResult* result_ptr = load_ad_future.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_TRUE(result_ptr->is_successful());
  EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
  EXPECT_FALSE(
      result_ptr->response_info().mediation_adapter_class_name().empty());
  EXPECT_FALSE(result_ptr->response_info().response_id().empty());
  EXPECT_FALSE(result_ptr->response_info().ToString().empty());

  load_ad_future.Release();
  delete interstitial;
}

TEST_F(FirebaseGmaTest, TestRewardedAdLoad) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();

  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // When the RewardedAd is initialized, load an ad.
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      rewarded->LoadAd(kRewardedAdUnit, GetAdRequest());

  // This test behaves differently if it's running in UI mode
  // (manually on a device) or in non-UI mode (via automated tests).
  if (ShouldRunUITests()) {
    // Run in manual mode: fail if any error occurs.
    WaitForCompletion(load_ad_future, "LoadAd");
  } else {
    // Run in automated test mode: don't fail if NoFill occurred.
    WaitForCompletionAnyResult(load_ad_future,
                               "LoadAd (ignoring NoFill error)");
    EXPECT_TRUE(load_ad_future.error() == firebase::gma::kAdErrorCodeNone ||
                load_ad_future.error() == firebase::gma::kAdErrorCodeNoFill);
  }
  if (load_ad_future.error() == firebase::gma::kAdErrorCodeNone) {
    // In UI mode, or in non-UI mode if a NoFill error didn't occur, check that
    // the ad loaded correctly.
    const firebase::gma::AdResult* result_ptr = load_ad_future.result();
    ASSERT_NE(result_ptr, nullptr);
    EXPECT_TRUE(result_ptr->is_successful());
    EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
    EXPECT_FALSE(
        result_ptr->response_info().mediation_adapter_class_name().empty());
    EXPECT_FALSE(result_ptr->response_info().response_id().empty());
    EXPECT_FALSE(result_ptr->response_info().ToString().empty());
  }
  load_ad_future.Release();
  delete rewarded;
}

// Interactive test section.  These have been placed up front so that the
// tester doesn't get bored waiting for them.
TEST_F(FirebaseGmaUITest, TestAdViewAdOpenedAdClosed) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Set the listener.
  TestAdListener ad_listener;
  ad_view->SetAdListener(&ad_listener);

  TestPaidEventListener paid_event_listener;
  ad_view->SetPaidEventListener(&paid_event_listener);

  // Load the AdView ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      ad_view->LoadAd(request);
  WaitForCompletion(load_ad_future, "LoadAd");

  if (load_ad_future.error() == firebase::gma::kAdErrorCodeNone) {
    WaitForCompletion(ad_view->Show(), "Show 0");

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
    EXPECT_EQ(paid_event_listener.num_paid_events(), 1);
#else
    LogDebug("Click the Ad, and then close the ad to continue");

    while (ad_listener.num_on_ad_clicked_ == 0) {
      app_framework::ProcessEvents(1000);
    }

    LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
    app_framework::ProcessEvents(2000);

    // Ensure all of the expected events were triggered on iOS.
    EXPECT_EQ(ad_listener.num_on_ad_clicked_, 1);
    EXPECT_EQ(ad_listener.num_on_ad_impression_, 1);
    EXPECT_EQ(paid_event_listener.num_paid_events(), 1);
    EXPECT_EQ(ad_listener.num_on_ad_opened_, 0);
    EXPECT_EQ(ad_listener.num_on_ad_closed_, 0);
#endif
  }

  load_ad_future.Release();
  ad_view->SetAdListener(nullptr);
  ad_view->SetPaidEventListener(nullptr);
  WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
  delete ad_view;
}

TEST_F(FirebaseGmaUITest, TestInterstitialAdLoadAndShow) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  firebase::gma::InterstitialAd* interstitial =
      new firebase::gma::InterstitialAd();

  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  TestFullScreenContentListener content_listener;
  interstitial->SetFullScreenContentListener(&content_listener);

  TestPaidEventListener paid_event_listener;
  interstitial->SetPaidEventListener(&paid_event_listener);

  // When the InterstitialAd is initialized, load an ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      interstitial->LoadAd(kInterstitialAdUnit, request);
  WaitForCompletion(load_ad_future, "LoadAd");

  if (load_ad_future.error() == firebase::gma::kAdErrorCodeNone) {
    WaitForCompletion(interstitial->Show(), "Show");

    LogDebug("Click the Ad, and then return to the app to continue");

    while (content_listener.num_ad_dismissed() == 0) {
      app_framework::ProcessEvents(1000);
    }

    LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
    app_framework::ProcessEvents(2000);

    EXPECT_EQ(content_listener.num_ad_clicked(), 1);
    EXPECT_EQ(content_listener.num_ad_showed_content(), 1);
    EXPECT_EQ(content_listener.num_ad_impressions(), 1);
    EXPECT_EQ(content_listener.num_ad_failed_to_show_content(), 0);
    EXPECT_EQ(content_listener.num_ad_dismissed(), 1);
    EXPECT_EQ(paid_event_listener.num_paid_events(), 1);

#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
    // Show the Ad again.  Note: Android's Interstitial ads fail silently
    // when attempting to show the ad twice.
    LogDebug("Attempting to show ad again, checking for correct error result.");
    WaitForCompletion(interstitial->Show(), "Show");
    app_framework::ProcessEvents(5000);
    EXPECT_THAT(content_listener.failure_codes(),
                ElementsAre(firebase::gma::kAdErrorCodeAdAlreadyUsed));
#endif  // TARGET_OS_IPHONE
  }

  load_ad_future.Release();
  interstitial->SetFullScreenContentListener(nullptr);
  interstitial->SetPaidEventListener(nullptr);

  delete interstitial;
}

TEST_F(FirebaseGmaUITest, TestRewardedAdLoadAndShow) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  // TODO(@drsanta): remove when GMA whitelists CI devices.
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;

  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();

  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  TestFullScreenContentListener content_listener;
  rewarded->SetFullScreenContentListener(&content_listener);

  TestPaidEventListener paid_event_listener;
  rewarded->SetPaidEventListener(&paid_event_listener);

  // When the RewardedAd is initialized, load an ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      rewarded->LoadAd(kRewardedAdUnit, request);
  WaitForCompletion(load_ad_future, "LoadAd");

  if (load_ad_future.error() == firebase::gma::kAdErrorCodeNone) {
    firebase::gma::RewardedAd::ServerSideVerificationOptions options;
    // We cannot programmatically verify that the GMA phone SDKs marshal
    // these values properly (there are no get methods). At least invoke the
    // method to ensure least we can set them without any exceptions occurring.
    options.custom_data = "custom data";
    options.user_id = "123456";
    rewarded->SetServerSideVerificationOptions(options);

    TestUserEarnedRewardListener earned_reward_listener;
    WaitForCompletion(rewarded->Show(&earned_reward_listener), "Show");

    LogDebug(
        "Wait for the Ad to finish playing, click the ad, return to the ad, "
        "then close the ad to continue.");

    while (content_listener.num_ad_dismissed() == 0) {
      app_framework::ProcessEvents(1000);
    }

    LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
    app_framework::ProcessEvents(2000);

    // If not running the UI test in CI (running manually), keep this check.
    // Else running the UI test in CI, skip this check.
    if (!ShouldRunUITests()) {
      EXPECT_EQ(content_listener.num_ad_clicked(), 1);
    }
    EXPECT_EQ(content_listener.num_ad_showed_content(), 1);
    EXPECT_EQ(content_listener.num_ad_impressions(), 1);
    EXPECT_EQ(content_listener.num_ad_dismissed(), 1);
    EXPECT_EQ(content_listener.num_ad_failed_to_show_content(), 0);
    EXPECT_EQ(earned_reward_listener.num_earned_rewards(), 1);
    EXPECT_EQ(paid_event_listener.num_paid_events(), 1);

    // Show the Ad again
    LogDebug("Attempting to show ad again, checking for correct error result.");
    WaitForCompletion(rewarded->Show(&earned_reward_listener), "Show");
    app_framework::ProcessEvents(2000);
    EXPECT_THAT(content_listener.failure_codes(),
                testing::ElementsAre(firebase::gma::kAdErrorCodeAdAlreadyUsed));
  }

  load_ad_future.Release();
  rewarded->SetFullScreenContentListener(nullptr);
  rewarded->SetPaidEventListener(nullptr);

  delete rewarded;
}

// Other AdView Tests

TEST_F(FirebaseGmaTest, TestAdViewLoadAdEmptyAdRequest) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");
  firebase::gma::AdRequest request;
  firebase::Future<firebase::gma::AdResult> load_ad_future;
  const firebase::gma::AdResult* result_ptr = nullptr;

  load_ad_future = ad_view->LoadAd(request);
  WaitForCompletion(load_ad_future, "LoadAd");
  result_ptr = load_ad_future.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_TRUE(result_ptr->is_successful());

  EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
  EXPECT_FALSE(
      result_ptr->response_info().mediation_adapter_class_name().empty());
  EXPECT_FALSE(result_ptr->response_info().response_id().empty());
  EXPECT_FALSE(result_ptr->response_info().ToString().empty());

  EXPECT_EQ(ad_view->ad_size().width(), kBannerWidth);
  EXPECT_EQ(ad_view->ad_size().height(), kBannerHeight);
  EXPECT_EQ(ad_view->ad_size().type(), firebase::gma::AdSize::kTypeStandard);

  load_ad_future.Release();
  WaitForCompletion(ad_view->Destroy(), "Destroy");

  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewLoadAdAnchorAdaptiveAd) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  using firebase::gma::AdSize;
  AdSize banner_ad_size =
      AdSize::GetCurrentOrientationAnchoredAdaptiveBannerAdSize(kBannerWidth);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  WaitForCompletion(ad_view->LoadAd(GetAdRequest()), "LoadAd");

  const AdSize ad_size = ad_view->ad_size();
  EXPECT_EQ(ad_size.width(), kBannerWidth);
  EXPECT_NE(ad_size.height(), 0);
  EXPECT_EQ(ad_size.type(), AdSize::kTypeAnchoredAdaptive);
  EXPECT_EQ(ad_size.orientation(), AdSize::kOrientationCurrent);
  WaitForCompletion(ad_view->Destroy(), "Destroy");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewLoadAdInlineAdaptiveAd) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;
  using firebase::gma::AdSize;

  using firebase::gma::AdSize;
  AdSize banner_ad_size =
      AdSize::GetCurrentOrientationInlineAdaptiveBannerAdSize(kBannerWidth);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  WaitForCompletion(ad_view->LoadAd(GetAdRequest()), "LoadAd");

  const AdSize ad_size = ad_view->ad_size();
  EXPECT_EQ(ad_size.width(), kBannerWidth);
  EXPECT_NE(ad_size.height(), 0);
  EXPECT_EQ(ad_size.type(), AdSize::kTypeInlineAdaptive);
  EXPECT_EQ(ad_size.orientation(), AdSize::kOrientationCurrent);
  WaitForCompletion(ad_view->Destroy(), "Destroy");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewLoadAdGetInlineAdaptiveBannerMaxHeight) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  using firebase::gma::AdSize;
  AdSize banner_ad_size =
      AdSize::GetInlineAdaptiveBannerAdSize(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  WaitForCompletion(ad_view->LoadAd(GetAdRequest()), "LoadAd");

  const AdSize ad_size = ad_view->ad_size();
  EXPECT_EQ(ad_size.width(), kBannerWidth);
  EXPECT_NE(ad_size.height(), 0);
  EXPECT_TRUE(ad_size.height() <= kBannerHeight);
  EXPECT_EQ(ad_size.type(), AdSize::kTypeInlineAdaptive);
  EXPECT_EQ(ad_size.orientation(), AdSize::kOrientationCurrent);
  WaitForCompletion(ad_view->Destroy(), "Destroy");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewLoadAdDestroyNotCalled) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");
  WaitForCompletion(ad_view->LoadAd(GetAdRequest()), "LoadAd");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewAdSizeCompareOp) {
  using firebase::gma::AdSize;
  EXPECT_TRUE(AdSize(50, 100) == AdSize(50, 100));
  EXPECT_TRUE(AdSize(100, 50) == AdSize(100, 50));
  EXPECT_FALSE(AdSize(50, 100) == AdSize(100, 50));
  EXPECT_FALSE(AdSize(10, 10) == AdSize(50, 50));

  EXPECT_FALSE(AdSize(50, 100) != AdSize(50, 100));
  EXPECT_FALSE(AdSize(100, 50) != AdSize(100, 50));
  EXPECT_TRUE(AdSize(50, 100) != AdSize(100, 50));
  EXPECT_TRUE(AdSize(10, 10) != AdSize(50, 50));

  EXPECT_TRUE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) ==
              AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100));
  EXPECT_FALSE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) !=
               AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100));

  EXPECT_TRUE(AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100) ==
              AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100));
  EXPECT_FALSE(AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100) !=
               AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100));

  EXPECT_TRUE(AdSize::GetInlineAdaptiveBannerAdSize(100, 50) ==
              AdSize::GetInlineAdaptiveBannerAdSize(100, 50));
  EXPECT_FALSE(AdSize::GetInlineAdaptiveBannerAdSize(100, 50) !=
               AdSize::GetInlineAdaptiveBannerAdSize(100, 50));

  EXPECT_TRUE(AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100) ==
              AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100));
  EXPECT_FALSE(AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100) !=
               AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100));

  EXPECT_TRUE(AdSize::GetPortraitInlineAdaptiveBannerAdSize(100) ==
              AdSize::GetPortraitInlineAdaptiveBannerAdSize(100));
  EXPECT_TRUE(AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100) ==
              AdSize::GetLandscapeInlineAdaptiveBannerAdSize(100));
  EXPECT_TRUE(AdSize::GetCurrentOrientationInlineAdaptiveBannerAdSize(100) ==
              AdSize::GetCurrentOrientationInlineAdaptiveBannerAdSize(100));

  EXPECT_FALSE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) ==
               AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100));
  EXPECT_TRUE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) !=
              AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100));

  EXPECT_FALSE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) ==
               AdSize(100, 100));
  EXPECT_TRUE(AdSize::GetLandscapeAnchoredAdaptiveBannerAdSize(100) !=
              AdSize(100, 100));

  EXPECT_FALSE(AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100) ==
               AdSize(100, 100));
  EXPECT_TRUE(AdSize::GetPortraitAnchoredAdaptiveBannerAdSize(100) !=
              AdSize(100, 100));
}

TEST_F(FirebaseGmaTest, TestAdViewDestroyBeforeInitialization) {
  SKIP_TEST_ON_DESKTOP;
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Destroy(), "Destroy AdView");
}

TEST_F(FirebaseGmaTest, TestAdViewAdSizeBeforeInitialization) {
  SKIP_TEST_ON_DESKTOP;
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  const firebase::gma::AdSize& ad_size = firebase::gma::AdSize(0, 0);
  EXPECT_TRUE(ad_view->ad_size() == ad_size);

  WaitForCompletion(ad_view->Destroy(), "Destroy AdView");
}

TEST_F(FirebaseGmaTest, TestAdView) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");
  EXPECT_TRUE(ad_view->ad_size() == banner_ad_size);

  // Set the listener.
  TestBoundingBoxListener bounding_box_listener;
  ad_view->SetBoundingBoxListener(&bounding_box_listener);
  PauseForVisualInspectionAndCallbacks();

  int expected_num_bounding_box_changes = 0;
  EXPECT_EQ(expected_num_bounding_box_changes,
            bounding_box_listener.bounding_box_changes_.size());

  // Load the AdView ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      ad_view->LoadAd(request);
  WaitForCompletion(load_ad_future, "LoadAd");

  const bool ad_loaded =
      load_ad_future.error() == firebase::gma::kAdErrorCodeNone;

  // Suppress the extensive testing below if the ad failed to load.
  if (ad_loaded) {
    EXPECT_EQ(ad_view->ad_size().width(), kBannerWidth);
    EXPECT_EQ(ad_view->ad_size().height(), kBannerHeight);
    EXPECT_EQ(expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());
    const firebase::gma::AdResult* result_ptr = load_ad_future.result();
    ASSERT_NE(result_ptr, nullptr);
    EXPECT_TRUE(result_ptr->is_successful());
    const firebase::gma::ResponseInfo response_info =
        result_ptr->ad_error().response_info();
    EXPECT_TRUE(response_info.adapter_responses().empty());

    // Make the AdView visible.
    WaitForCompletion(ad_view->Show(), "Show 0");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    // Move to each of the six pre-defined positions.
    WaitForCompletion(ad_view->SetPosition(firebase::gma::AdView::kPositionTop),
                      "SetPosition(Top)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionTop);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(
        ad_view->SetPosition(firebase::gma::AdView::kPositionTopLeft),
        "SetPosition(TopLeft)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionTopLeft);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(
        ad_view->SetPosition(firebase::gma::AdView::kPositionTopRight),
        "SetPosition(TopRight)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionTopRight);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(
        ad_view->SetPosition(firebase::gma::AdView::kPositionBottom),
        "SetPosition(Bottom)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionBottom);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(
        ad_view->SetPosition(firebase::gma::AdView::kPositionBottomLeft),
        "SetPosition(BottomLeft)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionBottomLeft);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(
        ad_view->SetPosition(firebase::gma::AdView::kPositionBottomRight),
        "SetPosition(BottomRight)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionBottomRight);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    // Move to some coordinates.
    WaitForCompletion(ad_view->SetPosition(100, 300), "SetPosition(x0, y0)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionUndefined);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(ad_view->SetPosition(100, 400), "SetPosition(x1, y1)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionUndefined);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    // Try hiding and showing the AdView.
    WaitForCompletion(ad_view->Hide(), "Hide 1");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(ad_view->Show(), "Show 1");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    // Move again after hiding/showing.
    WaitForCompletion(ad_view->SetPosition(100, 300), "SetPosition(x2, y2)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionUndefined);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(ad_view->SetPosition(100, 400), "SetPosition(x3, y3)");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(ad_view->bounding_box().position,
              firebase::gma::AdView::kPositionUndefined);
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    WaitForCompletion(ad_view->Hide(), "Hide 2");
    PauseForVisualInspectionAndCallbacks();
    EXPECT_EQ(expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    LogDebug("Waiting for a moment to ensure all callbacks are recorded.");
    app_framework::ProcessEvents(2000);
  }

  // Clean up the ad object.
  load_ad_future.Release();
  WaitForCompletion(ad_view->Destroy(), "Destroy AdView");
  ad_view->SetBoundingBoxListener(nullptr);
  delete ad_view;

  PauseForVisualInspectionAndCallbacks();

  if (ad_loaded) {
    // If the ad was show, do the final bounding box checks after the ad has
    // been destroyed.
#if defined(ANDROID) || TARGET_OS_IPHONE
    EXPECT_EQ(++expected_num_bounding_box_changes,
              bounding_box_listener.bounding_box_changes_.size());

    // As an extra check, all bounding boxes except the last should have the
    // same size aspect ratio that we requested. For example if you requested a
    // 320x50 banner, you can get one with the size 960x150. Use EXPECT_NEAR
    // because the calculation can have a small bit of error.
    double kAspectRatioAllowedError = 0.05;  // Allow about 5% of error.
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
          << "AdView size "
          << bounding_box_listener.bounding_box_changes_[i].width << "x"
          << bounding_box_listener.bounding_box_changes_[i].height
          << " does not have the same aspect ratio as requested size "
          << kBannerWidth << "x" << kBannerHeight << ".";
    }

    // And finally, the last bounding box change, when the AdView is deleted,
    // should have invalid values (-1,-1, -1, -1).
    EXPECT_TRUE(
        bounding_box_listener.bounding_box_changes_.back().x == -1 &&
        bounding_box_listener.bounding_box_changes_.back().y == -1 &&
        bounding_box_listener.bounding_box_changes_.back().width == -1 &&
        bounding_box_listener.bounding_box_changes_.back().height == -1);
#endif  // defined(ANDROID) || TARGET_OS_IPHONE
  }
}

TEST_F(FirebaseGmaTest, TestAdViewErrorNotInitialized) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::AdView* ad_view = new firebase::gma::AdView();

  WaitForCompletion(ad_view->LoadAd(GetAdRequest()), "LoadAd",
                    firebase::gma::kAdErrorCodeUninitialized);

  firebase::gma::AdView::Position position;
  WaitForCompletion(ad_view->SetPosition(position), "SetPosition(position)",
                    firebase::gma::kAdErrorCodeUninitialized);

  WaitForCompletion(ad_view->SetPosition(0, 0), "SetPosition(x,y)",
                    firebase::gma::kAdErrorCodeUninitialized);

  WaitForCompletion(ad_view->Hide(), "Hide",
                    firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(ad_view->Show(), "Show",
                    firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(ad_view->Pause(), "Pause",
                    firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(ad_view->Resume(), "Resume",
                    firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewErrorAlreadyInitialized) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  {
    firebase::gma::AdView* ad_view = new firebase::gma::AdView();
    firebase::Future<void> first_initialize = ad_view->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);
    firebase::Future<void> second_initialize = ad_view->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);

    WaitForCompletion(first_initialize, "First Initialize 1");
    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);

    first_initialize.Release();
    second_initialize.Release();
    WaitForCompletion(ad_view->Destroy(), "Destroy AdView 1");
    delete ad_view;
  }

  // Reverse the order of the completion waits.
  {
    firebase::gma::AdView* ad_view = new firebase::gma::AdView();
    firebase::Future<void> first_initialize = ad_view->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);
    firebase::Future<void> second_initialize = ad_view->Initialize(
        app_framework::GetWindowContext(), kBannerAdUnit, banner_ad_size);

    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);
    WaitForCompletion(first_initialize, "First Initialize 1");

    first_initialize.Release();
    second_initialize.Release();
    WaitForCompletion(ad_view->Destroy(), "Destroy AdView 2");
    delete ad_view;
  }
}

TEST_F(FirebaseGmaTest, TestAdViewErrorLoadInProgress) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Load the AdView ad.
  // Note potential flake: this test assumes the attempt to load an ad
  // won't resolve immediately.  If it does then the result may be two
  // successful ad loads instead of the expected
  // kAdErrorCodeLoadInProgress error.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> first_load_ad =
      ad_view->LoadAd(request);
  firebase::Future<firebase::gma::AdResult> second_load_ad =
      ad_view->LoadAd(request);

  WaitForCompletion(second_load_ad, "Second LoadAd",
                    firebase::gma::kAdErrorCodeLoadInProgress);
  WaitForCompletionAnyResult(first_load_ad, "First LoadAd");

  const firebase::gma::AdResult* result_ptr = second_load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeLoadInProgress);
  EXPECT_EQ(result_ptr->ad_error().message(), "Ad is currently loading.");
  EXPECT_EQ(result_ptr->ad_error().domain(), "SDK");
  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());

  first_load_ad.Release();
  second_load_ad.Release();

  WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewErrorBadAdUnitId) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBadAdUnit, banner_ad_size),
                    "Initialize");

  // Load the AdView ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad = ad_view->LoadAd(request);
  WaitForCompletion(load_ad, "LoadAd",
                    firebase::gma::kAdErrorCodeInvalidRequest);

  const firebase::gma::AdResult* result_ptr = load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeInvalidRequest);

  EXPECT_FALSE(result_ptr->ad_error().message().empty());
  EXPECT_EQ(result_ptr->ad_error().domain(), kErrorDomain);

  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  load_ad.Release();

  WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
  delete ad_view;
}

TEST_F(FirebaseGmaTest, TestAdViewErrorBadExtrasClassName) {
  SKIP_TEST_ON_DESKTOP;

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
  firebase::gma::AdView* ad_view = new firebase::gma::AdView();
  WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                        kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Load the AdView ad.
  firebase::gma::AdRequest request = GetAdRequest();
  request.add_extra(kAdNetworkExtrasInvalidClassName, "shouldnot", "work");
  WaitForCompletion(ad_view->LoadAd(request), "LoadAd",
                    firebase::gma::kAdErrorCodeAdNetworkClassLoadError);
  WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
  delete ad_view;
}

// Other InterstitialAd Tests

TEST_F(FirebaseGmaTest, TestInterstitialAdLoadEmptyRequest) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  firebase::gma::InterstitialAd* interstitial =
      new firebase::gma::InterstitialAd();

  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // When the InterstitialAd is initialized, load an ad.
  firebase::gma::AdRequest request;

  firebase::Future<firebase::gma::AdResult> load_ad_future =
      interstitial->LoadAd(kInterstitialAdUnit, request);

  WaitForCompletion(load_ad_future, "LoadAd");
  const firebase::gma::AdResult* result_ptr = load_ad_future.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_TRUE(result_ptr->is_successful());
  EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
  EXPECT_FALSE(
      result_ptr->response_info().mediation_adapter_class_name().empty());
  EXPECT_FALSE(result_ptr->response_info().response_id().empty());
  EXPECT_FALSE(result_ptr->response_info().ToString().empty());

  delete interstitial;
}

TEST_F(FirebaseGmaTest, TestInterstitialAdErrorNotInitialized) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::InterstitialAd* interstitial_ad =
      new firebase::gma::InterstitialAd();

  firebase::gma::AdRequest request = GetAdRequest();
  WaitForCompletion(interstitial_ad->LoadAd(kInterstitialAdUnit, request),
                    "LoadAd", firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(interstitial_ad->Show(), "Show",
                    firebase::gma::kAdErrorCodeUninitialized);

  delete interstitial_ad;
}

TEST_F(FirebaseGmaTest, TesInterstitialAdErrorAlreadyInitialized) {
  SKIP_TEST_ON_DESKTOP;

  {
    firebase::gma::InterstitialAd* interstitial_ad =
        new firebase::gma::InterstitialAd();
    firebase::Future<void> first_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());
    firebase::Future<void> second_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());

    WaitForCompletion(first_initialize, "First Initialize 1");
    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);

    first_initialize.Release();
    second_initialize.Release();

    delete interstitial_ad;
  }

  // Reverse the order of the completion waits.
  {
    firebase::gma::InterstitialAd* interstitial_ad =
        new firebase::gma::InterstitialAd();
    firebase::Future<void> first_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());
    firebase::Future<void> second_initialize =
        interstitial_ad->Initialize(app_framework::GetWindowContext());

    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);
    WaitForCompletion(first_initialize, "First Initialize 1");

    first_initialize.Release();
    second_initialize.Release();

    delete interstitial_ad;
  }
}

TEST_F(FirebaseGmaTest, TestInterstitialAdErrorLoadInProgress) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::InterstitialAd* interstitial_ad =
      new firebase::gma::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  // Note potential flake: this test assumes the attempt to load an ad
  // won't resolve immediately.  If it does then the result may be two
  // successful ad loads instead of the expected
  // kAdErrorCodeLoadInProgress error.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> first_load_ad =
      interstitial_ad->LoadAd(kInterstitialAdUnit, request);
  firebase::Future<firebase::gma::AdResult> second_load_ad =
      interstitial_ad->LoadAd(kInterstitialAdUnit, request);

  WaitForCompletion(second_load_ad, "Second LoadAd",
                    firebase::gma::kAdErrorCodeLoadInProgress);
  WaitForCompletionAnyResult(first_load_ad, "First LoadAd");

  const firebase::gma::AdResult* result_ptr = second_load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeLoadInProgress);
  EXPECT_EQ(result_ptr->ad_error().message(), "Ad is currently loading.");
  EXPECT_EQ(result_ptr->ad_error().domain(), "SDK");
  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete interstitial_ad;
}

TEST_F(FirebaseGmaTest, TestInterstitialAdErrorBadAdUnitId) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::InterstitialAd* interstitial_ad =
      new firebase::gma::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad =
      interstitial_ad->LoadAd(kBadAdUnit, request);
  WaitForCompletion(load_ad, "LoadAd",
                    firebase::gma::kAdErrorCodeInvalidRequest);

  const firebase::gma::AdResult* result_ptr = load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeInvalidRequest);
  EXPECT_FALSE(result_ptr->ad_error().message().empty());
  EXPECT_EQ(result_ptr->ad_error().domain(), kErrorDomain);
  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete interstitial_ad;
}

TEST_F(FirebaseGmaTest, TestInterstitialAdErrorBadExtrasClassName) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::InterstitialAd* interstitial_ad =
      new firebase::gma::InterstitialAd();
  WaitForCompletion(
      interstitial_ad->Initialize(app_framework::GetWindowContext()),
      "Initialize");

  // Load the interstitial ad.
  firebase::gma::AdRequest request = GetAdRequest();
  request.add_extra(kAdNetworkExtrasInvalidClassName, "shouldnot", "work");
  WaitForCompletion(interstitial_ad->LoadAd(kInterstitialAdUnit, request),
                    "LoadAd",
                    firebase::gma::kAdErrorCodeAdNetworkClassLoadError);
  delete interstitial_ad;
}

// Other RewardedAd Tests.

TEST_F(FirebaseGmaTest, TestRewardedAdLoadEmptyRequest) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_SIMULATOR;

  // Note: while showing an ad requires user interaction in another test,
  // this test is mean as a baseline loadAd functionality test.
  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();

  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // When the RewardedAd is initialized, load an ad.
  firebase::gma::AdRequest request;
  firebase::Future<firebase::gma::AdResult> load_ad_future =
      rewarded->LoadAd(kRewardedAdUnit, request);

  // This test behaves differently if it's running in UI mode
  // (manually on a device) or in non-UI mode (via automated tests).
  if (ShouldRunUITests()) {
    // Run in manual mode: fail if any error occurs.
    WaitForCompletion(load_ad_future, "LoadAd");
  } else {
    // Run in automated test mode: don't fail if NoFill occurred.
    WaitForCompletionAnyResult(load_ad_future,
                               "LoadAd (ignoring NoFill error)");
    EXPECT_TRUE(load_ad_future.error() == firebase::gma::kAdErrorCodeNone ||
                load_ad_future.error() == firebase::gma::kAdErrorCodeNoFill);
  }
  if (load_ad_future.error() == firebase::gma::kAdErrorCodeNone) {
    // In UI mode, or in non-UI mode if a NoFill error didn't occur, check that
    // the ad loaded correctly.
    const firebase::gma::AdResult* result_ptr = load_ad_future.result();
    ASSERT_NE(result_ptr, nullptr);
    EXPECT_TRUE(result_ptr->is_successful());
    EXPECT_FALSE(result_ptr->response_info().adapter_responses().empty());
    EXPECT_FALSE(
        result_ptr->response_info().mediation_adapter_class_name().empty());
    EXPECT_FALSE(result_ptr->response_info().response_id().empty());
    EXPECT_FALSE(result_ptr->response_info().ToString().empty());
  }
  load_ad_future.Release();
  delete rewarded;
}

TEST_F(FirebaseGmaTest, TestRewardedAdErrorNotInitialized) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::RewardedAd* rewarded_ad = new firebase::gma::RewardedAd();

  firebase::gma::AdRequest request = GetAdRequest();
  WaitForCompletion(rewarded_ad->LoadAd(kRewardedAdUnit, request), "LoadAd",
                    firebase::gma::kAdErrorCodeUninitialized);
  WaitForCompletion(rewarded_ad->Show(/*listener=*/nullptr), "Show",
                    firebase::gma::kAdErrorCodeUninitialized);

  delete rewarded_ad;
}

TEST_F(FirebaseGmaTest, TesRewardedAdErrorAlreadyInitialized) {
  SKIP_TEST_ON_DESKTOP;

  {
    firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();
    firebase::Future<void> first_initialize =
        rewarded->Initialize(app_framework::GetWindowContext());
    firebase::Future<void> second_initialize =
        rewarded->Initialize(app_framework::GetWindowContext());

    WaitForCompletionAnyResult(first_initialize, "First Initialize 1");
    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);

    first_initialize.Release();
    second_initialize.Release();

    delete rewarded;
  }

  // Reverse the order of the completion waits.
  {
    firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();
    firebase::Future<void> first_initialize =
        rewarded->Initialize(app_framework::GetWindowContext());
    firebase::Future<void> second_initialize =
        rewarded->Initialize(app_framework::GetWindowContext());

    WaitForCompletion(second_initialize, "Second Initialize 1",
                      firebase::gma::kAdErrorCodeAlreadyInitialized);
    WaitForCompletionAnyResult(first_initialize, "First Initialize 1");

    first_initialize.Release();
    second_initialize.Release();

    delete rewarded;
  }
}

TEST_F(FirebaseGmaTest, TestRewardedAdErrorLoadInProgress) {
  SKIP_TEST_ON_DESKTOP;

  // TODO(@drsanta): remove when GMA whitelists CI devices.
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;

  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();
  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // Load the rewarded ad.
  // Note potential flake: this test assumes the attempt to load an ad
  // won't resolve immediately.  If it does then the result may be two
  // successful ad loads instead of the expected
  // kAdErrorCodeLoadInProgress error.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> first_load_ad =
      rewarded->LoadAd(kRewardedAdUnit, request);
  firebase::Future<firebase::gma::AdResult> second_load_ad =
      rewarded->LoadAd(kRewardedAdUnit, request);

  WaitForCompletion(second_load_ad, "Second LoadAd",
                    firebase::gma::kAdErrorCodeLoadInProgress);
  WaitForCompletionAnyResult(first_load_ad, "First LoadAd");

  const firebase::gma::AdResult* result_ptr = second_load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeLoadInProgress);
  EXPECT_EQ(result_ptr->ad_error().message(), "Ad is currently loading.");
  EXPECT_EQ(result_ptr->ad_error().domain(), "SDK");
  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete rewarded;
}

TEST_F(FirebaseGmaTest, TestRewardedAdErrorBadAdUnitId) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();
  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // Load the rewarded ad.
  firebase::gma::AdRequest request = GetAdRequest();
  firebase::Future<firebase::gma::AdResult> load_ad =
      rewarded->LoadAd(kBadAdUnit, request);
  WaitForCompletion(load_ad, "LoadAd",
                    firebase::gma::kAdErrorCodeInvalidRequest);

  const firebase::gma::AdResult* result_ptr = load_ad.result();
  ASSERT_NE(result_ptr, nullptr);
  EXPECT_FALSE(result_ptr->is_successful());
  EXPECT_EQ(result_ptr->ad_error().code(),
            firebase::gma::kAdErrorCodeInvalidRequest);
  EXPECT_FALSE(result_ptr->ad_error().message().empty());
  EXPECT_EQ(result_ptr->ad_error().domain(), kErrorDomain);
  const firebase::gma::ResponseInfo response_info =
      result_ptr->ad_error().response_info();
  EXPECT_TRUE(response_info.adapter_responses().empty());
  delete rewarded;
}

TEST_F(FirebaseGmaTest, TestRewardedAdErrorBadExtrasClassName) {
  SKIP_TEST_ON_DESKTOP;

  firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();
  WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                    "Initialize");

  // Load the rewarded ad.
  firebase::gma::AdRequest request = GetAdRequest();
  request.add_extra(kAdNetworkExtrasInvalidClassName, "shouldnot", "work");
  WaitForCompletion(rewarded->LoadAd(kRewardedAdUnit, request), "LoadAd",
                    firebase::gma::kAdErrorCodeAdNetworkClassLoadError);
  delete rewarded;
}

// Stress tests.  These take a while so run them near the end.
TEST_F(FirebaseGmaTest, TestAdViewStress) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_EMULATOR;

  // TODO(@drsanta): remove when GMA whitelists CI devices
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  TEST_REQUIRES_USER_INTERACTION_ON_ANDROID;

  for (int i = 0; i < 10; ++i) {
    const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);
    firebase::gma::AdView* ad_view = new firebase::gma::AdView();
    WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                          kBannerAdUnit, banner_ad_size),
                      "TestAdViewStress Initialize");

    // Load the AdView ad.
    firebase::gma::AdRequest request = GetAdRequest();
    firebase::Future<firebase::gma::AdResult> future = ad_view->LoadAd(request);
    WaitForCompletionAnyResult(future, "TestAdViewStress LoadAd");
    // Stress tests may exhaust the ad pool. If so, loadAd will return
    // kAdErrorCodeNoFill.
    EXPECT_TRUE(future.error() == firebase::gma::kAdErrorCodeNone ||
                future.error() == firebase::gma::kAdErrorCodeNoFill);
    if (future.error() == firebase::gma::kAdErrorCodeNone) {
      EXPECT_EQ(ad_view->ad_size().width(), kBannerWidth);
      EXPECT_EQ(ad_view->ad_size().height(), kBannerHeight);
    }
    WaitForCompletion(ad_view->Destroy(), "Destroy the AdView");
    delete ad_view;
  }
}

TEST_F(FirebaseGmaTest, TestInterstitialAdStress) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_EMULATOR;

  // TODO(@drsanta): remove when GMA whitelists CI devices
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  TEST_REQUIRES_USER_INTERACTION_ON_ANDROID;

  for (int i = 0; i < 10; ++i) {
    firebase::gma::InterstitialAd* interstitial =
        new firebase::gma::InterstitialAd();

    WaitForCompletion(
        interstitial->Initialize(app_framework::GetWindowContext()),
        "TestInterstitialAdStress Initialize");

    // When the InterstitialAd is initialized, load an ad.
    firebase::gma::AdRequest request = GetAdRequest();
    firebase::Future<firebase::gma::AdResult> future =
        interstitial->LoadAd(kInterstitialAdUnit, request);
    WaitForCompletionAnyResult(future, "TestInterstitialAdStress LoadAd");
    // Stress tests may exhaust the ad pool. If so, loadAd will return
    // kAdErrorCodeNoFill.
    EXPECT_TRUE(future.error() == firebase::gma::kAdErrorCodeNone ||
                future.error() == firebase::gma::kAdErrorCodeNoFill);
    delete interstitial;
  }
}

TEST_F(FirebaseGmaTest, TestRewardedAdStress) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_EMULATOR;

  // TODO(@drsanta): remove when GMA whitelists CI devices.
  TEST_REQUIRES_USER_INTERACTION_ON_IOS;
  TEST_REQUIRES_USER_INTERACTION_ON_ANDROID;

  for (int i = 0; i < 10; ++i) {
    firebase::gma::RewardedAd* rewarded = new firebase::gma::RewardedAd();

    WaitForCompletion(rewarded->Initialize(app_framework::GetWindowContext()),
                      "TestRewardedAdStress Initialize");

    // When the RewardedAd is initialized, load an ad.
    firebase::gma::AdRequest request = GetAdRequest();
    firebase::Future<firebase::gma::AdResult> future =
        rewarded->LoadAd(kRewardedAdUnit, request);
    WaitForCompletionAnyResult(future, "TestRewardedAdStress LoadAd");
    // Stress tests may exhaust the ad pool. If so, loadAd will return
    // kAdErrorCodeNoFill.
    EXPECT_TRUE(future.error() == firebase::gma::kAdErrorCodeNone ||
                future.error() == firebase::gma::kAdErrorCodeNoFill);
    delete rewarded;
  }
}

#if defined(ANDROID) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE)
// Test runs & compiles for phones only.

struct ThreadArgs {
  firebase::gma::AdView* ad_view;
  sem_t* semaphore;
};

static void* DeleteAdViewOnSignal(void* args) {
  ThreadArgs* thread_args = static_cast<ThreadArgs*>(args);
  sem_wait(thread_args->semaphore);
  delete thread_args->ad_view;
  return nullptr;
}

TEST_F(FirebaseGmaTest, TestAdViewMultithreadDeletion) {
  SKIP_TEST_ON_DESKTOP;
  SKIP_TEST_ON_MOBILE;  // TODO(b/172832275): This test is temporarily
                        // disabled on all platforms due to flakiness
                        // on Android. Once it's fixed, this test should
                        // be re-enabled on mobile.

  const firebase::gma::AdSize banner_ad_size(kBannerWidth, kBannerHeight);

  for (int i = 0; i < 5; ++i) {
    firebase::gma::AdView* ad_view = new firebase::gma::AdView();
    WaitForCompletion(ad_view->Initialize(app_framework::GetWindowContext(),
                                          kBannerAdUnit, banner_ad_size),
                      "Initialize");
    sem_t semaphore;
    sem_init(&semaphore, 0, 1);

    ThreadArgs args = {ad_view, &semaphore};

    pthread_t t1;
    int err = pthread_create(&t1, nullptr, &DeleteAdViewOnSignal, &args);
    EXPECT_EQ(err, 0);

    ad_view->Destroy();
    sem_post(&semaphore);

    // Blocks until DeleteAdViewOnSignal function is done.
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
