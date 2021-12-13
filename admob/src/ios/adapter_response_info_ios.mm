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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#include <string>

#include "admob/src/include/firebase/admob.h"
#include "admob/src/include/firebase/admob/types.h"
#include "admob/src/ios/adapter_response_info_ios.h"
#include "admob/src/ios/ad_result_ios.h"
#include "app/src/mutex.h"
#include "app/src/util_ios.h"

namespace firebase {
namespace admob {

AdapterResponseInfo::AdapterResponseInfo(
  const AdapterResponseInfoInternal& internal) : ad_result_(AdResultInternal())
{
  FIREBASE_ASSERT(internal.ad_network_response_info);

  AdResultInternal ad_result_internal;
  ad_result_internal.native_ad_error = internal.ad_network_response_info.error;
  ad_result_ = AdResult(ad_result_internal);
  
  adapter_class_name_ = util::NSStringToString(
    internal.ad_network_response_info.adNetworkClassName);
  
  // ObjC latency is an NSInterval, which is in seconds.  Convert to millis.
  latency_ = (int64_t)(internal.ad_network_response_info.latency) *
    (int64_t)1000;

  NSString *to_string = [NSString stringWithFormat:@"AdapterResponseInfo"
        "class name: %@, latency : %ld, ad result: %@", 
        internal.ad_network_response_info.adNetworkClassName,
        internal.ad_network_response_info.latency,
        internal.ad_network_response_info.error];
  to_string_ = util::NSStringToString(to_string);
}

}  // namespace admob
}  // namespace firebase
