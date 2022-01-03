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

#ifndef FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_TYPES_H_
#define FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_TYPES_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "firebase/future.h"
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
namespace gma {

struct AdResultInternal;
struct AdapterResponseInfoInternal;
struct BoundingBox;
struct ResponseInfoInternal;

class AdViewBoundingBoxListener;
class GmaInternal;
class AdViewInternal;
class BannerView;
class InterstitialAd;
class PaidEventListener;
class ResponseInfo;

/// This is a platform specific datatype that is required to create
/// a Google Mobile Ads ad.
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

#ifdef INTERNAL_EXPERIMENTAL
// LINT.IfChange
#endif  // INTERNAL_EXPERIMENTAL
/// Error codes returned by Future::error().
enum AdError {
  /// Call completed successfully.
  kAdErrorNone,
  /// The ad has not been fully initialized.
  kAdErrorUninitialized,
  /// The ad is already initialized (repeat call).
  kAdErrorAlreadyInitialized,
  /// A call has failed because an ad is currently loading.
  kAdErrorLoadInProgress,
  /// A call to load an ad has failed due to an internal SDK error.
  kAdErrorInternalError,
  /// A call to load an ad has failed due to an invalid request.
  kAdErrorInvalidRequest,
  /// A call to load an ad has failed due to a network error.
  kAdErrorNetworkError,
  /// A call to load an ad has failed because no ad was available to serve.
  kAdErrorNoFill,
  /// An attempt has been made to show an ad on an Android Activity that has
  /// no window token (such as one that's not done initializing).
  kAdErrorNoWindowToken,
  /// An attempt to load an Ad Network extras class for an ad request has
  /// failed.
  kAdErrorAdNetworkClassLoadError,
  /// The ad server experienced a failure processing the request.
  kAdErrorServerError,
  /// The current device’s OS is below the minimum required version.
  kAdErrorOSVersionTooLow,
  /// The request was unable to be loaded before being timed out.
  kAdErrorTimeout,
  /// Will not send request because the interstitial object has already been
  /// used.
  kAdErrorInterstitialAlreadyUsed,
  /// The mediation response was invalid.
  kAdErrorMediationDataError,
  /// Error finding or creating a mediation ad network adapter.
  kAdErrorMediationAdapterError,
  /// Attempting to pass an invalid ad size to an adapter.
  kAdErrorMediationInvalidAdSize,
  /// Invalid argument error.
  kAdErrorInvalidArgument,
  /// Received invalid response.
  kAdErrorReceivedInvalidResponse,
  /// Will not send a request because the rewarded ad object has already been
  /// used.
  kAdErrorRewardedAdAlreadyUsed,
  /// A mediation ad network adapter received an ad request, but did not fill.
  /// The adapter’s error is included as an underlyingError.
  kAdErrorMediationNoFill,
  /// Will not send request because the ad object has already been used.
  kAdErrorAdAlreadyUsed,
  /// Will not send request because the application identifier is missing.
  kAdErrorApplicationIdentifierMissing,
  /// Android Ad String is invalid.
  kAdErrorInvalidAdString,
  /// The ad can not be shown when app is not in the foreground.
  kAdErrorAppNotInForeground,
  /// A mediation adapter failed to show the ad.
  kAdErrorMediationShowError,
  /// The ad is not ready to be shown.
  kAdErrorAdNotReady,
  /// Ad is too large for the scene.
  kAdErrorAdTooLarge,
  /// Attempted to present ad from a non-main thread. This is an internal
  /// error which should be reported to support if encountered.
  kAdErrorNotMainThread,
  /// Fallback error for any unidentified cases.
  kAdErrorUnknown,
};

/// A listener for receiving notifications during the lifecycle of a BannerAd.
class AdListener {
 public:
  virtual ~AdListener();

  /// Called when a click is recorded for an ad.
  virtual void OnAdClicked() {}

  /// Called when the user is about to return to the application after clicking
  /// on an ad.
  virtual void OnAdClosed() {}

  /// Called when an impression is recorded for an ad.
  virtual void OnAdImpression() {}

  /// Called when an ad opens an overlay that covers the screen.
  virtual void OnAdOpened() {}
};

/// Information about why an ad operation failed.
class AdResult {
 public:
  /// Default Constructor.
  AdResult();

