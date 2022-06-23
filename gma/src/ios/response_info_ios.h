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

#ifndef FIREBASE_GMA_SRC_IOS_RESPONSE_INFO_IOS_H_
#define FIREBASE_GMA_SRC_IOS_RESPONSE_INFO_IOS_H_

#import <GoogleMobileAds/GoogleMobileAds.h>

namespace firebase {
namespace gma {

struct ResponseInfoInternal {
  GADResponseInfo* gad_response_info;
};

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_IOS_RESPONSE_INFO_IOS_H_
