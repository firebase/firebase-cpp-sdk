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

#include "admob/src/common/interstitial_ad_internal.h"

#include "admob/src/include/firebase/admob/interstitial_ad.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if FIREBASE_PLATFORM_ANDROID
#include "admob/src/android/interstitial_ad_internal_android.h"
#elif FIREBASE_PLATFORM_IOS
#include "admob/src/ios/interstitial_ad_internal_ios.h"
#else
#include "admob/src/stub/interstitial_ad_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS

namespace firebase {
namespace admob {
namespace internal {

InterstitialAdInternal::InterstitialAdInternal(InterstitialAd* base)
    : base_(base), future_data_(kInterstitialAdFnCount), listener_(nullptr) {}

InterstitialAdInternal* InterstitialAdInternal::CreateInstance(
    InterstitialAd* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new InterstitialAdInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS
  return new InterstitialAdInternalIOS(base);
#else
  return new InterstitialAdInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS
}

void InterstitialAdInternal::SetListener(InterstitialAd::Listener* listener) {
  MutexLock lock(listener_mutex_);
  listener_ = listener;
}

void InterstitialAdInternal::NotifyListenerOfPresentationStateChange(
    InterstitialAd::PresentationState state) {
  MutexLock lock(listener_mutex_);
  if (listener_ != nullptr) {
    listener_->OnPresentationStateChanged(base_, state);
  }
}

Future<void> InterstitialAdInternal::GetLastResult(InterstitialAdFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

}  // namespace internal
}  // namespace admob
}  // namespace firebase
