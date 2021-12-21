//  Copyright © 2016 Google. All rights reserved.

#include "admob/tools/ios/testapp/testapp/game_engine.h"

#include <cstddef>

#include "app/src/assert.h"

namespace rewarded_video = firebase::admob::rewarded_video;

// AdMob app ID.
const char* kAdMobAppID = "ca-app-pub-3940256099942544~1458002511";

// AdMob ad unit IDs.
const char* kBannerAdUnit = "ca-app-pub-3940256099942544/2934735716";
const char* kNativeExpressAdUnit = "ca-app-pub-3940256099942544/2562852117";
const char* kInterstitialAdUnit = "ca-app-pub-3940256099942544/4411468910";
const char* kRewardedVideoAdUnit = "ca-app-pub-2618531387707574/6671583249";

// A simple listener that logs changes to a BannerView.
class LoggingBannerViewListener : public firebase::admob::BannerView::Listener {
 public:
  LoggingBannerViewListener() {}
  void OnPresentationStateChanged(
      firebase::admob::BannerView* banner_view,
      firebase::admob::BannerView::PresentationState state) override {
    LogMessage("BannerView PresentationState has changed to %d.", state);
  }
  void OnBoundingBoxChanged(firebase::admob::BannerView* banner_view,
                            firebase::admob::BoundingBox box) override {
    LogMessage(
        "BannerView BoundingBox has changed to (x: %d, y: %d, width: %d, "
        "height %d)",
        box.x, box.y, box.width, box.height);
  }
};

// A simple listener that logs changes to a NativeExpressAdView.
class LoggingNativeExpressAdViewListener
    : public firebase::admob::NativeExpressAdView::Listener {
 public:
  LoggingNativeExpressAdViewListener() {}
  void OnPresentationStateChanged(
      firebase::admob::NativeExpressAdView* native_express_view,
      firebase::admob::NativeExpressAdView::PresentationState state) override {
    LogMessage("NativeExpressAdView PresentationState has changed to %d.",
               state);
  }
  void OnBoundingBoxChanged(
      firebase::admob::NativeExpressAdView* native_express_view,
      firebase::admob::BoundingBox box) override {
    LogMessage(
        "NativeExpressAd BoundingBox has changed to (x: %d, y: %d, width: %d, "
        "height %d)",
        box.x, box.y, box.width, box.height);
  }
};

// A simple listener that logs changes to an InterstitialAd.
class LoggingInterstitialAdListener
    : public firebase::admob::InterstitialAd::Listener {
 public:
  LoggingInterstitialAdListener() {}
  void OnPresentationStateChanged(
      firebase::admob::InterstitialAd* interstitial_ad,
      firebase::admob::InterstitialAd::PresentationState state) override {
    LogMessage("InterstitialAd PresentationState has changed to %d.", state);
  }
};

// A simple listener that logs changes to rewarded video state.
class LoggingRewardedVideoListener : public rewarded_video::Listener {
 public:
  LoggingRewardedVideoListener() {}
  void OnRewarded(rewarded_video::RewardItem reward) override {
    LogMessage("Reward user with %f %s.", reward.amount,
               reward.reward_type.c_str());
  }
  void OnPresentationStateChanged(
      rewarded_video::PresentationState state) override {
    LogMessage("Rewarded video PresentationState has changed to %d.", state);
  }
};

// The listeners for logging changes to the AdMob ad formats.
LoggingBannerViewListener banner_listener;
LoggingNativeExpressAdViewListener native_express_listener;
LoggingInterstitialAdListener interstitial_listener;
LoggingRewardedVideoListener rewarded_listener;

// GameEngine constructor.
GameEngine::GameEngine() {}

