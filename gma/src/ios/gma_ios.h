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

#ifndef FIREBASE_GMA_SRC_IOS_GMA_IOS_H_
#define FIREBASE_GMA_SRC_IOS_GMA_IOS_H_

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

// Completes AdResult futures for successful ad loads.
void CompleteLoadAdInternalSuccess(
    FutureCallbackData<AdResult>* callback_data,
    const ResponseInfoInternal& response_info_internal);

// Resolves LoadAd errors that exist in the C++ SDK before they reach the iOS
// SDK.
void CompleteLoadAdInternalError(FutureCallbackData<AdResult>* callback_data,
                                 AdErrorCode error_code,
                                 const char* error_message);

// Parses information from the NSError to populate an AdResult
// and completes the AdResult Future on iOS.
void CompleteAdResultError(FutureCallbackData<AdResult>* callback_data,
                           NSError* error, bool is_load_ad_error);

// Converts the iOS GMA GADAdValue structure into a CPP
// firebase::gma::Advalue structure.
AdValue ConvertGADAdValueToCppAdValue(GADAdValue* gad_value);

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_GMA_IOS_H_
