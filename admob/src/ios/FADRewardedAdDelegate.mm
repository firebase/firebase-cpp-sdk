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

#import "admob/src/ios/FADRewardedAdDelegate.h"

#include "admob/src/ios/ad_result_ios.h"
#include "admob/src/ios/rewarded_ad_internal_ios.h"

@interface FADRewardedAdDelegate () {
  /// The RewardedAdInternalIOS object.
  firebase::admob::internal::RewardedAdInternalIOS *_rewardedAd;
}
@end

@implementation FADRewardedAdDelegate : NSObject

#pragma mark - Initialization

- (instancetype)initWithInternalRewardedAd:
    (firebase::admob::internal::RewardedAdInternalIOS *)rewardedAd {
  self = [super init];
  if (self) {
    _rewardedAd = rewardedAd;
  }

  return self;
}

#pragma mark - GADFullScreenContentDelegate

// Capture AdMob iOS Full screen events and forward them to our C++
// translation layer.
- (void)adDidRecordImpression:(nonnull id<GADFullScreenPresentingAd>)ad {
  _rewardedAd->NotifyListenerOfAdImpression();
}

- (void)adDidRecordClick:(nonnull id<GADFullScreenPresentingAd>)ad {
  _rewardedAd->NotifyListenerOfAdClickedFullScreenContent();
}

- (void)ad:(nonnull id<GADFullScreenPresentingAd>)ad
didFailToPresentFullScreenContentWithError:(nonnull NSError *)error {
  firebase::admob::AdResultInternal ad_result_internal;
  ad_result_internal.is_wrapper_error = false;
  ad_result_internal.is_successful = false;
  ad_result_internal.ios_error = error;
  // Invoke AdMobInternal, a friend of AdResult, to have it access its
  // protected constructor with the AdError data.
  const firebase::admob::AdResult& ad_result = firebase::admob::AdMobInternal::CreateAdResult(ad_result_internal);
  _rewardedAd->NotifyListenerOfAdFailedToShowFullScreenContent(ad_result);
}

- (void)adDidPresentFullScreenContent:(nonnull id<GADFullScreenPresentingAd>)ad {
  _rewardedAd->NotifyListenerOfAdShowedFullScreenContent();
}

- (void)adDidDismissFullScreenContent:(nonnull id<GADFullScreenPresentingAd>)ad {
  _rewardedAd->NotifyListenerOfAdDismissedFullScreenContent();
}

@end
