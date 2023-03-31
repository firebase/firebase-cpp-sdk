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

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "app/src/assert.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/types.h"
#include "gma/src/ios/ad_error_ios.h"
#include "gma/src/ios/adapter_response_info_ios.h"

namespace firebase {
namespace gma {

AdapterResponseInfo::AdapterResponseInfo(const AdapterResponseInfoInternal& internal)
    : ad_result_() {
  FIREBASE_ASSERT(internal.ad_network_response_info);

  if (internal.ad_network_response_info.error != nil) {
    AdErrorInternal ad_error_internal;
    ad_error_internal.native_ad_error = internal.ad_network_response_info.error;
    AdError ad_error = AdError(ad_error_internal);
    if (ad_error.code() != kAdErrorCodeNone) {
      ad_result_ = AdResult(AdError(ad_error_internal));
    }
  }

  adapter_class_name_ =
      util::NSStringToString(internal.ad_network_response_info.adNetworkClassName);

  // ObjC latency is an NSInterval, which is in seconds.  Convert to millis.
  latency_ = (int64_t)(internal.ad_network_response_info.latency) * (int64_t)1000;

  to_string_ = util::NSStringToString(internal.ad_network_response_info.description);
}

}  // namespace gma
}  // namespace firebase
