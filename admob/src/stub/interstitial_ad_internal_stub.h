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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_INTERSTITIAL_AD_INTERNAL_STUB_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_INTERSTITIAL_AD_INTERNAL_STUB_H_

#include "admob/src/common/interstitial_ad_internal.h"

namespace firebase {
namespace admob {
namespace internal {

/// Stub version of InterstitialAdInternal, for use on desktop platforms. AdMob
/// is forbidden on desktop, so this version creates and immediately completes
/// the Future for each method.
class InterstitialAdInternalStub : public InterstitialAdInternal {
 public:
  explicit InterstitialAdInternalStub(InterstitialAd* base)
      : InterstitialAdInternal(base) {}

  ~InterstitialAdInternalStub() override {}

  Future<void> Initialize(AdParent parent, const char* ad_unit_id) override {
    return CreateAndCompleteFutureStub(kInterstitialAdFnInitialize);
  }

  Future<void> LoadAd(const AdRequest& request) override {
    return CreateAndCompleteFutureStub(kInterstitialAdFnLoadAd);
  }

  Future<void> Show() override {
    return CreateAndCompleteFutureStub(kInterstitialAdFnShow);
  }

  InterstitialAd::PresentationState GetPresentationState() const override {
    return InterstitialAd::PresentationState::kPresentationStateHidden;
  }

 private:
  Future<void> CreateAndCompleteFutureStub(InterstitialAdFn fn) {
    CreateAndCompleteFuture(fn, kAdMobErrorNone, nullptr, &future_data_);
    return GetLastResult(fn);
  }
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_INTERSTITIAL_AD_INTERNAL_STUB_H_
