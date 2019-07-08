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

#include "admob/src/ios/banner_view_internal_ios.h"

#import "admob/src/ios/FADBannerView.h"
#import "admob/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace internal {

BannerViewInternalIOS::BannerViewInternalIOS(BannerView* base)
    : BannerViewInternal(base),
      future_handle_for_load_(ReferenceCountedFutureImpl::kInvalidHandle),
      banner_view_(nil),
      destroy_mutex_(Mutex::kModeNonRecursive) {}

BannerViewInternalIOS::~BannerViewInternalIOS() {
  Destroy();
  destroy_mutex_.Acquire();
  destroy_mutex_.Release();
}

Future<void> BannerViewInternalIOS::Initialize(AdParent parent, const char* ad_unit_id,
                                               AdSize size) {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnInitialize, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    banner_view_ = [[FADBannerView alloc] initWithView:parent
                                              adUnitID:@(ad_unit_id)
                                                adSize:size
                                    internalBannerView:this];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
  });
  return GetLastResult(kBannerViewFnInitialize);
}

Future<void> BannerViewInternalIOS::LoadAd(const AdRequest& request) {
  if (future_handle_for_load_ != ReferenceCountedFutureImpl::kInvalidHandle) {
    // Checks if an outstanding Future exists.
    // It is safe to call the LoadAd method concurrently on multiple threads; however, the second
    // call will gracefully fail to make sure that only one call to the Mobile Ads SDK's
    // loadRequest: method is made at a time.
    CreateAndCompleteFuture(kBannerViewFnLoadAd, kAdMobErrorLoadInProgress,
                            kAdLoadInProgressErrorMessage, &future_data_);
    return GetLastResult(kBannerViewFnLoadAd);
  }
  // The LoadAd() future is created here, but is completed in the CompleteLoadFuture() method. If
  // the ad request successfully received the ad, CompleteLoadFuture() is called in the
  // adViewDidReceiveAd: callback with AdMobError equal to admob::kAdMobErrorNone. If the ad request
  // failed, CompleteLoadFuture() is called in the adView:didFailToReceiveAdWithError: callback with
  // an AdMobError that's not equal to admob::kAdMobErrorNone.
  future_handle_for_load_ = CreateFuture(kBannerViewFnLoadAd, &future_data_);
  AdRequest *request_copy = new AdRequest;
  *request_copy = request;
  dispatch_async(dispatch_get_main_queue(), ^{
    // A GADRequest from an admob::AdRequest.
    GADRequest *ad_request = GADRequestFromCppAdRequest(*request_copy);
    delete request_copy;
    // Make the banner view ad request.
    [banner_view_ loadRequest:ad_request];
  });
  return GetLastResult(kBannerViewFnLoadAd);
}

Future<void> BannerViewInternalIOS::Hide() {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnHide, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ hide];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
  });
  return GetLastResult(kBannerViewFnHide);
}

Future<void> BannerViewInternalIOS::Show() {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnShow, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ show];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
    NotifyListenerOfBoundingBoxChange([banner_view_ boundingBox]);
  });
  return GetLastResult(kBannerViewFnShow);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Pause() {
  // Required method. No-op.
  CreateAndCompleteFuture(kBannerViewFnPause, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kBannerViewFnPause);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Resume() {
  // Required method. No-op.
  CreateAndCompleteFuture(kBannerViewFnResume, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kBannerViewFnResume);
}

/// Cleans up any resources created in BannerViewInternalIOS.
Future<void> BannerViewInternalIOS::Destroy() {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnDestroy, &future_data_);
  destroy_mutex_.Acquire();
  void (^destroyBlock)() = ^{
    [banner_view_ destroy];
    // Remove the FADBannerView (i.e. the container view of GADBannerView) from the superview.
    [banner_view_ removeFromSuperview];
    if (banner_view_) {
      NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
      NotifyListenerOfBoundingBoxChange([banner_view_ boundingBox]);
      banner_view_ = nil;
    }
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return GetLastResult(kBannerViewFnDestroy);
}

Future<void> BannerViewInternalIOS::MoveTo(int x, int y) {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnMoveTo, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError error = kAdMobErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (banner_view_) {
      [banner_view_ moveBannerViewToXCoordinate:x yCoordinate:y];
      error = kAdMobErrorNone;
      error_msg = nullptr;
      NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
    }
    CompleteFuture(error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kBannerViewFnMoveTo);
}

Future<void> BannerViewInternalIOS::MoveTo(BannerView::Position position) {
  const firebase::FutureHandle handle = CreateFuture(kBannerViewFnMoveTo, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError error = kAdMobErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (banner_view_) {
      [banner_view_ moveBannerViewToPosition:position];
      error = kAdMobErrorNone;
      error_msg = nullptr;
      NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
    }
    CompleteFuture(error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kBannerViewFnMoveTo);
}

BannerView::PresentationState BannerViewInternalIOS::GetPresentationState() const {
  return [banner_view_ presentationState];
}

BoundingBox BannerViewInternalIOS::GetBoundingBox() const {
  return [banner_view_ boundingBox];
}

void BannerViewInternalIOS::CompleteLoadFuture(AdMobError error, const char* error_msg) {
  CompleteFuture(error, error_msg, future_handle_for_load_, &future_data_);
  future_handle_for_load_ = ReferenceCountedFutureImpl::kInvalidHandle;
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
