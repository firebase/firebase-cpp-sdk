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

#include <memory>
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

struct AdResultInternal;
struct AdapterResponseInfoInternal;
struct LoadAdResultInternal;
struct ResponseInfoInternal;

class AdmobInternal;
class BannerView;
class InterstitialAd;

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
typedef void* AdParent;
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

/// Information about why an ad operation failed.
class AdResult {
 public:
  virtual ~AdResult();

  /// If the operation was successful then the other error reporting methods
  /// of this object will return defaults.
  virtual bool is_successful() const;

  /// Retrieves an AdResult which represents the cause of this error.
  ///
  /// @return a pointer to an AdResult which represents the cause of this
  /// AdResult.  If there was no cause, or if this result was successful,
  /// then nullptr is returned.
  virtual std::unique_ptr<AdResult> GetCause();

  /// Gets the error's code.
  virtual int code();

  /// Gets the domain of the error.
  virtual const std::string& domain();

  /// Gets the message describing the error.
  virtual const std::string& message();

  /// Returns a log friendly string version of this object.
  virtual const std::string& ToString();

  /// A domain string which represents an undefined error domain.
  ///
  /// The Admob SDK returns this domain for domain() method invocations when
  /// converting error information from legacy mediation adapter callbacks.
  static const char* kUndefinedDomain;

 protected:
  friend class AdMobInternal;

  AdResult();
  explicit AdResult(const AdResultInternal& ad_result_internal);

  // Copy Constructor
  AdResult(const AdResult& ad_result);
  AdResult& operator=(const AdResult& obj);

  /// Sets the internally cached string. Used by the LoadAdError subclass.
  void set_to_string(std::string to_string);

  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  AdResultInternal* internal_;

 private:
  friend class AdapterResponseInfo;

  bool is_successful_;
  int code_;
  std::string domain_;
  std::string message_;
  std::string to_string_;
};

/// @brief Response information for an individual ad network contained within
/// a @ref ResponseInfo object.
class AdapterResponseInfo {
 public:
  /// Information about the Ad Error, if one occurred.
  ///
  /// @return the error that occurred while rendering the ad.  If no error
  /// occurred then the AdResults's successful method will return false.
  AdResult ad_result() const { return ad_result_; }

  /// Returns a string representation of a class name that identifies the ad
  /// network adapter.
  const std::string& adapter_class_name() const { return adapter_class_name_; }

  /// Amount of time the ad network spent loading an ad.
  ///
  /// @return number of milliseconds the network spent loading an ad. This value
  /// is 0 if the network did not make a load attempt.
  int64_t latency_in_millis() const { return latency_; }

  /// A log friendly string version of this object.
  const std::string& ToString() const { return to_string_; }

 private:
  friend class ResponseInfo;

  /// Constructs an Adapter Response Info Object.
  explicit AdapterResponseInfo(const AdapterResponseInfoInternal& internal);

  AdResult ad_result_;
  std::string adapter_class_name_;
  int64_t latency_;
  std::string to_string_;
};

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
  const char* key;
  /// The value for an "extra."
  const char* value;
};

/// @brief The information needed to request an ad.
struct AdRequest {
  /// An array of keywords or phrases describing the current user activity, such
  /// as "Sports Scores" or "Football."
  const char** keywords;
  /// The number of entries in the array referenced by keywords.
  unsigned int keyword_count;
  /// A @ref KeyValuePair specifying additional parameters accepted by an ad
  /// network.
  const KeyValuePair* extras;
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

/// Information about an ad response.
class ResponseInfo {
 public:
  /// Constructor creates an uninitialized ResponseInfo.
  ResponseInfo();

  /// Gets the AdaptorReponseInfo objects for the ad response.
  ///
  /// @return a vector of AdapterResponseInfo objects containing metadata for
  ///   each adapter included in the ad response.
  const std::vector<AdapterResponseInfo>& adapter_responses() const {
    return adapter_responses_;
  }

  /// Future Milestone.
  /// A class name that identifies the ad network that returned the ad.
  /// Returns an empty string if the ad failed to load.
  const std::string& mediation_adapter_class_name() const {
    return mediation_adapter_class_name_;
  }

  /// Gets the response ID string for the loaded ad.
  const std::string& response_id() const { return response_id_; }

  /// Gets a log friendly string version of this object.
  const std::string& ToString() const { return to_string_; }

 private:
  friend class LoadAdResult;

  explicit ResponseInfo(const ResponseInfoInternal& internal);

  std::vector<AdapterResponseInfo> adapter_responses_;
  std::string mediation_adapter_class_name_;
  std::string response_id_;
  std::string to_string_;
};

/// @brief Information about why an ad load operation failed.
class LoadAdResult : public AdResult {
 public:
  LoadAdResult();

  // Copy Consturctor.
  LoadAdResult(const LoadAdResult& load_ad_result);

  /// Gets the ResponseInfo if an error occurred, with a collection of
  /// information from each adapter.
  const ResponseInfo& response_info() const { return response_info_; }

 private:
  friend class AdmobInternal;

  explicit LoadAdResult(const LoadAdResultInternal& load_ad_result_internal);

  ResponseInfo response_info_;
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
