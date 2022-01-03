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

#include "admob/src/ios/FADBannerView.h"

namespace admob = firebase::admob;

@interface FADBannerView () <GADBannerViewDelegate> {
  /// The publisher-provided view that is the parent view of the banner view.
  UIView *__weak _parentView;

  /// The banner view.
  GADBannerView *_bannerView;

  /// The current horizontal layout constraint for the banner view.
  NSLayoutConstraint *_bannerViewHorizontalLayoutConstraint;

  /// The current vertical layout constraint for the banner view.
  NSLayoutConstraint *_bannerViewVerticalLayoutConstraint;

  /// The banner view's ad unit ID.
  NSString *_adUnitID;

  /// The banner view's ad size.
  admob::AdSize _adSize;

  /// The BannerViewInternalIOS object.
  admob::internal::BannerViewInternalIOS *_cppBannerView;

  /// Indicates if the ad loaded.
  BOOL _adLoaded;
}

@end

@implementation FADBannerView

#pragma mark - Initialization

- (instancetype)initWithView:(UIView *)view
                    adUnitID:(NSString *)adUnitID
                      adSize:(admob::AdSize)adSize
          internalBannerView:(firebase::admob::internal::BannerViewInternalIOS *)cppBannerView {
  CGRect frame = CGRectMake(0, 0, adSize.width, adSize.height);
  self = [super initWithFrame:frame];
  if (self) {
    _parentView = view;
    _adUnitID = [adUnitID copy];
    _adSize = adSize;
    _cppBannerView = cppBannerView;
    _presentationState = admob::BannerView::kPresentationStateHidden;
    [self setUpBannerView];
  }
  return self;
}

/// Called from the designated initializer. Sets up a banner view.
- (void)setUpBannerView {
  GADAdSize size = GADAdSizeFromCGSize(CGSizeMake(_adSize.width, _adSize.height));
  _bannerView = [[GADBannerView alloc] initWithAdSize:size];
  _bannerView.adUnitID = _adUnitID;
  _bannerView.delegate = self;

  // The FADBannerView is hidden until the publisher calls Show().
  self.hidden = YES;

  [self addSubview:_bannerView];
  _bannerView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add layout constraints to center the banner view vertically and horizontally.
  NSDictionary *viewDictionary = NSDictionaryOfVariableBindings(_bannerView);
  NSArray *verticalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_bannerView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:verticalConstraints];
  NSArray *horizontalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_bannerView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:horizontalConstraints];

  UIView *strongView = _parentView;
  _bannerView.rootViewController = strongView.window.rootViewController;
  [strongView addSubview:self];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self updateBannerViewLayoutConstraintsWithPosition:(admob::BannerView::kPositionTopLeft)
                                          xCoordinate:0
                                          yCoordinate:0];
}

#pragma mark - Property Accessor Methods

- (admob::BoundingBox)boundingBox {
  admob::BoundingBox box;
  if (!_bannerView) {
    return box;
  }
  // Map the width and height and the x and y coordinates of the BoundingBox to pixel values using
  // the scale factor associated with the device's screen.
  CGFloat scale = [UIScreen mainScreen].scale;
  CGRect standardizedRect = CGRectStandardize(self.frame);
  box.width = standardizedRect.size.width * scale;
  box.height = standardizedRect.size.height * scale;
  box.x = standardizedRect.origin.x * scale;
  box.y = standardizedRect.origin.y * scale;
  return box;
}

#pragma mark - Public Methods

- (void)loadRequest:(GADRequest *)request {
  // Make the banner ad request.
  [_bannerView loadRequest:request];
}

- (void)hide {
  self.hidden = YES;
  _presentationState = admob::BannerView::kPresentationStateHidden;
}

- (void)show {
  self.hidden = NO;
  if (_adLoaded) {
    _presentationState = admob::BannerView::kPresentationStateVisibleWithAd;
  } else {
    _presentationState = admob::BannerView::kPresentationStateVisibleWithoutAd;
  }
}

- (void)destroy {
  [_bannerView removeFromSuperview];
  _bannerView.delegate = nil;
  _bannerView = nil;
  _presentationState = admob::BannerView::kPresentationStateHidden;
}

- (void)moveBannerViewToXCoordinate:(int)x yCoordinate:(int)y {
  // The moveBannerViewToXCoordinate:yCoordinate: method gets the x-coordinate and y-coordinate in
  // pixels. Need to convert the pixels to points before updating the banner view's layout
  // constraints.
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat xPoints = x / scale;
  CGFloat yPoints = y / scale;
  [self updateBannerViewLayoutConstraintsWithPosition:(admob::BannerView::kPositionTopLeft)
                                          xCoordinate:xPoints
                                          yCoordinate:yPoints];
}

- (void)moveBannerViewToPosition:(admob::BannerView::Position)position {
  [self updateBannerViewLayoutConstraintsWithPosition:position
                                          xCoordinate:0
                                          yCoordinate:0];
}

#pragma mark - Private Methods

