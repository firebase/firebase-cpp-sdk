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

#import "gma/src/ios/FADAdSize.h"

#include "gma/src/common/gma_common.h"

namespace firebase {
namespace gma {

GADAdSize GADSizeFromCppAdSize(const AdSize& ad_size) {
  switch (ad_size.type()) {
    case AdSize::kTypeAnchoredAdaptive:
      switch (ad_size.orientation()) {
        case AdSize::kOrientationLandscape:
          return GADLandscapeAnchoredAdaptiveBannerAdSizeWithWidth(ad_size.width());
        case AdSize::kOrientationPortrait:
          return GADPortraitAnchoredAdaptiveBannerAdSizeWithWidth(ad_size.width());
        case AdSize::kOrientationCurrent:
          return GADCurrentOrientationAnchoredAdaptiveBannerAdSizeWithWidth(ad_size.width());
        default:
          FIREBASE_ASSERT_MESSAGE(true, "Unknown AdSize Orientation");
      }
      break;
    case AdSize::kTypeInlineAdaptive:
      if (ad_size.height() != 0) {
        return GADInlineAdaptiveBannerAdSizeWithWidthAndMaxHeight(ad_size.width(),
                                                                  ad_size.height());
      }
      switch (ad_size.orientation()) {
        case AdSize::kOrientationLandscape:
          return GADLandscapeInlineAdaptiveBannerAdSizeWithWidth(ad_size.width());
        case AdSize::kOrientationPortrait:
          return GADPortraitInlineAdaptiveBannerAdSizeWithWidth(ad_size.width());
        case AdSize::kOrientationCurrent:
          return GADCurrentOrientationInlineAdaptiveBannerAdSizeWithWidth(ad_size.width());
        default:
          FIREBASE_ASSERT_MESSAGE(true, "Unknown AdSize Orientation");
      }
      break;
    case AdSize::kTypeStandard:
      return GADAdSizeFromCGSize(CGSizeMake(ad_size.width(), ad_size.height()));
      break;
    default:
      FIREBASE_ASSERT_MESSAGE(true, "Unknown AdSize Type");
  }
}

}  // namespace gma
}  // namespace firebase
