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

#ifndef FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_TYPES_H_
#define FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_TYPES_H_

#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
extern "C" {
#include <objc/objc.h>
}  // extern "C"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace admob {

/// This is a platform specific datatype that is required to create an AdMob ad.
///
/// The following defines the datatype on each platform:
/// <ul>
///   <li>Android: A `jobject` which references an Android Activity.</li>
///   <li>iOS: An `id` which references an iOS UIView.</li>
/// </ul>
#if FIREBASE_PLATFORM_ANDROID
/// An Android Activity from Java.
typedef jobject AdParent;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
/// A pointer to an iOS UIView.
typedef id AdParent;
#else
/// A void pointer for stub classes.
typedef void *AdParent;
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

#ifdef INTERNAL_EXPERIMENTAL
// LINT.IfChange
#endif  // INTERNAL_EXPERIMENTAL
/// Error codes returned by Future::error().
enum AdMobError {
  /// Call completed successfully.
  kAdMobErrorNone,
  /// The ad has not been fully initialized.
  kAdMobErrorUninitialized,
  /// The ad is already initialized (repeat call).
  kAdMobErrorAlreadyInitialized,
  /// A call has failed because an ad is currently loading.
  kAdMobErrorLoadInProgress,
  /// A call to load an ad has failed due to an internal SDK error.
  kAdMobErrorInternalError,
  /// A call to load an ad has failed due to an invalid request.
  kAdMobErrorInvalidRequest,
  /// A call to load an ad has failed due to a network error.
  kAdMobErrorNetworkError,
  /// A call to load an ad has failed because no ad was available to serve.
  kAdMobErrorNoFill,
  /// An attempt has been made to show an ad on an Android Activity that has
  /// no window token (such as one that's not done initializing).
  kAdMobErrorNoWindowToken,
  /// Fallback error for any unidentified cases.
  kAdMobErrorUnknown,
};
#ifdef INTERNAL_EXPERIMENTAL
// LINT.ThenChange(//depot_firebase_cpp/admob/client/cpp/src_java/com/google/firebase/admob/internal/cpp/ConstantsHelper.java)
#endif  // INTERNAL_EXPERIMENTAL

/// The size of a banner ad.
class AdSize {
 public:
  ///  Denotes the orientation of the AdSize.
  enum Orientation {
    /// AdSize should reflect the current orientation of the device.
    kOrientationCurrent = 0,

    /// AdSize will be adaptively formatted in Landscape mode.
    kOrientationLandscape,

    /// AdSize will be adaptively formatted in Portrait mode.
    kOrientationPortrait
  };

  /// Denotes the type size object that the @ref AdSize represents.
  enum Type {
    /// The standard AdSize type of a set height and width.
    kTypeStandard = 0,

    /// An adaptive size anchored to a portion of the screen.
    kTypeAnchoredAdaptive,
  };

  /// Mobile Marketing Association (MMA) banner ad size (320x50
  /// density-independent pixels).
  static const AdSize kBanner;

  /// Interactive Advertising Bureau (IAB) full banner ad size
  /// (468x60 density-independent pixels).
  static const AdSize kFullBanner;

  /// Taller version of kGADAdSizeBanner. Typically 320x100.
  static const AdSize kLargeBanner;

  /// Interactive Advertising Bureau (IAB) leaderboard ad size
  /// (728x90 density-independent pixels).
  static const AdSize kLeaderBoard;

  /// Interactive Advertising Bureau (IAB) medium rectangle ad size
  /// (300x250 density-independent pixels).
  static const AdSize kMediumRectangle;

  /// Creates a new AdSize.
  ///
  /// @param[in] width The width of the ad in density-independent pixels.
  /// @param[in] height The height of the ad in density-independent pixels.
  AdSize(uint32_t width, uint32_t height);

  /// @brief Creates an AdSize with the given width and a Google-optimized
  /// height to create a banner ad in landscape mode.
  ///
  /// @param[in] width The width of the ad in density-independent pixels.
  ///
  /// @return an AdSize with the given width and a Google-optimized height
  /// to create a banner ad. The size returned will have an aspect ratio
  /// similar to BANNER, suitable for anchoring near the top or bottom of
  /// your app.
  static AdSize GetLandscapeAnchoredAdaptiveBannerAdSize(uint32_t width);

