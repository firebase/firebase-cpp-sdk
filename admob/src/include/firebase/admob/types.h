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

#include <string>
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

/// @brief Types of ad sizes.
enum AdSizeType { kAdSizeStandard = 0 };

/// @brief An ad size value to be used in requesting ads.
struct AdSize {
  /// The type of ad size.
  AdSizeType ad_size_type;
  /// Height of the ad (in points or dp).
  int height;
  /// Width of the ad (in points or dp).
  int width;
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
    kUnderAgeOfConsentUnspecified,

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
