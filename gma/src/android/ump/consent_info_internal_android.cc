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

#include "gma/src/android/ump/consent_info_internal_android.h"

#include "app/src/thread.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

// This explicitly implements the constructor for the outer class,
// ConsentInfoInternal.
ConsentInfoInternal* ConsentInfoInternal::CreateInstance() {
  return new ConsentInfoInternalAndroid();
}

ConsentInfoInternalAndroid::ConsentInfoInternalAndroid() {}

ConsentInfoInternalAndroid::~ConsentInfoInternalAndroid() {}

Future<void> ConsentInfoInternalAndroid::RequestConsentInfoUpdate(
    const ConsentRequestParameters& params) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnRequestConsentInfoUpdate);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

ConsentStatus ConsentInfoInternalAndroid::GetConsentStatus() {
  return kConsentStatusUnknown;
}
ConsentFormStatus ConsentInfoInternalAndroid::GetConsentFormStatus() {
  return kConsentFormStatusUnknown;
}

Future<void> ConsentInfoInternalAndroid::LoadConsentForm() {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnLoadConsentForm);

  CompleteFuture(handle, kConsentFormSuccess);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::ShowConsentForm(FormParent parent) {
  SafeFutureHandle<void> handle = CreateFuture(kConsentInfoFnShowConsentForm);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

Future<void> ConsentInfoInternalAndroid::LoadAndShowConsentFormIfRequired(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnLoadAndShowConsentFormIfRequired);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

PrivacyOptionsRequirementStatus
ConsentInfoInternalAndroid::GetPrivacyOptionsRequirementStatus() {
  return kPrivacyOptionsRequirementStatusUnknown;
}

Future<void> ConsentInfoInternalAndroid::ShowPrivacyOptionsForm(
    FormParent parent) {
  SafeFutureHandle<void> handle =
      CreateFuture(kConsentInfoFnShowPrivacyOptionsForm);

  CompleteFuture(handle, kConsentRequestSuccess);
  return MakeFuture<void>(futures(), handle);
}

bool ConsentInfoInternalAndroid::CanRequestAds() { return false; }

void ConsentInfoInternalAndroid::Reset() {}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
