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

#ifndef FIREBASE_GMA_SRC_STUB_UMP_CONSENT_INFO_INTERNAL_STUB_H_
#define FIREBASE_GMA_SRC_STUB_UMP_CONSENT_INFO_INTERNAL_STUB_H_

#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// The stub interface implements a few specific workflows, for testing:
//
// Before requesting: consent and privacy options requirement will be Unknown.
//
// After requesting:
//
// If debug_geography == EEA, consent will be Required, privacy options
// NotRequired. After calling ShowConsentForm() or
// LoadAndShowConsentFormIfRequired(), it will change to change to Obtained and
// privacy options will become Required, and when the privacy options form is
// shown, consent will go back to Required.
//
// If debug_geography == NonEEA, consent will be NotRequired. No privacy options
// form is required.
//
// If debug_geography == Disabled, consent will be Obtained and privacy options
// will be NotRequired.
//
// If tag_for_under_age_of_consent = true, LoadConsentForm and
// LoadAndShowConsentFormIfRequired will fail with kConsentFormErrorUnavailable.
//
// CanRequestAds returns true if consent is NotRequired or Obtained.
class ConsentInfoInternalStub : public ConsentInfoInternal {
 public:
  ConsentInfoInternalStub();
  ~ConsentInfoInternalStub() override;

  ConsentStatus GetConsentStatus() override { return consent_status_; }
  ConsentFormStatus GetConsentFormStatus() override {
    return consent_form_status_;
  }

  Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params) override;
  Future<void> LoadConsentForm() override;
  Future<void> ShowConsentForm(FormParent parent) override;

  Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) override;

  PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus() override;
  Future<void> ShowPrivacyOptionsForm(FormParent parent) override;

  bool CanRequestAds() override;

  void Reset() override;

 private:
  ConsentStatus consent_status_;
  ConsentFormStatus consent_form_status_;
  PrivacyOptionsRequirementStatus privacy_options_requirement_status_;
  ConsentDebugGeography debug_geo_;
  bool under_age_of_consent_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_STUB_UMP_CONSENT_INFO_INTERNAL_STUB_H_
