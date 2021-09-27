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

#ifndef FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_
#define FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_

#include "admob/src/common/banner_view_internal.h"
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

}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_ANDROID_AD_RESULT_ANDROID_H_
