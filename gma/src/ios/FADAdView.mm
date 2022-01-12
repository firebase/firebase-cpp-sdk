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

#include "gma/src/ios/FADAdView.h"

#import "gma/src/ios/gma_ios.h"
#import "gma/src/ios/FADAdSize.h"

namespace gma = firebase::gma;

@interface FADAdView () <GADBannerViewDelegate> {
  /// The publisher-provided view that is the parent view of the banner view.
  UIView *__weak _parentView;

  /// The banner view.
  GADBannerView *_AdView;

  /// The current horizontal layout constraint for the banner view.
  NSLayoutConstraint *_AdViewHorizontalLayoutConstraint;

  /// The current vertical layout constraint for the banner view.
  NSLayoutConstraint *_AdViewVerticalLayoutConstraint;

  /// The banner view's ad unit ID.
  NSString *_adUnitID;

  /// The banner view's ad size.
  GADAdSize _adSize;

  /// The AdViewInternalIOS object.
  gma::internal::AdViewInternalIOS *_cppAdView;

  /// Indicates if the ad loaded.
  BOOL _adLoaded;
}

@end

@implementation FADAdView

firebase::gma::AdView::Position _position;

#pragma mark - Initialization

- (instancetype)initWithView:(UIView *)view
                    adUnitID:(NSString *)adUnitID
                      adSize:(firebase::gma::AdSize)adSize
          internalAdView:(firebase::gma::internal::AdViewInternalIOS *)cppAdView {
  GADAdSize gadsize = GADSizeFromCppAdSize(adSize);
  CGRect frame = CGRectMake(0, 0, gadsize.size.width, gadsize.size.height);
  self = [super initWithFrame:frame];
  if (self) {
    _parentView = view;
    _adUnitID = [adUnitID copy];
    _adSize = gadsize;
    _cppAdView = cppAdView;
    [self setUpAdView];
  }
  return self;
}

/// Called from the designated initializer. Sets up a banner view.
- (void)setUpAdView {
  _AdView = [[GADBannerView alloc] initWithAdSize:_adSize];
  _AdView.adUnitID = _adUnitID;
  _AdView.delegate = self;
  __unsafe_unretained typeof(self) weakSelf = self;
  _AdView.paidEventHandler = ^void(GADAdValue *_Nonnull adValue) {
    // Establish the strong self reference
    __strong typeof(self) strongSelf = weakSelf;
    if (strongSelf) {
      strongSelf->_cppAdView->NotifyListenerOfPaidEvent(
          firebase::gma::ConvertGADAdValueToCppAdValue(adValue));
    }
  };

  // The FADAdView is hidden until the publisher calls Show().
  self.hidden = YES;

  [self addSubview:_AdView];
  _AdView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add layout constraints to center the banner view vertically and horizontally.
  NSDictionary *viewDictionary = NSDictionaryOfVariableBindings(_AdView);
  NSArray *verticalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_AdView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:verticalConstraints];
  NSArray *horizontalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_AdView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:horizontalConstraints];

  UIView *strongView = _parentView;
  _AdView.rootViewController = strongView.window.rootViewController;
  [strongView addSubview:self];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self updateAdViewLayoutConstraintsWithPosition:(gma::AdView::kPositionTopLeft)
                                          xCoordinate:0
                                          yCoordinate:0];
}

#pragma mark - Property Accessor Methods

