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

// An Objective-C++ wrapper class that conforms to the GADRewardBasedVideoAdDelegate protocol.
// When the delegate for receiving state change messages from a GADRewardBasedVideoAd is notified,
// this wrapper class forwards the notification to the RewardedVideoInternalIOS object to handle
// the state changes for rewarded video.

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {
class RewardedVideoInternalIOS;
}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase

NS_ASSUME_NONNULL_BEGIN

@interface FADRewardBasedVideoAdDelegate : NSObject<GADRewardBasedVideoAdDelegate>

/// Returns a FADRewardBasedVideoAdDelegate object with RewardedVideoInternalIOS.
- (FADRewardBasedVideoAdDelegate *)initWithRewardedVideoInternal:
    (firebase::admob::rewarded_video::internal::RewardedVideoInternalIOS *)rewardedVideo;

@end

NS_ASSUME_NONNULL_END
