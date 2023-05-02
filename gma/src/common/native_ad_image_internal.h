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

#ifndef FIREBASE_GMA_SRC_COMMON_NATIVE_AD_IMAGE_INTERNAL_H_
#define FIREBASE_GMA_SRC_COMMON_NATIVE_AD_IMAGE_INTERNAL_H_

#include <string>

#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"

namespace firebase {
namespace gma {

#if FIREBASE_PLATFORM_ANDROID
typedef jobject NativeSdkNativeAdImage;
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
typedef NSObject* NativeSdkNativeAdImage;
#else
typedef void* NativeSdkNativeAdImage;
#endif

struct NativeAdImageInternal {
  // Default constructor.
  NativeAdImageInternal() { native_ad_image = nullptr; }

  // A cached value of native ad image URI.
  std::string uri;

  // A cached value of native ad image scale.
  double scale;

  // native_ad_image is a reference to a NativeAdImage object returned by the
  // iOS or Android GMA SDK.
  NativeSdkNativeAdImage native_ad_image;

  Mutex mutex;
};

}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_COMMON_NATIVE_AD_IMAGE_INTERNAL_H_
