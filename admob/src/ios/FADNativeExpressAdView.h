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

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include "admob/src/ios/native_express_ad_view_internal_ios.h"

NS_ASSUME_NONNULL_BEGIN

@interface FADNativeExpressAdView : UIView

/// The native express ad view's BoundingBox.
@property(nonatomic, readonly) firebase::admob::BoundingBox boundingBox;

/// The native express ad view's presentation state.
@property(nonatomic, readonly)
    firebase::admob::NativeExpressAdView::PresentationState presentationState;

/// Unavailable (NSObject).
- (instancetype)init NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;

/// Designated Initializer. Returns a FADNativeExpressAdView object with the publisher-provided
/// UIView (the parent view of FADNativeExpressAdView), ad unit ID, ad size, and
/// NativeExpressAdViewInternalIOS object.
- (instancetype)initWithView:(UIView *)view
                       adUnitID:(NSString *)adUnitID
                         adSize:(firebase::admob::AdSize)adSize
    internalNativeExpressAdView:
        (firebase::admob::internal::NativeExpressAdViewInternalIOS *)cppNativeExpressAdView
    NS_DESIGNATED_INITIALIZER;

/// Requests a native express ad.
- (void)loadRequest:(GADRequest *)request;

/// Hides the native express ad view.
- (void)hide;

/// Shows the native express ad view.
- (void)show;

/// Destroys the native express ad view.
- (void)destroy;

/// Moves the native express ad view to an X coordinate and a Y coordinate.
- (void)moveNativeExpressAdViewToXCoordinate:(int)x yCoordinate:(int)y;

/// Moves the native express ad view to a NativeExpressAdView::Position.
- (void)moveNativeExpressAdViewToPosition:(firebase::admob::NativeExpressAdView::Position)position;

@end

NS_ASSUME_NONNULL_END
