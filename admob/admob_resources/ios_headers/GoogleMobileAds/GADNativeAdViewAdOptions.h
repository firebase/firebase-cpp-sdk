//
//  GADNativeAdViewAdOptions.h
//  Google Mobile Ads SDK
//
//  Copyright 2016 Google LLC. All rights reserved.
//

#import <GoogleMobileAds/GADAdLoader.h>

/// Position of the AdChoices icon in the containing ad.
typedef NS_ENUM(NSInteger, GADAdChoicesPosition) {
  GADAdChoicesPositionTopRightCorner,     ///< Top right corner.
  GADAdChoicesPositionTopLeftCorner,      ///< Top left corner.
  GADAdChoicesPositionBottomRightCorner,  ///< Bottom right corner.
  GADAdChoicesPositionBottomLeftCorner    ///< Bottom Left Corner.
};

/// Ad loader options for configuring the view of native ads.
@interface GADNativeAdViewAdOptions : GADAdLoaderOptions

/// Indicates preferred location of AdChoices icon. Default is GADAdChoicesPositionTopRightCorner.
@property(nonatomic, assign) GADAdChoicesPosition preferredAdChoicesPosition;

@end
