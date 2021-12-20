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

#ifndef FIREBASE_GMA_SRC_ANDROID_RESPONSE_INFO_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_RESPONSE_INFO_ANDROID_H_

#include <jni.h>

#include "app/src/util_android.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

struct ResponseInfoInternal {
  jobject j_response_info;
};

// Used to set up the cache of class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define RESPONSEINFO_METHODS(X)                                              \
  X(GetAdapterResponses, "getAdapterResponses",                              \
    "()Ljava/util/List;"),                                                   \
  X(GetMediationAdapterClassName, "getMediationAdapterClassName",            \
    "()Ljava/lang/String;"),                                                 \
  X(GetResponseId, "getResponseId", "()Ljava/lang/String;"),                 \
  X(ToString, "toString", "()Ljava/lang/String;")
// clang-format on

METHOD_LOOKUP_DECLARATION(response_info, RESPONSEINFO_METHODS);

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_RESPONSE_INFO_ANDROID_H_