// Sets up AdMob C++.
void GameEngine::Initialize(firebase::admob::AdParent ad_parent) {
  FIREBASE_ASSERT(kTestBannerView != kTestNativeExpressAdView &&
                  "kTestBannerView and kTestNativeExpressAdView cannot both be "
                  "true/false at the same time.");
  FIREBASE_ASSERT(kTestInterstitialAd != kTestRewardedVideo &&
                  "kTestInterstitialAd and kTestRewardedVideo cannot both be "
                  "true/false at the same time.");

  firebase::admob::Initialize(kAdMobAppID);
  parent_view_ = ad_parent;

  if (kTestBannerView) {
    // Create an ad size and initialize the BannerView.
    firebase::admob::AdSize bannerAdSize;
    bannerAdSize.width = 320;
    bannerAdSize.height = 50;
    banner_view_ = new firebase::admob::BannerView();
    banner_view_->Initialize(parent_view_, kBannerAdUnit, bannerAdSize);
    banner_view_listener_set_ = false;
  }

  if (kTestNativeExpressAdView) {
    // Create an ad size and initialize the NativeExpressAdView.
    firebase::admob::AdSize nativeExpressAdSize;
    nativeExpressAdSize.width = 320;
    nativeExpressAdSize.height = 220;
    native_express_view_ = new firebase::admob::NativeExpressAdView();
    native_express_view_->Initialize(parent_view_, kNativeExpressAdUnit,
                                     nativeExpressAdSize);
    native_express_ad_view_listener_set_ = false;
  }

  if (kTestInterstitialAd) {
    // Initialize the InterstitialAd.
    interstitial_ad_ = new firebase::admob::InterstitialAd();
    interstitial_ad_->Initialize(parent_view_, kInterstitialAdUnit);
    interstitial_ad_listener_set_ = false;
  }

  if (kTestRewardedVideo) {
    // Initialize the rewarded_video:: namespace.
    rewarded_video::Initialize();
    // If you want to poll the reward, uncomment the poll_listener_ code in the
    // update() function. When the poll_listener_code is commented out in
    // update(), then the LoggingRewardedVideoListener is used to log changes to
    // the rewarded video state.
    poll_listener_ = nullptr;
    rewarded_video_listener_set_ = false;
  }
}

// Creates the AdMob C++ ad request.
firebase::admob::AdRequest GameEngine::createRequest() {
  // Sample keywords to use in making the request.
  static const char* kKeywords[] = {"AdMob", "C++", "Fun"};

  // Sample test device IDs to use in making the request.
  static const char* kTestDeviceIDs[] = {"2077ef9a63d2b398840261c8221a0c9b",
                                         "098fe087d987c9a878965454a65654d7"};

  // Sample birthday value to use in making the request.
  static const int kBirthdayDay = 10;
  static const int kBirthdayMonth = 11;
  static const int kBirthdayYear = 1976;

  firebase::admob::AdRequest request;
  request.gender = firebase::admob::kGenderUnknown;

  request.tagged_for_child_directed_treatment =
      firebase::admob::kChildDirectedTreatmentStateTagged;

  request.birthday_day = kBirthdayDay;
  request.birthday_month = kBirthdayMonth;
  request.birthday_year = kBirthdayYear;

  request.keyword_count = sizeof(kKeywords) / sizeof(kKeywords[0]);
  request.keywords = kKeywords;

  static const firebase::admob::KeyValuePair kRequestExtras[] = {
      {"the_name_of_an_extra", "the_value_for_that_extra"}};
  request.extras_count = sizeof(kRequestExtras) / sizeof(kRequestExtras[0]);
  request.extras = kRequestExtras;

  request.test_device_id_count =
      sizeof(kTestDeviceIDs) / sizeof(kTestDeviceIDs[0]);
  request.test_device_ids = kTestDeviceIDs;

  return request;
}

// Updates the game engine (game loop).
void GameEngine::onUpdate() {
  if (kTestBannerView) {
    // Set the banner view listener.
    if (banner_view_->InitializeLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        banner_view_->InitializeLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        !banner_view_listener_set_) {
      banner_view_->SetListener(&banner_listener);
      banner_view_listener_set_ = true;
    }
  }

  if (kTestNativeExpressAdView) {
    // Set the native express ad view listener.
    if (native_express_view_->InitializeLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        native_express_view_->InitializeLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        !native_express_ad_view_listener_set_) {
      native_express_view_->SetListener(&native_express_listener);
      native_express_ad_view_listener_set_ = true;
    }
  }

  if (kTestInterstitialAd) {
    // Set the interstitial ad listener.
    if (interstitial_ad_->InitializeLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        interstitial_ad_->InitializeLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        !interstitial_ad_listener_set_) {
      interstitial_ad_->SetListener(&interstitial_listener);
      interstitial_ad_listener_set_ = true;
    }

    // Once the interstitial ad has been displayed to and dismissed by the user,
    // create a new interstitial ad.
    if (interstitial_ad_->ShowLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        interstitial_ad_->ShowLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        interstitial_ad_->GetPresentationState() ==
            firebase::admob::InterstitialAd::kPresentationStateHidden) {
      delete interstitial_ad_;
      interstitial_ad_ = nullptr;
      interstitial_ad_ = new firebase::admob::InterstitialAd();
      interstitial_ad_->Initialize(parent_view_, kInterstitialAdUnit);
      interstitial_ad_listener_set_ = false;
    }
  }

  if (kTestRewardedVideo) {
    // Set the rewarded video listener.
    if (rewarded_video::InitializeLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        rewarded_video::InitializeLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        !rewarded_video_listener_set_) {
      //        && poll_listener == nullptr) {
      rewarded_video::SetListener(&rewarded_listener);
      rewarded_video_listener_set_ = true;
      //      poll_listener_ = new
      //      firebase::admob::rewarded_video::PollableRewardListener();
      //      rewarded_video::SetListener(poll_listener_);
    }

    // Once the rewarded video ad has been displayed to and dismissed by the
    // user, create a new rewarded video ad.
    if (rewarded_video::ShowLastResult().Status() ==
            firebase::kFutureStatusComplete &&
        rewarded_video::ShowLastResult().Error() ==
            firebase::admob::kAdMobErrorNone &&
        rewarded_video::GetPresentationState() ==
            firebase::admob::rewarded_video::kPresentationStateHidden) {
      rewarded_video::Destroy();
      rewarded_video::Initialize();
      rewarded_video_listener_set_ = false;
    }
  }

  // Increment red if increasing, decrement otherwise.
  float diff = bg_intensity_increasing_ ? 0.0025f : -0.0025f;

  // Increment red up to 1.0, then back down to 0.0, repeat.
  bg_intensity_ += diff;
  if (bg_intensity_ >= 0.4f) {
    bg_intensity_increasing_ = false;
  } else if (bg_intensity_ <= 0.0f) {
    bg_intensity_increasing_ = true;
  }
}

