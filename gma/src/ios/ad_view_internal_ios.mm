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

#include "gma/src/ios/ad_view_internal_ios.h"

#import "gma/src/ios/gma_ios.h"
#import "gma/src/ios/FADAdView.h"
#import "gma/src/ios/FADRequest.h"

#include "app/src/util_ios.h"

namespace firebase {
namespace gma {
namespace internal {

AdViewInternalIOS::AdViewInternalIOS(AdView* base)
    : AdViewInternal(base),
      ad_load_callback_data_(nil),
      ad_view_(nil),
      destroy_mutex_(Mutex::kModeNonRecursive),
      initialized_(false) {}

AdViewInternalIOS::~AdViewInternalIOS() {
  Destroy();
  destroy_mutex_.Acquire();
  destroy_mutex_.Release();
}

Future<void> AdViewInternalIOS::Initialize(AdParent parent,
      const char* ad_unit_id, const AdSize& size) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kAdViewFnInitialize);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  if(initialized_) {
    CompleteFuture(kAdErrorCodeAlreadyInitialized,
      kAdAlreadyInitializedErrorMessage, future_handle, &future_data_);
  } else {
    initialized_ = true;
    ad_size_ = size;
    dispatch_async(dispatch_get_main_queue(), ^{
      ad_view_ = [[FADAdView alloc] initWithView:parent
                                              adUnitID:@(ad_unit_id)
                                                adSize:size
                                    internalAdView:this];
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
    });
  }
  return future;
}

Future<AdResult> AdViewInternalIOS::LoadAd(const AdRequest& request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult>* callback_data =
    CreateAdResultFutureCallbackData(kAdViewFnLoadAd,
                                     &future_data_);
  Future<AdResult> future = MakeFuture(&future_data_.future_impl,
    callback_data->future_handle);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(callback_data, kAdErrorCodeLoadInProgress,
      kAdLoadInProgressErrorMessage);
    return future;
  }
  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error_code = kAdErrorCodeNone;
    std::string error_message;

    // Create a GADRequest from an gma::AdRequest.
    GADRequest *ad_request =
     GADRequestFromCppAdRequest(request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if(error_code==kAdErrorCodeNone) {
        error_code = kAdErrorCodeInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteLoadAdInternalResult(ad_load_callback_data_, error_code,
                                   error_message.c_str());
      ad_load_callback_data_ = nil;
    } else {
      // Make the AdView ad request.
      [ad_view_ loadRequest:ad_request];
    }
  });
  return future;
}

Future<void> AdViewInternalIOS::SetPosition(int x, int y) {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kAdViewFnSetPosition);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error = kAdErrorCodeUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (ad_view_) {
      [ad_view_ moveAdViewToXCoordinate:x yCoordinate:y];
      error = kAdErrorCodeNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, future_handle, &future_data_);
  });
  return future;
}

Future<void> AdViewInternalIOS::SetPosition(AdView::Position position) {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kAdViewFnSetPosition);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error = kAdErrorCodeUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (ad_view_) {
      [ad_view_ moveAdViewToPosition:position];
      error = kAdErrorCodeNone;
      error_msg = nullptr;
    }
    CompleteFuture(error, error_msg, future_handle, &future_data_);
  });
  return future;
}

Future<void> AdViewInternalIOS::Hide() {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kAdViewFnHide);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    [ad_view_ hide];
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  });
  return future;
}

Future<void> AdViewInternalIOS::Show() {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
    future_data_.future_impl.SafeAlloc<void>(kAdViewFnShow);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    [ad_view_ show];
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  });
  return future;
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> AdViewInternalIOS::Pause() {
  firebase::MutexLock lock(mutex_);
  // Required method. No-op.
  return CreateAndCompleteFuture(kAdViewFnPause, kAdErrorCodeNone, nullptr,
    &future_data_);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> AdViewInternalIOS::Resume() {
  firebase::MutexLock lock(mutex_);
  // Required method. No-op.
  return CreateAndCompleteFuture(kAdViewFnResume, kAdErrorCodeNone, nullptr,
    &future_data_);
}

/// Cleans up any resources created in AdViewInternalIOS.
Future<void> AdViewInternalIOS::Destroy() {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
     future_data_.future_impl.SafeAlloc<void>(kAdViewFnDestroy);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
  destroy_mutex_.Acquire();
  void (^destroyBlock)() = ^{
    [ad_view_ destroy];
    // Remove the FADAdView (i.e. the container view of GADAdView)
    // from the superview.
    [ad_view_ removeFromSuperview];
    if (ad_view_) {
      // Consistent with Android SDK which returns a final bounding box of
      // -1 values upon deletion.
      bounding_box_.width = bounding_box_.height =
        bounding_box_.x = bounding_box_.y = -1;
      bounding_box_.position = AdView::kPositionUndefined;
      NotifyListenerOfBoundingBoxChange(bounding_box_);
      ad_view_ = nil;
    }
    if(ad_load_callback_data_ != nil) {
      delete ad_load_callback_data_;
      ad_load_callback_data_ = nil;
    }
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
    destroy_mutex_.Release();
  };
  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return future;
}

BoundingBox AdViewInternalIOS::bounding_box() const {
  return bounding_box_;
}

void AdViewInternalIOS::AdViewDidReceiveAd(int width, int height) {
  firebase::MutexLock lock(mutex_);
  update_ad_size_dimensions(width, height);
  if(ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalResult(ad_load_callback_data_, kAdErrorCodeNone,
      /*error_message=*/"");
    ad_load_callback_data_ = nil;
  }
}

void AdViewInternalIOS::AdViewDidFailToReceiveAdWithError(NSError *error) {
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
