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

#include "admob/src/ios/rewarded_video_internal_ios.h"

#import "admob/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {

/// The GADRewardBasedVideoAd shared instance.
GADRewardBasedVideoAd *reward_based_video_ad;

/// The FADRewardBasedVideoAdDelegate object that conforms to the GADRewardBasedVideoAdDelegate
/// protocol.
FADRewardBasedVideoAdDelegate *reward_based_video_ad_delegate;

/// The presentation state of rewarded video.
PresentationState presentation_state;

/// The handle to the future for the last call to LoadAd. This call is different than the other
/// asynchronous calls because it's completed in separate functions (the others are completed by
/// blocks a.k.a. lambdas or closures).
FutureHandle future_handle_for_load;

RewardedVideoInternalIOS::RewardedVideoInternalIOS()
    : RewardedVideoInternal(), destroy_mutex_(Mutex::kModeNonRecursive) {}

RewardedVideoInternalIOS::~RewardedVideoInternalIOS() {
  Destroy();
  destroy_mutex_.Acquire();
  destroy_mutex_.Release();
}

Future<void> RewardedVideoInternalIOS::Initialize() {
  reward_based_video_ad_delegate = [[FADRewardBasedVideoAdDelegate alloc]
      initWithRewardedVideoInternal:this];
  [GADRewardBasedVideoAd sharedInstance].delegate = reward_based_video_ad_delegate;
  presentation_state = kPresentationStateHidden;
  future_handle_for_load = ReferenceCountedFutureImpl::kInvalidHandle;
  CreateAndCompleteFuture(kRewardedVideoFnInitialize, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kRewardedVideoFnInitialize);
}

Future<void> RewardedVideoInternalIOS::LoadAd(const char* ad_unit_id, const AdRequest& request) {
  if (future_handle_for_load != ReferenceCountedFutureImpl::kInvalidHandle) {
    // Checks if an outstanding Future exists.
    // It is safe to call the LoadAd method concurrently on multiple threads; however, the second
    // call will gracefully fail to make sure that only one call to the Mobile Ads SDK's
    // loadRequest:withAdUnitID: method is made at a time.
    CreateAndCompleteFuture(kRewardedVideoFnLoadAd, kAdMobErrorLoadInProgress,
                            kAdLoadInProgressErrorMessage, &future_data_);
    return GetLastResult(kRewardedVideoFnLoadAd);
  }
  // The LoadAd() future is created here, but is completed in the CompleteLoadFuture() method. If
  // the ad request successfully received the ad, CompleteLoadFuture() is called in the
  // RewardBasedVideoAdDidReceiveAd() method with AdMobError equal to admob::kAdMobErrorNone. If the
  // ad request failed, CompleteLoadFuture() is called in the
  // RewardBasedVideoAdDidFailToLoadWithError() method with an AdMobError that's not equal to
  // admob::kAdMobErrorNone.
  future_handle_for_load = CreateFuture(kRewardedVideoFnLoadAd, &future_data_);
  AdRequest *request_copy = new AdRequest;
  *request_copy = request;
  NSString *adUnitId = @(ad_unit_id ? ad_unit_id : "");
  dispatch_async(dispatch_get_main_queue(), ^{
    // A GADRequest from an admob::AdRequest.
    GADRequest *ad_request = GADRequestFromCppAdRequest(*request_copy);
    delete request_copy;
    // Make the reward based video ad request.
    [[GADRewardBasedVideoAd sharedInstance] loadRequest:ad_request
                                           withAdUnitID:adUnitId];
  });
  return GetLastResult(kRewardedVideoFnLoadAd);
}

