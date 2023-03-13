/*
 * Copyright 2021 Google LLC
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

#ifndef FIREBASE_GMA_SRC_IOS_INTERSTITIAL_AD_INTERNAL_IOS_H_
#define FIREBASE_GMA_SRC_IOS_INTERSTITIAL_AD_INTERNAL_IOS_H_

#ifdef __OBJC__
#import "gma/src/ios/FADInterstitialDelegate.h"
#endif  // __OBJC__

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "app/src/include/firebase/internal/mutex.h"
#include "gma/src/common/interstitial_ad_internal.h"

namespace firebase {
namespace gma {
namespace internal {

class InterstitialAdInternalIOS : public InterstitialAdInternal {
 public:
  explicit InterstitialAdInternalIOS(InterstitialAd* base);
  ~InterstitialAdInternalIOS();

  Future<void> Initialize(AdParent parent) override;
  Future<AdResult> LoadAd(const char* ad_unit_id,
                          const AdRequest& request) override;
  Future<void> Show() override;

#ifdef __OBJC__
  void InterstitialDidReceiveAd(GADInterstitialAd* ad);
  void InterstitialDidFailToReceiveAdWithError(NSError* gad_error);
  void InterstitialWillPresentScreen();
  void InterstitialDidDismissScreen();
#endif  // __OBJC__

  bool is_initialized() const override { return initialized_; }

 private:
  /// Prevents duplicate invocations of initialize on the Interstitial Ad.
  bool initialized_;

  /// Contains information to asynchronously complete the LoadAd Future.
  FutureCallbackData<AdResult>* ad_load_callback_data_;

  /// The GADInterstitialAd object. Declared as an "id" type to avoid
  /// referencing an Objective-C class in this header.
  id interstitial_;

  /// The publisher-provided view (UIView) that's the parent view of the
  /// interstitial ad. Declared as an "id" type to avoid referencing an
  /// Objective-C class in this header.
  id parent_view_;

  /// The FADInterstitialDelegate object that conforms to the
  /// GADInterstitialDelegate protocol. Declared as an "id" type to avoid
  /// referencing an Objective-C++ class in this header.
  id interstitial_delegate_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_INTERSTITIAL_AD_INTERNAL_IOS_H_
