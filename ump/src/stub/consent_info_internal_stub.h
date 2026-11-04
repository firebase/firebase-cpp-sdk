// Copyright 2023 Google LLC

#ifndef FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_
#define FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_

#include "firebase/internal/mutex.h"
#include "ump/src/common/consent_info_internal.h"

namespace firebase {
namespace ump {
namespace internal {

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
  ConsentStatus consent_status_ = kConsentStatusUnknown;
  ConsentFormStatus consent_form_status_ = kConsentFormStatusUnknown;
  PrivacyOptionsRequirementStatus privacy_options_requirement_status_ =
      kPrivacyOptionsRequirementStatusUnknown;
  ConsentDebugGeography debug_geo_ = kConsentDebugGeographyDisabled;
  bool under_age_of_consent_ = false;
  bool has_requested_consent_info_update_ = false;
  bool is_form_loaded_ = false;
};

}  // namespace internal
}  // namespace ump
}  // namespace firebase

#endif  // FIREBASE_UMP_SRC_STUB_CONSENT_INFO_INTERNAL_STUB_H_