/// Updates the layout constraints for the banner view.
- (void)updateBannerViewLayoutConstraintsWithPosition:(admob::BannerView::Position)position
                                          xCoordinate:(CGFloat)x
                                          yCoordinate:(CGFloat)y {
  NSLayoutAttribute verticalLayoutAttribute = NSLayoutAttributeNotAnAttribute;
  NSLayoutAttribute horizontalLayoutAttribute = NSLayoutAttributeNotAnAttribute;

  switch (position) {
    case admob::BannerView::kPositionTop:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::BannerView::kPositionBottom:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::BannerView::kPositionTopLeft:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case admob::BannerView::kPositionTopRight:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    case admob::BannerView::kPositionBottomLeft:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case admob::BannerView::kPositionBottomRight:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    default:
      break;
  }

  // Remove the existing vertical and horizontal layout constraints for the banner view, if they
  // exist.
  UIView *strongView = _parentView;
  if (_bannerViewVerticalLayoutConstraint && _bannerViewHorizontalLayoutConstraint) {
    NSArray *bannerViewLayoutConstraints = @[ _bannerViewVerticalLayoutConstraint,
                                              _bannerViewHorizontalLayoutConstraint ];
    [strongView removeConstraints:bannerViewLayoutConstraints];
  }

  // Set the vertical and horizontal layout constraints for the banner view.
  _bannerViewVerticalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:verticalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:verticalLayoutAttribute
                                  multiplier:1
                                    constant:y];
  [strongView addConstraint:_bannerViewVerticalLayoutConstraint];
  _bannerViewHorizontalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:horizontalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:horizontalLayoutAttribute
                                  multiplier:1
                                    constant:x];
  [strongView addConstraint:_bannerViewHorizontalLayoutConstraint];
  [self setNeedsLayout];
}

/// Updates the presentation state of the banner view. This method gets called when the observer is
/// notified that the application is active again (i.e. when the user returns to the application
/// from Safari or the App Store).
- (void)applicationDidBecomeActive:(NSNotification *)notification {
  _presentationState = admob::BannerView::kPresentationStateVisibleWithAd;
  _cppBannerView->NotifyListenerOfPresentationStateChange(_presentationState);
  // Remove the observer that was registered in the adViewWillLeaveApplication: callback.
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:UIApplicationDidBecomeActiveNotification
                                                object:nil];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  _cppBannerView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

#pragma mark - GADBannerViewDelegate

- (void)adViewDidReceiveAd:(GADBannerView *)bannerView {
  _adLoaded = YES;
  _cppBannerView->CompleteLoadFuture(admob::kAdMobErrorNone, nullptr);
  // Only update the presentation state if the FADBannerView is already visible.
  if (!self.hidden) {
    _presentationState = admob::BannerView::kPresentationStateVisibleWithAd;
    _cppBannerView->NotifyListenerOfPresentationStateChange(_presentationState);
    // Loading an ad can sometimes cause the bounds to change.
    _cppBannerView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
  }
}

- (void)adView:(GADBannerView *)bannerView didFailToReceiveAdWithError:(GADRequestError *)error {
  admob::AdMobError api_error;
  const char* error_msg = error.localizedDescription.UTF8String;
  switch (error.code) {
    case kGADErrorInvalidRequest:
      api_error = admob::kAdMobErrorInvalidRequest;
      break;
    case kGADErrorNoFill:
      api_error = admob::kAdMobErrorNoFill;
      break;
    case kGADErrorNetworkError:
      api_error = admob::kAdMobErrorNetworkError;
      break;
    case kGADErrorInternalError:
      api_error = admob::kAdMobErrorInternalError;
      break;
    default:
      // NOTE: Changes in the iOS SDK can result in new error codes being added. Fall back to
      // admob::kAdMobErrorInternalError if this SDK doesn't handle error.code.
      firebase::LogDebug("Unknown error code %d. Defaulting to internal error.", error.code);
      api_error = admob::kAdMobErrorInternalError;
      error_msg = admob::kInternalSDKErrorMesage;
      break;
  }
  _cppBannerView->CompleteLoadFuture(api_error, error_msg);
}

- (void)adViewWillPresentScreen:(GADBannerView *)bannerView {
  _presentationState = admob::BannerView::kPresentationStateCoveringUI;
  _cppBannerView->NotifyListenerOfPresentationStateChange(_presentationState);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

- (void)adViewDidDismissScreen:(GADBannerView *)bannerView {
  _presentationState = admob::BannerView::kPresentationStateVisibleWithAd;
  _cppBannerView->NotifyListenerOfPresentationStateChange(_presentationState);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

- (void)adViewWillLeaveApplication:(GADBannerView *)bannerView {
  _presentationState = admob::BannerView::kPresentationStateCoveringUI;
  _cppBannerView->NotifyListenerOfPresentationStateChange(_presentationState);
  // The FADBannerView object will need to get notified when the application becomes active again so
  // it can update the GADBannerView's presentation state.
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
}

@end
