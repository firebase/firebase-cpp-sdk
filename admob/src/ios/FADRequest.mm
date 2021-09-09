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

namespace firebase {
namespace admob {

GADRequest *GADRequestFromCppAdRequest(AdRequest adRequest) {
  // Create the GADRequest.
  GADRequest *request = [GADRequest request];
  // Keywords.
  if (adRequest.keyword_count > 0) {
    NSMutableArray *keywords = [[NSMutableArray alloc] init];
    for (int i = 0; i < adRequest.keyword_count; i++) {
      [keywords addObject:@(adRequest.keywords[i])];
    }
    request.keywords = keywords;
  }

  // Extras.
  if (adRequest.extras_count > 0) {
    NSMutableDictionary *additionalParameters = [[NSMutableDictionary alloc] init];
    for (int i = 0; i < adRequest.extras_count; i++) {
      NSString *key = @(adRequest.extras[i].key);
      NSString *value = @(adRequest.extras[i].value);
      additionalParameters[key] = value;
    }
    GADExtras *extras = [[GADExtras alloc] init];
    extras.additionalParameters = additionalParameters;
    [request registerAdNetworkExtras:extras];
  }

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  request.requestAgent = @(GetRequestAgentString());

  return request;
}

}
}
