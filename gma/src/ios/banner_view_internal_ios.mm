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

#include "gma/src/ios/banner_view_internal_ios.h"

#import "gma/src/ios/gma_ios.h"
#import "gma/src/ios/FADBannerView.h"
#import "gma/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace gma {
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

Future<void> BannerViewInternalIOS::Initialize(AdParent parent,
      const char* ad_unit_id, const AdSize& size) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnInitialize);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  if(initialized_) {
    CompleteFuture(kAdErrorAlreadyInitialized,
      kAdAlreadyInitializedErrorMessage, future_handle, &future_data_);
  } else {
    initialized_ = true;
    dispatch_async(dispatch_get_main_queue(), ^{
      banner_view_ = [[FADBannerView alloc] initWithView:parent
                                              adUnitID:@(ad_unit_id)
                                                adSize:size
                                    internalBannerView:this];
    CompleteFuture(kAdErrorNone, nullptr, future_handle, &future_data_);
    });
  }
  return future;
}

Future<AdResult> BannerViewInternalIOS::LoadAd(const AdRequest& request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult>* callback_data =
    CreateAdResultFutureCallbackData(kBannerViewFnLoadAd,
                                     &future_data_);
  Future<AdResult> future = MakeFuture(&future_data_.future_impl,
    callback_data->future_handle);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(callback_data, kAdErrorLoadInProgress,
      kAdLoadInProgressErrorMessage);
    return future;
  }
  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdError error_code = kAdErrorNone;
    std::string error_message;

    // Create a GADRequest from an gma::AdRequest.
    GADRequest *ad_request =
     GADRequestFromCppAdRequest(request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if(error_code==kAdErrorNone) {
        error_code = kAdErrorInternalError;
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
  return future;
}

Future<void> BannerViewInternalIOS::SetPosition(int x, int y) {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnSetPosition);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    AdError error = kAdErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (banner_view_) {
      [banner_view_ moveBannerViewToXCoordinate:x yCoordinate:y];
      error = kAdErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, future_handle, &future_data_);
  });
  return future;
}

Future<void> BannerViewInternalIOS::SetPosition(BannerView::Position position) {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnSetPosition);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    AdError error = kAdErrorUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (banner_view_) {
      [banner_view_ moveBannerViewToPosition:position];
      error = kAdErrorNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, future_handle, &future_data_);
  });
  return future;
}

Future<void> BannerViewInternalIOS::Hide() {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnHide);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ hide];
    CompleteFuture(kAdErrorNone, nullptr, future_handle, &future_data_);
  });
  return future;
}

Future<void> BannerViewInternalIOS::Show() {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kBannerViewFnShow);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    [banner_view_ show];
    CompleteFuture(kAdErrorNone, nullptr, future_handle, &future_data_);
  });
  return future;
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Pause() {
  firebase::MutexLock lock(mutex_);
  // Required method. No-op.
  return CreateAndCompleteFuture(kBannerViewFnPause, kAdErrorNone, nullptr,
    &future_data_);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> BannerViewInternalIOS::Resume() {
  firebase::MutexLock lock(mutex_);
  // Required method. No-op.
  return CreateAndCompleteFuture(kBannerViewFnResume, kAdErrorNone, nullptr,
    &future_data_);
}

/// Cleans up any resources created in BannerViewInternalIOS.
Future<void> BannerViewInternalIOS::Destroy() {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
     future_data_.future_impl.SafeAlloc<void>(kBannerViewFnDestroy);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
  destroy_mutex_.Acquire();
  void (^destroyBlock)() = ^{
    [banner_view_ destroy];
    // Remove the FADBannerView (i.e. the container view of GADBannerView)
    // from the superview.
    [banner_view_ removeFromSuperview];
    if (banner_view_) {
      // Consistent with Android SDK which returns a final bounding box of
      // -1 values upon deletion.
      bounding_box_.width = bounding_box_.height =
        bounding_box_.x = bounding_box_.y = -1;
      bounding_box_.position = AdView::kPositionUndefined;
      NotifyListenerOfBoundingBoxChange(bounding_box_);
      banner_view_ = nil;
    }
    if(ad_load_callback_data_ != nil) {
      delete ad_load_callback_data_;
      ad_load_callback_data_ = nil;
    }
    CompleteFuture(kAdErrorNone, nullptr, future_handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return future;
}

BoundingBox BannerViewInternalIOS::bounding_box() const {
  return bounding_box_;
}

void BannerViewInternalIOS::BannerViewDidReceiveAd() {
  firebase::MutexLock lock(mutex_);
  if(ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(ad_load_callback_data_, kAdErrorNone,
      /*error_message=*/"");
    ad_load_callback_data_ = nil;
  }
}

void BannerViewInternalIOS::BannerViewDidFailToReceiveAdWithError(NSError *error) {
  firebase::MutexLock lock(mutex_);
  FIREBASE_ASSERT(error);
  if(ad_load_callback_data_ != nil) {
    CompleteAdResultError(ad_load_callback_data_, error,
                          /*is_load_ad_error=*/true);
    ad_load_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