  /// Copy Constructor.
  AdResult(const AdResult& ad_result);

  /// Destructor.
  virtual ~AdResult();

  /// Assignment operator.
  AdResult& operator=(const AdResult& obj);

  /// If the operation was successful then the other error reporting methods
  /// of this object will return defaults.
  bool is_successful() const;

  /// Retrieves an AdResult which represents the cause of this error.
  ///
  /// @return a pointer to an AdResult which represents the cause of this
  /// AdResult.  If there was no cause, or if this result was successful,
  /// then nullptr is returned.
  std::unique_ptr<AdResult> GetCause() const;

  /// Gets the error's code.
  AdError code() const;

  /// Gets the domain of the error.
  const std::string& domain() const;

  /// Gets the message describing the error.
  const std::string& message() const;

  /// Gets the ResponseInfo if an error occurred during a loadAd operation.
  /// The ResponseInfo will have empty fields if no error occurred, or if this
  /// AdResult does not represent an error stemming from a loadAd operation.
  const ResponseInfo& response_info() const;

  /// Returns a log friendly string version of this object.
  const std::string& ToString() const;

  /// A domain string which represents an undefined error domain.
  ///
  /// The GMA SDK returns this domain for domain() method invocations when
  /// converting error information from legacy mediation adapter callbacks.
  static const char* const kUndefinedDomain;

 protected:
  /// Constructor used when building results in Ad event callbacks.
  explicit AdResult(const AdResultInternal& ad_result_internal);

 private:
  friend class AdapterResponseInfo;
  friend class GmaInternal;
  friend class BannerView;
  friend class InterstitialAd;

  // Collection of response from adapters if this Result is due to a loadAd
  // operation.
  ResponseInfo* response_info_;

  // An internal, platform-specific implementation object that this class uses
  // to interact with the Google Mobile Ads SDKs for iOS and Android.
  AdResultInternal* internal_;
};

/// A snapshot of a mediation adapter's initialization status.
class AdapterStatus {
 public:
  AdapterStatus() : is_initialized_(false), latency_(0) {}

  /// Detailed description of the status.
  ///
  /// This method should only be used for informational purposes, such as
  /// logging. Use @ref is_initialized to make logical decisions regarding an
  /// adapter's status.
  const std::string& description() const { return description_; }

  /// Returns the adapter's initialization state.
  bool is_initialized() const { return is_initialized_; }

  /// The adapter's initialization latency in milliseconds.
  /// 0 if initialization has not yet ended.
  int latency() const { return latency_; }

#if !defined(DOXYGEN)
  // Equality operator for testing.
  bool operator==(const AdapterStatus& rhs) const {
    return (description() == rhs.description() &&
            is_initialized() == rhs.is_initialized() &&
            latency() == rhs.latency());
  }
#endif  // !defined(DOXYGEN)

 private:
  friend class GmaInternal;
  std::string description_;
  bool is_initialized_;
  int latency_;
};

/// An immutable snapshot of the GMA SDK’s initialization status, categorized
/// by mediation adapter.
class AdapterInitializationStatus {
 public:
  /// Initialization status of each known ad network, keyed by its adapter's
  /// class name.
  std::map<std::string, AdapterStatus> GetAdapterStatusMap() const {
    return adapter_status_map_;
  }
#if !defined(DOXYGEN)
  // Equality operator for testing.
  bool operator==(const AdapterInitializationStatus& rhs) const {
    return (GetAdapterStatusMap() == rhs.GetAdapterStatusMap());
  }
#endif  // !defined(DOXYGEN)

 private:
  friend class GmaInternal;
  std::map<std::string, AdapterStatus> adapter_status_map_;
};

/// Listener to be invoked when the Ad Inspector has been closed.
class AdInspectorClosedListener {
 public:
  virtual ~AdInspectorClosedListener();

  /// Called when the user clicked the ad.
  virtual void OnAdInspectorClosed() { }
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
  static const AdSize kLeaderboard;

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
  bool operator==(const AdSize& rhs) const;

