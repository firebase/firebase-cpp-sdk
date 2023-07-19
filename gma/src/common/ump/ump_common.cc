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

#include "firebase/gma/ump/ump.h"

namespace firebase {
namespace gma {
namespace ump {

  const char* GetConsentRequestErrorMessage(ConsentRequestError error_code) {
    switch(error_code) {
    case kConsentRequestSuccess:
      return "Success";
      case kConsentRequestErrorInvalidAppId:
#if FIREBASE_PLATFORM_ANDROID
	return "Missing or invalid com.google.android.gms.ads.APPLICATION_ID in AndroidManifest.xml.";
#elif FIREBASE_PLATFORM_IOS
	return "Missing or invalid GADApplicationidentifier in Info.plist.";
#else
	return "Missing or invalid App ID.";
#endif
    case kConsentRequestErrorNetwork:
      return "Network error.";
    case kConsentRequestErrorTagForAgeOfConsentNotSet:
      return "You must explicitly call ConsentRequestParameters.set_tag_for_under_age_of_consent()."
    case kConsentRequestErrorInternal:
      return "Internal error";
    case kConsentRequestErrorCodeMisconfiguration:
      return "Code misconfiguration error";
    case kConsentRequestErrorUnknown:
      return "Unknown error";
    default:
      return "Bad error code"
    }
  }

 
}  // namespace ump
}  // namespace gma
}  // namespace firebase


