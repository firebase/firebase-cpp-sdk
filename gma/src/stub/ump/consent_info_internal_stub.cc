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

#include "gma/src/stub/ump/consent_info_internal_stub.h"

#include "app/src/thread.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalStub();
}

ConsentInfoInternalStub::ConsentInfoInternalStub()
    : consent_status_(kConsentStatusUnknown),
      consent_form_status_(kConsentFormStatusUnknown),
      privacy_options_requirement_status_(
          kPrivacyOptionsRequirementStatusUnknown) {}

ConsentInfoInternalStub::~ConsentInfoInternalStub() {}

Future<ConsentStatus> ConsentInfoInternalStub::RequestConsentStatus(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<ConsentStatus> handle =
      CreateFuture<ConsentStatus>(kConsentInfoFnRequestConsentStatus);

  if (!params.has_tag_for_under_age_of_consent()) {
    CompleteFuture<ConsentStatus>(
        handle, kConsentRequestErrorTagForAgeOfConsentNotSet, consent_status_);
    return MakeFuture<ConsentStatus>(futures(), handle);
  }

  ConsentStatus new_consent_status = kConsentStatusObtained;
  // Simulate consent status based on debug geo.
  if (params.has_debug_settings()) {
    if (params.debug_settings().debug_geography == kConsentDebugGeographyEEA) {
      new_consent_status = kConsentStatusRequired;
    } else if (params.debug_settings().debug_geography ==
               kConsentDebugGeographyNonEEA) {
      new_consent_status = kConsentStatusNotRequired;
    }
  }
  consent_status_ = new_consent_status;
  consent_form_status_ = kConsentFormStatusUnavailable;

  // Simulate privacy options based on under age of consent.
  if (params.tag_for_under_age_of_consent()) {
    privacy_options_requirement_status_ =
        kPrivacyOptionsRequirementStatusRequired;
  } else {
    privacy_options_requirement_status_ =
        kPrivacyOptionsRequirementStatusNotRequired;
  }

  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess,
                                consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

Future<ConsentFormStatus> ConsentInfoInternalStub::LoadConsentForm() {
  SafeFutureHandle<ConsentFormStatus> handle =
      CreateFuture<ConsentFormStatus>(kConsentInfoFnLoadConsentForm);

  consent_form_status_ = kConsentFormStatusAvailable;
  CompleteFuture<ConsentFormStatus>(handle, kConsentFormSuccess,
                                    consent_form_status_);
  return MakeFuture<ConsentFormStatus>(futures(), handle);
}

Future<ConsentStatus> ConsentInfoInternalStub::ShowConsentForm(
    FormParent parent) {
  SafeFutureHandle<ConsentStatus> handle =
      CreateFuture<ConsentStatus>(kConsentInfoFnShowConsentForm);

  consent_status_ = kConsentStatusObtained;
  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess,
                                consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

Future<ConsentStatus> ConsentInfoInternalStub::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  SafeFutureHandle<ConsentStatus> handle = CreateFuture<ConsentStatus>(
      kConsentInfoFnLoadAndShowConsentFormIfRequired);

  if (consent_status_ == kConsentStatusRequired) {
    consent_status_ = kConsentStatusObtained;
  }
  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess,
                                consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalStub::GetPrivacyOptionsRequirementStatus() {
  return privacy_options_requirement_status_;
}

Future<ConsentStatus> ConsentInfoInternalStub::ShowPrivacyOptionsForm(
    FormParent parent) {
  SafeFutureHandle<ConsentStatus> handle =
      CreateFuture<ConsentStatus>(kConsentInfoFnShowPrivacyOptionsForm);

  if (consent_status_ == kConsentStatusObtained) {
    consent_status_ = kConsentStatusRequired;
  }
  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess,
                                consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

bool ConsentInfoInternalStub::CanRequestAds() {
  bool consent_status_ok = (consent_status_ == kConsentStatusObtained ||
                            consent_status_ == kConsentStatusNotRequired);
  bool privacy_options_ok = (privacy_options_requirement_status_ !=
                             kPrivacyOptionsRequirementStatusUnknown);
  return consent_status_ok && privacy_options_ok;
}

void ConsentInfoInternalStub::Reset() {
  consent_status_ = kConsentStatusUnknown;
  consent_form_status_ = kConsentFormStatusUnknown;
}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
