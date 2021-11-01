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

#import "admob/src/ios/admob_ios.h"
#import "admob/src/ios/FADBannerView.h"
#import "admob/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace admob {
namespace internal {

BannerViewInternalIOS::BannerViewInternalIOS(BannerView* base)
    : BannerViewInternal(base),
      ad_load_callback_data_(nil),
      initialized_(false),
      banner_view_(nil),
      destroy_mutex_(Mutex::kModeNonRecursive) {}

BannerViewInternalIOS::~BannerViewInternalIOS() {
  Destroy();
  destroy_mutex_.Acquire();
  destroy_mutex_.Release();
}

Future<void> BannerViewInternalIOS::Initialize(AdParent parent, const char* ad_unit_id,
                                               const AdSize& size) {
  const SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnInitialize);

  if(initialized_) {
    CompleteFuture(kAdMobErrorAlreadyInitialized,
      kAdAlreadyInitializedErrorMessage, future_handle, &future_data_);
  } else {
    initialized_ = true;
    dispatch_async(dispatch_get_main_queue(), ^{
      banner_view_ = [[FADBannerView alloc] initWithView:parent
                                              adUnitID:@(ad_unit_id)
                                                adSize:size
                                    internalBannerView:this];
    CompleteFuture(kAdMobErrorNone, nullptr, future_handle, &future_data_);
    });
  }
  return MakeFuture(&future_data_.future_impl, future_handle);
}

Future<LoadAdResult> BannerViewInternalIOS::LoadAd(const AdRequest& request) {
  FutureCallbackData<LoadAdResult>* callback_data =
    CreateLoadAdResultFutureCallbackData(kBannerViewFnLoadAd,
                                         &future_data_);
  SafeFutureHandle<LoadAdResult> future_handle = callback_data->future_handle;

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(callback_data, kAdMobErrorLoadInProgress,
      kAdLoadInProgressErrorMessage);
    return MakeFuture(&future_data_.future_impl, future_handle);
  }
  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the LoadAdResult.
  ad_load_callback_data_ = callback_data;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdMobError error_code = kAdMobErrorNone;
    std::string error_message;

    // Create a GADRequest from an admob::AdRequest.
    GADRequest *ad_request =
     GADRequestFromCppAdRequest(request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if(error_code==kAdMobErrorNone) {
        error_code = kAdMobErrorInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteLoadAdInternalResult(ad_load_callback_data_, error_code,
                                   error_message.c_str());
      ad_load_callback_data_ = nil;
    } else {
      // Make the banner view ad request.
      [banner_view_ loadRequest:ad_request];
    }
  });
  return MakeFuture(&future_data_.future_impl, future_handle);
}

Future<void> BannerViewInternalIOS::Hide() {
  const SafeFutureHandle<void> handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnHide);
  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ hide];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
  });
  return MakeFuture(&future_data_.future_impl, handle);
}

Future<void> BannerViewInternalIOS::Show() {
  const firebase::SafeFutureHandle<void> handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnShow);
  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ show];
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    NotifyListenerOfPresentationStateChange([banner_view_ presentationState]);
    NotifyListenerOfBoundingBoxChange([banner_view_ boundingBox]);
  });
  return MakeFuture(&future_data_.future_impl, handle);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Pause() {
  // Required method. No-op.
  return CreateAndCompleteFuture(kBannerViewFnPause, kAdMobErrorNone, nullptr,
    &future_data_);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Resume() {
  // Required method. No-op.
  return CreateAndCompleteFuture(kBannerViewFnResume, kAdMobErrorNone, nullptr, &future_data_);
}

/// Cleans up any resources created in BannerViewInternalIOS.
Future<void> BannerViewInternalIOS::Destroy() {
  const firebase::SafeFutureHandle<void> handle =
     future_data_.future_impl.SafeAlloc<void>(kBannerViewFnDestroy);
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
    if(ad_load_callback_data_ != nil) {
      delete ad_load_callback_data_;
      ad_load_callback_data_ = nil;
    }
    CompleteFuture(kAdMobErrorNone, nullptr, handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return MakeFuture(&future_data_.future_impl, handle);
}

Future<void> BannerViewInternalIOS::MoveTo(int x, int y) {
  const firebase::SafeFutureHandle<void> handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnMoveTo);
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
  return MakeFuture(&future_data_.future_impl, handle);
}

Future<void> BannerViewInternalIOS::MoveTo(BannerView::Position position) {
  const firebase::SafeFutureHandle<void> handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnMoveTo);
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
  return MakeFuture(&future_data_.future_impl, handle);
}

BannerView::PresentationState BannerViewInternalIOS::GetPresentationState() const {
  return [banner_view_ presentationState];
}

BoundingBox BannerViewInternalIOS::GetBoundingBox() const {
  return [banner_view_ boundingBox];
}

void BannerViewInternalIOS::BannerViewDidReceiveAd() {
  if(ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(ad_load_callback_data_, kAdMobErrorNone, /*error_message=*/"");
    ad_load_callback_data_ = nil;
  }
}

void BannerViewInternalIOS::BannerViewDidFailToReceiveAdWithError(NSError *error) {
  FIREBASE_ASSERT(error);
  if(ad_load_callback_data_ != nil) {
    CompleteLoadAdIOSResult(ad_load_callback_data_, error);
    ad_load_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
