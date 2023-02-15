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
#import <GoogleMobileAds/GoogleMobileAds.h>

#include <string>

#include "gma/src/include/firebase/gma/types.h"

NS_ASSUME_NONNULL_BEGIN

namespace firebase {
namespace gma {

/// Returns a GADRequest from a gma::AdRequest.
/// Converts instances of the AdRequest struct used by the C++ wrapper to
/// to Mobile Ads SDK GADRequest objects.
///
/// @param[in] request The AdRequest struct to be converted into a
/// GAdRequest.
/// @param[out] error kAdErrorCodeNone on success, or another error if
/// problems occurred.
/// @param[out] error_message a string representation of any error that
/// occurs.
/// @return On success, a pointer to a GADRequest object representing the
/// AdRequest, or nullptr on error.
GADRequest* GADRequestFromCppAdRequest(const AdRequest& adRequest,
                                       gma::AdErrorCode* error,
                                       std::string* error_message);

// Converts the iOS error codes from an AdRequest to the CPP platform
// independent error codes defined in AdErrorCode.
AdErrorCode MapAdRequestErrorCodeToCPPErrorCode(GADErrorCode error_code);

// Converts the iOS error codes from attempting to show a full screen
// ad into the platform independent error codes defined in AdError.
AdErrorCode MapFullScreenContentErrorCodeToCPPErrorCode(
    GADPresentationErrorCode error_code);

}  // namespace gma
}  // namespace firebase

NS_ASSUME_NONNULL_END
