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

#include "admob/src/common/rewarded_ad_internal.h"

#include "admob/src/include/firebase/admob/rewarded_ad.h"
#include "app/src/include/firebase/future.h"
#include "app/src/include/firebase/internal/platform.h"
#include "app/src/mutex.h"
#include "app/src/reference_counted_future_impl.h"

#if FIREBASE_PLATFORM_ANDROID
#include "admob/src/android/rewarded_ad_internal_android.h"
#elif FIREBASE_PLATFORM_IOS || FIREBASE_PLATFORM_TVOS
#include "admob/src/ios/rewarded_ad_internal_ios.h"
#else
#include "admob/src/stub/rewarded_ad_internal_stub.h"
#endif  // FIREBASE_PLATFORM_ANDROID, FIREBASE_PLATFORM_IOS,
        // FIREBASE_PLATFORM_TVOS

namespace firebase {
namespace admob {
namespace internal {

RewardedAdInternal::RewardedAdInternal(RewardedAd* base)
    : base_(base),
      future_data_(kRewardedAdFnCount),
      full_screen_content_listener_(nullptr),
      paid_event_listener_(nullptr),
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

void RewardedAdInternal::SetFullScreenContentListener(
    FullScreenContentListener* listener) {
  MutexLock lock(listener_mutex_);
  full_screen_content_listener_ = listener;
}

void RewardedAdInternal::SetPaidEventListener(PaidEventListener* listener) {
  MutexLock lock(listener_mutex_);
  paid_event_listener_ = listener;
}

void RewardedAdInternal::NotifyListenerOfUserEarnedReward(
    const std::string& type, int64_t amount) {
  MutexLock lock(listener_mutex_);
  if (user_earned_reward_listener_ != nullptr) {
    user_earned_reward_listener_->OnUserEarnedReward(AdReward(type, amount));
  }
}

void RewardedAdInternal::NotifyListenerOfAdClickedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdClicked();
  }
}

void RewardedAdInternal::NotifyListenerOfAdDismissedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdDismissedFullScreenContent();
  }
}

void RewardedAdInternal::NotifyListenerOfAdFailedToShowFullScreenContent(
    const AdResult& ad_result) {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdFailedToShowFullScreenContent(ad_result);
  }
}

void RewardedAdInternal::NotifyListenerOfAdImpression() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdImpression();
  }
}

void RewardedAdInternal::NotifyListenerOfAdShowedFullScreenContent() {
  MutexLock lock(listener_mutex_);
  if (full_screen_content_listener_ != nullptr) {
    full_screen_content_listener_->OnAdShowedFullScreenContent();
  }
}

void RewardedAdInternal::NotifyListenerOfPaidEvent(const AdValue& ad_value) {
  MutexLock lock(listener_mutex_);
  if (paid_event_listener_ != nullptr) {
    paid_event_listener_->OnPaidEvent(ad_value);
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

}  // namespace internal
}  // namespace admob
}  // namespace firebase
