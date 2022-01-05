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

#ifndef FIREBASE_GMA_SRC_COMMON_AD_RESULT_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_AD_RESULT_INTERNAL_H_

#include <string>

#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

#if FIREBASE_PLATFORM_ANDROID
typedef jobject NativeSdkAdError;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
typedef const NSError* NativeSdkAdError;
#else
typedef void* NativeSdkAdError;
#endif

struct AdResultInternal {
  // The type of AdResult, based on the operation that was requested.
  enum AdResultInternalType {
    // Standard AdResult type for most Ad operations.
    kAdResultInternalAdError = 1,
    // AdResult represents an error the GMA SDK wrapper.
    kAdResultInternalWrapperError,
    // AdResult from a LoadAd operation.
    kAdResultInternalLoadAdError,
    // AdResult from an attempt to show a full screen ad.
    kAdResultInternalFullScreenContentError,
    // Ad Result from OpenAdInspector operations.
    kAdResultInternalOpenAdInspectorError
  };

  // Default constructor.
  AdResultInternal() {
    ad_result_type = kAdResultInternalAdError;
    code = kAdErrorNone;
    native_ad_error = nullptr;
  }

  // The type of AdResult, based on the operation that was requested.
  AdResultInternalType ad_result_type;

  // True if this was a successful result.
  bool is_successful;

  // An error code.
  AdError code;

  // A cached value of com.google.android.gms.ads.AdError.domain.
  std::string domain;

  // A cached value of com.google.android.gms.ads.AdError.message.
  std::string message;

  // A cached result from invoking com.google.android.gms.ads.AdError.ToString.
  std::string to_string;

  // If this is not a successful result, or if it's a wrapper error, then
  // native_ad_error is a reference to a error object returned by the iOS or
  // Android GMA SDK.
  NativeSdkAdError native_ad_error;

  Mutex mutex;
};

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_AD_RESULT_INTERNAL_H_
