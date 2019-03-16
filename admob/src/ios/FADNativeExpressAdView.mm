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

#include "admob/src/ios/FADNativeExpressAdView.h"

namespace admob = firebase::admob;

@interface FADNativeExpressAdView () <GADNativeExpressAdViewDelegate> {
  /// The publisher-provided view that is the parent view of the native express ad view.
  UIView *__weak _parentView;

  /// The native express ad view.
  GADNativeExpressAdView *_nativeExpressAdView;

  /// The current horizontal layout constraint for the native express ad view.
  NSLayoutConstraint *_nativeExpressAdViewHorizontalLayoutConstraint;

  /// The current vertical layout constraint for the native express ad view.
  NSLayoutConstraint *_nativeExpressAdViewVerticalLayoutConstraint;

  /// The native express ad view's ad unit ID.
  NSString *_adUnitID;

  /// The native express ad view's ad size.
  admob::AdSize _adSize;

  /// The NativeExpressAdViewInternalIOS object.
  admob::internal::NativeExpressAdViewInternalIOS *_cppNativeExpressAdView;

  /// Indicates if the ad loaded.
  BOOL _adLoaded;
}

@end

@implementation FADNativeExpressAdView

#pragma mark - Initialization

- (instancetype)initWithView:(UIView *)view
                       adUnitID:(NSString *)adUnitID
                         adSize:(admob::AdSize)adSize
    internalNativeExpressAdView:
        (firebase::admob::internal::NativeExpressAdViewInternalIOS *)cppNativeExpressAdView {
  CGRect frame = CGRectMake(0, 0, adSize.width, adSize.height);
  self = [super initWithFrame:frame];
  if (self) {
    _parentView = view;
    _adUnitID = [adUnitID copy];
    _adSize = adSize;
    _cppNativeExpressAdView = cppNativeExpressAdView;
    _presentationState = admob::NativeExpressAdView::kPresentationStateHidden;
    [self setUpNativeExpressAdView];
  }
  return self;
}

/// Called from the designated initializer. Sets up a native express ad view.
- (void)setUpNativeExpressAdView {
  GADAdSize size = GADAdSizeFromCGSize(CGSizeMake(_adSize.width, _adSize.height));
  _nativeExpressAdView = [[GADNativeExpressAdView alloc] initWithAdSize:size];
  _nativeExpressAdView.adUnitID = _adUnitID;
  _nativeExpressAdView.delegate = self;

  // The FADNativeExpressAdView is hidden until the publisher calls Show().
  self.hidden = YES;

  [self addSubview:_nativeExpressAdView];
  _nativeExpressAdView.translatesAutoresizingMaskIntoConstraints = NO;

  // Add layout constraints to center the native express ad view vertically and horizontally.
  NSDictionary *viewDictionary = NSDictionaryOfVariableBindings(_nativeExpressAdView);
  NSArray *verticalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"V:|[_nativeExpressAdView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:verticalConstraints];
  NSArray *horizontalConstraints =
      [NSLayoutConstraint constraintsWithVisualFormat:@"H:|[_nativeExpressAdView]|"
                                              options:0
                                              metrics:nil
                                                views:viewDictionary];
  [self addConstraints:horizontalConstraints];

  UIView *strongView = _parentView;
  _nativeExpressAdView.rootViewController = strongView.window.rootViewController;
  [strongView addSubview:self];
  self.translatesAutoresizingMaskIntoConstraints = NO;
  [self updateNativeExpressAdViewLayoutConstraintsWithPosition:
            (admob::NativeExpressAdView::kPositionTopLeft)
                                                   xCoordinate:0
                                                   yCoordinate:0];
}

#pragma mark - Property Accessor Methods

