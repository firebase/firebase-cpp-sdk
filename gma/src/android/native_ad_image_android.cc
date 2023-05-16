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

#include "gma/src/android/native_ad_image_android.h"

#include <jni.h>

#include <memory>
#include <string>
#include <vector>

#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/types.h"

namespace firebase {
namespace gma {

METHOD_LOOKUP_DEFINITION(native_image,
                         PROGUARD_KEEP_CLASS
                         "com/google/android/gms/ads/nativead/NativeAd$Image",
                         NATIVEADIMAGE_METHODS);

METHOD_LOOKUP_DEFINITION(download_helper,
                         "com/google/firebase/gma/internal/cpp/DownloadHelper",
                         DOWNLOADHELPER_METHODS);

NativeAdImage::NativeAdImage() {
  // Initialize the default constructor with some helpful debug values in the
  // case a NativeAdImage makes it to the application in this default state.
  internal_ = new NativeAdImageInternal();
  internal_->scale = 0;
  internal_->uri = "This NativeAdImage has not been initialized.";
  internal_->native_ad_image = nullptr;
}

NativeAdImage::NativeAdImage(
    const NativeAdImageInternal& native_ad_image_internal) {
  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);

  internal_ = new NativeAdImageInternal();
  internal_->native_ad_image = nullptr;

  FIREBASE_ASSERT(native_ad_image_internal.native_ad_image != nullptr);

  internal_->native_ad_image =
      env->NewGlobalRef(native_ad_image_internal.native_ad_image);

  util::CheckAndClearJniExceptions(env);

  // NativeAdImage Uri.
  jobject j_uri =
      env->CallObjectMethod(internal_->native_ad_image,
                            native_image::GetMethodId(native_image::kGetUri));
  util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(j_uri);
  internal_->uri = util::JniUriToString(env, j_uri);

  // NativeAdImage scale.
  jdouble j_scale =
      env->CallDoubleMethod(internal_->native_ad_image,
                            native_image::GetMethodId(native_image::kGetScale));
  util::CheckAndClearJniExceptions(env);
  FIREBASE_ASSERT(j_scale);
  internal_->scale = static_cast<double>(j_scale);
}

NativeAdImage::NativeAdImage(const NativeAdImage& source_native_image)
    : NativeAdImage() {
  // Reuse the assignment operator.
  *this = source_native_image;
}

NativeAdImage& NativeAdImage::operator=(const NativeAdImage& r_native_image) {
  if (&r_native_image == this) {
    return *this;
  }

  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(internal_);
  FIREBASE_ASSERT(r_native_image.internal_);

  NativeAdImageInternal* preexisting_internal = internal_;
  {
    // Lock the parties so they're not deleted while the copying takes place.
    MutexLock r_native_image_lock(r_native_image.internal_->mutex);
    MutexLock lock(internal_->mutex);
    internal_ = new NativeAdImageInternal();

    internal_->uri = r_native_image.internal_->uri;
    internal_->scale = r_native_image.internal_->scale;
    if (r_native_image.internal_->native_ad_image != nullptr) {
      internal_->native_ad_image =
          env->NewGlobalRef(r_native_image.internal_->native_ad_image);
    }
    if (preexisting_internal->native_ad_image) {
      env->DeleteGlobalRef(preexisting_internal->native_ad_image);
      preexisting_internal->native_ad_image = nullptr;
    }
  }

  // Deleting the internal_ deletes the mutex within it, so we wait for
  // complete deletion until after the mutex lock leaves scope.
  delete preexisting_internal;

  return *this;
}

NativeAdImage::~NativeAdImage() {
  FIREBASE_ASSERT(internal_);

  if (internal_->native_ad_image != nullptr) {
    JNIEnv* env = GetJNI();
    FIREBASE_ASSERT(env);
    env->DeleteGlobalRef(internal_->native_ad_image);
    internal_->native_ad_image = nullptr;
  }

  delete internal_;
  internal_ = nullptr;
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

/// Gets the auto loaded image as a vector of bytes.
const std::vector<unsigned char> NativeAdImage::image() const {
  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);
  FIREBASE_ASSERT(internal_);

  std::vector<unsigned char> img_data;
  if (internal_->uri.empty()) {
    return img_data;
  }

  jstring uri_jstring = env->NewStringUTF(internal_->uri.c_str());
  jobject helper = env->NewObject(
      download_helper::GetClass(),
      download_helper::GetMethodId(download_helper::kConstructor), uri_jstring);

  FIREBASE_ASSERT(helper);
  env->DeleteLocalRef(uri_jstring);
  if (util::CheckAndClearJniExceptions(env)) {
    if (helper) env->DeleteLocalRef(helper);
    return img_data;
  }

  jobject img_bytes = env->CallObjectMethod(
      helper, download_helper::GetMethodId(download_helper::kDownload));

  FIREBASE_ASSERT(img_bytes);
  if (util::CheckAndClearJniExceptions(env)) {
    if (helper) env->DeleteLocalRef(helper);
    img_bytes = nullptr;
    return img_data;
  }

  env->DeleteLocalRef(helper);
  return util::JniByteArrayToVector(env, img_bytes);
}

}  // namespace gma
}  // namespace firebase
