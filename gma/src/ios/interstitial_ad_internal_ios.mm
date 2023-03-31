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

#include "gma/src/ios/interstitial_ad_internal_ios.h"

#import "gma/src/ios/FADRequest.h"
#import "gma/src/ios/gma_ios.h"

#include "app/src/util_ios.h"
#include "gma/src/ios/response_info_ios.h"

namespace firebase {
namespace gma {
namespace internal {

InterstitialAdInternalIOS::InterstitialAdInternalIOS(InterstitialAd *base)
    : InterstitialAdInternal(base),
      initialized_(false),
      ad_load_callback_data_(nil),
      interstitial_(nil),
      parent_view_(nil),
      interstitial_delegate_(nil) {}

InterstitialAdInternalIOS::~InterstitialAdInternalIOS() {
  firebase::MutexLock lock(mutex_);
  ((GADInterstitialAd *)interstitial_).fullScreenContentDelegate = nil;
  interstitial_delegate_ = nil;
  interstitial_ = nil;
  if (ad_load_callback_data_ != nil) {
    delete ad_load_callback_data_;
    ad_load_callback_data_ = nil;
  }
}

Future<void> InterstitialAdInternalIOS::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kInterstitialAdFnInitialize);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  if (initialized_) {
    CompleteFuture(kAdErrorCodeAlreadyInitialized, kAdAlreadyInitializedErrorMessage, future_handle,
                   &future_data_);
  } else {
    initialized_ = true;
    parent_view_ = (UIView *)parent;
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  }
  return future;
}

Future<AdResult> InterstitialAdInternalIOS::LoadAd(const char *ad_unit_id,
                                                   const AdRequest &request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult> *callback_data =
      CreateAdResultFutureCallbackData(kInterstitialAdFnLoadAd, &future_data_);
  Future<AdResult> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                kAdLoadInProgressErrorMessage);
    return future;
  }

  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  interstitial_delegate_ = [[FADInterstitialDelegate alloc] initWithInternalInterstitialAd:this];

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
      // Make the interstitial ad request.
      [GADInterstitialAd loadWithAdUnitID:local_ad_unit_id
                                  request:ad_request
                        completionHandler:^(GADInterstitialAd *ad, NSError *error)  // NO LINT
                                          {
                                            if (error) {
                                              InterstitialDidFailToReceiveAdWithError(error);
                                            } else {
                                              InterstitialDidReceiveAd(ad);
                                            }
                                          }];
    }
  });

  return future;
}

Future<void> InterstitialAdInternalIOS::Show() {
  firebase::MutexLock lock(mutex_);
  const firebase::SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kInterstitialAdFnShow);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);
  dispatch_async(dispatch_get_main_queue(), ^{
    AdErrorCode error_code = kAdErrorCodeLoadInProgress;
    const char *error_message = kAdLoadInProgressErrorMessage;
    if (interstitial_ == nil) {
      error_code = kAdErrorCodeUninitialized;
      error_message = kAdUninitializedErrorMessage;
    } else {
      [interstitial_ presentFromRootViewController:[parent_view_ window].rootViewController];
      error_code = kAdErrorCodeNone;
      error_message = nullptr;
    }
    CompleteFuture(error_code, error_message, future_handle, &future_data_);
  });
  return future;
}

void InterstitialAdInternalIOS::InterstitialDidReceiveAd(GADInterstitialAd *ad) {
  firebase::MutexLock lock(mutex_);
  interstitial_ = ad;
  ad.fullScreenContentDelegate = interstitial_delegate_;
  ad.paidEventHandler = ^void(GADAdValue *_Nonnull adValue) {
    NotifyListenerOfPaidEvent(firebase::gma::ConvertGADAdValueToCppAdValue(adValue));
  };

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalSuccess(ad_load_callback_data_, ResponseInfoInternal({ad.responseInfo}));
    ad_load_callback_data_ = nil;
  }
}

void InterstitialAdInternalIOS::InterstitialDidFailToReceiveAdWithError(NSError *gad_error) {
  firebase::MutexLock lock(mutex_);
  FIREBASE_ASSERT(gad_error);
  if (ad_load_callback_data_ != nil) {
    CompleteAdResultError(ad_load_callback_data_, gad_error, /*is_load_ad_error=*/true);
    ad_load_callback_data_ = nil;
  }
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
