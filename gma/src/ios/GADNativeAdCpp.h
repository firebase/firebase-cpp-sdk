/*
 * Copyright 2023 Google LLC
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
#import <GoogleMobileAds/GADNativeAd.h>
#import <UIKit/UIKit.h>

@interface GADNativeAd()

/// AdChoices icon image.
@property(nonatomic, readonly, strong, nullable) GADNativeAdImage *adChoicesIcon;

/// Used only by allowlisted ad units. Provide a dictionary containing click data.
- (void)performClickWithData:(nonnull NSDictionary *)clickData;

/// Used only by allowlisted ad units. Provide a dictionary containing impression data. Returns YES
/// if the impression is successfully recorded, otherwise returns NO.
- (BOOL)recordImpressionWithData:(nonnull NSDictionary *)impressionData;

@end