  /// Comparison operator.
  ///
  /// @returns true if `rhs` refers to a different AdSize as `this`.
  bool operator!=(const AdSize& rhs) const;

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
  bool is_equal(const AdSize& ad_size) const;

  /// Denotes the orientation for anchored adaptive AdSize objects.
  Orientation orientation_;

  /// Advertisement width in platform-indepenent pixels.
  uint32_t width_;

  /// Advertisement width in platform-indepenent pixels.
  uint32_t height_;

  /// The type of AdSize (standard or adaptive)
  Type type_;
};

/// Contains targeting information used to fetch an ad.
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
  explicit AdRequest(const char* content_url);

  ~AdRequest();

  /// The content URL targeting information.
  ///
  /// @return the content URL for the @ref AdRequest. The string will be empty
  /// if no content URL has been configured.
  const std::string& content_url() const { return content_url_; }

  /// A Map of adapter class names to their collection of extra parameters, as
  /// configured via @ref add_extra.
  const std::map<std::string, std::map<std::string, std::string> >& extras()
      const {
    return extras_;
  }

  /// Keywords which will help GMA to provide targeted ads, as added by
  /// @ref add_keyword.
  const std::unordered_set<std::string>& keywords() const { return keywords_; }

  /// Returns the set of neighboring content URLs or an empty set if no URLs
  /// were set via @ref add_neighboring_content_urls().
  const std::unordered_set<std::string>& neighboring_content_urls() const {
    return neighboring_content_urls_;
  }

  /// Add a network extra for the associated ad_network.
  ///
  /// Appends an extra to the corresponding list of extras for the ad_network.
  /// Each ad network can have multiple extra strings.
  ///
  /// @param[in] adapter_class_name the ad network adapter for which to add the
  /// extra.
  /// @param[in] extra_key a key which will be passed to the corresponding ad
  /// network adapter.
  /// @param[in] extra_value the value associated with extra_key.
  void add_extra(const char* adapter_class_name, const char* extra_key,
                 const char* extra_value);

  /// Adds a keyword for targeting purposes.
  ///
  /// Multiple keywords may be added via repeated invocations of this method.
  ///
  /// @param[in] keyword a string that GMA will use to aid in targeting ads.
  void add_keyword(const char* keyword);

  /// When requesting an ad, apps may pass the URL of the content they are
  /// serving. This enables keyword targeting to match the ad with the content.
  ///
  /// The URL is ignored if null or the number of characters exceeds 512.
  ///
  /// @param[in] content_url the url of the content being viewed.
  void set_content_url(const char* content_url);

  /// Adds to the list of URLs which represent web content near an ad.
  ///
  /// Promotes brand safety and allows displayed ads to have an app level
  /// rating (MA, T, PG, etc) that is more appropriate to neighboring content.
  ///
  /// Subsequent invocations append to the existing list.
  ///
  /// @param[in] neighboring_content_urls neighboring content URLs to be
  /// attached to the existing neighboring content URLs.
  void add_neighboring_content_urls(
      const std::vector<std::string>& neighboring_content_urls);

 private:
  std::string content_url_;
  std::map<std::string, std::map<std::string, std::string> > extras_;
  std::unordered_set<std::string> keywords_;
  std::unordered_set<std::string> neighboring_content_urls_;
};

/// Describes a reward credited to a user for interacting with a RewardedAd.
class AdReward {
 public:
  /// Creates an @ref AdReward.
  AdReward(const std::string& type, int64_t amount)
      : type_(type), amount_(amount) {}

  /// Returns the reward amount.
  int64_t amount() const { return amount_; }

  /// Returns the type of the reward.
  const std::string& type() const { return type_; }

 private:
  const int64_t amount_;
  const std::string type_;
};

/// The monetary value earned from an ad.
class AdValue {
 public:
  /// Allowed constants for @ref precision_type().
  enum PrecisionType {
    /// An ad value with unknown precision.
    kdValuePrecisionUnknown = 0,
    /// An ad value estimated from aggregated data.
    kAdValuePrecisionEstimated,
    /// A publisher-provided ad value, such as manual CPMs in a mediation group.
    kAdValuePrecisionPublisherProvided = 2,
    /// The precise value paid for this ad.
    kAdValuePrecisionPrecise = 3
  };

