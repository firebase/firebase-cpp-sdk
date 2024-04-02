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

// An Objective-C++ wrapper class that conforms to the GADNativeAdLoaderDelegate protocol. When the
// delegate for receiving state change messages from a GADNativeAd is notified, this wrapper
// class forwards the notification to the NativeAdInternalIOS object to handle the state
// changes for an native ad.

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

namespace firebase {
namespace gma {
namespace internal {
class NativeAdInternalIOS;
}  // namespace internal
}  // namespace gma
}  // namespace firebase

NS_ASSUME_NONNULL_BEGIN

@interface FADNativeDelegate : NSObject <GADNativeAdLoaderDelegate, GADAdLoaderDelegate, GADNativeAdDelegate>

/// Returns a FADNativeDelegate object with NativeAdInternalIOS.
- (FADNativeDelegate *)initWithInternalNativeAd:
    (firebase::gma::internal::NativeAdInternalIOS *)nativeAd;

@end

NS_ASSUME_NONNULL_END
