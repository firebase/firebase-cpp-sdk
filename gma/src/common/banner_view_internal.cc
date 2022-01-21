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

#include "gma/src/common/banner_view_internal.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/banner_view.h"

#if FIREBASE_PLATFORM_ANDROID
#include "gma/src/android/banner_view_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "gma/src/ios/banner_view_internal_ios.h"
#else
#include "gma/src/stub/banner_view_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace gma {
namespace internal {

BannerViewInternal::BannerViewInternal(BannerView* base)
    : base_(base),
      future_data_(kBannerViewFnCount),
      ad_listener_(nullptr),
      bounding_box_listener_(nullptr),
      paid_event_listener_(nullptr) {}

BannerViewInternal::~BannerViewInternal() {
  ad_listener_ = nullptr;
  bounding_box_listener_ = nullptr;
  paid_event_listener_ = nullptr;
}

BannerViewInternal* BannerViewInternal::CreateInstance(BannerView* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new BannerViewInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  return new BannerViewInternalIOS(base);
#else
  return new BannerViewInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS
}

void BannerViewInternal::SetAdListener(AdListener* listener) {
  MutexLock lock(listener_mutex_);
  ad_listener_ = listener;
}

void BannerViewInternal::SetBoundingBoxListener(
    AdViewBoundingBoxListener* listener) {
  MutexLock lock(listener_mutex_);
  bounding_box_listener_ = listener;
}

void BannerViewInternal::SetPaidEventListener(PaidEventListener* listener) {
  MutexLock lock(listener_mutex_);
  paid_event_listener_ = listener;
}

void BannerViewInternal::NotifyListenerOfBoundingBoxChange(BoundingBox box) {
  MutexLock lock(listener_mutex_);
  if (bounding_box_listener_ != nullptr) {
    bounding_box_listener_->OnBoundingBoxChanged(base_, box);
  }
}

void BannerViewInternal::NotifyListenerAdClicked() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClicked();
  }
}

void BannerViewInternal::NotifyListenerAdClosed() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdClosed();
  }
}

void BannerViewInternal::NotifyListenerAdImpression() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdImpression();
  }
}

void BannerViewInternal::NotifyListenerAdOpened() {
  MutexLock lock(listener_mutex_);
  if (ad_listener_ != nullptr) {
    ad_listener_->OnAdOpened();
  }
}

void BannerViewInternal::NotifyListenerOfPaidEvent(const AdValue& ad_value) {
  MutexLock lock(listener_mutex_);
  if (paid_event_listener_ != nullptr) {
    paid_event_listener_->OnPaidEvent(ad_value);
  }
}

Future<void> BannerViewInternal::GetLastResult(BannerViewFn fn) {
  FIREBASE_ASSERT(fn != kBannerViewFnLoadAd);
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

Future<AdResult> BannerViewInternal::GetLoadAdLastResult() {
  return static_cast<const Future<AdResult>&>(
      future_data_.future_impl.LastResult(kBannerViewFnLoadAd));
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
