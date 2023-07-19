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

#ifndef FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_TYPES_H_
#define FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_TYPES_H_

#include <string>
#include <utility>
#include <vector>

#include "firebase/internal/platform.h"

namespace firebase {
namespace gma {
namespace ump {

/// Debug values for testing geography.
enum ConsentDebugGeography {
  /// Disable geography debugging.
  kConsentDebugGeographyDisabled = 0,
  /// Geography appears as in EEA for debug devices.
  kConsentDebugGeographyEEA,
  /// Geography appears as not in EEA for debug devices.
  kConsentDebugGeographyNonEEA
};

/// Debug settings for `ConsentInfo::RequestConsentStatus()`. These let you
/// force a speific geographic location. Be sure to include debug device IDs to
/// enable this on hardware. Debug features are always enabled for simulators.
struct ConsentDebugSettings {
  /// Create a default debug setting, with debugging disabled.
  ConsentDebugSettings() : debug_geography(kConsentDebugGeographyDisabled) {}

  /// The geographical location, for debugging.
  ConsentDebugGeography debug_geography;
  /// A list of all device IDs that are allowed to use debug settings. You can
  /// obtain this from the device log after running with debug settings enabled.
  std::vector<std::string> debug_device_ids;
};

/// Parameters for the `ConsentInfo::RequestConsentStatus()` operation. You must
/// explicitly set the age of consent tag (to true or false) or the operation
/// will fail.
class ConsentRequestParameters {
 public:
  ConsentRequestParameters()
      : has_debug_settings_(false),
        tag_for_under_age_of_consent_(false),
        has_tag_for_under_age_of_consent_(false) {}
  /// Set the age of consent tag. This indicates whether the user is tagged for
  /// under age of consent. This is a required setting.
  void set_tag_for_under_age_of_consent(bool tag) {
    tag_for_under_age_of_consent_ = tag;
    has_tag_for_under_age_of_consent_ = true;
  }
  /// Get the age of consent tag. This indicates whether the user is tagged for
  /// under age of consent.
  bool tag_for_under_age_of_consent() const {
    return tag_for_under_age_of_consent_;
  }
  /// Get whether the age of consent tag was previously set.
  bool has_tag_for_under_age_of_consent() const {
    return has_tag_for_under_age_of_consent_;
  }

  /// Set the debug settings.
  void set_debug_settings(const ConsentDebugSettings& settings) {
    debug_settings_ = settings;
    has_debug_settings_ = true;
  }
  /// Set the debug settings without copying data, which could be useful if you
  /// have a large list of debug device IDs.
  void set_debug_settings(const ConsentDebugSettings&& settings) {
    debug_settings_ = std::move(settings);
    has_debug_settings_ = true;
  }
  /// Get the debug settings.
  const ConsentDebugSettings& debug_settings() const { return debug_settings_; }
  /// Get whether debug settings were set.
  bool has_debug_settings() const { return has_debug_settings_; }

 private:
  ConsentDebugSettings debug_settings_;
  bool tag_for_under_age_of_consent_;
  bool has_debug_settings_;
  bool has_tag_for_under_age_of_consent_;
};

/// This is a platform specific datatype that is required to show a consent form
/// on screen.
///
/// The following defines the datatype on each platform:
/// <ul>
///   <li>Android: A `jobject` which references an Android Activity.</li>
///   <li>iOS: An `id` which references an iOS UIViewController.</li>
/// </ul>
#if FIREBASE_PLATFORM_ANDROID
/// An Android Activity from Java.
typedef jobject FormParent;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
/// A pointer to an iOS UIViewController.
typedef id FormParent;
#else
/// A void pointer for stub classes.
typedef void* FormParent;
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

/// Consent status values.
enum ConsentStatus {
  /// Unknown status, e.g. prior to calling Request, or if the request fails.
  kConsentStatusUnknown = 0,
  /// Consent is required, but not obtained
  kConsentStatusRequired,
  /// Consent is not required
  kConsentStatusNotRequired,
  /// Consent was required, and has been obtained
  kConsentStatusObtained
};

/// Errors that can occur during a RequestConsentStatus operation.
enum ConsentRequestError {
  /// The operation succeeded.
  kConsentRequestSuccess = 0,
  /// Invalid GMA App ID specified in AndroidManifest.xml or Info.plist.
  kConsentRequestErrorInvalidAppId,
  /// A network error occurred.
  kConsentRequestErrorNetwork,
  /// The tag for age of consent was not set. You must call
  /// `ConsentRequestParameters::set_tag_for_under_age_of_consent()` before the
  /// request.
  kConsentRequestErrorTagForAgeOfConsentNotSet,
  /// An internal error occurred.
  kConsentRequestErrorInternal,
  /// This error is undocumented.
  kConsentRequestErrorCodeMisconfiguration,
  /// An unknown error occurred.
  kConsentRequestErrorUnknown,
  /// The operation is already in progress. Use
  /// `ConsentInfo::RequestConsentStatusLastResult()`
  /// to get the status.
  kConsentRequestErrorOperationInProgress
};

/// Status of the consent form, whether it is available to show or not.
enum ConsentFormStatus {
  /// Status is unknown. Call `ConsentInfo::RequestConsentStatus()` to update
  /// this.
  kConsentFormStatusUnknown = 0,
  /// The consent form is unavailable. Call `ConsentInfo::LoadConsentForm()` to
  /// load it.
  kConsentFormStatusUnavailable,
  /// The consent form is available. Call `ConsentInfo::ShowConsentForm()` to
  /// display it.
  kConsentFormStatusAvailable,
};

/// Errors when loading or showing the consent form.
enum ConsentFormError {
  /// The operation succeeded.
  kConsentFormSuccess = 0,
  /// The load request timed out. Try again.
  kConsentFormErrorTimeout,
  /// Failed to show the consent form because it has not been loaded.
  kConsentFormErrorNotLoaded,
  /// An internal error occurred.
  kConsentFormErrorInternal,
  /// An unknown error occurred.
  kConsentFormErrorUnknown,
  /// This form was already used.
  kConsentFormErrorCodeAlreadyUsed,
  /// An invalid operation occurred. Try again.
  kConsentFormErrorInvalidOperation,
  /// General network issues occurred. Try again.
  kConsentFormErrorNetwork,
  /// The operation is already in progress. Call
  /// `ConsentInfo::LoadConsentFormLastResult()` or
  /// `ConsentInfo::ShowConsentFormLastResult()` to get the status.
  kConsentFormErrorOperationInProgress
};

}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_TYPES_H_
