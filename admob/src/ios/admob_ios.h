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

#ifndef FIREBASE_ADMOB_SRC_IOS_ADMOB_IOS_H_
#define FIREBASE_ADMOB_SRC_IOS_ADMOB_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include "admob/src/common/admob_common.h"
#include "admob/src/include/firebase/admob/types.h"

namespace firebase {
namespace admob {

// Resolves LoadAd errors that exist in the C++ SDK before they reach the iOS
// SDK.
void CompleteLoadAdInternalResult(
  FutureCallbackData<AdResult>* callback_data, AdMobError error_code,
  const char* error_message);

// Parses information from the NSError to populate an AdResult
// and completes the AdResult Future on iOS.
void CompleteLoadAdIOSResult(FutureCallbackData<AdResult>* callback_data,
                             NSError *error);


// Converts the iOS Admob GADAdValue structure into a CPP
// firebase::admob::Advalue structure.
AdValue ConvertGADAdValueToCppAdValue(GADAdValue* gad_value);

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_IOS_ADMOB_IOS_H_
