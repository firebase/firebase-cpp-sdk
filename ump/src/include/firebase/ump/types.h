// Copyright 2023 Google LLC

#ifndef FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_TYPES_H_
#define FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_TYPES_H_

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
#endif

namespace firebase {
namespace ump {

enum ConsentDebugGeography {
  kConsentDebugGeographyDisabled = 0,
  kConsentDebugGeographyEEA = 1,
  kConsentDebugGeographyNonEEA = 2,
  kConsentDebugGeographyRegulatedUSState = 3,
  kConsentDebugGeographyOther = 4
};

struct ConsentDebugSettings {
  ConsentDebugSettings() : debug_geography(kConsentDebugGeographyDisabled) {}

  ConsentDebugGeography debug_geography;
  std::vector<std::string> debug_device_ids;

#if !defined(SWIG)
  bool operator==(const ConsentDebugSettings& other) const noexcept {
    return debug_geography == other.debug_geography &&
           debug_device_ids == other.debug_device_ids;
  }
  bool operator!=(const ConsentDebugSettings& other) const noexcept {
    return !(*this == other);
  }
#endif  // !defined(SWIG)
};

struct ConsentRequestParameters {
  ConsentRequestParameters() : tag_for_under_age_of_consent(false) {}

  ConsentDebugSettings debug_settings;
  bool tag_for_under_age_of_consent;

#if !defined(SWIG)
  bool operator==(const ConsentRequestParameters& other) const noexcept {
    return debug_settings == other.debug_settings &&
           tag_for_under_age_of_consent == other.tag_for_under_age_of_consent;
  }
  bool operator!=(const ConsentRequestParameters& other) const noexcept {
    return !(*this == other);
  }
#endif  // !defined(SWIG)
};

#if FIREBASE_PLATFORM_ANDROID
typedef jobject FormParent;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
typedef id FormParent;
#else
typedef void* FormParent;
#endif

enum ConsentStatus {
  kConsentStatusUnknown = 0,
  kConsentStatusRequired,
  kConsentStatusNotRequired,
  kConsentStatusObtained
};

enum ConsentRequestError {
  kConsentRequestSuccess = 0,
  kConsentRequestErrorInvalidAppId,
  kConsentRequestErrorNetwork,
  kConsentRequestErrorInternal,
  kConsentRequestErrorMisconfiguration,
  kConsentRequestErrorUnknown,
  kConsentRequestErrorInvalidOperation,
  kConsentRequestErrorOperationInProgress,
  kConsentRequestErrorTimeout
};

enum ConsentFormStatus {
  kConsentFormStatusUnknown = 0,
  kConsentFormStatusUnavailable,
  kConsentFormStatusAvailable,
};

enum ConsentFormError {
  kConsentFormSuccess = 0,
  kConsentFormErrorTimeout,
  kConsentFormErrorInternal,
  kConsentFormErrorUnknown,
  kConsentFormErrorUnavailable,
  kConsentFormErrorAlreadyUsed,
  kConsentFormErrorInvalidOperation,
  kConsentFormErrorOperationInProgress,
  kConsentFormErrorNetwork
};

enum PrivacyOptionsRequirementStatus {
  kPrivacyOptionsRequirementStatusUnknown = 0,
  kPrivacyOptionsRequirementStatusNotRequired,
  kPrivacyOptionsRequirementStatusRequired
};

}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_INCLUDE_FIREBASE_UMP_TYPES_H_
