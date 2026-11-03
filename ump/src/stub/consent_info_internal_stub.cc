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

#include "ump/src/stub/consent_info_internal_stub.h"

namespace firebase {
namespace ump {
namespace internal {

ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalStub();
}

ConsentInfoInternalStub::ConsentInfoInternalStub()
    : consent_status_(kConsentStatusUnknown),
      consent_form_status_(kConsentFormStatusUnknown),
      privacy_options_requirement_status_(
          kPrivacyOptionsRequirementStatusUnknown),
      debug_geo_(kConsentDebugGeographyDisabled),
      under_age_of_consent_(false),
      has_requested_consent_info_update_(false),
      is_form_loaded_(false) {}

ConsentInfoInternalStub::~ConsentInfoInternalStub() = default;

ConsentStatus ConsentInfoInternalStub::GetConsentStatus() {
  MutexLock lock(state_mutex_);
  return consent_status_;
}

ConsentFormStatus ConsentInfoInternalStub::GetConsentFormStatus() {
  MutexLock lock(state_mutex_);
  if (!has_requested_consent_info_update_) {
    return kConsentFormStatusUnknown;
  }
  return consent_form_status_;
}

Future<void> ConsentInfoInternalStub::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnRequestConsentInfoUpdate,
                                  kConsentRequestErrorOperationInProgress,
                                  &handle, &in_progress)) {
    return in_progress;
  }

  {
    MutexLock lock(state_mutex_);
    ConsentStatus new_consent_status = kConsentStatusObtained;
    if (params.debug_settings.debug_geography == kConsentDebugGeographyEEA ||
        params.debug_settings.debug_geography ==
            kConsentDebugGeographyRegulatedUSState) {
      new_consent_status = kConsentStatusRequired;
    } else if (params.debug_settings.debug_geography ==
                   kConsentDebugGeographyNonEEA ||
               params.debug_settings.debug_geography ==
                   kConsentDebugGeographyOther) {
      new_consent_status = kConsentStatusNotRequired;
    }

    consent_status_ = new_consent_status;
    under_age_of_consent_ = params.tag_for_under_age_of_consent;
    consent_form_status_ =
        (under_age_of_consent_ || consent_status_ != kConsentStatusRequired)
            ? kConsentFormStatusUnavailable
            : kConsentFormStatusAvailable;
    debug_geo_ = params.debug_settings.debug_geography;
    privacy_options_requirement_status_ =
        kPrivacyOptionsRequirementStatusNotRequired;
    has_requested_consent_info_update_ = true;
    is_form_loaded_ = false;
  }

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalStub::LoadConsentForm() {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnLoadConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  ConsentFormError result = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (consent_form_status_ != kConsentFormStatusAvailable) {
      is_form_loaded_ = false;
      result = kConsentFormErrorUnavailable;
    } else {
      is_form_loaded_ = true;
    }
  }

  CompleteFuture(handle, result);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalStub::ShowConsentForm(FormParent parent) {
  (void)parent;
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  ConsentFormError result = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (!is_form_loaded_) {
      result = kConsentFormErrorUnavailable;
    } else {
      is_form_loaded_ = false;
      consent_status_ = kConsentStatusObtained;
      if (debug_geo_ == kConsentDebugGeographyEEA ||
          debug_geo_ == kConsentDebugGeographyRegulatedUSState) {
        privacy_options_requirement_status_ =
            kPrivacyOptionsRequirementStatusRequired;
      } else {
        privacy_options_requirement_status_ =
            kPrivacyOptionsRequirementStatusNotRequired;
      }
    }
  }

  CompleteFuture(handle, result);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalStub::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  (void)parent;
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(
          kConsentInfoFnLoadAndShowConsentFormIfRequired,
          kConsentFormErrorOperationInProgress, &handle, &in_progress)) {
    return in_progress;
  }

  ConsentFormError result = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (consent_status_ == kConsentStatusRequired &&
        consent_form_status_ != kConsentFormStatusAvailable) {
      result = kConsentFormErrorUnavailable;
    } else if (consent_status_ == kConsentStatusRequired) {
      consent_status_ = kConsentStatusObtained;
      if (debug_geo_ == kConsentDebugGeographyEEA ||
          debug_geo_ == kConsentDebugGeographyRegulatedUSState) {
        privacy_options_requirement_status_ =
            kPrivacyOptionsRequirementStatusRequired;
      } else {
        privacy_options_requirement_status_ =
            kPrivacyOptionsRequirementStatusNotRequired;
      }
    }
  }

  CompleteFuture(handle, result);
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalStub::GetPrivacyOptionsRequirementStatus() {
  MutexLock lock(state_mutex_);
  return privacy_options_requirement_status_;
}

Future<void> ConsentInfoInternalStub::ShowPrivacyOptionsForm(
    FormParent parent) {
  (void)parent;
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowPrivacyOptionsForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }

  ConsentFormError result = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (privacy_options_requirement_status_ !=
        kPrivacyOptionsRequirementStatusRequired) {
      result = kConsentFormErrorUnavailable;
    } else if (consent_status_ == kConsentStatusObtained) {
      consent_status_ = kConsentStatusRequired;
      privacy_options_requirement_status_ =
          kPrivacyOptionsRequirementStatusNotRequired;
    }
  }

  CompleteFuture(handle, result);
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalStub::CanRequestAds() {
  MutexLock lock(state_mutex_);
  if (!has_requested_consent_info_update_) {
    return false;
  }
  bool consent_status_ok = (consent_status_ == kConsentStatusObtained ||
                            consent_status_ == kConsentStatusNotRequired);
  bool privacy_options_ok = (privacy_options_requirement_status_ !=
                             kPrivacyOptionsRequirementStatusUnknown);
  return consent_status_ok && privacy_options_ok;
}

void ConsentInfoInternalStub::Reset() {
  MutexLock lock(state_mutex_);
  consent_status_ = kConsentStatusUnknown;
  consent_form_status_ = kConsentFormStatusUnknown;
  privacy_options_requirement_status_ =
      kPrivacyOptionsRequirementStatusUnknown;
  debug_geo_ = kConsentDebugGeographyDisabled;
  under_age_of_consent_ = false;
  has_requested_consent_info_update_ = false;
  is_form_loaded_ = false;
}

}  // namespace internal
}  // namespace ump
}  // namespace firebase