Future<void> RewardedVideoInternalIOS::Show(AdParent parent) {
  const FutureHandle handle = CreateFuture(kRewardedVideoFnShow, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError api_error = kAdMobErrorLoadInProgress;
    const char* error_msg = kAdLoadInProgressErrorMessage;
    if ([GADRewardBasedVideoAd sharedInstance].isReady) {
      UIView *parent_view = parent;
      [[GADRewardBasedVideoAd sharedInstance]
          presentFromRootViewController:parent_view.window.rootViewController];
      api_error = kAdMobErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(api_error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kRewardedVideoFnShow);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> RewardedVideoInternalIOS::Pause() {
  // Required method. No-op.
  CreateAndCompleteFuture(kRewardedVideoFnPause, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kRewardedVideoFnPause);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> RewardedVideoInternalIOS::Resume() {
  // Required method. No-op.
  CreateAndCompleteFuture(kRewardedVideoFnResume, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kRewardedVideoFnResume);
}

/// Cleans up any resources created in RewardedVideoInternalIOS.
Future<void> RewardedVideoInternalIOS::Destroy() {
  // Clean up any resources created in RewardedVideoInternalIOS.
  const firebase::FutureHandle handle = CreateFuture(kRewardedVideoFnDestroy, &future_data_);
  destroy_mutex_.Acquire();
  void (^destroyBlock)() = ^{
    reward_based_video_ad.delegate = nil;
    reward_based_video_ad_delegate = nil;
    reward_based_video_ad = nil;
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return GetLastResult(kRewardedVideoFnDestroy);
}

PresentationState RewardedVideoInternalIOS::GetPresentationState() {
  return presentation_state;
}

void RewardedVideoInternalIOS::CompleteLoadFuture(AdMobError error, const char* error_msg) {
  CompleteFuture(error, error_msg, future_handle_for_load, &future_data_);
  future_handle_for_load = ReferenceCountedFutureImpl::kInvalidHandle;
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidReceiveAd
    (GADRewardBasedVideoAd *reward_based_video_ad) {
  CompleteLoadFuture(kAdMobErrorNone, nullptr);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidOpen
    (GADRewardBasedVideoAd *reward_based_video_ad) {
  presentation_state = kPresentationStateCoveringUI;
  NotifyListenerOfPresentationStateChange(presentation_state);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidStartPlaying
    (GADRewardBasedVideoAd *reward_based_video_ad) {
  presentation_state = kPresentationStateVideoHasStarted;
  NotifyListenerOfPresentationStateChange(presentation_state);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidCompletePlaying
    (GADRewardBasedVideoAd *reward_based_video_ad) {
  presentation_state = kPresentationStateVideoHasCompleted;
  NotifyListenerOfPresentationStateChange(presentation_state);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidClose
    (GADRewardBasedVideoAd *reward_based_video_ad) {
  presentation_state = kPresentationStateHidden;
  NotifyListenerOfPresentationStateChange(presentation_state);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidRewardUserWithReward
    (GADRewardBasedVideoAd *reward_based_video_ad, GADAdReward *reward) {
  RewardItem reward_item;
  reward_item.amount = reward.amount.floatValue;
  reward_item.reward_type = reward.type.UTF8String;
  NotifyListenerOfReward(reward_item);
}

void RewardedVideoInternalIOS::RewardBasedVideoAdDidFailToLoadWithError
    (GADRewardBasedVideoAd *reward_based_video_ad, NSError *error) {
  AdMobError api_error;
  const char* error_msg = error.localizedDescription.UTF8String;
  switch (error.code) {
    case kGADErrorInvalidRequest:
      api_error = kAdMobErrorInvalidRequest;
      break;
    case kGADErrorNoFill:
      api_error = kAdMobErrorNoFill;
      break;
    case kGADErrorNetworkError:
      api_error = kAdMobErrorNetworkError;
      break;
    case kGADErrorInternalError:
      api_error = kAdMobErrorInternalError;
      break;
    default:
      // NOTE: Changes in the iOS SDK can result in new error codes being added. Fall back to
      // admob::kAdMobErrorInternalError if this SDK doesn't handle error.code.
      LogDebug("Unknown error code %d. Defaulting to internal error.", error.code);
      api_error = kAdMobErrorInternalError;
      error_msg = kInternalSDKErrorMesage;
      break;
  }
  CompleteLoadFuture(api_error, error_msg);
}

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase
