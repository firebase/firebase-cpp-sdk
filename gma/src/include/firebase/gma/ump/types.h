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

enum ConsentDebugGeography {
  kConsentDebugGeographyDisabled = 0,
  kConsentDebugGeographyEEA,
  kConsentDebugGeographyNonEEA
};

struct ConsentDebugSettings {
  ConsentDebugSettings() : debug_geography(kConsentDebugGeographyDisabled) {}

  ConsentDebugGeography debug_geography;
  std::vector<std::string> debug_device_ids;
};

// Parameters for the consent info request operation.
class ConsentRequestParameters {
 public:
  ConsentRequestParameters()
      : has_debug_settings_(false),
        tag_for_under_age_of_consent_(false),
        has_tag_for_under_age_of_consent_(false) {}
  // Set the age of consent tag.
  void set_tag_for_under_age_of_consent(bool tag) {
    tag_for_under_age_of_consent_ = tag;
    has_tag_for_under_age_of_consent_ = true;
  }
  // Get the age of consent tag.
  bool tag_for_under_age_of_consent() const {
    return tag_for_under_age_of_consent_;
  }
  // Get whether the age of consent tag was previously set.
  bool has_tag_for_under_age_of_consent() const {
    return has_tag_for_under_age_of_consent_;
  }

  // Set the debug settings.
  void set_debug_settings(const ConsentDebugSettings& settings) {
    debug_settings_ = settings;
    has_debug_settings_ = true;
  }
  // Set the debug settings without copying data, useful if you have a large
  // list of debug device IDs.
  void set_debug_settings(const ConsentDebugSettings&& settings) {
    debug_settings_ = std::move(settings);
    has_debug_settings_ = true;
  }
  // Get the debug settings.
  const ConsentDebugSettings& debug_settings() const { return debug_settings_; }
  // Get whether debug settings were set.
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

enum ConsentStatus {
  // Unknown status, e.g. prior to calling Request, or if the request fails.
  kConsentStatusUnknown = 0,
  // Consent is required, but not obtained
  kConsentStatusRequired,
  // Consent is not required
  kConsentStatusNotRequired,
  // Consent was required, and has been obtained
  kConsentStatusObtained
};

enum ConsentRequestError {
  kConsentRequestSuccess = 0,
  kConsentRequestErrorInvalidAppId,
  kConsentRequestErrorNetwork,
  kConsentRequestErrorTagForAgeOfConsentNotSet,
  kConsentRequestErrorInternal,
  kConsentRequestErrorCodeMisconfiguration,
  kConsentRequestErrorUnknown,
  kConsentRequestErrorOperationInProgress
};

enum ConsentFormStatus {
  kConsentFormStatusUnknown = 0,
  kConsentFormStatusUnavailable,
  kConsentFormStatusAvailable,
};

enum ConsentFormError {
  kConsentFormSuccess = 0,
  kConsentFormErrorTimeout,
  kConsentFormErrorNotLoaded,
  kConsentFormErrorInternal,
  kConsentFormErrorUnknown,
  kConsentFormErrorCodeAlreadyUsed,
  kConsentFormErrorInvalidOperation,
  kConsentFormErrorNetwork,
  kConsentFormErrorOperationInProgress
};

}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_INCLUDE_FIREBASE_GMA_UMP_TYPES_H_
