/*
 * Copyright 2016 Google LLC
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

#import "admob/src/ios/FADRequest.h"

#include "admob/src/common/admob_common.h"
#include "app/src/util_ios.h"

#include "app/src/log.h"

namespace firebase {
namespace admob {

GADRequest *GADRequestFromCppAdRequest(const AdRequest& adRequest, 
                                      admob::AdMobError* error, 
                                      std::string* error_message) {
  FIREBASE_ASSERT(error);
  FIREBASE_ASSERT(error_message);
  *error = kAdMobErrorNone;

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
      *error = kAdMobErrorAdNetworkClassLoadError;
      return nullptr;
    }

    // Attempt allocate a object of the class, and check to see if it's
    // of an expected type.
    id gadExtrasId = [[extrasClass alloc] init];
    if (![gadExtrasId isKindOfClass:[GADExtras class]]) {
      *error_message = "Failed to load extras class inherited from GADExtras: ";
      error_message->append(adapterClassName);
      LogError(error_message->c_str());
      *error = kAdMobErrorAdNetworkClassLoadError;
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

AdMobError MapADErrorCodeToCPPErrorCode(GADErrorCode error_code) {
  // iOS error code sourced from
  // https://developers.google.com/admob/ios/api/reference/Enums/GADErrorCode
  switch(error_code)
  {
    case GADErrorInvalidRequest:               // 0
      return kAdMobErrorInvalidRequest;
    case GADErrorNoFill:                       // 1
      return kAdMobErrorNoFill;
    case GADErrorNetworkError:                 // 2
      return kAdMobErrorNetworkError;
    case GADErrorServerError:                  // 3
      return kAdMobErrorServerError;
    case GADErrorOSVersionTooLow:              // 4
      return kAdMobErrorOSVersionTooLow;
    case GADErrorTimeout:                      // 5
      return kAdMobErrorTimeout;
    // no error 6.
    case GADErrorMediationDataError:           // 7
      return kAdMobErrorMediationDataError;
    case GADErrorMediationAdapterError:        // 8
      return kAdMobErrorMediationAdapterError;
    case GADErrorMediationNoFill:              // 9
      return kAdMobErrorMediationNoFill;
    case GADErrorMediationInvalidAdSize:       // 10
      return kAdMobErrorMediationInvalidAdSize;
    case GADErrorInternalError:                // 11
      return kAdMobErrorInternalError;
    case GADErrorInvalidArgument:              // 12
      return kAdMobErrorInvalidArgument;
    case GADErrorReceivedInvalidResponse:      // 13
      return kAdMobErrorReceivedInvalidResponse;
    case GADErrorAdAlreadyUsed:                // 19 (no error #s 14-18)
      return kAdMobErrorAdAlreadyUsed;
    case GADErrorApplicationIdentifierMissing: // 20
      return kAdMobErrorApplicationIdentifierMissing;
    default:
      return kAdMobErrorUnknown;
  }
}

}
}
