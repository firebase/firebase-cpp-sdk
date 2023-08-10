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

#include "gma/src/android/gma_android.h"
#include "gma/src/common/gma_common.h"
#include "gma/src/include/firebase/gma.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"
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

  // Images requested with an android user agent may return webp images. Trim
  // webp parameter from image url to get the original JPG/PNG image.
  std::size_t eq_pos = internal_->uri.rfind("=");
  std::size_t webp_pos = internal_->uri.rfind("-rw");
  if (webp_pos != std::string::npos && eq_pos != std::string::npos &&
      webp_pos > eq_pos) {
    internal_->uri.replace(webp_pos, 3, "");
  }

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

  JNIEnv* env = GetJNI();
  FIREBASE_ASSERT(env);
  if (internal_->native_ad_image != nullptr) {
    env->DeleteGlobalRef(internal_->native_ad_image);
    internal_->native_ad_image = nullptr;
  }

  if (internal_->helper != nullptr) {
    env->DeleteGlobalRef(internal_->helper);
    internal_->helper = nullptr;
  }
  internal_->callback_data = nullptr;

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

/// Triggers the auto loaded image and returns an ImageResult future.
Future<ImageResult> NativeAdImage::LoadImage() const {
  firebase::MutexLock lock(internal_->mutex);

  JNIEnv* env = ::firebase::gma::GetJNI();
  FIREBASE_ASSERT(env);

  if (internal_->uri.empty()) {
    return CreateAndCompleteFutureWithImageResult(
        kNativeAdImageFnLoadImage, kAdErrorCodeImageUrlMalformed,
        kImageUrlMalformedErrorMessage, &internal_->future_data, ImageResult());
  }

  jstring uri_jstring = env->NewStringUTF(internal_->uri.c_str());
  jobject helper_ref = env->NewObject(
      download_helper::GetClass(),
      download_helper::GetMethodId(download_helper::kConstructor), uri_jstring);

  FIREBASE_ASSERT(helper_ref);
  internal_->helper = env->NewGlobalRef(helper_ref);
  FIREBASE_ASSERT(internal_->helper);

  env->DeleteLocalRef(uri_jstring);
  if (util::CheckAndClearJniExceptions(env)) {
    return CreateAndCompleteFutureWithImageResult(
        kNativeAdImageFnLoadImage, kAdErrorCodeImageUrlMalformed,
        kImageUrlMalformedErrorMessage, &internal_->future_data, ImageResult());
  }

  FutureCallbackData<ImageResult>* callback_data =
      CreateImageResultFutureCallbackData(kNativeAdImageFnLoadImage,
                                          &internal_->future_data);

  Future<ImageResult> future = MakeFuture(&internal_->future_data.future_impl,
                                          callback_data->future_handle);

  env->CallVoidMethod(internal_->helper,
                      download_helper::GetMethodId(download_helper::kDownload),
                      reinterpret_cast<jlong>(callback_data));

  util::CheckAndClearJniExceptions(env);
  return future;
}

Future<ImageResult> NativeAdImage::LoadImageLastResult() const {
  return static_cast<const Future<ImageResult>&>(
      internal_->future_data.future_impl.LastResult(kNativeAdImageFnLoadImage));
}

}  // namespace gma
}  // namespace firebase
