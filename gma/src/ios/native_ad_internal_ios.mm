/*
 * Copyright 2023 Google LLC
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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include "gma/src/ios/native_ad_internal_ios.h"

#import "gma/src/ios/FADRequest.h"
#import "gma/src/ios/GADNativeAdCpp.h"
#import "gma/src/ios/gma_ios.h"

#include "app/src/util_ios.h"
#include "gma/src/ios/response_info_ios.h"
#include "gma/src/ios/native_ad_image_ios.h"

namespace firebase {
namespace gma {
namespace internal {

NativeAdInternalIOS::NativeAdInternalIOS(NativeAd *base)
    : NativeAdInternal(base),
      initialized_(false),
      ad_load_callback_data_(nil),
      native_ad_(nil),
      parent_view_(nil),
      native_ad_delegate_(nil),
      ad_loader_(nil) {}

NativeAdInternalIOS::~NativeAdInternalIOS() {
  firebase::MutexLock lock(mutex_);
  native_ad_delegate_ = nil;
  native_ad_ = nil;
  ad_loader_ = nil;
  if (ad_load_callback_data_ != nil) {
    delete ad_load_callback_data_;
    ad_load_callback_data_ = nil;
  }
}

Future<void> NativeAdInternalIOS::Initialize(AdParent parent) {
  firebase::MutexLock lock(mutex_);
  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kNativeAdFnInitialize);
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

Future<void> NativeAdInternalIOS::RecordImpression(const Variant& impression_data) {
  firebase::MutexLock lock(mutex_);
  if (!initialized_) {
    return CreateAndCompleteFuture(kNativeAdFnRecordImpression,
                                   kAdErrorCodeUninitialized,
                                   kAdUninitializedErrorMessage, &future_data_);
  }

  NSDictionary *impression_payload = variantmap_to_nsdictionary(impression_data);
  if (impression_payload == nullptr) {
    return CreateAndCompleteFuture(
        kNativeAdFnRecordImpression, kAdErrorCodeInvalidArgument,
        kUnsupportedVariantTypeErrorMessage, &future_data_);
  }

  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kNativeAdFnRecordImpression);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    bool recorded = [(GADNativeAd*)native_ad_ recordImpressionWithData:impression_payload];
    if(recorded) {
      CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
    } else {
      CompleteFuture(kAdErrorCodeInvalidRequest, kRecordImpressionFailureErrorMessage,
                     future_handle, &future_data_);
    }
  });

  return future;
}

Future<void> NativeAdInternalIOS::PerformClick(const Variant& click_data) {
  firebase::MutexLock lock(mutex_);
  if (!initialized_) {
    return CreateAndCompleteFuture(kNativeAdFnPerformClick,
                                   kAdErrorCodeUninitialized,
                                   kAdUninitializedErrorMessage, &future_data_);
  }

  NSDictionary *click_payload = variantmap_to_nsdictionary(click_data);
  if (click_payload == nullptr) {
    return CreateAndCompleteFuture(
        kNativeAdFnPerformClick, kAdErrorCodeInvalidArgument,
        kUnsupportedVariantTypeErrorMessage, &future_data_);
  }

  const SafeFutureHandle<void> future_handle =
      future_data_.future_impl.SafeAlloc<void>(kNativeAdFnPerformClick);
  Future<void> future = MakeFuture(&future_data_.future_impl, future_handle);

  dispatch_async(dispatch_get_main_queue(), ^{
    [(GADNativeAd*)native_ad_ performClickWithData:click_payload];
    CompleteFuture(kAdErrorCodeNone, nullptr, future_handle, &future_data_);
  });

  return future;
}

NSDictionary* NativeAdInternalIOS::variantmap_to_nsdictionary(const Variant &variant_data) {
  if (!variant_data.is_map()) {
    return nullptr;
  }

  NSMutableDictionary *dict = [[NSMutableDictionary alloc] init];

  for (const auto& kvp : variant_data.map()) {
    const Variant& key = kvp.first;
    const Variant& value = kvp.second;

    if (!key.is_string()) {
      return nullptr;
    }

    if (value.is_int64()) {
      [dict setObject:[NSNumber numberWithLongLong:value.int64_value()]
                     forKey:@(key.string_value())];
    } else if (value.is_double()) {
      [dict setObject:[NSNumber numberWithDouble:value.double_value()]
                     forKey:@(key.string_value())];
    } else if (value.is_string()) {
      [dict setObject:@(value.string_value())
                     forKey:@(key.string_value())];
    } else if (value.is_map()) {
      NSDictionary *val_dict = variantmap_to_nsdictionary(value);
      [dict setObject:val_dict
                     forKey:@(key.string_value())];
    } else {
      // Unsupported value type.
      return nullptr;
    }
  }

  NSDictionary *ret = [dict copy];
  return ret;
}

Future<AdResult> NativeAdInternalIOS::LoadAd(const char *ad_unit_id, const AdRequest &request) {
  firebase::MutexLock lock(mutex_);
  FutureCallbackData<AdResult> *callback_data =
      CreateAdResultFutureCallbackData(kNativeAdFnLoadAd, &future_data_);
  Future<AdResult> future = MakeFuture(&future_data_.future_impl, callback_data->future_handle);

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalError(callback_data, kAdErrorCodeLoadInProgress,
                                kAdLoadInProgressErrorMessage);
    return future;
  }

  // Persist a pointer to the callback data so that we may use it after the iOS
  // SDK returns the AdResult.
  ad_load_callback_data_ = callback_data;

  native_ad_delegate_ = [[FADNativeDelegate alloc] initWithInternalNativeAd:this];

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
      GADAdLoader *adLoader =
          [[GADAdLoader alloc] initWithAdUnitID:local_ad_unit_id
                             rootViewController:[parent_view_ window].rootViewController
                                        adTypes:@[ GADAdLoaderAdTypeNative ]
                                        options:nil];
      adLoader.delegate = native_ad_delegate_;
      [adLoader loadRequest:ad_request];
      // Have to keep a persistent reference to GADAdLoader until the ad finishes loading.
      ad_loader_ = adLoader;
    }
  });

  return future;
}

void NativeAdInternalIOS::NativeAdDidReceiveAd(GADNativeAd *ad) {
  firebase::MutexLock lock(mutex_);
  ad.delegate = native_ad_delegate_;
  native_ad_ = ad;

  NSObject *gad_icon = ad.icon;
  if (gad_icon != nil)
  {
    NativeAdImageInternal icon_internal;
    icon_internal.native_ad_image = gad_icon;
    GmaInternal::InsertNativeInternalImage(this, icon_internal, "icon", true );
  }

  NSObject *gad_choices_icon = ad.adChoicesIcon;
  if (gad_choices_icon != nil)
  {
    NativeAdImageInternal adchoices_icon_internal;
    adchoices_icon_internal.native_ad_image = gad_choices_icon;
    GmaInternal::InsertNativeInternalImage(this, adchoices_icon_internal, "adchoices_icon", true );
  }

  NSArray *gad_images = ad.images;
  for(NSObject *gad_image in gad_images)
  {
    NativeAdImageInternal image_internal;
    image_internal.native_ad_image = gad_image;
    GmaInternal::InsertNativeInternalImage(this, image_internal, "image", false );
  }

  if (ad_load_callback_data_ != nil) {
    CompleteLoadAdInternalSuccess(ad_load_callback_data_, ResponseInfoInternal({ad.responseInfo}));
    ad_load_callback_data_ = nil;
  }
}

void NativeAdInternalIOS::NativeAdDidFailToReceiveAdWithError(NSError *gad_error) {
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
