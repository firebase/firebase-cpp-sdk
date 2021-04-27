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
#include "firebase/admob.h"
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
const char* kRewardedVideoAdUnit = "ca-app-pub-3940256099942544/5224354917";
#else
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/2934735716";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/4411468910";
const char* kRewardedVideoAdUnit = "ca-app-pub-3940256099942544/1712485313";
#endif

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
  initializer.Initialize(
      shared_app_, nullptr, [](::firebase::App* app, void* /* userdata */) {
        LogDebug("Try to initialize AdMob");
        return ::firebase::admob::Initialize(*app, kAdMobAppID);
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

void FirebaseAdMobTest::SetUp() { FirebaseTest::SetUp(); }

void FirebaseAdMobTest::TearDown() { FirebaseTest::TearDown(); }

firebase::admob::AdRequest FirebaseAdMobTest::GetAdRequest() {
  // Sample keywords to use in making the request.
  static const char* kKeywords[] = {"AdMob", "C++", "Fun"};

  // Sample birthday value to use in making the request.
  static const int kBirthdayDay = 10;
  static const int kBirthdayMonth = 11;
  static const int kBirthdayYear = 1976;

  // Sample test device IDs to use in making the request.
  static const char* kTestDeviceIDs[] = {"2077ef9a63d2b398840261c8221a0c9b",
                                         "098fe087d987c9a878965454a65654d7"};

  firebase::admob::AdRequest request;
  // If the app is aware of the user's gender, it can be added to the targeting
  // information. Otherwise, "unknown" should be used.
  request.gender = firebase::admob::kGenderUnknown;

  // This value allows publishers to specify whether they would like the request
  // to be treated as child-directed for purposes of the Children’s Online
  // Privacy Protection Act (COPPA).
  // See http://business.ftc.gov/privacy-and-security/childrens-privacy.
  request.tagged_for_child_directed_treatment =
      firebase::admob::kChildDirectedTreatmentStateTagged;

  // The user's birthday, if known. Note that months are indexed from one.
  request.birthday_day = kBirthdayDay;
  request.birthday_month = kBirthdayMonth;
  request.birthday_year = kBirthdayYear;

  // Additional keywords to be used in targeting.
  request.keyword_count = sizeof(kKeywords) / sizeof(kKeywords[0]);
  request.keywords = kKeywords;

  // "Extra" key value pairs can be added to the request as well. Typically
  // these are used when testing new features.
  static const firebase::admob::KeyValuePair kRequestExtras[] = {
      {"the_name_of_an_extra", "the_value_for_that_extra"}};
  request.extras_count = sizeof(kRequestExtras) / sizeof(kRequestExtras[0]);
  request.extras = kRequestExtras;

  // This example uses ad units that are specially configured to return test ads
  // for every request. When using your own ad unit IDs, however, it's important
  // to register the device IDs associated with any devices that will be used to
  // test the app. This ensures that regardless of the ad unit ID, those
  // devices will always receive test ads in compliance with AdMob policy.
  //
  // Device IDs can be obtained by checking the logcat or the Xcode log while
  // debugging. They appear as a long string of hex characters.
  request.test_device_id_count =
      sizeof(kTestDeviceIDs) / sizeof(kTestDeviceIDs[0]);
  request.test_device_ids = kTestDeviceIDs;
  return request;
}

// Test cases below.

TEST_F(FirebaseAdMobTest, TestGetAdRequest) { GetAdRequest(); }

// A simple listener to help test changes to a BannerView.
class TestBannerViewListener : public firebase::admob::BannerView::Listener {
 public:
  void OnPresentationStateChanged(
      firebase::admob::BannerView* banner_view,
      firebase::admob::BannerView::PresentationState state) override {
    presentation_states_.push_back(state);
  }
  void OnBoundingBoxChanged(firebase::admob::BannerView* banner_view,
                            firebase::admob::BoundingBox box) override {
    bounding_box_changes_.push_back(box);
  }
  std::vector<firebase::admob::BannerView::PresentationState>
      presentation_states_;
  std::vector<firebase::admob::BoundingBox> bounding_box_changes_;
};

TEST_F(FirebaseAdMobTest, TestBannerView) {
  // AdMob cannot be tested on Firebase Test Lab, so disable tests on FTL.
  TEST_REQUIRES_USER_INTERACTION;

  static const int kBannerWidth = 320;
  static const int kBannerHeight = 50;

  firebase::admob::AdSize banner_ad_size;
  banner_ad_size.ad_size_type = firebase::admob::kAdSizeStandard;
  banner_ad_size.width = kBannerWidth;
  banner_ad_size.height = kBannerHeight;

  firebase::admob::BannerView* banner = new firebase::admob::BannerView();
  WaitForCompletion(banner->Initialize(app_framework::GetWindowContext(),
                                       kBannerAdUnit, banner_ad_size),
                    "Initialize");

  // Set the listener.
  TestBannerViewListener banner_listener;
  banner->SetListener(&banner_listener);

  // Load the banner ad.
  firebase::admob::AdRequest request = GetAdRequest();
  WaitForCompletion(banner->LoadAd(request), "LoadAd");

  std::vector<firebase::admob::BannerView::PresentationState>
      expected_presentation_states;
  int expected_num_bounding_box_changes = 0;

  // Make the BannerView visible.
  WaitForCompletion(banner->Show(), "Show 0");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  // Move to each of the six pre-defined positions.

  WaitForCompletion(banner->MoveTo(firebase::admob::BannerView::kPositionTop),
                    "MoveTo(Top)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(
      banner->MoveTo(firebase::admob::BannerView::kPositionTopLeft),
      "MoveTo(TopLeft)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(
      banner->MoveTo(firebase::admob::BannerView::kPositionTopRight),
      "MoveTo(TopRight)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(
      banner->MoveTo(firebase::admob::BannerView::kPositionBottom),
      "Moveto(Bottom)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(
      banner->MoveTo(firebase::admob::BannerView::kPositionBottomLeft),
      "MoveTo(BottomLeft)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(
      banner->MoveTo(firebase::admob::BannerView::kPositionBottomRight),
      "MoveTo(BottomRight)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  // Move to some coordinates.
  WaitForCompletion(banner->MoveTo(100, 300), "MoveTo(x0, y0)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(banner->MoveTo(100, 400), "MoveTo(x1, y1)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  // Try hiding and showing the BannerView.
  WaitForCompletion(banner->Hide(), "Hide 1");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateHidden);

  WaitForCompletion(banner->Show(), "Show 1");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  // Move again after hiding/showing.
  WaitForCompletion(banner->MoveTo(100, 300), "MoveTo(x2, y2)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(banner->MoveTo(100, 400), "Moveto(x3, y3)");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateVisibleWithAd);
  expected_num_bounding_box_changes++;

  WaitForCompletion(banner->Hide(), "Hide 2");
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateHidden);

  delete banner;
  expected_presentation_states.push_back(
      firebase::admob::BannerView::kPresentationStateHidden);
  expected_num_bounding_box_changes++;

#if defined(__ANDROID__) || TARGET_OS_IPHONE
  // Ensure that we got all the presentation state changes.
  EXPECT_EQ(banner_listener.presentation_states_, expected_presentation_states);

  // For the bounding box, check that we got the number of bounding box events
  // we expect, since we don't know the exact bounding box coordinates to
  // expect.
  EXPECT_EQ(banner_listener.bounding_box_changes_.size(),
            expected_num_bounding_box_changes);

  // As an extra check, all bounding boxes except the last should have the same
  // size aspect ratio that we requested. For example if you requested a 320x50
  // banner, you can get one with the size 960x150. Use EXPECT_NEAR because the
  // calculation can have a small bit of error.
  double kAspectRatioAllowedError = 0.02;  // Allow about 2% of error.
  double expected_aspect_ratio =
      static_cast<double>(kBannerWidth) / static_cast<double>(kBannerHeight);
  for (int i = 0; i < banner_listener.bounding_box_changes_.size() - 1; ++i) {
    double actual_aspect_ratio =
        static_cast<double>(banner_listener.bounding_box_changes_[i].width) /
        static_cast<double>(banner_listener.bounding_box_changes_[i].height);
    EXPECT_NEAR(actual_aspect_ratio, expected_aspect_ratio,
                kAspectRatioAllowedError)
        << "Banner size " << banner_listener.bounding_box_changes_[i].width
        << "x" << banner_listener.bounding_box_changes_[i].height
        << " does not have the same aspect ratio as requested size "
        << kBannerWidth << "x" << kBannerHeight << ".";
  }

  // And finally, the last bounding box change, when the banner is deleted,
  // should be (0,0,0,0).
  EXPECT_TRUE(banner_listener.bounding_box_changes_.back().x == 0 &&
              banner_listener.bounding_box_changes_.back().y == 0 &&
              banner_listener.bounding_box_changes_.back().width == 0 &&
              banner_listener.bounding_box_changes_.back().height == 0);
#endif
}

// A simple listener to help test changes to a InterstitialAd.
class TestInterstitialAdListener
    : public firebase::admob::InterstitialAd::Listener {
 public:
  void OnPresentationStateChanged(
      firebase::admob::InterstitialAd* interstitial_ad,
      firebase::admob::InterstitialAd::PresentationState state) override {
    presentation_states_.push_back(state);
  }
  std::vector<firebase::admob::InterstitialAd::PresentationState>
      presentation_states_;
};

TEST_F(FirebaseAdMobTest, TestInterstitialAd) {
  TEST_REQUIRES_USER_INTERACTION;

  firebase::admob::InterstitialAd* interstitial =
      new firebase::admob::InterstitialAd();
  WaitForCompletion(interstitial->Initialize(app_framework::GetWindowContext(),
                                             kInterstitialAdUnit),
                    "Initialize");
  TestInterstitialAdListener interstitial_listener;
  interstitial->SetListener(&interstitial_listener);

  firebase::admob::AdRequest request = GetAdRequest();
  // When the InterstitialAd is initialized, load an ad.
  WaitForCompletion(interstitial->LoadAd(request), "LoadAd");
  std::vector<firebase::admob::InterstitialAd::PresentationState>
      expected_presentation_states;

  WaitForCompletion(interstitial->Show(), "Show");
  expected_presentation_states.push_back(
      firebase::admob::InterstitialAd::PresentationState::
          kPresentationStateCoveringUI);
  // Wait for the user to close the interstitial ad.
  while (interstitial->presentation_state() !=
         firebase::admob::InterstitialAd::PresentationState::
             kPresentationStateHidden) {
    app_framework::ProcessEvents(1000);
  }
  expected_presentation_states.push_back(
      firebase::admob::InterstitialAd::PresentationState::
          kPresentationStateHidden);
#if defined(__ANDROID__) || TARGET_OS_IPHONE
  EXPECT_EQ(interstitial_listener.presentation_states_,
            expected_presentation_states);
#endif
  delete interstitial;
}

// A simple listener to help test changes to rewarded video state.
class TestRewardedVideoListener
    : public firebase::admob::rewarded_video::Listener {
 public:
  TestRewardedVideoListener() { got_reward_ = false; }
  void OnRewarded(firebase::admob::rewarded_video::RewardItem reward) override {
    got_reward_ = true;
    reward_type_ = reward.reward_type;
    reward_amount_ = reward.amount;
  }
  void OnPresentationStateChanged(
      firebase::admob::rewarded_video::PresentationState state) override {
    presentation_states_.push_back(state);
  }
  bool got_reward_;
  std::string reward_type_;
  float reward_amount_;
  std::vector<firebase::admob::rewarded_video::PresentationState>
      presentation_states_;
};

TEST_F(FirebaseAdMobTest, TestRewardedVideoAd) {
  TEST_REQUIRES_USER_INTERACTION;

  namespace rewarded_video = firebase::admob::rewarded_video;
  WaitForCompletion(rewarded_video::Initialize(), "Initialize");

  TestRewardedVideoListener rewarded_listener;
  rewarded_video::SetListener(&rewarded_listener);

  firebase::admob::AdRequest request = GetAdRequest();
  WaitForCompletion(rewarded_video::LoadAd(kRewardedVideoAdUnit, request),
                    "LoadAd");

  std::vector<rewarded_video::PresentationState> expected_presentation_states;

  WaitForCompletion(rewarded_video::Show(app_framework::GetWindowContext()),
                    "Show");

  expected_presentation_states.push_back(
      rewarded_video::PresentationState::kPresentationStateCoveringUI);
  expected_presentation_states.push_back(
      rewarded_video::PresentationState::kPresentationStateVideoHasStarted);

  // Wait a moment, then pause, then resume.
  ProcessEvents(1000);
  WaitForCompletion(rewarded_video::Pause(), "Pause");
  ProcessEvents(1000);
  WaitForCompletion(rewarded_video::Resume(), "Resume");

#if defined(__ANDROID__) || TARGET_OS_IPHONE
  // Wait for video to complete.
  while (
      rewarded_listener.presentation_states_.back() !=
      rewarded_video::PresentationState::kPresentationStateVideoHasCompleted) {
    ProcessEvents(1000);
  }
  expected_presentation_states.push_back(
      rewarded_video::PresentationState::kPresentationStateVideoHasCompleted);

  EXPECT_TRUE(rewarded_listener.got_reward_);
  EXPECT_NE(rewarded_listener.reward_type_, "");
  EXPECT_NE(rewarded_listener.reward_amount_, 0);
  LogDebug("Got reward: %.02f %s", rewarded_listener.reward_amount_,
           rewarded_listener.reward_type_.c_str());

  EXPECT_EQ(rewarded_listener.presentation_states_,
            expected_presentation_states);
#endif
  rewarded_video::Destroy();
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

  static const int kBannerWidth = 320;
  static const int kBannerHeight = 50;

  firebase::admob::AdSize banner_ad_size;
  banner_ad_size.ad_size_type = firebase::admob::kAdSizeStandard;
  banner_ad_size.width = kBannerWidth;
  banner_ad_size.height = kBannerHeight;

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
