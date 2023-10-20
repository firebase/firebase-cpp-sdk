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

#include "gma/src/common/ump/consent_info_internal.h"

#include "app/src/include/firebase/internal/platform.h"

namespace firebase {
namespace gma {
namespace ump {
namespace internal {

ConsentInfoInternal::ConsentInfoInternal() : futures_(kConsentInfoFnCount) {}

ConsentInfoInternal::~ConsentInfoInternal() {}

const char* ConsentInfoInternal::GetConsentRequestErrorMessage(
    ConsentRequestError error_code) {
  switch (error_code) {
    case kConsentRequestSuccess:
      return "Success";
    case kConsentRequestErrorInvalidAppId:
#if FIREBASE_PLATFORM_ANDROID
      return "Missing or invalid com.google.android.gms.ads.APPLICATION_ID in "
             "AndroidManifest.xml";
#elif FIREBASE_PLATFORM_IOS
      return "Missing or invalid GADApplicationidentifier in Info.plist";
#else
      return "Missing or invalid App ID";
#endif
    case kConsentRequestErrorNetwork:
      return "Network error";
    case kConsentRequestErrorInternal:
      return "Internal error";
    case kConsentRequestErrorMisconfiguration:
      return "A misconfiguration exists in the UI";
    case kConsentRequestErrorUnknown:
      return "Unknown error";
    case kConsentRequestErrorInvalidOperation:
      return "Invalid operation";
    case kConsentRequestErrorOperationInProgress:
      return "Operation already in progress. Please wait for it to finish by "
             "checking RequestConsentInfoUpdateLastResult().";
    default:
      return "Bad error code";
  }
}

const char* ConsentInfoInternal::GetConsentFormErrorMessage(
    ConsentFormError error_code) {
  switch (error_code) {
    case kConsentFormSuccess:
      return "Success";
    case kConsentFormErrorTimeout:
      return "Timed out";
    case kConsentFormErrorUnavailable:
      return "The form is unavailable.";
    case kConsentFormErrorInternal:
      return "Internal error";
    case kConsentFormErrorUnknown:
      return "Unknown error";
    case kConsentFormErrorAlreadyUsed:
      return "The form was already used";
    case kConsentFormErrorInvalidOperation:
      return "Invalid operation";
    case kConsentFormErrorOperationInProgress:
      return "Operation already in progress. Please wait for it to finish by "
             "checking LoadFormLastResult() or ShowFormLastResult().";
    default:
      return "Bad error code";
  }
}

}  // namespace internal
}  // namespace ump
}  // namespace gma
}  // namespace firebase
