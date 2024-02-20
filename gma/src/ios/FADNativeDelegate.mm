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

#import "gma/src/ios/FADNativeDelegate.h"

#include "gma/src/ios/ad_error_ios.h"
#include "gma/src/ios/native_ad_internal_ios.h"

@interface FADNativeDelegate () {
  /// The NativeAdInternalIOS object.
  firebase::gma::internal::NativeAdInternalIOS *_nativeAd;
}
@end

@implementation FADNativeDelegate : NSObject

#pragma mark - Initialization

- (instancetype)initWithInternalNativeAd:(firebase::gma::internal::NativeAdInternalIOS *)nativeAd {
  self = [super init];
  if (self) {
    _nativeAd = nativeAd;
  }

  return self;
}

#pragma mark GADAdLoaderDelegate implementation

- (void)adLoader:(GADAdLoader *)adLoader didFailToReceiveAdWithError:(NSError *)error {
  _nativeAd->NativeAdDidFailToReceiveAdWithError(error);
}

#pragma mark GADNativeAdLoaderDelegate implementation

- (void)adLoader:(GADAdLoader *)adLoader didReceiveNativeAd:(GADNativeAd *)nativeAd {
  _nativeAd->NativeAdDidReceiveAd(nativeAd);
}

#pragma mark GADNativeAdDelegate implementation

- (void)nativeAdDidRecordClick:(nonnull GADNativeAd *)nativeAd {
  _nativeAd->NotifyListenerAdClicked();
}

- (void)nativeAdDidRecordImpression:(nonnull GADNativeAd *)nativeAd {
  _nativeAd->NotifyListenerAdImpression();
}

- (void)nativeAdWillPresentScreen:(nonnull GADNativeAd *)nativeAd {
  _nativeAd->NotifyListenerAdOpened();
}

- (void)nativeAdDidDismissScreen:(nonnull GADNativeAd *)nativeAd {
  _nativeAd->NotifyListenerAdClosed();
}

@end
