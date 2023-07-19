/*
 * Copyright 2023 Google LLC
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

#ifndef FIREBASE_GMA_SRC_ANDROID_NATIVE_AD_IMAGE_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_NATIVE_AD_IMAGE_ANDROID_H_

#include <string>

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_android.h"
#include "gma/src/common/native_ad_image_internal.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

// Used to set up the cache of class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define NATIVEADIMAGE_METHODS(X)                                             \
  X(GetScale, "getScale", "()D"),                                            \
  X(GetUri, "getUri", "()Landroid/net/Uri;"),                                \
  X(GetDrawable, "getDrawable", "()Landroid/graphics/drawable/Drawable;")

#define DOWNLOADHELPER_METHODS(X)                                        \
  X(Constructor, "<init>", "(Ljava/lang/String;)V"),                     \
  X(AddHeader, "addHeader", "(Ljava/lang/String;Ljava/lang/String;)V"),  \
  X(Download, "download", "(J)V"),                                       \
  X(GetResponseCode, "getResponseCode", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(native_image, NATIVEADIMAGE_METHODS);
METHOD_LOOKUP_DECLARATION(download_helper, DOWNLOADHELPER_METHODS);

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_NATIVE_AD_IMAGE_ANDROID_H_
