/*
 * Copyright 2016 Google LLC
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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_INTERSTITIAL_AD_INTERNAL_ANDROID_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_INTERSTITIAL_AD_INTERNAL_ANDROID_H_

#include "admob/src/common/interstitial_ad_internal.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

// Used to set up the cache of InterstitialAdHelper class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define INTERSTITIALADHELPER_METHODS(X)                                        \
  X(Constructor, "<init>", "(J)V"),                                            \
  X(Initialize, "initialize", "(JLandroid/app/Activity;Ljava/lang/String;)V"), \
  X(Show, "show", "(J)V"),                                                     \
  X(LoadAd, "loadAd", "(JLcom/google/android/gms/ads/AdRequest;)V"),           \
  X(GetPresentationState, "getPresentationState", "()I"),                      \
  X(Disconnect, "disconnect", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(interstitial_ad_helper, INTERSTITIALADHELPER_METHODS);

namespace internal {

class InterstitialAdInternalAndroid : public InterstitialAdInternal {
 public:
  InterstitialAdInternalAndroid(InterstitialAd* base);
  ~InterstitialAdInternalAndroid() override;

  Future<void> Initialize(AdParent parent, const char* ad_unit_id) override;
  Future<void> LoadAd(const AdRequest& request) override;
  Future<void> Show() override;

  InterstitialAd::PresentationState GetPresentationState() const override;

 private:
  // Reference to the Java helper object used to interact with the Mobile Ads
  // SDK.
  jobject helper_;
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_INTERSTITIAL_AD_INTERNAL_ANDROID_H_