// Handles user tapping on one of the kNumberOfButtons.
void GameEngine::onTap(float x, float y) {
  int button_number = -1;
  GLfloat viewport_x = 1 - (((width_ - x) * 2) / width_);
  GLfloat viewport_y = 1 - (((y)*2) / height_);

  for (int i = 0; i < kNumberOfButtons; i++) {
    if ((viewport_x >= vertices_[i * 8]) &&
        (viewport_x <= vertices_[i * 8 + 2]) &&
        (viewport_y <= vertices_[i * 8 + 1]) &&
        (viewport_y >= vertices_[i * 8 + 5])) {
      button_number = i;
      break;
    }
  }

  // The BannerView or NativeExpressAdView's bounding box.
  firebase::admob::BoundingBox box;

  switch (button_number) {
    case 0:
      if (kTestBannerView) {
        // Load the banner ad.
        if (banner_view_->InitializeLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            banner_view_->InitializeLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          banner_view_->LoadAd(createRequest());
        }
      }
      if (kTestNativeExpressAdView) {
        // Load the native express ad.
        if (native_express_view_->InitializeLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            native_express_view_->InitializeLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          native_express_view_->LoadAd(createRequest());
        }
      }
      break;
    case 1:
      if (kTestBannerView) {
        // Show/Hide the BannerView.
        if (banner_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            banner_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone &&
            banner_view_->GetPresentationState() ==
                firebase::admob::BannerView::kPresentationStateHidden) {
          banner_view_->Show();
        } else if (banner_view_->LoadAdLastResult().Status() ==
                       firebase::kFutureStatusComplete &&
                   banner_view_->GetPresentationState() ==
                       firebase::admob::BannerView::
                           kPresentationStateVisibleWithAd) {
          banner_view_->Hide();
        }
      }
      if (kTestNativeExpressAdView) {
        // Show/Hide the NativeExpressAdView.
        if (native_express_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            native_express_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone &&
            native_express_view_->GetPresentationState() ==
                firebase::admob::NativeExpressAdView::
                    kPresentationStateHidden) {
          native_express_view_->Show();
        } else if (native_express_view_->LoadAdLastResult().Status() ==
                       firebase::kFutureStatusComplete &&
                   native_express_view_->LoadAdLastResult().Error() ==
                       firebase::admob::kAdMobErrorNone &&
                   native_express_view_->GetPresentationState() ==
                       firebase::admob::NativeExpressAdView::
                           kPresentationStateVisibleWithAd) {
          native_express_view_->Hide();
        }
      }
      break;
    case 2:
      if (kTestBannerView) {
        // Move the BannerView to a predefined position.
        if (banner_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            banner_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          banner_view_->MoveTo(firebase::admob::BannerView::kPositionBottom);
        }
      }
      if (kTestNativeExpressAdView) {
        // Move the NativeExpressAdView to a predefined position.
        if (native_express_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            native_express_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          native_express_view_->MoveTo(
              firebase::admob::NativeExpressAdView::kPositionBottom);
        }
      }
      break;
    case 3:
      if (kTestBannerView) {
        // Move the BannerView to a specific x and y coordinate.
        if (banner_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            banner_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          int x = 100;
          int y = 200;
          banner_view_->MoveTo(x, y);
        }
      }
      if (kTestNativeExpressAdView) {
        // Move the NativeExpressAdView to a specific x and y coordinate.
        if (native_express_view_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            native_express_view_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          int x = 100;
          int y = 200;
          native_express_view_->MoveTo(x, y);
        }
      }
      if (kTestRewardedVideo) {
        // Poll the reward.
        if (poll_listener_ != nullptr) {
          while (poll_listener_->PollReward(&reward_)) {
            LogMessage("Reward user with %f %s.", reward_.amount,
                       reward_.reward_type.c_str());
          }
        }
      }
      break;
    case 4:
      if (kTestInterstitialAd) {
        // Load the interstitial ad.
        if (interstitial_ad_->InitializeLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            interstitial_ad_->InitializeLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          interstitial_ad_->LoadAd(createRequest());
        }
      }
      if (kTestRewardedVideo) {
        // Load the rewarded video ad.
        if (rewarded_video::InitializeLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            rewarded_video::InitializeLastResult().Error() ==
                firebase::admob::kAdMobErrorNone) {
          rewarded_video::LoadAd(kRewardedVideoAdUnit, createRequest());
        }
      }
      break;
    case 5:
      if (kTestInterstitialAd) {
        // Show the interstitial ad.
        if (interstitial_ad_->LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            interstitial_ad_->LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone &&
            interstitial_ad_->ShowLastResult().Status() !=
                firebase::kFutureStatusComplete) {
          interstitial_ad_->Show();
        }
      }
      if (kTestRewardedVideo) {
        // Show the rewarded video ad.
        if (rewarded_video::LoadAdLastResult().Status() ==
                firebase::kFutureStatusComplete &&
            rewarded_video::LoadAdLastResult().Error() ==
                firebase::admob::kAdMobErrorNone &&
            rewarded_video::ShowLastResult().Status() !=
                firebase::kFutureStatusComplete) {
          rewarded_video::Show(parent_view_);
        }
      }
      break;
    default:
      break;
  }
}

