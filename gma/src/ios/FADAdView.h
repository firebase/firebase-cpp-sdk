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

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include "gma/src/ios/ad_view_internal_ios.h"

NS_ASSUME_NONNULL_BEGIN

@interface FADAdView : UIView

/// The banner view's BoundingBox.
@property(nonatomic, readonly) firebase::gma::BoundingBox boundingBox;

/// Unavailable (NSObject).
- (instancetype)init NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;

/// Designated Initializer. Returns a FADAdView object with the publisher-provided UIView (the
/// parent view of FADAdView), ad unit ID, ad size, and AdViewInternalIOS object.
- (instancetype)initWithView:(UIView *)view
                    adUnitID:(NSString *)adUnitID
                      adSize:(firebase::gma::AdSize)adSize
              internalAdView:(firebase::gma::internal::AdViewInternalIOS *)cppAdView  // NOLINT
    NS_DESIGNATED_INITIALIZER;

/// Requests a banner ad.
- (void)loadRequest:(GADRequest *)request;

/// Hides the banner view.
- (void)hide;

/// Shows the banner view.
- (void)show;

/// Destroys the banner view.
- (void)destroy;

/// Moves the banner view to an X coordinate and a Y coordinate.
- (void)moveAdViewToXCoordinate:(int)x yCoordinate:(int)y;

/// Moves the banner view to a AdView::Position.
- (void)moveAdViewToPosition:(firebase::gma::AdView::Position)position;  // NOLINT

@end

NS_ASSUME_NONNULL_END
