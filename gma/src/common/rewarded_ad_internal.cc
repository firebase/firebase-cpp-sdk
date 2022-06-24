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

#include "gma/src/common/rewarded_ad_internal.h"

#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/mutex.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/reference_counted_future_impl.h"
#include "gma/src/include/firebase/gma/rewarded_ad.h"

#if FIREBASE_PLATFORM_ANDROID
#include "gma/src/android/rewarded_ad_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "gma/src/ios/rewarded_ad_internal_ios.h"
#else
#include "gma/src/stub/rewarded_ad_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

#include <string>

namespace firebase {
namespace gma {
namespace internal {

RewardedAdInternal::RewardedAdInternal(RewardedAd* base)
    : base_(base),
      future_data_(kRewardedAdFnCount),
      user_earned_reward_listener_(nullptr) {}

RewardedAdInternal* RewardedAdInternal::CreateInstance(RewardedAd* base) {
#if FIREBASE_PLATFORM_ANDROID
  return new RewardedAdInternalAndroid(base);
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
  return new RewardedAdInternalIOS(base);
#else
  return new RewardedAdInternalStub(base);
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS
}

void RewardedAdInternal::NotifyListenerOfUserEarnedReward(
    const std::string& type, int64_t amount) {
  MutexLock lock(listener_mutex_);
  if (user_earned_reward_listener_ != nullptr) {
    user_earned_reward_listener_->OnUserEarnedReward(AdReward(type, amount));
  }
}

Future<void> RewardedAdInternal::GetLastResult(RewardedAdFn fn) {
  return static_cast<const Future<void>&>(
      future_data_.future_impl.LastResult(fn));
}

Future<AdResult> RewardedAdInternal::GetLoadAdLastResult() {
  return static_cast<const Future<AdResult>&>(
      future_data_.future_impl.LastResult(kRewardedAdFnLoadAd));
}

void RewardedAdInternal::SetServerSideVerificationOptions(
    const RewardedAd::ServerSideVerificationOptions&
        serverSideVerificationOptions) {
  serverSideVerificationOptions_ = serverSideVerificationOptions;
}

}  // namespace internal
}  // namespace gma
}  // namespace firebase