- (admob::BoundingBox)boundingBox {
  admob::BoundingBox box;
  if (!_nativeExpressAdView) {
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
  // Make the native express ad request.
  [_nativeExpressAdView loadRequest:request];
}

- (void)hide {
  self.hidden = YES;
  _presentationState = admob::NativeExpressAdView::kPresentationStateHidden;
}

- (void)show {
  self.hidden = NO;
  if (_adLoaded) {
    _presentationState = admob::NativeExpressAdView::kPresentationStateVisibleWithAd;
  } else {
    _presentationState = admob::NativeExpressAdView::kPresentationStateVisibleWithoutAd;
  }
}

- (void)destroy {
  [_nativeExpressAdView removeFromSuperview];
  _nativeExpressAdView.delegate = nil;
  _nativeExpressAdView = nil;
  _presentationState = admob::NativeExpressAdView::kPresentationStateHidden;
}

- (void)moveNativeExpressAdViewToXCoordinate:(int)x yCoordinate:(int)y {
  // The moveNativeExpressAdViewToXCoordinate:yCoordinate: method gets the x-coordinate and
  // y-coordinate in pixels. Need to convert the pixels to points before updating the native express
  // ad view's layout constraints.
  CGFloat scale = [UIScreen mainScreen].scale;
  CGFloat xPoints = x / scale;
  CGFloat yPoints = y / scale;
  [self updateNativeExpressAdViewLayoutConstraintsWithPosition:
            (admob::NativeExpressAdView::kPositionTopLeft)
                                                   xCoordinate:xPoints
                                                   yCoordinate:yPoints];
}

- (void)moveNativeExpressAdViewToPosition:(admob::NativeExpressAdView::Position)position {
  [self updateNativeExpressAdViewLayoutConstraintsWithPosition:position
                                                   xCoordinate:0
                                                   yCoordinate:0];
}

#pragma mark - Private Methods

/// Updates the layout constraints for the native express ad view.
- (void)updateNativeExpressAdViewLayoutConstraintsWithPosition:
            (admob::NativeExpressAdView::Position)position
                                                   xCoordinate:(CGFloat)x
                                                   yCoordinate:(CGFloat)y {
  NSLayoutAttribute verticalLayoutAttribute = NSLayoutAttributeNotAnAttribute;
  NSLayoutAttribute horizontalLayoutAttribute = NSLayoutAttributeNotAnAttribute;

  switch (position) {
    case admob::NativeExpressAdView::kPositionTop:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::NativeExpressAdView::kPositionBottom:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeCenterX;
      break;
    case admob::NativeExpressAdView::kPositionTopLeft:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case admob::NativeExpressAdView::kPositionTopRight:
      verticalLayoutAttribute = NSLayoutAttributeTop;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    case admob::NativeExpressAdView::kPositionBottomLeft:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeLeft;
      break;
    case admob::NativeExpressAdView::kPositionBottomRight:
      verticalLayoutAttribute = NSLayoutAttributeBottom;
      horizontalLayoutAttribute = NSLayoutAttributeRight;
      break;
    default:
      break;
  }

  // Remove the existing vertical and horizontal layout constraints for the native express ad view,
  // if they exist.
  UIView *strongView = _parentView;
  if (_nativeExpressAdViewVerticalLayoutConstraint &&
      _nativeExpressAdViewHorizontalLayoutConstraint) {
    NSArray *nativeExpressAdViewLayoutConstraints = @[
      _nativeExpressAdViewVerticalLayoutConstraint,
      _nativeExpressAdViewHorizontalLayoutConstraint
    ];
    [strongView removeConstraints:nativeExpressAdViewLayoutConstraints];
  }

  // Set the vertical and horizontal layout constraints for the native express ad view.
  _nativeExpressAdViewVerticalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:verticalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:verticalLayoutAttribute
                                  multiplier:1
                                    constant:y];
  [strongView addConstraint:_nativeExpressAdViewVerticalLayoutConstraint];
  _nativeExpressAdViewHorizontalLayoutConstraint =
      [NSLayoutConstraint constraintWithItem:self
                                   attribute:horizontalLayoutAttribute
                                   relatedBy:NSLayoutRelationEqual
                                      toItem:strongView
                                   attribute:horizontalLayoutAttribute
                                  multiplier:1
                                    constant:x];
  [strongView addConstraint:_nativeExpressAdViewHorizontalLayoutConstraint];
  [self setNeedsLayout];
}

/// Updates the presentation state of the native express ad view. This method gets called when the
/// observer is notified that the application is active again (i.e. when the user returns to the
/// application from Safari or the App Store).
- (void)applicationDidBecomeActive:(NSNotification *)notification {
  _presentationState = admob::NativeExpressAdView::kPresentationStateVisibleWithAd;
  _cppNativeExpressAdView->NotifyListenerOfPresentationStateChange(_presentationState);
  // Remove the observer that was registered in the nativeExpressAdViewWillLeaveApplication:
  // callback.
  [[NSNotificationCenter defaultCenter] removeObserver:self
                                                  name:UIApplicationDidBecomeActiveNotification
                                                object:nil];
}

#pragma mark - UIView

- (void)layoutSubviews {
  [super layoutSubviews];
  _cppNativeExpressAdView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

#pragma mark - GADNativeExpressAdViewDelegate

- (void)nativeExpressAdViewDidReceiveAd:(GADNativeExpressAdView *)nativeExpressAdView {
  _adLoaded = YES;
  _cppNativeExpressAdView->CompleteLoadFuture(admob::kAdMobErrorNone, nullptr);
  // Only update the presentation state if the FADNativeExpressAdView is already visible.
  if (!self.hidden) {
    _presentationState = admob::NativeExpressAdView::kPresentationStateVisibleWithAd;
    _cppNativeExpressAdView->NotifyListenerOfPresentationStateChange(_presentationState);
    // Loading an ad can sometimes cause the bounds to change.
    _cppNativeExpressAdView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
  }
}

- (void)nativeExpressAdView:(GADNativeExpressAdView *)nativeExpressAdView
    didFailToReceiveAdWithError:(GADRequestError *)error {
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
  _cppNativeExpressAdView->CompleteLoadFuture(api_error, error_msg);
}

- (void)nativeExpressAdViewWillPresentScreen:(GADNativeExpressAdView *)nativeExpressAdView {
  _presentationState = admob::NativeExpressAdView::kPresentationStateCoveringUI;
  _cppNativeExpressAdView->NotifyListenerOfPresentationStateChange(_presentationState);
  _cppNativeExpressAdView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

- (void)nativeExpressAdViewDidDismissScreen:(GADNativeExpressAdView *)nativeExpressAdView {
  _presentationState = admob::NativeExpressAdView::kPresentationStateVisibleWithAd;
  _cppNativeExpressAdView->NotifyListenerOfPresentationStateChange(_presentationState);
  _cppNativeExpressAdView->NotifyListenerOfBoundingBoxChange(self.boundingBox);
}

- (void)nativeExpressAdViewWillLeaveApplication:(GADNativeExpressAdView *)nativeExpressAdView {
  _presentationState = admob::NativeExpressAdView::kPresentationStateCoveringUI;
  _cppNativeExpressAdView->NotifyListenerOfPresentationStateChange(_presentationState);
  // The FADNativeExpressAdView object will need to get notified when the application becomes active
  // again so it can update the GADNativeExpressAdView's presentation state.
  [[NSNotificationCenter defaultCenter] addObserver:self
                                           selector:@selector(applicationDidBecomeActive:)
                                               name:UIApplicationDidBecomeActiveNotification
                                             object:nil];
}

@end
