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

GADRequest *GADRequestFromCppAdRequest(const AdRequest& adRequest) {
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
      LogError("Failed to resolve extras class: \"%s\"", adapterClassName.c_str());
      continue;
    }

    // Attempt allocate a object of the class, and check to see if it's
    // of an expected type.
    id gadExtrasId = [[extrasClass alloc] init];
    if (![gadExtrasId isKindOfClass:[GADExtras class]]) {
      LogError("Failed to load extras class inherited from GADExtras: \"%s\"",
          adapterClassName.c_str());
      continue;
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

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  gadRequest.requestAgent = @(GetRequestAgentString());

  return gadRequest;
}

}
}
