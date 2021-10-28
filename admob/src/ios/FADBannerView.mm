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

#import "admob/src/ios/admob_ios.h"
#import "admob/src/ios/FADAdSize.h"

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
  GADAdSize _adSize;

  /// The BannerViewInternalIOS object.
  admob::internal::BannerViewInternalIOS *_cppBannerView;

  /// Indicates if the ad loaded.
  BOOL _adLoaded;
}

@end

@implementation FADBannerView

firebase::admob::BannerView::Position _position;

#pragma mark - Initialization

- (instancetype)initWithView:(UIView *)view
                    adUnitID:(NSString *)adUnitID
                      adSize:(firebase::admob::AdSize)adSize
          internalBannerView:(firebase::admob::internal::BannerViewInternalIOS *)cppBannerView {
  GADAdSize gadsize = GADSizeFromCppAdSize(adSize);
  CGRect frame = CGRectMake(0, 0, gadsize.size.width, gadsize.size.height);
  self = [super initWithFrame:frame];
  if (self) {
    _parentView = view;
    _adUnitID = [adUnitID copy];
    _adSize = gadsize;
    _cppBannerView = cppBannerView;
    [self setUpBannerView];
  }
  return self;
}

/// Called from the designated initializer. Sets up a banner view.
- (void)setUpBannerView {
  _bannerView = [[GADBannerView alloc] initWithAdSize:_adSize];
  _bannerView.adUnitID = _adUnitID;
  _bannerView.delegate = self;
  __unsafe_unretained typeof(self) weakSelf = self;
  _bannerView.paidEventHandler = ^void(GADAdValue *_Nonnull adValue) {
    // Establish the strong self reference
    __strong typeof(self) strongSelf = weakSelf;
    if (strongSelf) {
      strongSelf->_cppBannerView->NotifyListenerOfPaidEvent(
          firebase::admob::ConvertGADAdValueToCppAdValue(adValue));
    }
  };

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
  box.position = _position;
  return box;
}

#pragma mark - Public Methods

- (void)loadRequest:(GADRequest *)request {
  // Make the banner ad request.
  [_bannerView loadRequest:request];
}

- (void)hide {
  self.hidden = YES;
}

- (void)show {
  self.hidden = NO;
  admob::BoundingBox bounding_box = self.boundingBox;
  _cppBannerView->set_bounding_box(bounding_box);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(bounding_box);
}

- (void)destroy {
  [_bannerView removeFromSuperview];
  _bannerView.delegate = nil;
  _bannerView = nil;
}

- (void)moveBannerViewToXCoordinate:(int)x yCoordinate:(int)y {
  // The moveBannerViewToXCoordinate:yCoordinate: method gets the x-coordinate and y-coordinate in
  // pixels. Need to convert the pixels to points before updating the banner view's layout
  // constraints.
  _position = firebase::admob::AdView::kPositionUndefined;
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat xPoints = x / scale;
  CGFloat yPoints = y / scale;
  [self updateBannerViewLayoutConstraintsWithPosition:(_position)
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

  _position = position;
  switch (position) {
    case admob::BannerView::kPositionTop:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::BannerView::kPositionBottom:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::BannerView::kPositionUndefined:
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
  admob::BoundingBox bounding_box = self.boundingBox;
  _cppBannerView->set_bounding_box(bounding_box);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(bounding_box);
}

#pragma mark - GADBannerViewDelegate

- (void)bannerViewDidReceiveAd:(nonnull GADBannerView *)bannerView {
  _adLoaded = YES;
  _cppBannerView->BannerViewDidReceiveAd();
  //if(!self.hidden) {
  {
    admob::BoundingBox bounding_box = self.boundingBox;
    //_cppBannerView->set_bounding_box(bounding_box);
    //_cppBannerView->NotifyListenerOfBoundingBoxChange(bounding_box);
  }
}
- (void)bannerView:(nonnull GADBannerView *)bannerView didFailToReceiveAdWithError:(nonnull NSError *)error {
  firebase::LogError("DEDB didFailToReceiveAdWithError in FADBannerView.mm");
  _cppBannerView->BannerViewDidFailToReceiveAdWithError(error);
}

- (void)bannerViewDidRecordClick:(nonnull GADBannerView *)bannerView {
   _cppBannerView->NotifyListenerAdClicked();
}

- (void)bannerViewDidRecordImpression:(nonnull GADBannerView *)bannerView {
  _cppBannerView->NotifyListenerAdImpression();
}

// Note that the following callbacks are only called on in-app overlay events. 
// See https://www.googblogs.com/google-mobile-ads-sdk-a-note-on-ad-click-events/
// and https://groups.google.com/g/google-admob-ads-sdk/c/lzdt5szxSVU
- (void)bannerViewWillPresentScreen:(nonnull GADBannerView *)bannerView {
  admob::BoundingBox bounding_box = self.boundingBox;
  _cppBannerView->set_bounding_box(bounding_box);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(bounding_box);
  _cppBannerView->NotifyListenerAdOpened();
}

- (void)bannerViewDidDismissScreen:(nonnull GADBannerView *)bannerView {
  admob::BoundingBox bounding_box = self.boundingBox;
  _cppBannerView->set_bounding_box(bounding_box);
  _cppBannerView->NotifyListenerOfBoundingBoxChange(bounding_box);
  _cppBannerView->NotifyListenerAdClosed();
}

@end
