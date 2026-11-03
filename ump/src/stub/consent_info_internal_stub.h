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

#ifndef FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_
#define FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_

#include "firebase/internal/mutex.h"
#include "ump/src/common/consent_info_internal.h"

namespace firebase {
namespace ump {
namespace internal {

// The stub interface implements deterministic UMP workflows for testing:
//
// Before requesting: consent, form, and privacy options statuses are Unknown.
//
// After RequestConsentInfoUpdate():
// - If debug_geography == EEA or RegulatedUSState: consent is Required, form is
//   Available (unless tag_for_under_age_of_consent is true), and privacy
//   options are NotRequired. After ShowConsentForm() or
//   LoadAndShowConsentFormIfRequired(), consent changes to Obtained and privacy
//   options become Required.
// - If debug_geography == NonEEA or Other: consent is NotRequired, form is
//   Unavailable, and privacy options are NotRequired.
// - If debug_geography == Disabled: consent is Obtained, form is Unavailable,
//   and privacy options are NotRequired.
class ConsentInfoInternalStub : public ConsentInfoInternal {
 public:
  ConsentInfoInternalStub();
  ~ConsentInfoInternalStub() override;

  ConsentStatus GetConsentStatus() override;
  ConsentFormStatus GetConsentFormStatus() override;

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
  Mutex state_mutex_;
  ConsentStatus consent_status_;
  ConsentFormStatus consent_form_status_;
  PrivacyOptionsRequirementStatus privacy_options_requirement_status_;
  ConsentDebugGeography debug_geo_;
  bool under_age_of_consent_;
  bool has_requested_consent_info_update_;
  bool is_form_loaded_;
};

}  // namespace internal
}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_
