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

#include "admob/src/ios/load_ad_result_ios.h"

#import <GoogleMobileAds/GoogleMobileAds.h>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/ios/response_info_ios.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

LoadAdResult::LoadAdResult(const LoadAdResultInternal& load_ad_result_internal)
  : AdResult(load_ad_result_internal.ad_result_internal), 
    response_info_(ResponseInfoInternal( {nil} ) ) {

  FIREBASE_ASSERT(load_ad_result_internal.ad_result_internal.ios_error);
  ResponseInfoInternal response_info_internal = ResponseInfoInternal( {
    load_ad_result_internal.ad_result_internal.ios_error.userInfo[GADErrorUserInfoKeyResponseInfo] 
  });
  response_info_ = ResponseInfo(response_info_internal);
}

}  // namespace admob
}  // namespace firebase
