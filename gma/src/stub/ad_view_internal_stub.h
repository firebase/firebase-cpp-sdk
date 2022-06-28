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

#ifndef FIREBASE_GMA_SRC_STUB_AD_VIEW_INTERNAL_STUB_H_
#define FIREBASE_GMA_SRC_STUB_AD_VIEW_INTERNAL_STUB_H_

#include "gma/src/common/ad_view_internal.h"

namespace firebase {
namespace gma {
namespace internal {

/// Stub version of AdViewInternal, for use on desktop platforms. GMA is
/// forbidden on desktop, so this version creates and immediately completes the
/// Future for each method.
class AdViewInternalStub : public AdViewInternal {
 public:
  explicit AdViewInternalStub(AdView* base) : AdViewInternal(base) {}

  ~AdViewInternalStub() override {}

  Future<void> Initialize(AdParent parent, const char* ad_unit_id,
                          const AdSize& size) override {
    return CreateAndCompleteFutureStub(kAdViewFnInitialize);
  }

  Future<AdResult> LoadAd(const AdRequest& request) override {
    return CreateAndCompleteAdResultFutureStub(kAdViewFnLoadAd);
  }

  BoundingBox bounding_box() const override { return BoundingBox(); }

  Future<void> SetPosition(int x, int y) override {
    return CreateAndCompleteFutureStub(kAdViewFnSetPosition);
  }

  Future<void> SetPosition(AdView::Position position) override {
    return CreateAndCompleteFutureStub(kAdViewFnSetPosition);
  }

  Future<void> Hide() override {
    return CreateAndCompleteFutureStub(kAdViewFnHide);
  }

  Future<void> Show() override {
    return CreateAndCompleteFutureStub(kAdViewFnShow);
  }

  Future<void> Pause() override {
    return CreateAndCompleteFutureStub(kAdViewFnPause);
  }

  Future<void> Resume() override {
    return CreateAndCompleteFutureStub(kAdViewFnResume);
  }

  Future<void> Destroy() override {
    return CreateAndCompleteFutureStub(kAdViewFnDestroy);
  }

  bool is_initialized() const override { return true; }

  AdSize ad_size() const { return AdSize(0, 0); }

 private:
  Future<void> CreateAndCompleteFutureStub(AdViewFn fn) {
    return CreateAndCompleteFuture(fn, kAdErrorCodeNone, nullptr,
                                   &future_data_);
  }

  Future<AdResult> CreateAndCompleteAdResultFutureStub(AdViewFn fn) {
    return CreateAndCompleteFutureWithResult(fn, kAdErrorCodeNone, nullptr,
                                             &future_data_, AdResult());
  }
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_STUB_AD_VIEW_INTERNAL_STUB_H_
