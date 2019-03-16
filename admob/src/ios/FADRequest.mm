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

  // Gender.
  switch (adRequest.gender) {
    case kGenderUnknown:
      request.gender = kGADGenderUnknown;
      break;
    case kGenderMale:
      request.gender = kGADGenderMale;
      break;
    case kGenderFemale:
      request.gender = kGADGenderFemale;
      break;
    default:
      request.gender = kGADGenderUnknown;
      break;
  }

  // Child-directed treatment.
  switch (adRequest.tagged_for_child_directed_treatment) {
    case kChildDirectedTreatmentStateTagged:
      [request tagForChildDirectedTreatment:YES];
      break;
    case kChildDirectedTreatmentStateNotTagged:
      [request tagForChildDirectedTreatment:NO];
      break;
    case kChildDirectedTreatmentStateUnknown:
      break;
  }

  // Test devices.
  if (adRequest.test_device_id_count > 0) {
    NSMutableArray *testDevices = [[NSMutableArray alloc] init];
    for (int i = 0; i < adRequest.test_device_id_count; i++) {
      [testDevices addObject:@(adRequest.test_device_ids[i])];
    }
    request.testDevices = testDevices;
  }

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

  // Birthday.
  NSDateComponents *components = [[NSDateComponents alloc] init];
  components.month = adRequest.birthday_month;
  components.day = adRequest.birthday_day;
  components.year = adRequest.birthday_year;
  request.birthday = [[NSCalendar currentCalendar] dateFromComponents:components];

  // Set the request agent string so requests originating from this library can
  // be tracked and reported on as a group.
  request.requestAgent = @(GetRequestAgentString());

  return request;
}

}
}
