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

#import "admob/src/ios/FADRewardBasedVideoAdDelegate.h"

#include "admob/src/ios/rewarded_video_internal_ios.h"

@interface FADRewardBasedVideoAdDelegate () {
  /// The RewardedVideoInternalIOS object.
  firebase::admob::rewarded_video::internal::RewardedVideoInternalIOS *_rewardedVideo;
}

@end

@implementation FADRewardBasedVideoAdDelegate : NSObject

#pragma mark - Initialization

- (instancetype)initWithRewardedVideoInternal:
    (firebase::admob::rewarded_video::internal::RewardedVideoInternalIOS *)rewardedVideo {
  self = [super init];
  if (self) {
    _rewardedVideo = rewardedVideo;
  }

  return self;
}

#pragma mark - GADRewardBasedVideoAdDelegate

- (void)rewardBasedVideoAdDidReceiveAd:(GADRewardBasedVideoAd *)rewardBasedVideoAd {
  _rewardedVideo->RewardBasedVideoAdDidReceiveAd(rewardBasedVideoAd);
}

- (void)rewardBasedVideoAdDidOpen:(GADRewardBasedVideoAd *)rewardBasedVideoAd {
  _rewardedVideo->RewardBasedVideoAdDidOpen(rewardBasedVideoAd);
}

- (void)rewardBasedVideoAdDidStartPlaying:(GADRewardBasedVideoAd *)rewardBasedVideoAd {
  _rewardedVideo->RewardBasedVideoAdDidStartPlaying(rewardBasedVideoAd);
}

- (void)rewardBasedVideoAdDidCompletePlaying:(GADRewardBasedVideoAd *)rewardBasedVideoAd {
  _rewardedVideo->RewardBasedVideoAdDidCompletePlaying(rewardBasedVideoAd);
}

- (void)rewardBasedVideoAdDidClose:(GADRewardBasedVideoAd *)rewardBasedVideoAd {
  _rewardedVideo->RewardBasedVideoAdDidClose(rewardBasedVideoAd);
}

- (void)rewardBasedVideoAd:(GADRewardBasedVideoAd *)rewardBasedVideoAd
   didRewardUserWithReward:(GADAdReward *)reward {
  _rewardedVideo->RewardBasedVideoAdDidRewardUserWithReward(rewardBasedVideoAd, reward);
}

- (void)rewardBasedVideoAd:(GADRewardBasedVideoAd *)rewardBasedVideoAd
    didFailToLoadWithError:(NSError *)error {
  _rewardedVideo->RewardBasedVideoAdDidFailToLoadWithError(rewardBasedVideoAd, error);
}

@end
