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

#include "admob/src/ios/native_express_ad_view_internal_ios.h"

#import "admob/src/ios/FADNativeExpressAdView.h"
#import "admob/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace internal {

NativeExpressAdViewInternalIOS::NativeExpressAdViewInternalIOS(NativeExpressAdView* base)
    : NativeExpressAdViewInternal(base),
      future_handle_for_load_(ReferenceCountedFutureImpl::kInvalidHandle),
      native_express_ad_view_(nil),
      destroy_mutex_(Mutex::kModeNonRecursive) {}

NativeExpressAdViewInternalIOS::~NativeExpressAdViewInternalIOS() {
  Destroy();
  destroy_mutex_.Acquire();
  destroy_mutex_.Release();
}

Future<void> NativeExpressAdViewInternalIOS::Initialize(AdParent parent, const char* ad_unit_id,
                                                        AdSize size) {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnInitialize,
                                                     &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    native_express_ad_view_ = [[FADNativeExpressAdView alloc] initWithView:parent
                                                                  adUnitID:@(ad_unit_id)
                                                                    adSize:size
                                               internalNativeExpressAdView:this];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
  });
  return GetLastResult(kNativeExpressAdViewFnInitialize);
}

Future<void> NativeExpressAdViewInternalIOS::LoadAd(const AdRequest& request) {
  if (future_handle_for_load_ != ReferenceCountedFutureImpl::kInvalidHandle) {
    // Checks if an outstanding Future exists.
    // It is safe to call the LoadAd method concurrently on multiple threads; however, the second
    // call will gracefully fail to make sure that only one call to the Mobile Ads SDK's
    // loadRequest: method is made at a time.
    CreateAndCompleteFuture(kNativeExpressAdViewFnLoadAd, kAdMobErrorLoadInProgress,
                            kAdLoadInProgressErrorMessage, &future_data_);
    return GetLastResult(kNativeExpressAdViewFnLoadAd);
  }
  // The LoadAd() future is created here, but is completed in the CompleteLoadFuture() method. If
  // the ad request successfully received the ad, CompleteLoadFuture() is called in the
  // nativeExpressAdViewDidReceiveAd: callback with AdMobError equal to admob::kAdMobErrorNone. If
  // the ad request failed, CompleteLoadFuture() is called in the
  // nativeExpressAdView:didFailToReceiveAdWithError: callback with an AdMobError that's not equal
  // to admob::kAdMobErrorNone.
  future_handle_for_load_ = CreateFuture(kNativeExpressAdViewFnLoadAd, &future_data_);
  const AdRequest request_copy = request;
  dispatch_async(dispatch_get_main_queue(), ^{
    // A GADRequest from an admob::AdRequest.
    GADRequest *ad_request = GADRequestFromCppAdRequest(request_copy);
    // Make the native express ad view ad request.
    [native_express_ad_view_ loadRequest:ad_request];
  });
  return GetLastResult(kNativeExpressAdViewFnLoadAd);
}

Future<void> NativeExpressAdViewInternalIOS::Hide() {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnHide, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    [native_express_ad_view_ hide];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([native_express_ad_view_ presentationState]);
  });
  return GetLastResult(kNativeExpressAdViewFnHide);
}

Future<void> NativeExpressAdViewInternalIOS::Show() {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnShow, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    [native_express_ad_view_ show];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([native_express_ad_view_ presentationState]);
    NotifyListenerOfBoundingBoxChange([native_express_ad_view_ boundingBox]);
  });
  return GetLastResult(kNativeExpressAdViewFnShow);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> NativeExpressAdViewInternalIOS::Pause() {
  // Required method. No-op.
  CreateAndCompleteFuture(kNativeExpressAdViewFnPause, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kNativeExpressAdViewFnPause);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> NativeExpressAdViewInternalIOS::Resume() {
  // Required method. No-op.
  CreateAndCompleteFuture(kNativeExpressAdViewFnResume, kAdMobErrorNone, nullptr, &future_data_);
  return GetLastResult(kNativeExpressAdViewFnResume);
}

/// Cleans up any resources created in NativeExpressAdViewInternalIOS.
Future<void> NativeExpressAdViewInternalIOS::Destroy() {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnDestroy, &future_data_);
  destroy_mutex_.Acquire();
  void (^destroyBlock)() = ^{
    [native_express_ad_view_ destroy];
    // Remove the FADNativeExpressAdView (i.e. the container view of GADNativeExpressAdView) from
    // the superview.
    [native_express_ad_view_ removeFromSuperview];
    if (native_express_ad_view_) {
      NotifyListenerOfPresentationStateChange([native_express_ad_view_ presentationState]);
      NotifyListenerOfBoundingBoxChange([native_express_ad_view_ boundingBox]);
      native_express_ad_view_ = nil;
    }
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return GetLastResult(kNativeExpressAdViewFnDestroy);
}

Future<void> NativeExpressAdViewInternalIOS::MoveTo(int x, int y) {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnMoveTo, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError error = kAdMobErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (native_express_ad_view_) {
      [native_express_ad_view_ moveNativeExpressAdViewToXCoordinate:x yCoordinate:y];
      error = kAdMobErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kNativeExpressAdViewFnMoveTo);
}

Future<void> NativeExpressAdViewInternalIOS::MoveTo(NativeExpressAdView::Position position) {
  const firebase::FutureHandle handle = CreateFuture(kNativeExpressAdViewFnMoveTo, &future_data_);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError error = kAdMobErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (native_express_ad_view_) {
      [native_express_ad_view_ moveNativeExpressAdViewToPosition:position];
      error = kAdMobErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, handle, &future_data_);
  });
  return GetLastResult(kNativeExpressAdViewFnMoveTo);
}

NativeExpressAdView::PresentationState NativeExpressAdViewInternalIOS::GetPresentationState()
    const {
  return [native_express_ad_view_ presentationState];
}

BoundingBox NativeExpressAdViewInternalIOS::GetBoundingBox() const {
  return [native_express_ad_view_ boundingBox];
}

void NativeExpressAdViewInternalIOS::CompleteLoadFuture(AdMobError error, const char* error_msg) {
  CompleteFuture(error, error_msg, future_handle_for_load_, &future_data_);
  future_handle_for_load_ = ReferenceCountedFutureImpl::kInvalidHandle;
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
