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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_REWARDED_VIDEO_INTERNAL_STUB_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_REWARDED_VIDEO_INTERNAL_STUB_H_

#include "admob/src/common/rewarded_video_internal.h"

namespace firebase {
namespace admob {
namespace rewarded_video {
namespace internal {

/// Stub version of RewardedVideoInternal, for use on desktop platforms. AdMob
/// is forbidden on desktop, so this version just creates and immediately
/// completes the Future for each method.
class RewardedVideoInternalStub : public RewardedVideoInternal {
 public:
  RewardedVideoInternalStub() : RewardedVideoInternal() {}

  Future<void> Initialize() override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnInitialize);
  }

  Future<void> LoadAd(const char* ad_unit_id,
                      const AdRequest& request) override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnLoadAd);
  }

  Future<void> Show(AdParent parent) override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnShow);
  }

  Future<void> Pause() override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnPause);
  }

  Future<void> Resume() override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnResume);
  }

  Future<void> Destroy() override {
    return CreateAndCompleteFutureStub(kRewardedVideoFnDestroy);
  }

  PresentationState GetPresentationState() override {
    return PresentationState::kPresentationStateHidden;
  }

 private:
  Future<void> CreateAndCompleteFutureStub(RewardedVideoFn fn) {
    CreateAndCompleteFuture(fn, kAdMobErrorNone, nullptr, &future_data_);
    return GetLastResult(fn);
  }
};

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_STUB_REWARDED_VIDEO_INTERNAL_STUB_H_
