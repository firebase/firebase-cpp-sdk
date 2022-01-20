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

#ifndef FIREBASE_GMA_SRC_ANDROID_REWARDED_AD_INTERNAL_ANDROID_H_
#define FIREBASE_GMA_SRC_ANDROID_REWARDED_AD_INTERNAL_ANDROID_H_

#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/util_android.h"
#include "gma/src/common/rewarded_ad_internal.h"

namespace firebase {
namespace gma {

// Used to set up the cache of RewardedAdHelper class method IDs to reduce
// time spent looking up methods by string.
// clang-format off
#define REWARDEDADHELPER_METHODS(X)                                            \
  X(Constructor, "<init>", "(J)V"),                                            \
  X(Initialize, "initialize", "(JLandroid/app/Activity;)V"),                   \
  X(Show, "show", "(JLjava/lang/String;Ljava/lang/String;)V"),                 \
  X(LoadAd, "loadAd",                                                          \
    "(JLjava/lang/String;Lcom/google/android/gms/ads/AdRequest;)V"),           \
  X(Disconnect, "disconnect", "()V")
// clang-format on

METHOD_LOOKUP_DECLARATION(rewarded_ad_helper, REWARDEDADHELPER_METHODS);

namespace internal {

class RewardedAdInternalAndroid : public RewardedAdInternal {
 public:
  explicit RewardedAdInternalAndroid(RewardedAd* base);
  ~RewardedAdInternalAndroid() override;

  Future<void> Initialize(AdParent parent) override;
  Future<AdResult> LoadAd(const char* ad_unit_id,
                          const AdRequest& request) override;
  Future<void> Show(UserEarnedRewardListener* listener) override;
  bool is_initialized() const override { return initialized_; }

 private:
  // Reference to the Java helper object used to interact with the Mobile Ads
  // SDK.
  jobject helper_;

  // Tracks if this Rewarded Ad has been initialized.
  bool initialized_;

  // Mutex to guard against concurrent operations;
  Mutex mutex_;
};

}  // namespace internal
}  // namespace gma
}  // namespace firebase

#endif  // FIREBASE_GMA_SRC_ANDROID_REWARDED_AD_INTERNAL_ANDROID_H_
