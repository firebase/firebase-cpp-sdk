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

#ifndef FIREBASE_ADMOB_SRC_STUB_BANNER_VIEW_INTERNAL_STUB_H_
#define FIREBASE_ADMOB_SRC_STUB_BANNER_VIEW_INTERNAL_STUB_H_

#include "admob/src/common/banner_view_internal.h"

namespace firebase {
namespace admob {
namespace internal {

/// Stub version of BannerViewInternal, for use on desktop platforms. AdMob is
/// forbidden on desktop, so this version creates and immediately completes the
/// Future for each method.
class BannerViewInternalStub : public BannerViewInternal {
 public:
  explicit BannerViewInternalStub(BannerView* base)
      : BannerViewInternal(base) {}

  ~BannerViewInternalStub() override {}

  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          const AdSize& size) override {
    return CreateAndCompleteFutureStub(kBannerViewFnInitialize);
  }

  Future<AdResult> LoadAd(const AdRequest& request) override {
    return CreateAndCompleteAdResultFutureStub(kBannerViewFnLoadAd);
  }

  BoundingBox bounding_box() const override { return BoundingBox(); }

  Future<void> SetPosition(int x, int y) override {
    return CreateAndCompleteFutureStub(kBannerViewFnSetPosition);
  }

  Future<void> SetPosition(BannerView::Position position) override {
    return CreateAndCompleteFutureStub(kBannerViewFnSetPosition);
  }

  Future<void> Hide() override {
    return CreateAndCompleteFutureStub(kBannerViewFnHide);
  }

  Future<void> Show() override {
    return CreateAndCompleteFutureStub(kBannerViewFnShow);
  }

  Future<void> Pause() override {
    return CreateAndCompleteFutureStub(kBannerViewFnPause);
  }

  Future<void> Resume() override {
    return CreateAndCompleteFutureStub(kBannerViewFnResume);
  }

  Future<void> Destroy() override {
    return CreateAndCompleteFutureStub(kBannerViewFnDestroy);
  }

  bool is_initialized() const override { return true; }

 private:
  Future<void> CreateAndCompleteFutureStub(BannerViewFn fn) {
    return CreateAndCompleteFuture(fn, kAdMobErrorNone, nullptr, &future_data_);
  }

  Future<AdResult> CreateAndCompleteAdResultFutureStub(BannerViewFn fn) {
    return CreateAndCompleteFutureWithResult(fn, kAdMobErrorNone, nullptr,
                                             &future_data_, AdResult());
  }
};

}  // namespace internal
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_SRC_STUB_BANNER_VIEW_INTERNAL_STUB_H_
