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

#import "gma/src/ios/FADAdView.h"
#import "gma/src/ios/FADRequest.h"
#import "gma/src/ios/gma_ios.h"

#include "app/src/util_ios.h"
#include "gma/src/ios/response_info_ios.h"

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
  if (ad_load_callback_data_ != nil) {
    delete ad_load_callback_data_;
    ad_load_callback_data_ = nil;
  }

  id ad_view = ad_view_;
  ad_view_ = nil;

  void (^destroyBlock)() = ^{
    if (ad_view) {
      // Remove the FADAdView (i.e. the container view of GADAdView)
      // from the superview.
      [ad_view removeFromSuperview];
      [ad_view destroy];
    }
  };

  util::DispatchAsyncSafeMainQueue(destroyBlock);
}

Future<void> AdViewInternalIOS::Initialize(AdParent parent, const char* ad_unit_id,
                                           const AdSize& size) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kAdViewFnInitialize);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (initialized_) {
    CompleteFuture(kAdErrorCodeAlreadyInitialized, kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
  } else {
    initialized_ = true;

    // Guard against parameter object destruction before the async operation
    // executes (below).
    AdParent local_ad_parent = parent;
    NSString* local_ad_unit_id = @(ad_unit_id);
    ad_size_ = size;

    dispatch_async(dispatch_get_main_queue(), ^{
      ad_view_ = [[FADAdView alloc] initWithView:local_ad_parent
                                        adUnitID:local_ad_unit_id
                                          adSize:ad_size_
                                  internalAdView:this];
      CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
    });
  }
  return future;
}

Future<AdResult> AdViewInternalIOS::LoadAd(const AdRequest& request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult>* callback_data =
      CreateAdResultFutureCallbackData(kAdViewFnLoadAd, &future_data_);
  Future<AdResult> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                kAdLoadInProgressErrorMessage);
    return future;
  }
  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  // Guard against parameter object destruction before the async operation
  // executes (below).
  AdRequest local_ad_request = request;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error_code = kAdErrorCodeNone;
    std::string error_message;

    // Create a GADRequest from a gma::AdRequest.
    GADRequest* ad_request =
        GADRequestFromCppAdRequest(local_ad_request, &error_code, &error_message);
    if (ad_request == nullptr) {
      if (error_code == kAdErrorCodeNone) {
        error_code = kAdErrorCodeInternalError;
        error_message = kAdCouldNotParseAdRequestErrorMessage;
      }
      CompleteLoadAdInternalError(ad_load_callback_data_, error_code, error_message.c_str());
      ad_load_callback_data_ = nil;
    } else {
      // Make the AdView ad request.
      [(GADBannerView*)ad_view_ loadRequest:ad_request];
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

  // Guard against parameter object destruction before the async operation
  // executes (below).
  AdView::Position local_position = position;

  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error = kAdErrorCodeUninitialized;
    const char* error_msg = kAdUninitializedErrorMessage;
    if (ad_view_) {
      [ad_view_ moveAdViewToPosition:local_position];
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
  return CreateAndCompleteFuture(kAdViewFnPause, kAdErrorCodeNone, nullptr, &future_data_);
}

/// This method is part of the C++ interface and is used only on Android.
Future<void> AdViewInternalIOS::Resume() {
  firebase::MutexLock lock(mutex_);
  // Required method. No-op.
  return CreateAndCompleteFuture(kAdViewFnResume, kAdErrorCodeNone, nullptr, &future_data_);
}

/// Cleans up any resources created in AdViewInternalIOS.
Future<void> AdViewInternalIOS::Destroy() {
  firebase::MutexLock lock(mutex_);
  destroy_mutex_.Acquire();

  const firebase::SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kAdViewFnDestroy);

  if (ad_load_callback_data_ != nil) {
    delete ad_load_callback_data_;
    ad_load_callback_data_ = nil;
  }

  // Consistent with Android SDK which returns a final bounding box of
  // -1 values upon deletion.
  bounding_box_.width = bounding_box_.height = bounding_box_.x = bounding_box_.y = -1;
  bounding_box_.position = AdView::kPositionUndefined;
  NotifyListenerOfBoundingBoxChange(bounding_box_);

  id ad_view = ad_view_;
  ad_view_ = nil;

  void (^destroyBlock)() = ^{
    if (ad_view) {
      // Remove the FADAdView (i.e. the container view of GADAdView)
      // from the superview.
      [ad_view removeFromSuperview];
      [ad_view destroy];
    }

    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
    destroy_mutex_.Release();
  };

  util::DispatchAsyncSafeMainQueue(destroyBlock);
  return MakeFuture(&future_data_.future_impl, future_handle);
}

BoundingBox AdViewInternalIOS::bounding_box() const { return bounding_box_; }

void AdViewInternalIOS::AdViewDidReceiveAd(int width, int height,
                                           GADResponseInfo* gad_response_info) {
  firebase::MutexLock lock(mutex_);
  update_ad_size_dimensions(width, height);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalSuccess(ad_load_callback_data_,
                                  ResponseInfoInternal({gad_response_info}));
    ad_load_callback_data_ = nil;
  }
}

void AdViewInternalIOS::AdViewDidFailToReceiveAdWithError(NSError* error) {
  firebase::MutexLock lock(mutex_);
  FIREBASE_ASSERT(error);
  if (ad_load_callback_data_ != nil) {
    CompleteAdResultError(ad_load_callback_data_, error,
                          /*is_load_ad_error=*/true);
    ad_load_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
