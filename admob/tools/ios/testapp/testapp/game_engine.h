//  Copyright © 2016 Google. All rights reserved.

#ifndef GAME_ENGINE_H_
#define GAME_ENGINE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <OpenGLES/ES2/gl.h>

#include "firebase/admob.h"
#include "firebase/admob/banner_view.h"
#include "firebase/admob/interstitial_ad.h"
#include "firebase/admob/types.h"

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

// Cross platform logging method.
extern "C" int LogMessage(const char* format, ...);

class GameEngine {
  static const int kNumberOfButtons = 6;

  // Set these flags to enable the ad formats that you want to test.
  static const bool kTestBannerView = true;
  static const bool kTestInterstitialAd = true;

 public:
  GameEngine();

  void Initialize(firebase::admob::AdParent ad_parent);
  void onUpdate();
  void onTap(float x, float y);
  void onSurfaceCreated();
  void onSurfaceChanged(int width, int height);
  void onDrawFrame();

 private:
  firebase::admob::AdRequest createRequest();

  firebase::admob::BannerView* banner_view_;
  firebase::admob::InterstitialAd* interstitial_ad_;

  bool banner_view_listener_set_;
  bool interstitial_ad_listener_set_;

  firebase::admob::AdParent parent_view_;

  bool bg_intensity_increasing_;
  float bg_intensity_;

  GLuint vertex_shader_;
  GLuint fragment_shader_;
  GLuint shader_program_;
  int height_;
  int width_;
  GLfloat vertices_[kNumberOfButtons * 8];
};

#endif  // GAME_ENGINE_H_
