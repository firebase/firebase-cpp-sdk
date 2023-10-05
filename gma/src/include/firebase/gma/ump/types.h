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
namespace ump {

/// Debug values for testing geography.
enum ConsentDebugGeography {
  /// Disable geography debugging.
  kConsentDebugGeographyDisabled = 0,
  /// Geography appears as in EEA (European Economic Area) for debug devices.
  kConsentDebugGeographyEEA,
  /// Geography appears as not in EEA for debug devices.
  kConsentDebugGeographyNonEEA
};

/// Debug settings for `ConsentInfo::RequestConsentInfoUpdate()`. These let you
/// force a specific geographic location. Be sure to include debug device IDs to
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

/// Parameters for the `ConsentInfo::RequestConsentInfoUpdate()` operation.
struct ConsentRequestParameters {
  ConsentRequestParameters() : tag_for_under_age_of_consent(false) {}

  /// Debug settings for the consent request.
  ConsentDebugSettings debug_settings;

  /// Whether the user is under the age of consent.
  bool tag_for_under_age_of_consent;
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

/// Errors that can occur during a RequestConsentInfoUpdate operation.
enum ConsentRequestError {
  /// The operation succeeded.
  kConsentRequestSuccess = 0,
  /// Invalid GMA App ID specified in AndroidManifest.xml or Info.plist.
  kConsentRequestErrorInvalidAppId,
  /// A network error occurred.
  kConsentRequestErrorNetwork,
  /// An internal error occurred.
  kConsentRequestErrorInternal,
  /// A misconfiguration exists in the UI.
  kConsentRequestErrorMisconfiguration,
  /// An unknown error occurred.
  kConsentRequestErrorUnknown,
  /// An invalid operation occurred. Try again.
  kConsentRequestErrorInvalidOperation,
  /// The operation is already in progress. Use
  /// `ConsentInfo::RequestConsentInfoUpdateLastResult()`
  /// to get the status.
  kConsentRequestErrorOperationInProgress
};

/// Status of the consent form, whether it is available to show or not.
enum ConsentFormStatus {
  /// Status is unknown. Call `ConsentInfo::RequestConsentInfoUpdate()` to
  /// update this.
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
  /// An internal error occurred.
  kConsentFormErrorInternal,
  /// An unknown error occurred.
  kConsentFormErrorUnknown,
  /// The form is unavailable.
  kConsentFormErrorUnavailable,
  /// This form was already used.
  kConsentFormErrorAlreadyUsed,
  /// An invalid operation occurred. Try again.
  kConsentFormErrorInvalidOperation,
  /// The operation is already in progress. Call
  /// `ConsentInfo::LoadConsentFormLastResult()` or
  /// `ConsentInfo::ShowConsentFormLastResult()` to get the status.
  kConsentFormErrorOperationInProgress
};

/// Whether the privacy options need to be displayed.
enum PrivacyOptionsRequirementStatus {
  /// Privacy options requirement status is unknown. Call
  /// `ConsentInfo::RequestConsentInfoUpdate()` to update.
  kPrivacyOptionsRequirementStatusUnknown = 0,
  /// Privacy options are not required to be shown.
  kPrivacyOptionsRequirementStatusNotRequired,
  /// Privacy options must be shown. Call
  /// `ConsentInfo::ShowPrivacyOptionsForm()` to fulfil this requirement.
  kPrivacyOptionsRequirementStatusRequired
};

}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_TYPES_H_
