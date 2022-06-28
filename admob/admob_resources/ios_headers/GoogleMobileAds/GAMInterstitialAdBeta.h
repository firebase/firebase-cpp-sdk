//
//  GAMInterstitialAdBeta.h
//  Google Mobile Ads SDK
//
//  Copyright 2020 Google LLC. All rights reserved.
//

#import <GoogleMobileAds/DFPRequest.h>
#import <GoogleMobileAds/GADAppEventDelegateBeta.h>
#import <GoogleMobileAds/GADInterstitialAdBeta.h>

@class GAMInterstitialAdBeta;
typedef void (^GAMInterstitialAdBetaLoadCompletionHandler)(
    GAMInterstitialAdBeta *_Nullable interstitialAd, NSError *_Nullable error);

/// Google Ad Manager interstitial ad, a full-screen advertisement shown at natural
/// transition points in your application such as between game levels or news stories.
@interface GAMInterstitialAdBeta : GADInterstitialAdBeta

/// Optional delegate that is notified when creatives send app events.
@property(nonatomic, weak, nullable) id<GADAppEventDelegateBeta> appEventDelegate;

/// Loads an interstitial ad.
///
/// @param adUnitID An ad unit ID created in the Ad Manager UI.
/// @param request An ad request object. If nil, a default ad request object is used.
/// @param completionHandler A handler to execute when the load operation finishes or times out.
+ (void)loadWithAdManagerAdUnitID:(nonnull NSString *)adUnitID
                          request:(nullable DFPRequest *)request
                completionHandler:
                    (nonnull GAMInterstitialAdBetaLoadCompletionHandler)completionHandler;

+ (void)loadWithAdUnitID:(nonnull NSString *)adUnitID
                 request:(nullable GADRequest *)request
       completionHandler:(nonnull GADInterstitialAdBetaLoadCompletionHandler)completionHandler
    NS_UNAVAILABLE;

@end
