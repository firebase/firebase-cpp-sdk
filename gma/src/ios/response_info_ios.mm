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

#include "gma/src/ios/response_info_ios.h"

#include <string>
#include <vector>

#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/types.h"
#include "gma/src/ios/adapter_response_info_ios.h"

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_ios.h"
#include "gma/src/ios/ad_error_ios.h"

namespace firebase {
namespace gma {

ResponseInfo::ResponseInfo() { to_string_ = "This ResponseInfo has not been initialized."; }

ResponseInfo::ResponseInfo(const ResponseInfoInternal& response_info_internal) {
  if (response_info_internal.gad_response_info == nil) {
    return;
  }

  response_id_ =
      util::NSStringToString(response_info_internal.gad_response_info.responseIdentifier);

  mediation_adapter_class_name_ =
      util::NSStringToString(response_info_internal.gad_response_info.adNetworkClassName);

  NSEnumerator* enumerator =
      [response_info_internal.gad_response_info.adNetworkInfoArray objectEnumerator];

  GADAdNetworkResponseInfo* gad_response_info;
  while (gad_response_info = [enumerator nextObject]) {
    AdapterResponseInfoInternal adapter_response_info_internal;
    adapter_response_info_internal.ad_network_response_info = gad_response_info;
    adapter_responses_.push_back(AdapterResponseInfo(adapter_response_info_internal));
  }

  NSString* to_string =
      [[NSString alloc] initWithFormat:@"ResponseInfo "
                                        "response_id: %@, mediation_adapter_classname : %@, "
                                        "adapter_response_info: %@",
                                       response_info_internal.gad_response_info.responseIdentifier,
                                       response_info_internal.gad_response_info.adNetworkClassName,
                                       response_info_internal.gad_response_info.adNetworkInfoArray];
  to_string_ = util::NSStringToString(to_string);
}

}  // namespace gma
}  // namespace firebase
