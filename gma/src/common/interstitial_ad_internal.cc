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

#include "gma/src/common/interstitial_ad_internal.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/interstitial_ad.h"

#if FIREBASE_PLATFORM_ANDROID
#include "gma/src/android/interstitial_ad_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "gma/src/ios/interstitial_ad_internal_ios.h"
#else
#include "gma/src/stub/interstitial_ad_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace gma {
namespace internal {

InterstitialAdInternal::InterstitialAdInternal(InterstitialAd* base)
    : base_(base), future_data_(kInterstitialAdFnCount) {}

InterstitialAdInternal* InterstitialAdInternal::CreateInstance(
    InterstitialAd* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new InterstitialAdInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  return new InterstitialAdInternalIOS(base);
#else
  return new InterstitialAdInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS
}

Future<void> InterstitialAdInternal::GetLastResult(InterstitialAdFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

Future<AdResult> InterstitialAdInternal::GetLoadAdLastResult() {
  return static_cast<const Future<AdResult>&>(
      future_data_.future_impl.LastResult(kInterstitialAdFnLoadAd));
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
