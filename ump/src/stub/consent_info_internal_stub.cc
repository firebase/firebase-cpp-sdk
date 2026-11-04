// Copyright 2023 Google LLC

#include "ump/src/stub/consent_info_internal_stub.h"

namespace firebase {
namespace ump {
namespace internal {

ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalStub();
}

ConsentInfoInternalStub::ConsentInfoInternalStub() = default;
ConsentInfoInternalStub::~ConsentInfoInternalStub() = default;

ConsentStatus ConsentInfoInternalStub::GetConsentStatus() {
  MutexLock lock(state_mutex_);
  return consent_status_;
}

ConsentFormStatus ConsentInfoInternalStub::GetConsentFormStatus() {
  MutexLock lock(state_mutex_);
  return has_requested_consent_info_update_ ? consent_form_status_
                                            : kConsentFormStatusUnknown;
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
    auto geo = params.debug_settings.debug_geography;
    consent_status_ = (geo == kConsentDebugGeographyEEA ||
                       geo == kConsentDebugGeographyRegulatedUSState)
                          ? kConsentStatusRequired
                      : (geo == kConsentDebugGeographyNonEEA ||
                         geo == kConsentDebugGeographyOther)
                          ? kConsentStatusNotRequired
                          : kConsentStatusObtained;
    under_age_of_consent_ = params.tag_for_under_age_of_consent;
    consent_form_status_ =
        (under_age_of_consent_ || consent_status_ != kConsentStatusRequired)
            ? kConsentFormStatusUnavailable
            : kConsentFormStatusAvailable;
    debug_geo_ = geo;
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
  ConsentFormError res = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    is_form_loaded_ = (consent_form_status_ == kConsentFormStatusAvailable);
    if (!is_form_loaded_) res = kConsentFormErrorUnavailable;
  }
  CompleteFuture(handle, res);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalStub::ShowConsentForm(FormParent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowConsentForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }
  ConsentFormError res = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (!is_form_loaded_) {
      res = kConsentFormErrorUnavailable;
    } else {
      is_form_loaded_ = false;
      consent_status_ = kConsentStatusObtained;
      privacy_options_requirement_status_ =
          (debug_geo_ == kConsentDebugGeographyEEA ||
           debug_geo_ == kConsentDebugGeographyRegulatedUSState)
              ? kPrivacyOptionsRequirementStatusRequired
              : kPrivacyOptionsRequirementStatusNotRequired;
    }
  }
  CompleteFuture(handle, res);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalStub::LoadAndShowConsentFormIfRequired(
    FormParent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(
          kConsentInfoFnLoadAndShowConsentFormIfRequired,
          kConsentFormErrorOperationInProgress, &handle, &in_progress)) {
    return in_progress;
  }
  ConsentFormError res = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (consent_status_ == kConsentStatusRequired) {
      if (consent_form_status_ != kConsentFormStatusAvailable) {
        res = kConsentFormErrorUnavailable;
      } else {
        consent_status_ = kConsentStatusObtained;
        privacy_options_requirement_status_ =
            (debug_geo_ == kConsentDebugGeographyEEA ||
             debug_geo_ == kConsentDebugGeographyRegulatedUSState)
                ? kPrivacyOptionsRequirementStatusRequired
                : kPrivacyOptionsRequirementStatusNotRequired;
      }
    }
  }
  CompleteFuture(handle, res);
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalStub::GetPrivacyOptionsRequirementStatus() {
  MutexLock lock(state_mutex_);
  return privacy_options_requirement_status_;
}

Future<void> ConsentInfoInternalStub::ShowPrivacyOptionsForm(FormParent) {
  SafeFutureHandle<void> handle;
  Future<void> in_progress;
  if (!BeginSingleFlightOperation(kConsentInfoFnShowPrivacyOptionsForm,
                                  kConsentFormErrorOperationInProgress, &handle,
                                  &in_progress)) {
    return in_progress;
  }
  ConsentFormError res = kConsentFormSuccess;
  {
    MutexLock lock(state_mutex_);
    if (privacy_options_requirement_status_ !=
        kPrivacyOptionsRequirementStatusRequired) {
      res = kConsentFormErrorUnavailable;
    } else if (consent_status_ == kConsentStatusObtained) {
      consent_status_ = kConsentStatusRequired;
      privacy_options_requirement_status_ =
          kPrivacyOptionsRequirementStatusNotRequired;
    }
  }
  CompleteFuture(handle, res);
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalStub::CanRequestAds() {
  MutexLock lock(state_mutex_);
  return has_requested_consent_info_update_ &&
         (consent_status_ == kConsentStatusObtained ||
          consent_status_ == kConsentStatusNotRequired) &&
         privacy_options_requirement_status_ !=
             kPrivacyOptionsRequirementStatusUnknown;
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
