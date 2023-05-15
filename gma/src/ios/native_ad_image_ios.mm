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

extern "C" {
#include <objc/objc.h>
}  // extern "C"

#import "gma/src/ios/FADRequest.h"

#include <string>

#include "app/src/assert.h"
#include "app/src/util_ios.h"
#include "gma/src/common/native_ad_image_internal.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/ios/native_ad_image_ios.h"

namespace firebase {
namespace gma {

NativeAdImage::NativeAdImage() {
  // Initialize the default constructor with some helpful debug values in the
  // case a NativeAdImage makes it to the application in this default state.
  internal_ = new NativeAdImageInternal();
  internal_->scale = 0;
  internal_->uri = "This NativeAdImage has not been initialized.";
  internal_->native_ad_image = nullptr;
}

NativeAdImage::NativeAdImage(const NativeAdImageInternal& native_ad_image_internal) {
  internal_ = new NativeAdImageInternal();
  internal_->native_ad_image = nullptr;

  FIREBASE_ASSERT(native_ad_image_internal.native_ad_image);

  internal_->native_ad_image = native_ad_image_internal.native_ad_image;
  GADNativeAdImage* gad_native_image = (GADNativeAdImage*)native_ad_image_internal.native_ad_image;

  FIREBASE_ASSERT(gad_native_image);

  internal_->uri = util::NSStringToString(gad_native_image.imageURL.absoluteString);
  internal_->scale = (double)gad_native_image.scale;
}

NativeAdImage::NativeAdImage(const NativeAdImage& source_native_image) : NativeAdImage() {
  // Reuse the assignment operator.
  *this = source_native_image;
}

NativeAdImage::~NativeAdImage() {
  FIREBASE_ASSERT(internal_);

  internal_->native_ad_image = nil;
  delete internal_;
  internal_ = nullptr;
}

NativeAdImage& NativeAdImage::operator=(const NativeAdImage& r_native_image) {
  FIREBASE_ASSERT(r_native_image.internal_);
  FIREBASE_ASSERT(internal_);

  NativeAdImageInternal* preexisting_internal = internal_;
  {
    MutexLock(r_native_image.internal_->mutex);
    MutexLock(internal_->mutex);
    internal_ = new NativeAdImageInternal();

    internal_->native_ad_image = r_native_image.internal_->native_ad_image;
    internal_->uri = r_native_image.internal_->uri;
    internal_->scale = r_native_image.internal_->scale;
  }

  // Deleting the internal deletes the mutex within it, so we have to delete
  // the internal after the mutex lock leaves scope.
  preexisting_internal->native_ad_image = nullptr;
  delete preexisting_internal;

  return *this;
}

/// Gets the native ad image URI.
const std::string& NativeAdImage::image_uri() const {
  FIREBASE_ASSERT(internal_);
  return internal_->uri;
}

/// Gets the image scale, which denotes the ratio of pixels to dp.
double NativeAdImage::scale() const {
  FIREBASE_ASSERT(internal_);
  return internal_->scale;
}

}  // namespace gma
}  // namespace firebase
