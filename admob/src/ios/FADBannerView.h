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

#include "admob/src/ios/banner_view_internal_ios.h"

NS_ASSUME_NONNULL_BEGIN

@interface FADBannerView : UIView

/// The banner view's BoundingBox.
@property(nonatomic, readonly) firebase::admob::BoundingBox boundingBox;

/// The banner view's presentation state.
@property(nonatomic, readonly) firebase::admob::BannerView::PresentationState presentationState;

/// Unavailable (NSObject).
- (instancetype)init NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

/// Unavailable (UIView).
- (instancetype)initWithCoder:(NSCoder *)coder NS_UNAVAILABLE;

/// Designated Initializer. Returns a FADBannerView object with the publisher-provided UIView (the
/// parent view of FADBannerView), ad unit ID, ad size, and BannerViewInternalIOS object.
- (instancetype)initWithView:(UIView *)view
                    adUnitID:(NSString *)adUnitID
                      adSize:(firebase::admob::AdSize)adSize
          internalBannerView:(firebase::admob::internal::BannerViewInternalIOS *)cppBannerView
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
- (void)moveBannerViewToXCoordinate:(int)x yCoordinate:(int)y;

/// Moves the banner view to a BannerView::Position.
- (void)moveBannerViewToPosition:(firebase::admob::BannerView::Position)position;

@end

NS_ASSUME_NONNULL_END