  /// Constructor
  AdValue(const char* currency_code, PrecisionType precision_type,
          int64_t value_micros)
      : currency_code_(currency_code),
        precision_type_(precision_type),
        value_micros_(value_micros) {}

  /// The value's ISO 4217 currency code.
  const std::string& currency_code() const { return currency_code_; }

  /// The precision of the reported ad value.
  PrecisionType precision_type() const { return precision_type_; }

  /// The ad's value in micro-units, where 1,000,000 micro-units equal one
  /// unit of the currency.
  int64_t value_micros() const { return value_micros_; }

 private:
  const std::string currency_code_;
  const PrecisionType precision_type_;
  const int64_t value_micros_;
};

/// @brief Base of all GMA Banner Views.
class AdView {
 public:
  /// The possible screen positions for a @ref AdView, configured via
  /// @ref SetPosition.
  enum Position {
    /// The position isn't one of the predefined screen locations.
    kPositionUndefined = -1,
    /// Top of the screen, horizontally centered.
    kPositionTop = 0,
    /// Bottom of the screen, horizontally centered.
    kPositionBottom,
    /// Top-left corner of the screen.
    kPositionTopLeft,
    /// Top-right corner of the screen.
    kPositionTopRight,
    /// Bottom-left corner of the screen.
    kPositionBottomLeft,
    /// Bottom-right corner of the screen.
    kPositionBottomRight,
  };

  virtual ~AdView();

  /// Retrieves the @ref AdView's current onscreen size and location.
  ///
  /// @return The current size and location. Values are in pixels, and location
  ///         coordinates originate from the top-left corner of the screen.
  virtual BoundingBox bounding_box() const = 0;

  /// Sets an AdListener for this ad view.
  ///
  /// param[in] listener A listener object which will be invoked when lifecycle
  /// events occur on this AdView.
  virtual void SetAdListener(AdListener* listener) = 0;

  /// Sets a listener to be invoked when the Ad's bounding box
  /// changes size or location.
  ///
  /// param[in] listener A listener object which will be invoked when the ad
  /// changes size, shape, or position.
  virtual void SetBoundingBoxListener(AdViewBoundingBoxListener* listener) = 0;

  /// Sets a listener to be invoked when this ad is estimated to have earned
  /// money.
  ///
  /// param[in] A listener object to be invoked when a paid event occurs on the
  /// ad.
  virtual void SetPaidEventListener(PaidEventListener* listener) = 0;

  /// Moves the @ref AdView so that its top-left corner is located at
  /// (x, y). Coordinates are in pixels from the top-left corner of the screen.
  ///
  /// When built for Android, the library will not display an ad on top of or
  /// beneath an Activity's status bar. If a call to MoveTo would result in an
  /// overlap, the @ref AdView is placed just below the status bar, so no
  /// overlap occurs.
  /// @param[in] x The desired horizontal coordinate.
  /// @param[in] y The desired vertical coordinate.
  ///
  /// @return a @ref Future which will be completed when this move operation
  /// completes.
  virtual Future<void> SetPosition(int x, int y) = 0;

  /// Moves the @ref AdView so that it's located at the given predefined
  /// position.
  ///
  /// @param[in] position The predefined position to which to move the
  ///   @ref AdView.
  ///
  /// @return a @ref Future which will be completed when this move operation
  /// completes.
  virtual Future<void> SetPosition(Position position) = 0;

  /// Returns a @ref Future containing the status of the last call to either
  /// version of @ref SetPosition.
  virtual Future<void> SetPositionLastResult() const = 0;

  /// Hides the AdView.
  virtual Future<void> Hide() = 0;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Hide.
  virtual Future<void> HideLastResult() const = 0;

  /// Shows the @ref AdView.
  virtual Future<void> Show() = 0;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Show.
  virtual Future<void> ShowLastResult() const = 0;

  /// Pauses the @ref AdView. Should be called whenever the C++ engine
  /// pauses or the application loses focus.
  virtual Future<void> Pause() = 0;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Pause.
  virtual Future<void> PauseLastResult() const = 0;

