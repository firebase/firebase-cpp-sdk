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

#ifndef FIREBASE_GMA_SRC_IOS_UMP_CONSENT_INFO_INTERNAL_IOS_H_
#define FIREBASE_GMA_SRC_IOS_UMP_CONSENT_INFO_INTERNAL_IOS_H_

#include <UserMessagingPlatform/UserMessagingPlatform.h>

#include "firebase/internal/mutex.h"
#include "gma/src/common/ump/consent_info_internal.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

class ConsentInfoInternalIos : public ConsentInfoInternal {
 public:
  ConsentInfoInternalIos();
  ~ConsentInfoInternalIos() override;

  ConsentStatus GetConsentStatus() override;
  Future<void> RequestConsentInfoUpdate(
      const ConsentRequestParameters& params) override;

  ConsentFormStatus GetConsentFormStatus() override;
  Future<void> LoadConsentForm() override;
  Future<void> ShowConsentForm(FormParent parent) override;

  Future<void> LoadAndShowConsentFormIfRequired(FormParent parent) override;

  PrivacyOptionsRequirementStatus GetPrivacyOptionsRequirementStatus() override;
  Future<void> ShowPrivacyOptionsForm(FormParent parent) override;

  bool CanRequestAds() override;

  void Reset() override;

 private:
  static ConsentInfoInternalIos* s_instance;
  static firebase::Mutex s_instance_mutex;
  static unsigned int s_instance_tag;

  void SetLoadedForm(UMPConsentForm *form) {
    loaded_form_ = form;
  }

  UMPConsentForm *loaded_form_;
};

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_UMP_CONSENT_INFO_INTERNAL_IOS_H_
