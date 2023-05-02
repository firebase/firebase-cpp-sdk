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

#ifndef FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_NATIVE_AD_H_
#define FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_NATIVE_AD_H_

#include <string>
#include <vector>

#include "firebase/future.h"
#include "firebase/gma/types.h"
#include "firebase/internal/common.h"

// Doxygen breaks trying to parse this file, and since it is internal logic,
// it doesn't need to be included in the generated documentation.
#ifndef DOXYGEN

namespace firebase {
namespace gma {

namespace internal {
// Forward declaration for platform-specific data, implemented in each library.
class NativeAdInternal;
}  // namespace internal

struct NativeAdImageInternal;

class GmaInternal;
class NativeAdImage;

class NativeAd {
 public:
  NativeAd();
  ~NativeAd();

  /// Initialize the NativeAd object.
  /// parent: The platform-specific UI element that will host the ad.
  Future<void> Initialize(AdParent parent);

  /// Returns a Future containing the status of the last call to
  /// Initialize.
  Future<void> InitializeLastResult() const;

  /// Begins an asynchronous request for an ad.
  ///
  /// ad_unit_id: The ad unit ID to use in loading the ad.
  /// request: An AdRequest struct with information about the request
  ///                    to be made (such as targeting info).
  Future<AdResult> LoadAd(const char* ad_unit_id, const AdRequest& request);

  /// Returns a Future containing the status of the last call to
  /// LoadAd.
  Future<AdResult> LoadAdLastResult() const;

  /// Returns the associated icon asset of the native ad.
  const NativeAdImage icon() const;

  /// Returns the associated image assets of the native ad.
  const std::vector<NativeAdImage>& images() const;

 private:
  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  internal::NativeAdInternal* internal_;
};

class NativeAdImage {
 public:
  /// Default Constructor.
  NativeAdImage();

  /// Copy Constructor.
  NativeAdImage(const NativeAdImage& source_native_image);

  /// Returns the image scale, which denotes the ratio of pixels to dp.
  double scale() const;

  /// Returns the image uri.
  const std::string& image_uri() const;

  virtual ~NativeAdImage();

  /// Assignment operator.
  NativeAdImage& operator=(const NativeAdImage& obj);

 private:
  friend class NativeAd;
  friend class GmaInternal;

  explicit NativeAdImage(const NativeAdImageInternal& native_ad_image_internal);

  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  NativeAdImageInternal* internal_;
};

}  // namespace gma
}  // namespace firebase

#endif  // DOXYGEN

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_INTERNAL_NATIVE_AD_H_
