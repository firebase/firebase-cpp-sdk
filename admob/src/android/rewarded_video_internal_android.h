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

#ifndef FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_REWARDED_VIDEO_INTERNAL_ANDROID_H_
#define FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_REWARDED_VIDEO_INTERNAL_ANDROID_H_

#include "admob/src/common/rewarded_video_internal.h"
#include "app/src/util_android.h"

namespace firebase {
namespace admob {
namespace rewarded_video {

// Used to set up the cache of RewardedVideoHelper class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define REWARDEDVIDEOHELPER_METHODS(X)                                         \
  X(Constructor, "<init>", "(JLandroid/app/Activity;)V"),                      \
  X(Initialize, "initialize", "(J)V"),                                         \
  X(Destroy, "destroy", "(J)V"),                                               \
  X(Pause, "pause", "(J)V"),                                                   \
  X(Resume, "resume", "(J)V"),                                                 \
  X(Show, "show", "(J)V"),                                                     \
  X(LoadAd, "loadAd",                                                          \
    "(JLjava/lang/String;Lcom/google/android/gms/ads/AdRequest;)V"),           \
  X(GetPresentationState, "getPresentationState", "()I")
// clang-format on

METHOD_LOOKUP_DECLARATION(rewarded_video_helper, REWARDEDVIDEOHELPER_METHODS);

namespace internal {

class RewardedVideoInternalAndroid : public RewardedVideoInternal {
 public:
  RewardedVideoInternalAndroid();
  ~RewardedVideoInternalAndroid() override;

  Future<void> Initialize() override;
  Future<void> LoadAd(const char* ad_unit_id,
                      const AdRequest& request) override;
  Future<void> Show(AdParent parent) override;
  Future<void> Pause() override;
  Future<void> Resume() override;
  Future<void> Destroy() override;

  PresentationState GetPresentationState() override;

 private:
  // Reference to the Java helper object used to interact with the Mobile Ads
  // SDK.
  jobject helper_;

  // Convenience method to "dry" the JNI calls that don't take parameters beyond
  // the future callback pointer.
  Future<void> InvokeNullary(RewardedVideoFn fn,
                             rewarded_video_helper::Method method);
};

}  // namespace internal
}  // namespace rewarded_video
}  // namespace admob
}  // namespace firebase

#endif  // FIREBASE_ADMOB_CLIENT_CPP_SRC_ANDROID_REWARDED_VIDEO_INTERNAL_ANDROID_H_
