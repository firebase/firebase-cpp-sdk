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

#ifndef FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_
#define FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_

#include <string>

#include "admob/src/include/firebase/admob/types.h"
#include "app/src/mutex.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {

// Used to set up the cache of class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define ADERROR_METHODS(X)                                                   \
  X(GetCause, "getCause",                                                    \
      "()Lcom/google/android/gms/ads/AdError;"),                             \
  X(GetCode, "getCode", "()I"),                                              \
  X(GetDomain, "getDomain", "()Ljava/lang/String;"),                         \
  X(GetMessage, "getMessage", "()Ljava/lang/String;"),                       \
  X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(ad_error, ADERROR_METHODS);

struct AdResultInternal {
  // True if the result contains an error originating from C++/Java wrapper
  // code. If false, then an Admob Android AdError has occurred.
  bool is_wrapper_error;

  // True if this was a successful result.
  bool is_successful;

  // An error code
  AdMobError code;

  // A cached value of com.google.android.gms.ads.AdError.domain
  std::string domain;

  // A cached value of com.google.android.gms.ads.AdError.message
  std::string message;

  // A cached result from invoking com.google.android.gms.ads.AdError.ToString.
  std::string to_string;

  // If this is not a successful result, or if it's a wrapper error, then
  // j_ad_error is a reference to a com.google.android.gms.ads.AdError produced
  // by the Admob Android SDK.
  jobject j_ad_error;

  Mutex mutex;
};

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_
