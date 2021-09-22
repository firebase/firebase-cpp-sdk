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

#include <map>
#include <string>
#include <unordered_set>
#include <vector>

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

/// @brief Generic Key-Value container used for the "extras" values in an
/// @ref firebase::admob::AdRequest.
struct KeyValuePair {
  /// The name for an "extra."
  const char *key;
  /// The value for an "extra."
  const char *value;
};

#if 0
/// @brief The information needed to request an ad.
struct AdRequest {
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
#else
class AdRequest {
 public:
  /// Creates an @ref AdRequest with no custom configuration.
  AdRequest();

  /// Creates an @ref AdRequest with the optional content URL.
  ///
  /// When requesting an ad, apps may pass the URL of the content they are
  /// serving. This enables keyword targeting to match the ad with the content.
  ///
  /// The URL is ignored if null or the number of characters exceeds 512.
  ///
  /// @param[in] content_url the url of the content being viewed.
  AdRequest(const char *content_url);

  ~AdRequest();

  /// The content URL targeting information.
  ///
  /// @return the content URL for the @ref AdRequest. The string will be empty
  /// if no content URL has been configured.
  const std::string &content_url() const { return content_url_; }

  /// A Map of ad network adapters to their collection of extra parameters, as
  /// configured via @ref add_extra.
  const std::map<std::string, std::map<std::string, std::string> > &extras()
      const {
    return extras_;
  }

  /// Keywords which will help Admob to provide targeted ads, as added by
  /// @ref add_keyword.
  const std::unordered_set<std::string> &keywords() const { return keywords_; }

  /// Add a network extra for the associated ad_network.
  ///
  /// Appends an extra to the corresponding list of extras for the ad_network.
  /// Each ad network can have multiple extra strings.
  ///
  /// @param[in] ad_network the ad network for which to add the extra.
  /// @param[in] extra_key a key which will be passed to the corresponding ad
  /// network adapter.
  /// @param[in] extra_value the value associated with @ref extra_key.
  void add_extra(const char *ad_network, const char *extra_key,
                 const char *extra_value);

  /// Adds a keyword for targeting purposes.
  ///
  /// Multiple keywords may be added via repeated invocations of this method.
  ///
  /// @param[in] a string that Admob will use to aid in targeting ads.
  void add_keyword(const char *keyword);

  /// When requesting an ad, apps may pass the URL of the content they are
  /// serving. This enables keyword targeting to match the ad with the content.
  ///
  /// The URL is ignored if null or the number of characters exceeds 512.
  ///
  /// @param[in] content_url the url of the content being viewed.
  void set_content_url(const char *content_url);

 private:
  std::string content_url_;
  std::map<std::string, std::map<std::string, std::string> > extras_;
  std::unordered_set<std::string> keywords_;
};
#endif

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

/// @brief Global configuration that will be used for every @ref AdRequest.
/// Set the configuration via @ref SetRequestConfiguration.
struct RequestConfiguration {
  /// A maximum ad content rating, which may be configured via
  /// @ref max_ad_content_rating.
  enum MaxAdContentRating {
    /// No content rating has been specified.
    kMaxAdContentRatingUnspecified = -1,

    /// Content suitable for general audiences, including families.
    kMaxAdContentRatingG,

    /// Content suitable only for mature audiences.
    kMaxAdContentRatingMA,

    /// Content suitable for most audiences with parental guidance.
    kMaxAdContentRatingPG,

    /// Content suitable for teen and older audiences.
    kMaxAdContentRatingT
  };

  /// Specify whether you would like your app to be treated as child-directed
  /// for purposes of the Children’s Online Privacy Protection Act (COPPA).
  /// Values defined here may be configured via
  /// @ref tag_for_child_directed_treatment.
  enum TagForChildDirectedTreatment {
    /// Indicates that the publisher has not specified whether the ad request
    /// should receive treatment for users in the European Economic Area (EEA)
    /// under the age of consent.
    kChildDirectedTreatmentUnspecified = -1,

    /// Indicates the publisher specified that the ad request should not receive
    /// treatment for users in the European Economic Area (EEA) under the age of
    /// consent.
    kChildDirectedTreatmentFalse,