  /// @brief Creates an AdSize with the given width and a Google-optimized
  /// height to create a banner ad in portrait mode.
  ///
  /// @param[in] width The width of the ad in density-independent pixels.
  ///
  /// @return an AdSize with the given width and a Google-optimized height
  /// to create a banner ad. The size returned will have an aspect ratio
  /// similar to BANNER, suitable for anchoring near the top or bottom
  /// of your app.
  static AdSize GetPortraitAnchoredAdaptiveBannerAdSize(uint32_t width);

  /// @brief Creates an AdSize with the given width and a Google-optimized
  /// height to create a banner ad given the current orientation.
  ///
  /// @param[in] width The width of the ad in density-independent pixels.
  ///
  /// @return an AdSize with the given width and a Google-optimized height
  /// to create a banner ad. The size returned will have an aspect ratio
  /// similar to AdSize, suitable for anchoring near the top or bottom of
  /// your app.
  static AdSize GetCurrentOrientationAnchoredAdaptiveBannerAdSize(
      uint32_t width);

  /// Comparison operator.
  ///
  /// @return true if `rhs` refers to the same AdSize as `this`.
  bool operator==(const AdSize &rhs) const;

  /// Comparison operator.
  ///
  /// @returns true if `rhs` refers to a different AdSize as `this`.
  bool operator!=(const AdSize &rhs) const;

  /// The width of the region represented by this AdSize.  Value is in
  /// density-independent pixels.
  uint32_t width() const { return width_; }

  /// The height of the region represented by this AdSize. Value is in
  /// density-independent pixels.
  uint32_t height() const { return height_; }

  /// The AdSize orientation.
  Orientation orientation() const { return orientation_; }

  /// The AdSize type, either standard size or adaptive.
  Type type() const { return type_; }

 private:
  /// Returns an Anchor Adpative AdSize Object given a width and orientation.
  static AdSize GetAnchoredAdaptiveBannerAdSize(uint32_t width,
                                                Orientation orientation);

  /// Returns true if the AdSize parameter is equivalient to this AdSize object.
  bool is_equal(const AdSize &ad_size) const;

  /// Denotes the orientation for anchored adaptive AdSize objects.
  Orientation orientation_;

  /// Advertisement width in platform-indepenent pixels.
  uint32_t width_;

  /// Advertisement width in platform-indepenent pixels.
  uint32_t height_;

  /// The type of AdSize (standard or adaptive)
  Type type_;
};

/// @brief Gender information used as part of the
/// @ref firebase::admob::AdRequest struct.
enum Gender {
  /// The gender of the current user is unknown or unspecified by the publisher.
  kGenderUnknown = 0,
  /// The current user is known to be male.
  kGenderMale,
  /// The current user is known to be female.
  kGenderFemale
};

/// @brief Indicates whether an ad request is considered tagged for
/// child-directed treatment.
enum ChildDirectedTreatmentState {
  /// The child-directed status for the request is not indicated.
  kChildDirectedTreatmentStateUnknown = 0,
  /// The request is tagged for child-directed treatment.
  kChildDirectedTreatmentStateTagged,
  /// The request is not tagged for child-directed treatment.
  kChildDirectedTreatmentStateNotTagged
};

/// @brief Generic Key-Value container used for the "extras" values in an
/// @ref firebase::admob::AdRequest.
struct KeyValuePair {
  /// The name for an "extra."
  const char *key;
  /// The value for an "extra."
  const char *value;
};

/// @brief The information needed to request an ad.
struct AdRequest {
  /// An array of test device IDs specifying devices that test ads will be
  /// returned for.
  const char **test_device_ids;
  /// The number of entries in the array referenced by test_device_ids.
  unsigned int test_device_id_count;
  /// An array of keywords or phrases describing the current user activity, such
  /// as "Sports Scores" or "Football."
  const char **keywords;
  /// The number of entries in the array referenced by keywords.
  unsigned int keyword_count;
  /// A @ref KeyValuePair specifying additional parameters accepted by an ad
  /// network.
  const KeyValuePair *extras;
  /// The number of entries in the array referenced by extras.
  unsigned int extras_count;
};

/// @brief The screen location and dimensions of an ad view once it has been
/// initialized.
struct BoundingBox {
  /// Default constructor which initializes all member variables to 0.
  BoundingBox() : height(0), width(0), x(0), y(0) {}
  /// Height of the ad in pixels.
  int height;
  /// Width of the ad in pixels.
  int width;
  /// Horizontal position of the ad in pixels from the left.
  int x;
  /// Vertical position of the ad in pixels from the top.
  int y;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_TYPES_H_
