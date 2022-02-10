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

#include "gma/src/common/ad_view_internal.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/ad_view.h"

#if FIREBASE_PLATFORM_ANDROID
#include "gma/src/android/ad_view_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "gma/src/ios/ad_view_internal_ios.h"
#else
#include "gma/src/stub/ad_view_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace gma {
namespace internal {

AdViewInternal::AdViewInternal(AdView* base)
    : base_(base),
      future_data_(kAdViewFnCount),
      ad_listener_(nullptr),
      ad_size_(0, 0),
      bounding_box_listener_(nullptr),
      paid_event_listener_(nullptr) {}

AdViewInternal::~AdViewInternal() {
  ad_listener_ = nullptr;
  bounding_box_listener_ = nullptr;
  paid_event_listener_ = nullptr;
  base_ = nullptr;
}

AdViewInternal* AdViewInternal::CreateInstance(AdView* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new AdViewInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  return new AdViewInternalIOS(base);
#else
  return new AdViewInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS
}

void AdViewInternal::SetAdListener(AdListener* listener) {
  MutexLock lock(listener_mutex_);
  ad_listener_ = listener;
}

void AdViewInternal::SetBoundingBoxListener(
    AdViewBoundingBoxListener* listener) {
  MutexLock lock(listener_mutex_);
  bounding_box_listener_ = listener;
}

void AdViewInternal::SetPaidEventListener(PaidEventListener* listener) {
  MutexLock lock(listener_mutex_);
  paid_event_listener_ = listener;
}

void AdViewInternal::NotifyListenerOfBoundingBoxChange(BoundingBox box) {
  MutexLock lock(listener_mutex_);
  if (bounding_box_listener_ != nullptr) {
    bounding_box_listener_->OnBoundingBoxChanged(base_, box);
  }
}

void AdViewInternal::NotifyListenerAdClicked() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClicked();
  }
}

void AdViewInternal::NotifyListenerAdClosed() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClosed();
  }
}

void AdViewInternal::NotifyListenerAdImpression() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdImpression();
  }
}

void AdViewInternal::NotifyListenerAdOpened() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdOpened();
  }
}

void AdViewInternal::NotifyListenerOfPaidEvent(const AdValue& ad_value) {
  MutexLock lock(listener_mutex_);
  if (paid_event_listener_ != nullptr) {
    paid_event_listener_->OnPaidEvent(ad_value);
  }
}

Future<void> AdViewInternal::GetLastResult(AdViewFn fn) {
  FIREBASE_ASSERT(fn != kAdViewFnLoadAd);
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

Future<AdResult> AdViewInternal::GetLoadAdLastResult() {
  return static_cast<const Future<AdResult>&>(
      future_data_.future_impl.LastResult(kAdViewFnLoadAd));
}

void AdViewInternal::update_ad_size_dimensions(int width, int height) {
  ad_size_.width_ = width;
  ad_size_.height_ = height;
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
