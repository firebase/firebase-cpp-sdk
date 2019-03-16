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
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if defined(__APPLE__)
#include "TargetConditionals.h"
#endif  // __APPLE__

#if defined(__ANDROID__)
#include "admob/src/android/interstitial_ad_internal_android.h"
#elif TARGET_OS_IPHONE
#include "admob/src/ios/interstitial_ad_internal_ios.h"
#else
#include "admob/src/stub/interstitial_ad_internal_stub.h"
#endif  // __ANDROID__, TARGET_OS_IPHONE

namespace firebase {
namespace admob {
namespace internal {

InterstitialAdInternal::InterstitialAdInternal(InterstitialAd* base)
    : base_(base), future_data_(kInterstitialAdFnCount), listener_(nullptr) {}

InterstitialAdInternal* InterstitialAdInternal::CreateInstance(
    InterstitialAd* base) {
#if defined(__ANDROID__)
  return new InterstitialAdInternalAndroid(base);
#elif TARGET_OS_IPHONE
  return new InterstitialAdInternalIOS(base);
#else
  return new InterstitialAdInternalStub(base);
#endif  // __ANDROID__, TARGET_OS_IPHONE
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
