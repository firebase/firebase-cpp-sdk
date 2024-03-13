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

#include "gma/src/common/native_ad_internal.h"

#include <string>

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/internal/native_ad.h"

#if FIREBASE_PLATFORM_ANDROID
#include "gma/src/android/native_ad_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "gma/src/ios/native_ad_internal_ios.h"
#else
#include "gma/src/stub/native_ad_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace gma {
namespace internal {

NativeAdInternal::NativeAdInternal(NativeAd* base)
    : base_(base), future_data_(kNativeAdFnCount), ad_listener_(nullptr) {}

NativeAdInternal* NativeAdInternal::CreateInstance(NativeAd* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new NativeAdInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  return new NativeAdInternalIOS(base);
#else
  return new NativeAdInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS
}

Future<void> NativeAdInternal::GetLastResult(NativeAdFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

Future<AdResult> NativeAdInternal::GetLoadAdLastResult() {
  return static_cast<const Future<AdResult>&>(
      future_data_.future_impl.LastResult(kNativeAdFnLoadAd));
}

void NativeAdInternal::SetAdListener(AdListener* listener) {
  MutexLock lock(listener_mutex_);
  ad_listener_ = listener;
}

void NativeAdInternal::NotifyListenerAdClicked() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClicked();
  }
}

void NativeAdInternal::NotifyListenerAdClosed() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClosed();
  }
}

void NativeAdInternal::NotifyListenerAdImpression() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdImpression();
  }
}

void NativeAdInternal::NotifyListenerAdOpened() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdOpened();
  }
}

void NativeAdInternal::insert_image(const NativeAdImage& image,
                                    const std::string& image_type) {
  if (image_type == "icon") {
    icon_ = image;
  } else if (image_type == "adchoices_icon") {
    adchoices_icon_ = image;
  } else {
    images_.push_back(image);
  }
}

void NativeAdInternal::clear_existing_images() { images_.clear(); }

}  // namespace internal
}  // namespace gma
}  // namespace firebase