// The vertex shader code string.
static const GLchar* kVertexShaderCodeString =
    "attribute vec2 position;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}";

// The fragment shader code string.
static const GLchar* kFragmentShaderCodeString =
    "precision mediump float;\n"
    "uniform vec4 myColor; \n"
    "void main() { \n"
    "    gl_FragColor = myColor; \n"
    "}";

// Creates the OpenGL surface.
void GameEngine::onSurfaceCreated() {
  vertex_shader_ = glCreateShader(GL_VERTEX_SHADER);
  fragment_shader_ = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(vertex_shader_, 1, &kVertexShaderCodeString, NULL);
  glCompileShader(vertex_shader_);

  GLint status;
  glGetShaderiv(vertex_shader_, GL_COMPILE_STATUS, &status);

  char buffer[512];
  glGetShaderInfoLog(vertex_shader_, 512, NULL, buffer);

  glShaderSource(fragment_shader_, 1, &kFragmentShaderCodeString, NULL);
  glCompileShader(fragment_shader_);

  glGetShaderiv(fragment_shader_, GL_COMPILE_STATUS, &status);

  glGetShaderInfoLog(fragment_shader_, 512, NULL, buffer);

  shader_program_ = glCreateProgram();
  glAttachShader(shader_program_, vertex_shader_);
  glAttachShader(shader_program_, fragment_shader_);

  glLinkProgram(shader_program_);
  glUseProgram(shader_program_);
}

// Updates the OpenGL surface.
void GameEngine::onSurfaceChanged(int width, int height) {
  width_ = width;
  height_ = height;

  GLfloat heightIncrement = 0.25f;
  GLfloat currentHeight = 0.93f;

  for (int i = 0; i < kNumberOfButtons; i++) {
    int base = i * 8;
    vertices_[base] = -0.9f;
    vertices_[base + 1] = currentHeight;
    vertices_[base + 2] = 0.9f;
    vertices_[base + 3] = currentHeight;
    vertices_[base + 4] = -0.9f;
    vertices_[base + 5] = currentHeight - heightIncrement;
    vertices_[base + 6] = 0.9f;
    vertices_[base + 7] = currentHeight - heightIncrement;
    currentHeight -= 1.2 * heightIncrement;
  }
}

// Draws the frame for the OpenGL surface.
void GameEngine::onDrawFrame() {
  glClearColor(0.0f, 0.0f, bg_intensity_, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  GLuint vbo;
  glGenBuffers(1, &vbo);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);

  GLfloat colorBytes[] = {0.9f, 0.9f, 0.9f, 1.0f};
  GLint colorLocation = glGetUniformLocation(shader_program_, "myColor");
  glUniform4fv(colorLocation, 1, colorBytes);

  GLint posAttrib = glGetAttribLocation(shader_program_, "position");
  glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, 0, 0);
  glEnableVertexAttribArray(posAttrib);

  for (int i = 0; i < kNumberOfButtons; i++) {
    glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
  }
}
