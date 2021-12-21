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

#include "admob/src/common/banner_view_internal.h"

#include "admob/src/include/firebase/admob/banner_view.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if FIREBASE_PLATFORM_ANDROID
#include "admob/src/android/banner_view_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "admob/src/ios/banner_view_internal_ios.h"
#else
#include "admob/src/stub/banner_view_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace admob {
namespace internal {

BannerViewInternal::BannerViewInternal(BannerView* base)
    : base_(base), future_data_(kBannerViewFnCount), listener_(nullptr) {}

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

void BannerViewInternal::SetListener(BannerView::Listener* listener) {
  MutexLock lock(listener_mutex_);
  listener_ = listener;
}

void BannerViewInternal::NotifyListenerOfPresentationStateChange(
    BannerView::PresentationState state) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnPresentationStateChanged(base_, state);
  }
}

void BannerViewInternal::NotifyListenerOfBoundingBoxChange(BoundingBox box) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnBoundingBoxChanged(base_, box);
  }
}

Future<void> BannerViewInternal::GetLastResult(BannerViewFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
