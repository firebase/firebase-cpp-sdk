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

#import "admob/src/ios/FADInterstitialDelegate.h"

#include "admob/src/ios/interstitial_ad_internal_ios.h"

@interface FADInterstitialDelegate () {
  /// The InterstitialAdInternalIOS object.
  firebase::admob::internal::InterstitialAdInternalIOS *_interstitialAd;
}

@end

@implementation FADInterstitialDelegate : NSObject

#pragma mark - Initialization

- (instancetype)initWithInternalInterstitialAd:
    (firebase::admob::internal::InterstitialAdInternalIOS *)interstitialAd {
  self = [super init];
  if (self) {
    _interstitialAd = interstitialAd;
  }

  return self;
}

#pragma mark - GADInterstitialDelegate

- (void)interstitialDidReceiveAd:(GADInterstitial *)ad {
  _interstitialAd->InterstitialDidReceiveAd(ad);
}

- (void)interstitial:(GADInterstitial *)ad didFailToReceiveAdWithError:(GADRequestError *)error {
  _interstitialAd->InterstitialDidFailToReceiveAdWithError(ad, error);
}

- (void)interstitialWillPresentScreen:(GADInterstitial *)ad {
  _interstitialAd->InterstitialWillPresentScreen(ad);
}

- (void)interstitialDidDismissScreen:(GADInterstitial *)ad {
  _interstitialAd->InterstitialDidDismissScreen(ad);
}

@end
