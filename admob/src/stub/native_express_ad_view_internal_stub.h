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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_NATIVE_EXPRESS_AD_VIEW_INTERNAL_STUB_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_NATIVE_EXPRESS_AD_VIEW_INTERNAL_STUB_H_

#include "admob/src/common/native_express_ad_view_internal.h"

namespace firebase {
namespace admob {
namespace internal {

/// Stub version of NativeExpressAdViewInternal, for use on desktop platforms.
/// AdMob is forbidden on desktop, so this version creates and immediately
/// completes the Future for each method.
class NativeExpressAdViewInternalStub : public NativeExpressAdViewInternal {
 public:
  explicit NativeExpressAdViewInternalStub(NativeExpressAdView* base)
      : NativeExpressAdViewInternal(base) {}

  ~NativeExpressAdViewInternalStub() override {}

  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          AdSize size) override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnInitialize);
  }

  Future<void> LoadAd(const AdRequest& request) override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnLoadAd);
  }

  Future<void> Hide() override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnHide);
  }

  Future<void> Show() override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnShow);
  }

  Future<void> Pause() override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnPause);
  }

  Future<void> Resume() override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnResume);
  }

  Future<void> Destroy() override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnDestroy);
  }

  Future<void> MoveTo(int x, int y) override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnMoveTo);
  }

  Future<void> MoveTo(NativeExpressAdView::Position position) override {
    return CreateAndCompleteFutureStub(kNativeExpressAdViewFnMoveTo);
  }

  NativeExpressAdView::PresentationState GetPresentationState() const override {
    return NativeExpressAdView::PresentationState::kPresentationStateHidden;
  }

  BoundingBox GetBoundingBox() const override { return BoundingBox(); }

 private:
  Future<void> CreateAndCompleteFutureStub(NativeExpressAdViewFn fn) {
    CreateAndCompleteFuture(fn, kAdMobErrorNone, nullptr, &future_data_);
    return GetLastResult(fn);
  }
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_NATIVE_EXPRESS_AD_VIEW_INTERNAL_STUB_H_
