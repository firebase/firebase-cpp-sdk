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

#ifndef FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_H_
#define FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_H_

#include "firebase/internal/platform.h"

#if FIREBASE_PLATFORM_ANDROID
#include <jni.h>
#endif  // FIREBASE_PLATFORM_ANDROID

#include "firebase/admob/banner_view.h"
#include "firebase/admob/interstitial_ad.h"
#include "firebase/admob/types.h"
#include "firebase/app.h"
#include "firebase/internal/common.h"

#if !defined(DOXYGEN) && !defined(SWIG)
FIREBASE_APP_REGISTER_CALLBACKS_REFERENCE(admob)
#endif  // !defined(DOXYGEN) && !defined(SWIG)

namespace firebase {

/// @brief API for AdMob with Firebase.
///
/// The AdMob API allows you to load and display mobile ads using the Google
/// Mobile Ads SDK. Each ad format has its own header file.
namespace admob {

/// Initializes AdMob via Firebase.
///
/// @param app The Firebase app for which to initialize mobile ads.
///
/// @return kInitResultSuccess if initialization succeeded, or
/// kInitResultFailedMissingDependency on Android if Google Play services is not
/// available on the current device and the Google Mobile Ads SDK requires
/// Google Play services (for example, when using 'play-services-ads-lite').
InitResult Initialize(const ::firebase::App& app);

#if FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
/// Initializes AdMob without Firebase for Android.
///
/// The arguments to @ref Initialize are platform-specific so the caller must do
/// something like this:
/// @code
/// #if defined(__ANDROID__)
/// firebase::admob::Initialize(jni_env, activity);
/// #else
/// firebase::admob::Initialize();
/// #endif
/// @endcode
///
/// @param[in] jni_env JNIEnv pointer.
/// @param[in] activity Activity used to start the application.
///
/// @return kInitResultSuccess if initialization succeeded, or
/// kInitResultFailedMissingDependency on Android if Google Play services is not
/// available on the current device and the AdMob SDK requires
/// Google Play services (for example when using 'play-services-ads-lite').
InitResult Initialize(JNIEnv* jni_env, jobject activity);

#endif  // defined(__ANDROID__) || defined(DOXYGEN)
#if !FIREBASE_PLATFORM_ANDROID || defined(DOXYGEN)
/// Initializes AdMob without Firebase for iOS.
InitResult Initialize();
#endif  // !defined(__ANDROID__) || defined(DOXYGEN)

/// Sets the global @ref RequestConfiguration that will be used for
/// every @ref AdRequest during the app's session.
///
/// @param[in] request_configuration The request configuration that should be applied
/// to all ad requests.
void SetRequestConfiguration(const RequestConfiguration& request_configuration);

/// Gets the global RequestConfiguration.
///
/// @return the currently active @ref RequestConfiguration that's being
/// used for every ad request.  
/// @note: on iOS, the 
/// @ref RequestConfiguration::tag_for_child_directed_treatment and 
/// @ref RequestConfiguration::tag_for_under_age_of_consent fields will be set
/// set to kChildDirectedTreatmentUnspecified, and kUnderAgeOfConsentUnspecified,
/// respectfully.
RequestConfiguration GetRequestConfiguration();

/// @brief Terminate AdMob.
///
/// Frees resources associated with AdMob that were allocated during
/// @ref firebase::admob::Initialize().
void Terminate();

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_INCLUDE_FIREBASE_ADMOB_H_
