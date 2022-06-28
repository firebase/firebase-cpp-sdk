//
//  GADRequestError.h
//  Google Mobile Ads SDK
//
//  Copyright 2011 Google LLC. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAdsDefines.h>

/// Google AdMob Ads error domain.
GAD_EXTERN NSString *_Nonnull const kGADErrorDomain;

/// NSError codes for GAD error domain.
typedef NS_ENUM(NSInteger, GADErrorCode) {
  /// The ad request is invalid. The localizedFailureReason error description will have more
  /// details. Typically this is because the ad did not have the ad unit ID or root view
  /// controller set.
  kGADErrorInvalidRequest = 0,

  /// The ad request was successful, but no ad was returned.
  kGADErrorNoFill = 1,

  /// There was an error loading data from the network.
  kGADErrorNetworkError = 2,

  /// The ad server experienced a failure processing the request.
  kGADErrorServerError = 3,

  /// The current device's OS is below the minimum required version.
  kGADErrorOSVersionTooLow = 4,

  /// The request was unable to be loaded before being timed out.
  kGADErrorTimeout = 5,

  /// Will not send request because the interstitial object has already been used.
  kGADErrorInterstitialAlreadyUsed GAD_DEPRECATED_MSG_ATTRIBUTE("Use kGADErrorAdAlreadyUsed.") = 6,

  /// The mediation response was invalid.
  kGADErrorMediationDataError = 7,

  /// Error finding or creating a mediation ad network adapter.
  kGADErrorMediationAdapterError = 8,

  /// Attempting to pass an invalid ad size to an adapter.
  kGADErrorMediationInvalidAdSize = 10,

  /// Internal error.
  kGADErrorInternalError = 11,

  /// Invalid argument error.
  kGADErrorInvalidArgument = 12,

  /// Received invalid response.
  kGADErrorReceivedInvalidResponse = 13,

  /// Will not send request because the rewarded ad object has already been used.
  kGADErrorRewardedAdAlreadyUsed GAD_DEPRECATED_MSG_ATTRIBUTE("Use kGADErrorAdAlreadyUsed.") = 14,

  /// A mediation ad network adapter received an ad request, but did not fill. The adapter's error
  /// is included as an underlyingError.
  kGADErrorMediationNoFill = 9,

  /// Will not send request because the ad object has already been used.
  kGADErrorAdAlreadyUsed = 19,

  /// Will not send request because the application identifier is missing.
  kGADErrorApplicationIdentifierMissing = 20,
};

/// Represents the error generated due to invalid request parameters.
@interface GADRequestError : NSError
@end
