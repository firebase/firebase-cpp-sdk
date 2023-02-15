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

#import <Foundation/Foundation.h>

// TODO(@dellabitta): remove the extern "C" block once b/228589822 has been
// resolved.
extern "C" {
#import <GoogleMobileAds/GoogleMobileAds.h>
}

#include "gma/src/include/firebase/gma/types.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace gma {

/// Returns a GADAdSize from a gma::AdSize.
GADAdSize GADSizeFromCppAdSize(const AdSize& ad_size);

}  // namespace gma
}  // namespace firebase

NS_ASSUME_NONNULL_END
