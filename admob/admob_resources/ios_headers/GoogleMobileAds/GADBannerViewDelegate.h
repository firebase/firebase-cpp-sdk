//
//  GADBannerViewDelegate.h
//  Google Mobile Ads SDK
//
//  Copyright 2011 Google LLC. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GADRequestError.h>

@class GADBannerView;

/// Delegate methods for receiving GADBannerView state change messages such as ad request status
/// and ad click lifecycle.
@protocol GADBannerViewDelegate <NSObject>

@optional

#pragma mark Ad Request Lifecycle Notifications

/// Tells the delegate that an ad request successfully received an ad. The delegate may want to add
/// the banner view to the view hierarchy if it hasn't been added yet.
- (void)adViewDidReceiveAd:(nonnull GADBannerView *)bannerView;

/// Tells the delegate that an ad request failed. The failure is normally due to network
/// connectivity or ad availablility (i.e., no fill).
- (void)adView:(nonnull GADBannerView *)bannerView
    didFailToReceiveAdWithError:(nonnull GADRequestError *)error;

/// Tells the delegate that an impression has been recorded for an ad.
- (void)adViewDidRecordImpression:(nonnull GADBannerView *)bannerView;

#pragma mark Click-Time Lifecycle Notifications

/// Tells the delegate that a full screen view will be presented in response to the user clicking on
/// an ad. The delegate may want to pause animations and time sensitive interactions.
- (void)adViewWillPresentScreen:(nonnull GADBannerView *)bannerView;

/// Tells the delegate that the full screen view will be dismissed.
- (void)adViewWillDismissScreen:(nonnull GADBannerView *)bannerView;

/// Tells the delegate that the full screen view has been dismissed. The delegate should restart
/// anything paused while handling adViewWillPresentScreen:.
- (void)adViewDidDismissScreen:(nonnull GADBannerView *)bannerView;

/// Tells the delegate that the user click will open another app, backgrounding the current
/// application. The standard UIApplicationDelegate methods, like applicationDidEnterBackground:,
/// are called immediately before this method is called.
- (void)adViewWillLeaveApplication:(nonnull GADBannerView *)bannerView;

@end
