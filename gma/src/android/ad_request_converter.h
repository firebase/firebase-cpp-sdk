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

#ifndef FIREBASE_ADMOB_SRC_ANDROID_AD_REQUEST_CONVERTER_H_
#define FIREBASE_ADMOB_SRC_ANDROID_AD_REQUEST_CONVERTER_H_

#include <jni.h>

#include "admob/src/include/firebase/admob/types.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

/// Converts instances of the AdRequest struct used by the C++ wrapper to
/// jobject references to Mobile Ads SDK AdRequest objects.
///
/// @param[in] request The AdRequest struct to be converted into a Java
/// AdRequest.
/// @param[out] error kAdMobErrorNone on success, or another error if
/// problems occurred.
/// @return On succes, a local reference to an Android object representing the
/// AdRequest, or nullptr on error.
jobject GetJavaAdRequestFromCPPAdRequest(const AdRequest& request,
                                         admob::AdMobError* error);

// Converts the Android AdRequest error codes to the CPP
// platform-independent error codes defined in AdMobError.
AdMobError MapAndroidAdRequestErrorCodeToCPPErrorCode(jint j_error_code);

// Converts the Android FullScreenContentCallback error codes to the CPP
// platform-independent error codes defined in AdMobError.
AdMobError MapAndroidFullScreenContentErrorCodeToCPPErrorCode(
    jint j_error_code);

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_ANDROID_AD_REQUEST_CONVERTER_H_
