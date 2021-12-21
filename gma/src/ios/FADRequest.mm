/*
 * Copyright 2021 Google LLC
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

#import "gma/src/ios/FADRequest.h"

#include "gma/src/common/gma_common.h"
#include "app/src/util_ios.h"

#include "app/src/log.h"

namespace firebase {
namespace gma {

GADRequest *GADRequestFromCppAdRequest(const AdRequest& adRequest, 
                                      gma::AdError* error, 
                                      std::string* error_message) {
  FIREBASE_ASSERT(error);
  FIREBASE_ASSERT(error_message);
  *error = kAdErrorNone;

  // Create the GADRequest.
  GADRequest *gadRequest = [GADRequest request];

  // Keywords.
  const std::unordered_set<std::string>& keywords = adRequest.keywords();
  if( keywords.size() > 0 ) {
    NSMutableArray *gadKeywords = [[NSMutableArray alloc] init];
    for (auto keyword = keywords.begin(); keyword != keywords.end(); ++keyword) {
      [gadKeywords addObject: util::StringToNSString(*keyword)];
    }
    gadRequest.keywords = gadKeywords;
  }
  
  // Extras.
  const std::map<std::string, std::map<std::string, std::string>>& extras =
      adRequest.extras();
  for (auto adapter_iter = extras.begin(); adapter_iter != extras.end(); 
       ++adapter_iter) {
    const std::string adapterClassName = adapter_iter->first;
    // Attempt to resolve the custom class.
    Class extrasClass = NSClassFromString(util::StringToNSString(adapterClassName));
    if (extrasClass == nil ) {
      *error_message = "Failed to resolve extras class: ";
      error_message->append(adapterClassName);
      LogError(error_message->c_str());
      *error = kAdErrorAdNetworkClassLoadError;
      return nullptr;
    }

    // Attempt allocate a object of the class, and check to see if it's
    // of an expected type.
    id gadExtrasId = [[extrasClass alloc] init];
    if (![gadExtrasId isKindOfClass:[GADExtras class]]) {
      *error_message = "Failed to load extras class inherited from GADExtras: ";
      error_message->append(adapterClassName);
      LogError(error_message->c_str());
      *error = kAdErrorAdNetworkClassLoadError;
      return nullptr;
    }

    // Add the key/value dictionary to the object.
    // adpter_iter->second is a std::map of the key/value pairs.
    if (!adapter_iter->second.empty()) {
      NSMutableDictionary *additionalParameters = [[NSMutableDictionary alloc] init];
      for (auto extra_iter = adapter_iter->second.begin();
           extra_iter != adapter_iter->second.end(); ++extra_iter) {
        NSString *key = util::StringToNSString(extra_iter->first);
        NSString *value = util::StringToNSString(extra_iter->second);
        additionalParameters[key] = value;
      }
      
      GADExtras* gadExtras = (GADExtras*)gadExtrasId;
      gadExtras.additionalParameters = additionalParameters;
      [gadRequest registerAdNetworkExtras:gadExtras];
    }
  }

  // Content URL
  if (!adRequest.content_url().empty()) {
    gadRequest.contentURL = util::StringToNSString(adRequest.content_url());
  }

  // Neighboring Content URLs
  if (!adRequest.neighboring_content_urls().empty()) {
    gadRequest.neighboringContentURLStrings =
      util::StringUnorderedSetToNSMutableArray(adRequest.neighboring_content_urls());
  }

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  gadRequest.requestAgent = @(GetRequestAgentString());

  return gadRequest;
}

AdError MapAdRequestErrorCodeToCPPErrorCode(GADErrorCode error_code) {
  // iOS error code sourced from
  // https://developers.google.com/admob/ios/api/reference/Enums/GADErrorCode
  switch(error_code)
  {
    case GADErrorInvalidRequest:               // 0
      return kAdErrorInvalidRequest;
    case GADErrorNoFill:                       // 1
      return kAdErrorNoFill;
    case GADErrorNetworkError:                 // 2
      return kAdErrorNetworkError;
    case GADErrorServerError:                  // 3
      return kAdErrorServerError;
    case GADErrorOSVersionTooLow:              // 4
      return kAdErrorOSVersionTooLow;
    case GADErrorTimeout:                      // 5
      return kAdErrorTimeout;
    // no error 6.
    case GADErrorMediationDataError:           // 7
      return kAdErrorMediationDataError;
    case GADErrorMediationAdapterError:        // 8
      return kAdErrorMediationAdapterError;
    case GADErrorMediationNoFill:              // 9
      return kAdErrorMediationNoFill;
    case GADErrorMediationInvalidAdSize:       // 10
      return kAdErrorMediationInvalidAdSize;
    case GADErrorInternalError:                // 11
      return kAdErrorInternalError;
    case GADErrorInvalidArgument:              // 12
      return kAdErrorInvalidArgument;
    case GADErrorReceivedInvalidResponse:      // 13
      return kAdErrorReceivedInvalidResponse;
    case GADErrorAdAlreadyUsed:                // 19 (no error #s 14-18)
      return kAdErrorAdAlreadyUsed;
    case GADErrorApplicationIdentifierMissing: // 20
      return kAdErrorApplicationIdentifierMissing;
    default:
      return kAdErrorUnknown;
  }
}

AdError MapFullScreenContentErrorCodeToCPPErrorCode(GADPresentationErrorCode error_code) {
  // iOS error code sourced from
  // https://developers.google.com/admob/ios/api/reference/Enums/GADPresentationErrorCode
  switch(error_code)
  {
    case GADPresentationErrorCodeAdNotReady:     // 15
      return kAdErrorAdNotReady;
    case GADPresentationErrorCodeAdTooLarge:     // 16
      return kAdErrorAdTooLarge;
    case GADPresentationErrorCodeInternal:       // 17
      return kAdErrorInternalError;
    case GADPresentationErrorCodeAdAlreadyUsed:  // 18
      return kAdErrorAdAlreadyUsed;
    case GADPresentationErrorNotMainThread:      // 21
      return kAdErrorNotMainThread;
    case GADPresentationErrorMediation:          // 22
      return kAdErrorMediationShowError;
    default:
      return kAdErrorUnknown;
  }
}

}  // namespace gma
}  // namespace firebase
