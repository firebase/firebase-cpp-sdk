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

#include "gma/src/ios/rewarded_ad_internal_ios.h"

#import "gma/src/ios/FADRequest.h"
#import "gma/src/ios/gma_ios.h"

#include "app/src/util_ios.h"
#include "gma/src/ios/response_info_ios.h"

namespace firebase {
namespace gma {
namespace internal {

RewardedAdInternalIOS::RewardedAdInternalIOS(RewardedAd *base)
    : RewardedAdInternal(base),
      initialized_(false),
      ad_load_callback_data_(nil),
      rewarded_ad_(nil),
      parent_view_(nil),
      rewarded_ad_delegate_(nil) {}

RewardedAdInternalIOS::~RewardedAdInternalIOS() {
  firebase::MutexLock lock(mutex_);
  // Clean up any resources created in RewardedAdInternalIOS.
  ((GADRewardedAd *)rewarded_ad_).fullScreenContentDelegate = nil;
  rewarded_ad_delegate_ = nil;
  rewarded_ad_ = nil;
  if (ad_load_callback_data_ != nil) {
    delete ad_load_callback_data_;
    ad_load_callback_data_ = nil;
  }
}

Future<void> RewardedAdInternalIOS::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kRewardedAdFnInitialize);

  if (initialized_) {
    CompleteFuture(kAdErrorCodeAlreadyInitialized, kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
  } else {
    initialized_ = true;
    parent_view_ = (UIView *)parent;
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  }
  return MakeFuture(&future_data_.future_impl, future_handle);
}

Future<AdResult> RewardedAdInternalIOS::LoadAd(const char *ad_unit_id, const AdRequest &request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult> *callback_data =
      CreateAdResultFutureCallbackData(kRewardedAdFnLoadAd, &future_data_);
  SafeFutureHandle<AdResult> future_handle = callback_data->future_handle;

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                kAdLoadInProgressErrorMessage);
    return MakeFuture(&future_data_.future_impl, future_handle);
  }

  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  rewarded_ad_delegate_ = [[FADRewardedAdDelegate alloc] initWithInternalRewardedAd:this];

  // Guard against parameter object destruction before the async operation
  // executes (below).
  AdRequest local_ad_request = request;
  NSString *local_ad_unit_id = @(ad_unit_id);

  dispatch_async(dispatch_get_main_queue(), ^{
    // Create a GADRequest from a gma::AdRequest.
    AdErrorCode error_code = kAdErrorCodeNone;
    std::string error_message;
    GADRequest *ad_request =
        GADRequestFromCppAdRequest(local_ad_request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if (error_code == kAdErrorCodeNone) {
        error_code = kAdErrorCodeInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteLoadAdInternalError(ad_load_callback_data_, error_code, error_message.c_str());
      ad_load_callback_data_ = nil;
    } else {
      // Make the rewarded ad request.
      [GADRewardedAd loadWithAdUnitID:local_ad_unit_id
                              request:ad_request
                    completionHandler:^(GADRewardedAd *ad, NSError *error) {
                      if (error) {
                        RewardedAdDidFailToReceiveAdWithError(error);
                      } else {
                        RewardedAdDidReceiveAd(ad);
                      }
                    }];
    }
  });

  return MakeFuture(&future_data_.future_impl, future_handle);
}

Future<void> RewardedAdInternalIOS::Show(UserEarnedRewardListener *listener) {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> handle =
      future_data_.future_impl.SafeAlloc<void>(kRewardedAdFnShow);
  user_earned_reward_listener_ = listener;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error_code = kAdErrorCodeLoadInProgress;
    const char *error_message = kAdLoadInProgressErrorMessage;
    if (rewarded_ad_ == nil) {
      error_code = kAdErrorCodeUninitialized;
      error_message = kAdUninitializedErrorMessage;
    } else {
      [rewarded_ad_ presentFromRootViewController:[parent_view_ window].rootViewController
                         userDidEarnRewardHandler:^{
                           GADAdReward *reward = ((GADRewardedAd *)rewarded_ad_).adReward;
                           NotifyListenerOfUserEarnedReward(util::NSStringToString(reward.type),
                                                            reward.amount.integerValue);
                         }];
      error_code = kAdErrorCodeNone;
      error_message = nullptr;
    }
    CompleteFuture(error_code, error_message, handle, &future_data_);
  });
  return MakeFuture(&future_data_.future_impl, handle);
}

void RewardedAdInternalIOS::RewardedAdDidReceiveAd(GADRewardedAd *ad) {
  firebase::MutexLock lock(mutex_);
  rewarded_ad_ = ad;
  ad.fullScreenContentDelegate = rewarded_ad_delegate_;
  ad.paidEventHandler = ^void(GADAdValue *_Nonnull adValue) {
    NotifyListenerOfPaidEvent(firebase::gma::ConvertGADAdValueToCppAdValue(adValue));
  };

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalSuccess(ad_load_callback_data_, ResponseInfoInternal({ad.responseInfo}));
    ad_load_callback_data_ = nil;
  }
}

void RewardedAdInternalIOS::RewardedAdDidFailToReceiveAdWithError(NSError *gad_error) {
  firebase::MutexLock lock(mutex_);
  FIREBASE_ASSERT(gad_error);
  if (ad_load_callback_data_ != nil) {
    CompleteAdResultError(ad_load_callback_data_, gad_error,
                          /*is_load_ad_error=*/true);
    ad_load_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
