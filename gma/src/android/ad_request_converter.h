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

#ifndef FIREBASE_GMA_SRC_ANDROID_AD_REQUEST_CONVERTER_H_
#define FIREBASE_GMA_SRC_ANDROID_AD_REQUEST_CONVERTER_H_

#include <jni.h>

#include "app/src/util_android.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

/// Converts instances of the AdRequest struct used by the C++ wrapper to
/// jobject references to Mobile Ads SDK AdRequest objects.
///
/// @param[in] request The AdRequest struct to be converted into a Java
/// AdRequest.
/// @param[out] error kAdErrorCodeNone on success, or another error if
/// problems occurred.
/// @return On success, a local reference to an Android object representing the
/// AdRequest, or nullptr on error.
jobject GetJavaAdRequestFromCPPAdRequest(const AdRequest& request,
                                         gma::AdErrorCode* error);

// Converts the Android AdRequest error codes to the CPP
// platform-independent error codes defined in AdErrorCode.
AdErrorCode MapAndroidAdRequestErrorCodeToCPPErrorCode(jint j_error_code);

// Converts the Android FullScreenContentCallback error codes to the CPP
// platform-independent error codes defined in AdErrorCode.
AdErrorCode MapAndroidFullScreenContentErrorCodeToCPPErrorCode(
    jint j_error_code);

// Converts the Android OpenAdInspector error codes to the CPP
// platform-independent error codes defined in AdErrorCode.
AdErrorCode MapAndroidOpenAdInspectorErrorCodeToCPPErrorCode(jint j_error_code);

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_AD_REQUEST_CONVERTER_H_
