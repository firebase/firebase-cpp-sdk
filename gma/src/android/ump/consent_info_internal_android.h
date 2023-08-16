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

#ifndef FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_

#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

class ConsentInfoInternalAndroid : public ConsentInfoInternal {
 public:
  ConsentInfoInternalAndroid();
  ~ConsentInfoInternalAndroid() override;

  ConsentStatus GetConsentStatus() const override;
  ConsentFormStatus GetConsentFormStatus() const override;

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
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_UMP_CONSENT_INFO_INTERNAL_ANDROID_H_
