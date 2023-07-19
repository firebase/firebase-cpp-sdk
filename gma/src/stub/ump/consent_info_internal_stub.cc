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

ConsentInfoInternalStub::ConsentInfoInternalStub() :
  consent_status_(kConsentStatusUnknown),
  consent_form_status_(kConsentFormStatusUnknown) {}


ConsentInfoInternalStub::~ConsentInfoInternalStub() {}

ConsentStatus ConsentInfoInternalStub::GetConsentStatus() {
  return kConsentStatusUnknown;
}

ConsentFormStatus ConsentInfoInternalStub::GetConsentFormStatus() {
  return kConsentFormStatusUnknown;
}

Future<ConsentStatus> ConsentInfoInternalStub::RequestConsentStatus(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<ConsentStatus> = CreateFuture<ConsentStatus>(kConsentInfoFnRequestConsentStatus);

  if (!params.has_tag_for_under_age_of_consent()) {
    CompleteFuture(handle, kConsentRequestErrorTagForAgeOfConsentNotSet);
    return MakeFuture<ConsentStatus>(futures(), handle);
  }

  consent_status_ = kConsentStatusRequired;
  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess, consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

Future<ConsentFormStatus> ConsentInfoInternalStub::LoadConsentForm() {
  SafeFutureHandle<ConsentFormStatus> = CreateFuture<ConsentFormStatus>(kConsentInfoFnLoadConsentForm);

  consent_form_status_ = kConsentFormStatusAvailable;
  CompleteFuture<ConsentFormStatus>(handle, kConsentFormSuccess, consent_form_status_);
  return MakeFuture<ConsentFormStatus>(futures(), handle);
}

Future<ConsentStatus> ConsentInfoInternalStub::ShowConsentForm(
    FormParent parent) {
  SafeFutureHandle<ConsentStatus> = CreateFuture<ConsentStatus>(kConsentInfoFnShowConsentForm);

  consent_status_ = kConsentStatusObtained;
  CompleteFuture<ConsentStatus>(handle, kConsentRequestSuccess, consent_status_);
  return MakeFuture<ConsentStatus>(futures(), handle);
}

void ConsentInfoInternalStub::Reset() {}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
