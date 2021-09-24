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

#import <Foundation/Foundation.h>
#import <GoogleMobileAds/GoogleMobileAds.h>

#include "admob/src/include/firebase/admob/types.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace admob {

/// Returns a GADRequest from an admob::AdRequest.
/// Converts instances of the AdRequest struct used by the C++ wrapper to
/// to Mobile Ads SDK GADRequest objects.
///
/// @param[in] request The AdRequest struct to be converted into a
/// GAdRequest.
/// @param[out] error kAdMobErrorNone on success, or another error if
/// problems occurred.
/// @return On succes, a pointer to a GADRequest object representing the
/// AdRequest, or nullptr on error.
GADRequest *GADRequestFromCppAdRequest(const AdRequest& adRequest,
                                       admob::AdMobError* error);

}  // namespace admob
}  // namespace firebase

NS_ASSUME_NONNULL_END
