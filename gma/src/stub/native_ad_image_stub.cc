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

#include <string>
#include <vector>

#include "gma/src/common/native_ad_image_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"

namespace firebase {
namespace gma {

NativeAdImage::NativeAdImage() {}

NativeAdImage::NativeAdImage(
    const NativeAdImageInternal& native_ad_image_internal) {}

NativeAdImage::~NativeAdImage() {}

NativeAdImage::NativeAdImage(const NativeAdImage& ad_image) : NativeAdImage() {}

NativeAdImage& NativeAdImage::operator=(const NativeAdImage& obj) {
  return *this;
}

/// Gets the image scale, which denotes the ratio of pixels to dp.
double NativeAdImage::scale() const {
  static const double empty = 0.0;
  return empty;
}

/// Gets the native ad image URI.
const std::string& NativeAdImage::image_uri() const {
  static const std::string empty;
  return empty;
}

Future<ImageResult> NativeAdImage::LoadImage() const {
  return CreateAndCompleteFutureWithImageResult(
      kNativeAdImageFnLoadImage, kAdErrorCodeNone, nullptr,
      &internal_->future_data, ImageResult());
}

Future<ImageResult> NativeAdImage::LoadImageLastResult() const {
  return CreateAndCompleteFutureWithImageResult(
      kNativeAdImageFnLoadImage, kAdErrorCodeNone, nullptr,
      &internal_->future_data, ImageResult());
}

}  // namespace gma
}  // namespace firebase