- (gma::BoundingBox)boundingBox {
  gma::BoundingBox box;
  if (!_AdView) {
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
  box.position = _position;
  return box;
}

#pragma mark - Public Methods

- (void)loadRequest:(GADRequest *)request {
  // Make the banner ad request.
  [_AdView loadRequest:request];
}

- (void)hide {
  self.hidden = YES;
}

- (void)show {
  self.hidden = NO;
  gma::BoundingBox bounding_box = self.boundingBox;
  _cppAdView->set_bounding_box(bounding_box);
  _cppAdView->NotifyListenerOfBoundingBoxChange(bounding_box);
}

- (void)destroy {
  [_AdView removeFromSuperview];
  _AdView.delegate = nil;
  _AdView = nil;
}

- (void)moveAdViewToXCoordinate:(int)x yCoordinate:(int)y {
  // The moveAdViewToXCoordinate:yCoordinate: method gets the x-coordinate and y-coordinate in
  // pixels. Need to convert the pixels to points before updating the banner view's layout
  // constraints.
  _position = firebase::gma::AdView::kPositionUndefined;
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat xPoints = x / scale;
  CGFloat yPoints = y / scale;
  [self updateAdViewLayoutConstraintsWithPosition:(_position)
                                          xCoordinate:xPoints
                                          yCoordinate:yPoints];
}

- (void)moveAdViewToPosition:(gma::AdView::Position)position {
  [self updateAdViewLayoutConstraintsWithPosition:position
                                          xCoordinate:0
                                          yCoordinate:0];
}

#pragma mark - Private Methods

/// Updates the layout constraints for the banner view.
- (void)updateAdViewLayoutConstraintsWithPosition:(gma::AdView::Position)position
                                          xCoordinate:(CGFloat)x
                                          yCoordinate:(CGFloat)y {
  NSLayoutAttribute verticalLayoutAttribute = NSLayoutAttributeNotAnAttribute;
  NSLayoutAttribute horizontalLayoutAttribute = NSLayoutAttributeNotAnAttribute;

  _position = position;
  switch (position) {
    case gma::AdView::kPositionTop:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case gma::AdView::kPositionBottom:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case gma::AdView::kPositionUndefined:
    case gma::AdView::kPositionTopLeft:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case gma::AdView::kPositionTopRight:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    case gma::AdView::kPositionBottomLeft:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case gma::AdView::kPositionBottomRight:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    default:
      break;
  }

  // Remove the existing vertical and horizontal layout constraints for the banner view, if they
  // exist.
  UIView *strongView = _parentView;
  if (_AdViewVerticalLayoutConstraint && _AdViewHorizontalLayoutConstraint) {
    NSArray *AdViewLayoutConstraints = @[ _AdViewVerticalLayoutConstraint,
                                              _AdViewHorizontalLayoutConstraint ];
    [strongView removeConstraints:AdViewLayoutConstraints];
  }

  // Set the vertical and horizontal layout constraints for the banner view.
  _AdViewVerticalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:verticalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:verticalLayoutAttribute
                                  multiplier:1
                                    constant:y];
  [strongView addConstraint:_AdViewVerticalLayoutConstraint];
  _AdViewHorizontalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:horizontalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:horizontalLayoutAttribute
                                  multiplier:1
                                    constant:x];
  [strongView addConstraint:_AdViewHorizontalLayoutConstraint];
  [self setNeedsLayout];
}

/// This method gets called when the observer is notified that the application
/// is active again (i.e. when the user returns to the application from Safari
/// or the App Store).
- (void)applicationDidBecomeActive:(NSNotification *)notification {
  // Remove the observer that was registered in the adViewWillLeaveApplication: callback.
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:UIApplicationDidBecomeActiveNotification
                                                object:nil];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  gma::BoundingBox bounding_box = self.boundingBox;
  _cppAdView->set_bounding_box(bounding_box);
  _cppAdView->NotifyListenerOfBoundingBoxChange(bounding_box);
}

#pragma mark - GADBannerViewDelegate

- (void)AdViewDidReceiveAd:(nonnull GADBannerView *)AdView {
  _adLoaded = YES;
  _cppAdView->AdViewDidReceiveAd();
  gma::BoundingBox bounding_box = self.boundingBox;
}
- (void)AdView:(nonnull GADBannerView *)AdView didFailToReceiveAdWithError:(nonnull NSError *)error {
  _cppAdView->AdViewDidFailToReceiveAdWithError(error);
}

- (void)AdViewDidRecordClick:(nonnull GADBannerView *)AdView {
   _cppAdView->NotifyListenerAdClicked();
}

- (void)AdViewDidRecordImpression:(nonnull GADBannerView *)AdView {
  _cppAdView->NotifyListenerAdImpression();
}

// Note that the following callbacks are only called on in-app overlay events. 
// See https://www.googblogs.com/google-mobile-ads-sdk-a-note-on-ad-click-events/
// and https://groups.google.com/g/google-admob-ads-sdk/c/lzdt5szxSVU
- (void)AdViewWillPresentScreen:(nonnull GADBannerView *)AdView {
  gma::BoundingBox bounding_box = self.boundingBox;
  _cppAdView->set_bounding_box(bounding_box);
  _cppAdView->NotifyListenerOfBoundingBoxChange(bounding_box);
  _cppAdView->NotifyListenerAdOpened();
}

- (void)AdViewDidDismissScreen:(nonnull GADBannerView *)AdView {
  gma::BoundingBox bounding_box = self.boundingBox;
  _cppAdView->set_bounding_box(bounding_box);
  _cppAdView->NotifyListenerOfBoundingBoxChange(bounding_box);
  _cppAdView->NotifyListenerAdClosed();
}

@end
