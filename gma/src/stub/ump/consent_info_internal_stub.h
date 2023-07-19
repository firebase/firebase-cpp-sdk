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

class ConsentInfoInternalStub : public ConsentInfoInternal {
 public:
  ConsentInfoInternalStub();
  ~ConsentInfoInternalStub() override;

  ConsentStatus GetConsentStatus() const override { return consent_status_; }
  ConsentFormStatus GetConsentFormStatus() const override {
    return consent_form_status_;
  }

  Future<ConsentStatus> RequestConsentStatus(
      const ConsentRequestParameters& params) override;
  Future<ConsentFormStatus> LoadConsentForm() override;
  Future<ConsentStatus> ShowConsentForm(FormParent parent) override;

  virtual Future<ConsentStatus> LoadAndShowConsentFormIfRequired(
      FormParent parent) override;

  virtual PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus()
      override;
  virtual Future<ConsentStatus> ShowPrivacyOptionsForm(
      FormParent parent) override;

  virtual bool CanRequestAds() override;

  void Reset() override;

 private:
  ConsentStatus consent_status_;
  ConsentFormStatus consent_form_status_;
  PrivacyOptionsRequirementStatus privacy_options_requirement_status_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_STUB_UMP_CONSENT_INFO_INTERNAL_STUB_H_
