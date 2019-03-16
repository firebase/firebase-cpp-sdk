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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_IOS_REWARDED_VIDEO_INTERNAL_IOS_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_IOS_REWARDED_VIDEO_INTERNAL_IOS_H_

#ifdef __OBJC__
#import "admob/src/ios/FADRewardBasedVideoAdDelegate.h"
#endif  // __OBJC__

#include "admob/src/common/rewarded_video_internal.h"

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {

class RewardedVideoInternalIOS : public RewardedVideoInternal {
 public:
  RewardedVideoInternalIOS();
  virtual ~RewardedVideoInternalIOS();

  Future<void> Initialize() override;
  Future<void> LoadAd(const char* ad_unit_id,
                      const AdRequest& request) override;
  Future<void> Show(AdParent parent) override;
  Future<void> Pause() override;
  Future<void> Resume() override;
  Future<void> Destroy() override;

  PresentationState GetPresentationState() override;

#ifdef __OBJC__
  void RewardBasedVideoAdDidReceiveAd(
      GADRewardBasedVideoAd *reward_based_video_ad);
  void RewardBasedVideoAdDidOpen(GADRewardBasedVideoAd *reward_based_video_ad);
  void RewardBasedVideoAdDidStartPlaying(
      GADRewardBasedVideoAd *reward_based_video_ad);
  void RewardBasedVideoAdDidCompletePlaying(
      GADRewardBasedVideoAd *reward_based_video_ad);
  void RewardBasedVideoAdDidClose(GADRewardBasedVideoAd *reward_based_video_ad);
  void RewardBasedVideoAdDidRewardUserWithReward(
      GADRewardBasedVideoAd *reward_based_video_ad, GADAdReward *reward);
  void RewardBasedVideoAdDidFailToLoadWithError(
      GADRewardBasedVideoAd *reward_based_video_ad, NSError *error);
#endif  // __OBJC__

 private:
  /// Completes the future for the LoadAd function.
  void CompleteLoadFuture(admob::AdMobError error, const char* error_msg);

  /// A mutex used to handle the destroy behavior, as it is asynchronous,
  /// and needs to be waited on in the destructor.
  Mutex destroy_mutex_;
};

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_IOS_REWARDED_VIDEO_INTERNAL_IOS_H_
