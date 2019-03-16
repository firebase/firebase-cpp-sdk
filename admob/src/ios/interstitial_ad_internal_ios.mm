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

#include "admob/src/ios/interstitial_ad_internal_ios.h"

#import "admob/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace internal {

InterstitialAdInternalIOS::InterstitialAdInternalIOS(InterstitialAd* base)
    : InterstitialAdInternal(base),
    presentation_state_(InterstitialAd::kPresentationStateHidden),
    future_handle_for_load_(ReferenceCountedFutureImpl::kInvalidHandle), interstitial_(nil),
    parent_view_(nil), interstitial_delegate_(nil) {}

InterstitialAdInternalIOS::~InterstitialAdInternalIOS() {
  // Clean up any resources created in InterstitialAdInternalIOS.
  Mutex mutex(Mutex::kModeNonRecursive);
  __block Mutex *mutex_in_block = &mutex;
  mutex.Acquire();
  void (^destroyBlock)() = ^{
    ((GADInterstitial *)interstitial_).delegate = nil;
    interstitial_delegate_ = nil;
    interstitial_ = nil;
    mutex_in_block->Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  mutex.Acquire();
  mutex.Release();
}

Future<void> InterstitialAdInternalIOS::Initialize(AdParent parent, const char* ad_unit_id) {
  parent_view_ = (UIView *)parent;
  interstitial_ = [[GADInterstitial alloc] initWithAdUnitID:@(ad_unit_id)];
  interstitial_delegate_ = [[FADInterstitialDelegate alloc] initWithInternalInterstitialAd:this];
  ((GADInterstitial *)interstitial_).delegate = interstitial_delegate_;
  CreateAndCompleteFuture(kInterstitialAdFnInitialize, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kInterstitialAdFnInitialize);
}

Future<void> InterstitialAdInternalIOS::LoadAd(const AdRequest& request) {
  if (future_handle_for_load_ != ReferenceCountedFutureImpl::kInvalidHandle) {
    // Checks if an outstanding Future exists.
    // It is safe to call the LoadAd method concurrently on multiple threads; however, the second
    // call will gracefully fail to make sure that only one call to the Mobile Ads SDK's
    // loadRequest: method is made at a time.
    CreateAndCompleteFuture(kInterstitialAdFnLoadAd, kAdMobErrorLoadInProgress,
                            kAdLoadInProgressErrorMessage, &future_data_);
    return GetLastResult(kInterstitialAdFnLoadAd);
  }
  // The LoadAd() future is created here, but is completed in the CompleteLoadFuture() method. If
  // the ad request successfully received the ad, CompleteLoadFuture() is called in the
  // InterstitialDidReceiveAd() method with AdMobError equal to admob::kAdMobErrorNone. If the ad
  // request failed, CompleteLoadFuture() is called in the
  // InterstitialDidFailToReceiveAdWithError() method with an AdMobError that's not equal to
  // admob::kAdMobErrorNone.
  future_handle_for_load_ = CreateFuture(kInterstitialAdFnLoadAd, &future_data_);
  AdRequest *request_copy = new AdRequest;
  *request_copy = request;
  dispatch_async(dispatch_get_main_queue(), ^{
    // A GADRequest from an admob::AdRequest.
    GADRequest *ad_request = GADRequestFromCppAdRequest(*request_copy);
    delete request_copy;
    // Make the interstitial ad request.
    [interstitial_ loadRequest:ad_request];
  });
  return GetLastResult(kInterstitialAdFnLoadAd);
}

Future<void> InterstitialAdInternalIOS::Show() {
  const firebase::FutureHandle handle = CreateFuture(kInterstitialAdFnShow, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError api_error = kAdMobErrorLoadInProgress;
    const char* error_msg = kAdLoadInProgressErrorMessage;
    if ([interstitial_ isReady]) {
      [interstitial_ presentFromRootViewController:[parent_view_ window].rootViewController];
      api_error = kAdMobErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(api_error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kInterstitialAdFnShow);
}

InterstitialAd::PresentationState InterstitialAdInternalIOS::GetPresentationState() const {
  return presentation_state_;
}

void InterstitialAdInternalIOS::CompleteLoadFuture(AdMobError error, const char* error_msg) {
  CompleteFuture(error, error_msg, future_handle_for_load_, &future_data_);
  future_handle_for_load_ = ReferenceCountedFutureImpl::kInvalidHandle;
}

void InterstitialAdInternalIOS::InterstitialDidReceiveAd(GADInterstitial *interstitial) {
  CompleteLoadFuture(kAdMobErrorNone, nullptr);
}

void InterstitialAdInternalIOS::InterstitialDidFailToReceiveAdWithError(
    GADInterstitial *interstitial, GADRequestError *error) {
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

void InterstitialAdInternalIOS::InterstitialWillPresentScreen(GADInterstitial *interstitial) {
  presentation_state_ = InterstitialAd::kPresentationStateCoveringUI;
  NotifyListenerOfPresentationStateChange(presentation_state_);
}

void InterstitialAdInternalIOS::InterstitialDidDismissScreen(GADInterstitial *interstitial) {
  presentation_state_ = InterstitialAd::kPresentationStateHidden;
  NotifyListenerOfPresentationStateChange(presentation_state_);
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
