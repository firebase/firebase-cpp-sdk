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

#import "gma/src/ios/FADInterstitialDelegate.h"

#include "gma/src/ios/ad_error_ios.h"
#include "gma/src/ios/interstitial_ad_internal_ios.h"

@interface FADInterstitialDelegate () {
  /// The InterstitialAdInternalIOS object.
  firebase::gma::internal::InterstitialAdInternalIOS *_interstitialAd;
}
@end

@implementation FADInterstitialDelegate : NSObject

#pragma mark - Initialization

- (instancetype)initWithInternalInterstitialAd:
    (firebase::gma::internal::InterstitialAdInternalIOS *)interstitialAd {
  self = [super init];
  if (self) {
    _interstitialAd = interstitialAd;
  }

  return self;
}

#pragma mark - GADFullScreenContentDelegate

// Capture GMA iOS Full screen events and forward them to our C++
// translation layer.
- (void)adDidRecordImpression:(nonnull id<GADFullScreenPresentingAd>)ad {
  _interstitialAd->NotifyListenerOfAdImpression();
}

- (void)adDidRecordClick:(nonnull id<GADFullScreenPresentingAd>)ad {
  _interstitialAd->NotifyListenerOfAdClickedFullScreenContent();
}

- (void)ad:(nonnull id<GADFullScreenPresentingAd>)ad
    didFailToPresentFullScreenContentWithError:(nonnull NSError *)error {
  firebase::gma::AdErrorInternal ad_error_internal;
  ad_error_internal.ad_error_type =
      firebase::gma::AdErrorInternal::kAdErrorInternalFullScreenContentError;
  ad_error_internal.is_successful = false;
  ad_error_internal.native_ad_error = error;
  // Invoke GmaInternal, a friend of AdResult, to have it access its
  // protected constructor with the AdError data.
  const firebase::gma::AdError &ad_error =
      firebase::gma::GmaInternal::CreateAdError(ad_error_internal);
  _interstitialAd->NotifyListenerOfAdFailedToShowFullScreenContent(ad_error);
}

- (void)adWillPresentFullScreenContent:(nonnull id<GADFullScreenPresentingAd>)ad {
  _interstitialAd->NotifyListenerOfAdShowedFullScreenContent();
}

- (void)adDidDismissFullScreenContent:(nonnull id<GADFullScreenPresentingAd>)ad {
  _interstitialAd->NotifyListenerOfAdDismissedFullScreenContent();
}

@end