    /// Indicates the publisher specified that the ad request should receive
    /// treatment for users in the European Economic Area (EEA) under the age of
    /// consent.
    kChildDirectedTreatmentTrue
  };

  /// Configuration values to mark your app to receive treatment for users in
  /// the European Economic Area (EEA) under the age of consent. Values defined
  /// here should be configured via @ref tag_for_under_age_of_consent.
  enum TagForUnderAgeOfConsent {
    /// Indicates that the publisher has not specified whether the ad request
    /// should receive treatment for users in the European Economic Area (EEA)
    /// under the age of consent.
    kUnderAgeOfConsentUnspecified = -1,

    /// Indicates the publisher specified that the ad request should not receive
    /// treatment for users in the European Economic Area (EEA) under the age of
    /// consent.
    kUnderAgeOfConsentFalse,

    /// Indicates the publisher specified that the ad request should receive
    /// treatment for users in the European Economic Area (EEA) under the age of
    /// consent.
    kUnderAgeOfConsentTrue
  };

  /// Sets a maximum ad content rating. AdMob ads returned for your app will
  /// have a content rating at or below that level.
  MaxAdContentRating max_ad_content_rating;

  /// @brief Allows you to specify whether you would like your app
  /// to be treated as child-directed for purposes of the Children’s Online
  /// Privacy Protection Act (COPPA) -
  /// http://business.ftc.gov/privacy-and-security/childrens-privacy.
  ///
  /// If you set this value to kChildDirectedTreatmentTrue, you will indicate
  /// that your app should be treated as child-directed for purposes of the
  /// Children’s Online Privacy Protection Act (COPPA).
  ///
  /// If you set this value to kChildDirectedTreatmentFalse, you will indicate
  /// that your app should not be treated as child-directed for purposes of the
  /// Children’s Online Privacy Protection Act (COPPA).
  ///
  /// If you do not set this value, or set this value to
  /// kChildDirectedTreatmentUnspecified, ad requests will include no indication
  /// of how you would like your app treated with respect to COPPA.
  ///
  /// By setting this value, you certify that this notification is accurate and
  /// you are authorized to act on behalf of the owner of the app. You
  /// understand that abuse of this setting may result in termination of your
  /// Google account.
  ///
  /// @note: it may take some time for this designation to be fully implemented
  /// in applicable Google services.
  ///
  TagForChildDirectedTreatment tag_for_child_directed_treatment;

  /// This value allows you to mark your app to receive treatment for users in
  /// the European Economic Area (EEA) under the age of consent. This feature is
  /// designed to help facilitate compliance with the General Data Protection
  /// Regulation (GDPR). Note that you may have other legal obligations under
  /// GDPR. Please review the European Union's guidance and consult with your
  /// own legal counsel. Please remember that Google's tools are designed to
  /// facilitate compliance and do not relieve any particular publisher of its
  /// obligations under the law.
  ///
  /// When using this feature, a Tag For Users under the Age of Consent in
  /// Europe (TFUA) parameter will be included in all ad requests. This
  /// parameter disables personalized advertising, including remarketing, for
  /// that specific ad request. It also disables requests to third-party ad
  /// vendors, such as ad measurement pixels and third-party ad servers.
  ///
  /// If you set this value to RequestConfiguration.kUnderAgeOfConsentTrue, you
  /// will indicate that you want your app to be handled in a manner suitable
  /// for users under the age of consent.
  ///
  /// If you set this value to RequestConfiguration.kUnderAgeOfConsentFalse,
  /// you will indicate that you don't want your app to be handled in a manner
  /// suitable for users under the age of consent.
  ///
  /// If you do not set this value, or set this value to
  /// kUnderAgeOfConsentUnspecified, your app will include no indication of how
  /// you would like your app to be handled in a manner suitable for users under
  /// the age of consent.
  TagForUnderAgeOfConsent tag_for_under_age_of_consent;

  /// Sets a list of test device IDs corresponding to test devices which will
  /// always request test ads.
  std::vector<std::string> test_device_ids;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_TYPES_H_