  /// Resumes the @ref AdView after pausing.
  virtual Future<void> Resume() = 0;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Resume.
  virtual Future<void> ResumeLastResult() const = 0;

  /// Cleans up and deallocates any resources used by the @ref BannerView.
  virtual Future<void> Destroy() = 0;

  /// Returns a @ref Future containing the status of the last call to
  /// @ref Destroy.
  virtual Future<void> DestroyLastResult() const = 0;

 protected:
  /// Pointer to a listener for AdListener events.
  AdListener* ad_listener_;

  /// Pointer to a listener for BoundingBox events.
  AdViewBoundingBoxListener* ad_view_bounding_box_listener_;

  /// Pointer to a listener for Paid events.
  PaidEventListener* paid_event_listener_;
};

/// A listener class that developers can extend and pass to a @ref AdView
/// object's @ref AdView::SetBoundingBoxListener method to be notified of
/// changes to the size of the Ad's bounding box.
class AdViewBoundingBoxListener {
 public:
  virtual ~AdViewBoundingBoxListener();

  /// This method is called when the @ref AdView object's bounding box
  /// changes.
  ///
  /// @param[in] ad_view The view whose bounding box changed.
  /// @param[in] box The new bounding box.
  virtual void OnBoundingBoxChanged(AdView* ad_view, BoundingBox box) = 0;
};

/// @brief The screen location and dimensions of an ad view once it has been
/// initialized.
struct BoundingBox {
  /// Default constructor which initializes all member variables to 0.
  BoundingBox()
      : height(0), width(0), x(0), y(0), position(AdView::kPositionUndefined) {}

  /// Height of the ad in pixels.
  int height;
  /// Width of the ad in pixels.
  int width;
  /// Horizontal position of the ad in pixels from the left.
  int x;
  /// Vertical position of the ad in pixels from the top.
  int y;

  /// The position of the AdView if one has been set as the target position, or
  /// kPositionUndefined otherwise.
  AdView::Position position;
};

/// @brief Listener to be invoked when ads show and dismiss full screen content,
/// such as a fullscreen ad experience or an in-app browser.
class FullScreenContentListener {
 public:
  virtual ~FullScreenContentListener();

  /// Called when the user clicked the ad.
  virtual void OnAdClicked() {}

  /// Called when the ad dismissed full screen content.
  virtual void OnAdDismissedFullScreenContent() {}

  /// Called when the ad failed to show full screen content.
  ///
  /// param[in] ad_result An object containing detailed information
  /// about the error.
  virtual void OnAdFailedToShowFullScreenContent(const AdResult& ad_result) {}

  /// Called when an impression is recorded for an ad.
  virtual void OnAdImpression() {}

  /// Called when the ad showed the full screen content.
  virtual void OnAdShowedFullScreenContent() {}
};

/// Listener to be invoked when ads have been estimated to earn money.
class PaidEventListener {
 public:
  virtual ~PaidEventListener();

  /// Called when an ad is estimated to have earned money.
  virtual void OnPaidEvent(const AdValue& value) {}
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

  /// A class name that identifies the ad network that returned the ad.
  /// Returns an empty string if the ad failed to load.
  const std::string& mediation_adapter_class_name() const {
    return mediation_adapter_class_name_;
  }

  /// Gets the response ID string for the loaded ad.  Returns an empty
  /// string if the ad fails to load.
  const std::string& response_id() const { return response_id_; }

  /// Gets a log friendly string version of this object.
  const std::string& ToString() const { return to_string_; }

 private:
  friend class AdResult;

  explicit ResponseInfo(const ResponseInfoInternal& internal);

  std::vector<AdapterResponseInfo> adapter_responses_;
  std::string mediation_adapter_class_name_;
  std::string response_id_;
  std::string to_string_;
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

  /// Sets a maximum ad content rating. GMA ads returned for your app will
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

/// Listener to be invoked when the user earned a reward.
class UserEarnedRewardListener {
 public:
  virtual ~UserEarnedRewardListener();
  /// Called when the user earned a reward. The app is responsible for
  /// crediting the user with the reward.
  ///
  /// param[in] reward the @ref AdReward that should be granted to the user.
  virtual void OnUserEarnedReward(const AdReward& reward) {}
};

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_TYPES_H_
